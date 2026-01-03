// Flex Compiler - Bytecode Compiler Expression Visitors

#include "bytecode_base.h"

namespace flex {

void Compiler::visit(IntegerLiteral& node) {
    int idx = addConstant(Value(node.value));
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(FloatLiteral& node) {
    int idx = addConstant(Value(node.value));
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(StringLiteral& node) {
    int idx = addConstant(Value(node.value));
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(InterpolatedString& node) {
    bool first = true;
    for (auto& part : node.parts) {
        if (auto* str = std::get_if<std::string>(&part)) {
            if (!str->empty()) {
                int idx = addConstant(Value(*str));
                emitOp(OpCode::CONST, idx);
                if (!first) emit(OpCode::ADD);
                first = false;
            }
        } else if (auto* expr = std::get_if<ExprPtr>(&part)) {
            (*expr)->accept(*this);
            if (!first) emit(OpCode::ADD);
            first = false;
        }
    }
    if (first) {
        int idx = addConstant(Value(std::string("")));
        emitOp(OpCode::CONST, idx);
    }
}

void Compiler::visit(BoolLiteral& node) {
    int idx = addConstant(Value(node.value));
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(NilLiteral& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(Identifier& node) {
    int local = resolveLocal(node.name);
    if (local != -1) {
        emitOp(OpCode::LOAD_LOCAL, local);
    } else {
        int nameIdx = addConstant(Value(node.name));
        emitOp(OpCode::LOAD_GLOBAL, nameIdx);
    }
}

void Compiler::visit(BinaryExpr& node) {
    if (node.op == TokenType::QUESTION_QUESTION) {
        node.left->accept(*this);
        emit(OpCode::DUP);
        size_t skipJump = emitJump(OpCode::JUMP_IF_TRUE);
        emit(OpCode::POP);
        node.right->accept(*this);
        patchJump(skipJump);
        return;
    }
    
    node.left->accept(*this);
    node.right->accept(*this);
    
    switch (node.op) {
        case TokenType::PLUS: emit(OpCode::ADD); break;
        case TokenType::MINUS: emit(OpCode::SUB); break;
        case TokenType::STAR: emit(OpCode::MUL); break;
        case TokenType::SLASH: emit(OpCode::DIV); break;
        case TokenType::PERCENT: emit(OpCode::MOD); break;
        case TokenType::EQ: emit(OpCode::EQ); break;
        case TokenType::NE: emit(OpCode::NE); break;
        case TokenType::LT: emit(OpCode::LT); break;
        case TokenType::GT: emit(OpCode::GT); break;
        case TokenType::LE: emit(OpCode::LE); break;
        case TokenType::GE: emit(OpCode::GE); break;
        case TokenType::AND: emit(OpCode::AND); break;
        case TokenType::OR: emit(OpCode::OR); break;
        default: throw FlexError("Unknown binary operator", node.location);
    }
}

void Compiler::visit(UnaryExpr& node) {
    node.operand->accept(*this);
    switch (node.op) {
        case TokenType::MINUS: emit(OpCode::NEG); break;
        case TokenType::NOT: emit(OpCode::NOT); break;
        default: throw FlexError("Unknown unary operator", node.location);
    }
}

void Compiler::visit(CallExpr& node) {
    node.callee->accept(*this);
    for (auto& arg : node.args) arg->accept(*this);
    for (auto& p : node.namedArgs) p.second->accept(*this);
    int argCount = static_cast<int>(node.args.size() + node.namedArgs.size());
    emitOp(OpCode::CALL, argCount);
}

void Compiler::visit(MemberExpr& node) {
    node.object->accept(*this);
    int idx = addConstant(Value(node.member));
    emitOp(OpCode::GET_MEMBER, idx);
}

void Compiler::visit(IndexExpr& node) {
    node.object->accept(*this);
    node.index->accept(*this);
    emit(OpCode::GET_INDEX);
}

void Compiler::visit(ListExpr& node) {
    for (auto& elem : node.elements) elem->accept(*this);
    emitOp(OpCode::MAKE_LIST, static_cast<int32_t>(node.elements.size()));
}

void Compiler::visit(RecordExpr& node) {
    for (auto& p : node.fields) {
        int idx = addConstant(Value(p.first));
        emitOp(OpCode::CONST, idx);
        p.second->accept(*this);
    }
    emitOp(OpCode::MAKE_RECORD, static_cast<int32_t>(node.fields.size()));
}

void Compiler::visit(MapExpr& node) {
    // Push all key-value pairs onto stack
    for (auto& entry : node.entries) {
        entry.first->accept(*this);   // key
        entry.second->accept(*this);  // value
    }
    emitOp(OpCode::MAKE_MAP, static_cast<int32_t>(node.entries.size()));
}

void Compiler::visit(RangeExpr& node) {
    node.start->accept(*this);
    node.end->accept(*this);
    if (node.step) {
        node.step->accept(*this);
    } else {
        int idx = addConstant(Value((int64_t)1));
        emitOp(OpCode::CONST, idx);
    }
    emit(OpCode::MAKE_RANGE);
}

void Compiler::visit(LambdaExpr& node) {
    node.body->accept(*this);
}

void Compiler::visit(TernaryExpr& node) {
    node.condition->accept(*this);
    size_t thenJump = emitJump(OpCode::JUMP_IF_FALSE);
    node.thenExpr->accept(*this);
    size_t elseJump = emitJump(OpCode::JUMP);
    patchJump(thenJump);
    node.elseExpr->accept(*this);
    patchJump(elseJump);
}

void Compiler::visit(ListCompExpr& node) {
    emitOp(OpCode::MAKE_LIST, 0);
    node.iterable->accept(*this);
    emit(OpCode::GET_ITER);
    
    size_t loopStart = chunk.currentOffset();
    emit(OpCode::DUP);
    emit(OpCode::ITER_NEXT);
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    beginScope();
    declareLocal(node.var);
    
    if (node.condition) {
        node.condition->accept(*this);
        size_t skipJump = emitJump(OpCode::JUMP_IF_FALSE);
        node.expr->accept(*this);
        emit(OpCode::POP);
        patchJump(skipJump);
    } else {
        node.expr->accept(*this);
        emit(OpCode::POP);
    }
    
    endScope();
    emitLoop(loopStart);
    patchJump(exitJump);
    emit(OpCode::POP);
}

void Compiler::visit(AddressOfExpr& node) { node.operand->accept(*this); }
void Compiler::visit(DerefExpr& node) { node.operand->accept(*this); }
void Compiler::visit(NewExpr& node) { 
    for (auto& arg : node.args) arg->accept(*this);
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}
void Compiler::visit(CastExpr& node) { node.expr->accept(*this); }
void Compiler::visit(AwaitExpr& node) { node.operand->accept(*this); }
void Compiler::visit(SpawnExpr& node) { node.operand->accept(*this); }
void Compiler::visit(DSLBlock& node) { 
    int idx = addConstant(Value(node.rawContent));
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(AssignExpr& node) {
    // Handle compound assignment (basic implementation for now)
    if (node.op != TokenType::ASSIGN) {
        // Load original value
        node.target->accept(*this);
        // Load new value
        node.value->accept(*this);
        // Binary op
        switch (node.op) {
            case TokenType::PLUS_ASSIGN: emit(OpCode::ADD); break;
            case TokenType::MINUS_ASSIGN: emit(OpCode::SUB); break;
            case TokenType::STAR_ASSIGN: emit(OpCode::MUL); break;
            case TokenType::SLASH_ASSIGN: emit(OpCode::DIV); break;
            default: break;
        }
    } else {
        node.value->accept(*this);
    }
    
    emit(OpCode::DUP); // Result of assignment is the value
    
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        int local = resolveLocal(id->name);
        if (local != -1) {
            emitOp(OpCode::STORE_LOCAL, local);
        } else {
            int nameIdx = addConstant(Value(id->name));
            emitOp(OpCode::STORE_GLOBAL, nameIdx);
        }
    } else if (auto* mem = dynamic_cast<MemberExpr*>(node.target.get())) {
        mem->object->accept(*this);
        int idx = addConstant(Value(mem->member));
        emitOp(OpCode::SET_MEMBER, idx);
    } else if (auto* idxExpr = dynamic_cast<IndexExpr*>(node.target.get())) {
        idxExpr->object->accept(*this);
        idxExpr->index->accept(*this);
        emit(OpCode::SET_INDEX);
    }
}

void Compiler::visit(PropagateExpr& node) {
    // Error propagation operator (?)
    // For bytecode VM, we need to:
    // 1. Evaluate the operand
    // 2. Check if it's an error
    // 3. If error, return early
    // 4. If Ok, unwrap the value
    
    // For now, just evaluate the operand and unwrap
    // (full implementation would need VM support for early return)
    node.operand->accept(*this);
    
    // The VM would need to handle Result types properly
    // For now, this is a simplified implementation
}

void Compiler::visit(ChanSendExpr& node) {
    // Channel send: ch <- value
    // For bytecode VM, we need to call a builtin
    node.channel->accept(*this);
    node.value->accept(*this);
    emit(OpCode::CHAN_SEND);
}

void Compiler::visit(ChanRecvExpr& node) {
    // Channel receive: <- ch
    node.channel->accept(*this);
    emit(OpCode::CHAN_RECV);
}

void Compiler::visit(MakeChanExpr& node) {
    // Create a new channel
    int typeIdx = addConstant(Value(node.elementType));
    emitOp(OpCode::CONST, typeIdx);
    int sizeIdx = addConstant(Value(node.bufferSize));
    emitOp(OpCode::CONST, sizeIdx);
    emit(OpCode::MAKE_CHAN);
}

// Synchronization primitives - stub implementations for bytecode VM
// These are primarily implemented in native codegen

void Compiler::visit(MakeMutexExpr& node) {
    (void)node;
    // Stub - native codegen handles this
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(MakeRWLockExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(MakeCondExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(MakeSemaphoreExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(MutexLockExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(MutexUnlockExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(RWLockReadExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(RWLockWriteExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(RWLockUnlockExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(CondWaitExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(CondSignalExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(CondBroadcastExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(SemAcquireExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(SemReleaseExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

void Compiler::visit(SemTryAcquireExpr& node) {
    (void)node;
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
}

} // namespace flex
