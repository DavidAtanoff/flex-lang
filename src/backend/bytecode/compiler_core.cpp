// Flex Compiler - Bytecode Compiler Core
// Main compile entry point and helpers

#include "bytecode_base.h"

namespace flex {

Compiler::Compiler() {}

Chunk Compiler::compile(Program& program) {
    chunk = Chunk();
    locals.clear();
    globalNames.clear();
    globalCount = 0;
    scopeDepth = 0;
    
    program.accept(*this);
    emit(OpCode::HALT);
    
    return std::move(chunk);
}

void Compiler::emit(OpCode op) {
    chunk.emit(op);
}

void Compiler::emitOp(OpCode op, int32_t operand) {
    chunk.emitOp(op, operand);
}

size_t Compiler::emitJump(OpCode op) {
    chunk.emitOp(op, 0);
    return chunk.currentOffset() - 1;
}

void Compiler::patchJump(size_t offset) {
    int32_t jump = static_cast<int32_t>(chunk.currentOffset() - offset - 1);
    chunk.patch(offset, jump);
}

void Compiler::emitLoop(size_t loopStart) {
    emitOp(OpCode::LOOP, static_cast<int32_t>(chunk.currentOffset() - loopStart + 1));
}

int Compiler::addConstant(Value val) {
    return static_cast<int>(chunk.addConstant(std::move(val)));
}

int Compiler::resolveLocal(const std::string& name) {
    for (int i = static_cast<int>(locals.size()) - 1; i >= 0; i--) {
        if (locals[i].name == name) {
            return i;
        }
    }
    return -1;
}

int Compiler::resolveGlobal(const std::string& name) {
    auto it = globalNames.find(name);
    if (it != globalNames.end()) {
        return it->second;
    }
    int idx = globalCount++;
    globalNames[name] = idx;
    return idx;
}

void Compiler::declareLocal(const std::string& name) {
    locals.push_back({name, scopeDepth});
}

void Compiler::beginScope() {
    scopeDepth++;
}

void Compiler::endScope() {
    scopeDepth--;
    while (!locals.empty() && locals.back().depth > scopeDepth) {
        emit(OpCode::POP);
        locals.pop_back();
    }
}

void Compiler::visit(Program& node) {
    for (auto& stmt : node.statements) {
        stmt->accept(*this);
    }
}

} // namespace flex
