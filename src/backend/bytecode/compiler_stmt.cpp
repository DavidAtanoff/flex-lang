// Flex Compiler - Bytecode Compiler Statement Visitors

#include "bytecode_base.h"

namespace flex {

void Compiler::visit(ExprStmt& node) {
    node.expr->accept(*this);
    emit(OpCode::POP);
}

void Compiler::visit(VarDecl& node) {
    if (node.initializer) {
        node.initializer->accept(*this);
    } else {
        int idx = addConstant(Value());
        emitOp(OpCode::CONST, idx);
    }
    
    // Use globals for all top-level code (not inside functions)
    // This avoids scoping issues with loops
    if (scopeDepth > 0 && inFunction) {
        declareLocal(node.name);
    } else {
        int nameIdx = addConstant(Value(node.name));
        emitOp(OpCode::STORE_GLOBAL, nameIdx);
        emit(OpCode::POP);  // STORE_GLOBAL peeks, so we need to pop
    }
}

void Compiler::visit(DestructuringDecl& node) {
    node.initializer->accept(*this);
    
    if (node.kind == DestructuringDecl::Kind::TUPLE) {
        for (size_t i = 0; i < node.names.size(); i++) {
            emit(OpCode::DUP);
            int idx = addConstant(Value(static_cast<int64_t>(i)));
            emitOp(OpCode::CONST, idx);
            emit(OpCode::GET_INDEX);
            
            if (scopeDepth > 0 && inFunction) {
                declareLocal(node.names[i]);
            } else {
                int nameIdx = addConstant(Value(node.names[i]));
                emitOp(OpCode::STORE_GLOBAL, nameIdx);
                emit(OpCode::POP);
            }
        }
    } else {
        for (const auto& name : node.names) {
            emit(OpCode::DUP);
            int idx = addConstant(Value(name));
            emitOp(OpCode::GET_MEMBER, idx);
            
            if (scopeDepth > 0 && inFunction) {
                declareLocal(name);
            } else {
                int nameIdx = addConstant(Value(name));
                emitOp(OpCode::STORE_GLOBAL, nameIdx);
                emit(OpCode::POP);
            }
        }
    }
    emit(OpCode::POP);
}

void Compiler::visit(AssignStmt& node) {
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        int local = resolveLocal(id->name);
        if (local != -1) {
            if (node.op == TokenType::PLUS_ASSIGN || node.op == TokenType::MINUS_ASSIGN) {
                emitOp(OpCode::LOAD_LOCAL, local);
                node.value->accept(*this);
                if (node.op == TokenType::PLUS_ASSIGN) emit(OpCode::ADD);
                else emit(OpCode::SUB);
            } else {
                node.value->accept(*this);
            }
            emitOp(OpCode::STORE_LOCAL, local);
            emit(OpCode::POP);
        } else {
            int nameIdx = addConstant(Value(id->name));
            if (node.op == TokenType::PLUS_ASSIGN || node.op == TokenType::MINUS_ASSIGN) {
                emitOp(OpCode::LOAD_GLOBAL, nameIdx);
                node.value->accept(*this);
                if (node.op == TokenType::PLUS_ASSIGN) emit(OpCode::ADD);
                else emit(OpCode::SUB);
            } else {
                node.value->accept(*this);
            }
            emitOp(OpCode::STORE_GLOBAL, nameIdx);
            emit(OpCode::POP);
        }
    } else if (auto* member = dynamic_cast<MemberExpr*>(node.target.get())) {
        node.value->accept(*this);
        member->object->accept(*this);
        int idx = addConstant(Value(member->member));
        emitOp(OpCode::SET_MEMBER, idx);
    } else if (auto* index = dynamic_cast<IndexExpr*>(node.target.get())) {
        node.value->accept(*this);
        index->object->accept(*this);
        index->index->accept(*this);
        emit(OpCode::SET_INDEX);
    }
}

void Compiler::visit(Block& node) {
    beginScope();
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
    endScope();
}

void Compiler::visit(IfStmt& node) {
    node.condition->accept(*this);
    size_t thenJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    node.thenBranch->accept(*this);
    size_t elseJump = emitJump(OpCode::JUMP);
    
    patchJump(thenJump);
    
    std::vector<size_t> elifJumps;
    for (auto& p : node.elifBranches) {
        p.first->accept(*this);
        size_t elifThenJump = emitJump(OpCode::JUMP_IF_FALSE);
        p.second->accept(*this);
        elifJumps.push_back(emitJump(OpCode::JUMP));
        patchJump(elifThenJump);
    }
    
    if (node.elseBranch) {
        node.elseBranch->accept(*this);
    }
    
    patchJump(elseJump);
    for (size_t jump : elifJumps) {
        patchJump(jump);
    }
}

void Compiler::visit(WhileStmt& node) {
    size_t loopStart = chunk.currentOffset();
    
    // Push loop context for break/continue
    loopStack.push_back({loopStart, {}});
    
    node.condition->accept(*this);
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    node.body->accept(*this);
    emitLoop(loopStart);
    
    patchJump(exitJump);
    
    // Patch all break jumps to here
    for (size_t breakJump : loopStack.back().breakJumps) {
        patchJump(breakJump);
    }
    loopStack.pop_back();
}

void Compiler::visit(ForStmt& node) {
    node.iterable->accept(*this);
    emit(OpCode::GET_ITER);
    
    std::string iterName = "$iter_" + std::to_string(chunk.currentOffset());
    int iterIdx = addConstant(Value(iterName));
    emitOp(OpCode::STORE_GLOBAL, iterIdx);
    emit(OpCode::POP);
    
    size_t loopStart = chunk.currentOffset();
    
    // Push loop context for break/continue
    loopStack.push_back({loopStart, {}});
    
    emitOp(OpCode::LOAD_GLOBAL, iterIdx);
    emit(OpCode::ITER_NEXT);
    size_t exitJump = emitJump(OpCode::JUMP_IF_FALSE);
    
    int varIdx = addConstant(Value(node.var));
    emitOp(OpCode::STORE_GLOBAL, varIdx);
    emit(OpCode::POP);
    
    emitOp(OpCode::STORE_GLOBAL, iterIdx);
    emit(OpCode::POP);
    
    if (auto* block = dynamic_cast<Block*>(node.body.get())) {
        for (auto& stmt : block->statements) {
            stmt->accept(*this);
        }
    } else {
        node.body->accept(*this);
    }
    
    emitLoop(loopStart);
    
    patchJump(exitJump);
    emit(OpCode::POP);
    emit(OpCode::POP);
    
    // Patch all break jumps to here
    for (size_t breakJump : loopStack.back().breakJumps) {
        patchJump(breakJump);
    }
    loopStack.pop_back();
}

void Compiler::visit(MatchStmt& node) {
    node.value->accept(*this);
    
    std::vector<size_t> endJumps;
    bool hasWildcard = false;
    
    for (auto& p : node.cases) {
        if (auto* id = dynamic_cast<Identifier*>(p.pattern.get())) {
            if (id->name == "_") {
                hasWildcard = true;
                continue;
            }
        }
        
        emit(OpCode::DUP);
        p.pattern->accept(*this);
        emit(OpCode::EQ);
        size_t skipJump = emitJump(OpCode::JUMP_IF_FALSE);
        
        // TODO: Handle guards in bytecode compiler
        
        emit(OpCode::POP);
        p.body->accept(*this);
        endJumps.push_back(emitJump(OpCode::JUMP));
        
        patchJump(skipJump);
    }
    
    emit(OpCode::POP);
    
    if (hasWildcard) {
        for (auto& p : node.cases) {
            if (auto* id = dynamic_cast<Identifier*>(p.pattern.get())) {
                if (id->name == "_") {
                    p.body->accept(*this);
                    break;
                }
            }
        }
    } else if (node.defaultCase) {
        node.defaultCase->accept(*this);
    }
    
    for (size_t jump : endJumps) {
        patchJump(jump);
    }
}

void Compiler::visit(ReturnStmt& node) {
    if (node.value) {
        node.value->accept(*this);
    } else {
        int idx = addConstant(Value());
        emitOp(OpCode::CONST, idx);
    }
    emit(OpCode::RETURN);
}

void Compiler::visit(TryStmt& node) {
    node.tryExpr->accept(*this);
}

void Compiler::visit(BreakStmt&) {
    if (!loopStack.empty()) {
        // Jump to end of loop (will be patched later)
        size_t jump = emitJump(OpCode::JUMP);
        loopStack.back().breakJumps.push_back(jump);
    }
}

void Compiler::visit(ContinueStmt&) {
    if (!loopStack.empty()) {
        // Jump back to loop start
        emitLoop(loopStack.back().loopStart);
    }
}
void Compiler::visit(DeleteStmt& node) { node.expr->accept(*this); emit(OpCode::POP); }
void Compiler::visit(LockStmt& node) { 
    // Stub - native codegen handles this properly
    // In bytecode mode, just execute the body without locking
    node.body->accept(*this);
}
void Compiler::visit(AsmStmt& node) { (void)node; /* Inline assembly not supported in bytecode mode */ }

} // namespace flex
