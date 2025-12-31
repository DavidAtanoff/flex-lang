// Flex Compiler - AST Printer Implementation

#include "ast_printer.h"
#include "backend/bytecode/bytecode.h"
#include <iostream>

namespace flex {

void ASTPrinter::print(const std::string& s) {
    std::cout << std::string(indent * 2, ' ') << s << "\n";
}

void ASTPrinter::visit(IntegerLiteral& n) { print("Int: " + std::to_string(n.value)); }
void ASTPrinter::visit(FloatLiteral& n) { print("Float: " + std::to_string(n.value)); }
void ASTPrinter::visit(StringLiteral& n) { print("String: \"" + n.value + "\""); }
void ASTPrinter::visit(InterpolatedString& n) { 
    print("InterpolatedString");
    indent++;
    for (auto& part : n.parts) {
        if (auto* str = std::get_if<std::string>(&part)) print("Part: \"" + *str + "\"");
        else if (auto* expr = std::get_if<ExprPtr>(&part)) (*expr)->accept(*this);
    }
    indent--;
}
void ASTPrinter::visit(BoolLiteral& n) { print("Bool: " + std::string(n.value ? "true" : "false")); }
void ASTPrinter::visit(NilLiteral&) { print("Nil"); }
void ASTPrinter::visit(Identifier& n) { print("Identifier: " + n.name); }

void ASTPrinter::visit(BinaryExpr& n) {
    print("BinaryExpr: " + tokenTypeToString(n.op));
    indent++; n.left->accept(*this); n.right->accept(*this); indent--;
}

void ASTPrinter::visit(UnaryExpr& n) {
    print("UnaryExpr: " + tokenTypeToString(n.op));
    indent++; n.operand->accept(*this); indent--;
}

void ASTPrinter::visit(CallExpr& n) {
    print("CallExpr");
    indent++; n.callee->accept(*this);
    for (auto& arg : n.args) arg->accept(*this);
    indent--;
}

void ASTPrinter::visit(MemberExpr& n) {
    print("MemberExpr: ." + n.member);
    indent++; n.object->accept(*this); indent--;
}

void ASTPrinter::visit(IndexExpr& n) {
    print("IndexExpr");
    indent++; n.object->accept(*this); n.index->accept(*this); indent--;
}

void ASTPrinter::visit(ListExpr& n) {
    print("ListExpr");
    indent++; for (auto& e : n.elements) e->accept(*this); indent--;
}


void ASTPrinter::visit(RecordExpr& n) {
    print("RecordExpr");
    indent++;
    for (auto& [name, val] : n.fields) {
        print(name + ":");
        indent++; val->accept(*this); indent--;
    }
    indent--;
}

void ASTPrinter::visit(RangeExpr& n) {
    print("RangeExpr");
    indent++; n.start->accept(*this); n.end->accept(*this);
    if (n.step) n.step->accept(*this);
    indent--;
}

void ASTPrinter::visit(LambdaExpr& n) {
    print("LambdaExpr");
    indent++; n.body->accept(*this); indent--;
}

void ASTPrinter::visit(TernaryExpr& n) {
    print("TernaryExpr");
    indent++; n.condition->accept(*this); n.thenExpr->accept(*this); n.elseExpr->accept(*this); indent--;
}

void ASTPrinter::visit(ListCompExpr& n) {
    print("ListCompExpr: " + n.var);
    indent++; n.expr->accept(*this); n.iterable->accept(*this);
    if (n.condition) n.condition->accept(*this);
    indent--;
}

void ASTPrinter::visit(AddressOfExpr& n) { print("AddressOf"); indent++; n.operand->accept(*this); indent--; }
void ASTPrinter::visit(DerefExpr& n) { print("Deref"); indent++; n.operand->accept(*this); indent--; }
void ASTPrinter::visit(NewExpr& n) {
    print("New: " + n.typeName);
    indent++; for (auto& arg : n.args) arg->accept(*this); indent--;
}
void ASTPrinter::visit(CastExpr& n) { print("Cast: " + n.targetType); indent++; n.expr->accept(*this); indent--; }
void ASTPrinter::visit(AwaitExpr& n) { print("Await"); indent++; n.operand->accept(*this); indent--; }
void ASTPrinter::visit(SpawnExpr& n) { print("Spawn"); indent++; n.operand->accept(*this); indent--; }
void ASTPrinter::visit(DSLBlock& n) {
    print("DSLBlock: " + n.dslName);
    indent++; print("Content: " + n.rawContent.substr(0, 50) + (n.rawContent.length() > 50 ? "..." : "")); indent--;
}

void ASTPrinter::visit(ExprStmt& n) { print("ExprStmt"); indent++; n.expr->accept(*this); indent--; }

void ASTPrinter::visit(VarDecl& n) {
    std::string mod = n.isConst ? "const " : (n.isMutable ? "var " : "let ");
    print("VarDecl: " + mod + n.name + (n.typeName.empty() ? "" : ": " + n.typeName));
    if (n.initializer) { indent++; n.initializer->accept(*this); indent--; }
}

void ASTPrinter::visit(DestructuringDecl& n) {
    std::string kind = n.kind == DestructuringDecl::Kind::TUPLE ? "tuple" : "record";
    std::string names;
    for (size_t i = 0; i < n.names.size(); i++) {
        if (i > 0) names += ", ";
        names += n.names[i];
    }
    print("DestructuringDecl: " + kind + " (" + names + ")");
    if (n.initializer) { indent++; n.initializer->accept(*this); indent--; }
}

void ASTPrinter::visit(AssignStmt& n) {
    print("AssignStmt: " + tokenTypeToString(n.op));
    indent++; n.target->accept(*this); n.value->accept(*this); indent--;
}

void ASTPrinter::visit(Block& n) {
    print("Block");
    indent++; for (auto& s : n.statements) s->accept(*this); indent--;
}

void ASTPrinter::visit(IfStmt& n) {
    print("IfStmt");
    indent++; n.condition->accept(*this); n.thenBranch->accept(*this);
    if (n.elseBranch) n.elseBranch->accept(*this);
    indent--;
}

void ASTPrinter::visit(WhileStmt& n) {
    print("WhileStmt");
    indent++; n.condition->accept(*this); n.body->accept(*this); indent--;
}

void ASTPrinter::visit(ForStmt& n) {
    print("ForStmt: " + n.var);
    indent++; n.iterable->accept(*this); n.body->accept(*this); indent--;
}

void ASTPrinter::visit(MatchStmt& n) {
    print("MatchStmt");
    indent++; n.value->accept(*this);
    for (auto& [pat, body] : n.cases) { pat->accept(*this); body->accept(*this); }
    indent--;
}

void ASTPrinter::visit(ReturnStmt& n) {
    print("ReturnStmt");
    if (n.value) { indent++; n.value->accept(*this); indent--; }
}

void ASTPrinter::visit(BreakStmt&) { print("BreakStmt"); }
void ASTPrinter::visit(ContinueStmt&) { print("ContinueStmt"); }

void ASTPrinter::visit(TryStmt& n) {
    print("TryStmt");
    indent++; n.tryExpr->accept(*this); n.elseExpr->accept(*this); indent--;
}

void ASTPrinter::visit(FnDecl& n) {
    std::string typeParams;
    if (!n.typeParams.empty()) {
        typeParams = "[";
        for (size_t i = 0; i < n.typeParams.size(); i++) {
            if (i > 0) typeParams += ", ";
            typeParams += n.typeParams[i];
        }
        typeParams += "]";
    }
    std::string params;
    for (auto& [name, type] : n.params) {
        if (!params.empty()) params += ", ";
        params += name;
        if (!type.empty()) params += ": " + type;
    }
    print("FnDecl: " + n.name + typeParams + "(" + params + ")" + 
          (n.returnType.empty() ? "" : " -> " + n.returnType));
    indent++; if (n.body) n.body->accept(*this); indent--;
}

void ASTPrinter::visit(RecordDecl& n) {
    std::string typeParams;
    if (!n.typeParams.empty()) {
        typeParams = "[";
        for (size_t i = 0; i < n.typeParams.size(); i++) {
            if (i > 0) typeParams += ", ";
            typeParams += n.typeParams[i];
        }
        typeParams += "]";
    }
    print("RecordDecl: " + n.name + typeParams);
    indent++; for (auto& [name, type] : n.fields) print(name + ": " + type); indent--;
}

void ASTPrinter::visit(EnumDecl& n) {
    print("EnumDecl: " + n.name);
    indent++;
    for (auto& [name, val] : n.variants) print(name + (val.has_value() ? " = " + std::to_string(*val) : ""));
    indent--;
}

void ASTPrinter::visit(TypeAlias& n) { print("TypeAlias: " + n.name + " = " + n.targetType); }

void ASTPrinter::visit(TraitDecl& n) {
    std::string typeParams;
    if (!n.typeParams.empty()) {
        typeParams = "[";
        for (size_t i = 0; i < n.typeParams.size(); i++) {
            if (i > 0) typeParams += ", ";
            typeParams += n.typeParams[i];
        }
        typeParams += "]";
    }
    print("TraitDecl: " + n.name + typeParams);
    indent++; for (auto& method : n.methods) method->accept(*this); indent--;
}

void ASTPrinter::visit(ImplBlock& n) {
    std::string desc = n.traitName.empty() ? n.typeName : n.traitName + " for " + n.typeName;
    print("ImplBlock: " + desc);
    indent++; for (auto& method : n.methods) method->accept(*this); indent--;
}

void ASTPrinter::visit(UnsafeBlock& n) { print("UnsafeBlock"); indent++; n.body->accept(*this); indent--; }
void ASTPrinter::visit(ImportStmt& n) { print("ImportStmt: " + n.path + (n.alias.empty() ? "" : " as " + n.alias)); }

void ASTPrinter::visit(ExternDecl& n) {
    print("ExternDecl: " + n.abi + " " + n.library);
    indent++; for (auto& fn : n.functions) fn->accept(*this); indent--;
}

void ASTPrinter::visit(MacroDecl& n) { print("MacroDecl: " + n.name); }

void ASTPrinter::visit(SyntaxMacroDecl& n) {
    print("SyntaxMacroDecl: " + n.name);
    indent++; for (auto& decl : n.body) decl->accept(*this); indent--;
}

void ASTPrinter::visit(LayerDecl& n) {
    print("LayerDecl: " + n.name);
    indent++; for (auto& decl : n.declarations) decl->accept(*this); indent--;
}

void ASTPrinter::visit(UseStmt& n) { 
    std::string info = "UseStmt: " + n.layerName;
    if (!n.alias.empty()) info += " as " + n.alias;
    if (!n.importItems.empty()) {
        info += " {";
        for (size_t i = 0; i < n.importItems.size(); i++) {
            if (i > 0) info += ", ";
            info += n.importItems[i];
        }
        info += "}";
    }
    print(info);
}
void ASTPrinter::visit(ModuleDecl& n) { 
    print("ModuleDecl: " + n.name); 
    indent++; 
    for (auto& s : n.body) s->accept(*this); 
    indent--; 
}
void ASTPrinter::visit(DeleteStmt& n) { print("DeleteStmt"); indent++; n.expr->accept(*this); indent--; }
void ASTPrinter::visit(Program& n) { print("Program"); indent++; for (auto& s : n.statements) s->accept(*this); indent--; }

void printTokens(const std::vector<Token>& tokens) {
    std::cout << "=== Tokens ===\n";
    for (const auto& tok : tokens) std::cout << tok.toString() << "\n";
    std::cout << "\n";
}

void printBytecode(Chunk& chunk) {
    std::cout << "=== Bytecode ===\n";
    std::cout << "Constants:\n";
    for (size_t i = 0; i < chunk.constants.size(); i++) {
        std::cout << "  [" << i << "] " << chunk.constants[i].toString() << "\n";
    }
    std::cout << "\nInstructions:\n";
    for (size_t i = 0; i < chunk.code.size(); i++) {
        auto& instr = chunk.code[i];
        std::cout << "  " << i << ": " << opCodeToString(instr.op);
        if (instr.operand != 0 || 
            instr.op == OpCode::CONST || instr.op == OpCode::LOAD_GLOBAL ||
            instr.op == OpCode::STORE_GLOBAL || instr.op == OpCode::LOAD_LOCAL ||
            instr.op == OpCode::STORE_LOCAL || instr.op == OpCode::JUMP ||
            instr.op == OpCode::JUMP_IF_FALSE || instr.op == OpCode::JUMP_IF_TRUE ||
            instr.op == OpCode::LOOP || instr.op == OpCode::CALL ||
            instr.op == OpCode::MAKE_LIST || instr.op == OpCode::MAKE_RECORD) {
            std::cout << " " << instr.operand;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

void ASTPrinter::visit(AssignExpr& n) {
    print("AssignExpr: " + tokenTypeToString(n.op));
    indent++; n.target->accept(*this); n.value->accept(*this); indent--;
}

} // namespace flex
