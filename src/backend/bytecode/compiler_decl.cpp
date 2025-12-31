// Flex Compiler - Bytecode Compiler Declaration Visitors

#include "bytecode_base.h"

namespace flex {

void Compiler::visit(FnDecl& node) {
    auto func = std::make_shared<FlexFunction>();
    func->name = node.name;
    for (auto& p : node.params) {
        func->params.push_back(p.first);
    }
    
    size_t skipJump = emitJump(OpCode::JUMP);
    
    func->codeStart = chunk.currentOffset();
    
    beginScope();
    inFunction = true;
    
    for (auto& p : node.params) {
        declareLocal(p.first);
    }
    
    node.body->accept(*this);
    
    int idx = addConstant(Value());
    emitOp(OpCode::CONST, idx);
    emit(OpCode::RETURN);
    
    func->codeEnd = chunk.currentOffset();
    inFunction = false;
    endScope();
    
    patchJump(skipJump);
    
    int funcIdx = addConstant(Value(func));
    emitOp(OpCode::CONST, funcIdx);
    int nameIdx = addConstant(Value(node.name));
    emitOp(OpCode::STORE_GLOBAL, nameIdx);
}

void Compiler::visit(RecordDecl& node) {
    int idx = addConstant(Value(node.name));
    emitOp(OpCode::CONST, idx);
    int nameIdx = addConstant(Value(node.name));
    emitOp(OpCode::STORE_GLOBAL, nameIdx);
}

void Compiler::visit(UseStmt& node) { (void)node; }
void Compiler::visit(ModuleDecl& node) {
    // Compile all declarations in the module
    for (auto& stmt : node.body) {
        stmt->accept(*this);
    }
}
void Compiler::visit(EnumDecl& node) { (void)node; }
void Compiler::visit(TypeAlias& node) { (void)node; }
void Compiler::visit(TraitDecl& node) { (void)node; }

void Compiler::visit(ImplBlock& node) { 
    for (auto& method : node.methods) {
        method->accept(*this);
    }
}

void Compiler::visit(UnsafeBlock& node) { node.body->accept(*this); }
void Compiler::visit(ImportStmt& node) { (void)node; }
void Compiler::visit(ExternDecl& node) { (void)node; }
void Compiler::visit(MacroDecl& node) { (void)node; }
void Compiler::visit(SyntaxMacroDecl& node) { (void)node; }
void Compiler::visit(LayerDecl& node) { (void)node; }

} // namespace flex
