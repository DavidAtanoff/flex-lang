// Flex Compiler - AST Printer
#ifndef FLEX_AST_PRINTER_H
#define FLEX_AST_PRINTER_H

#include "frontend/ast/ast.h"
#include "backend/bytecode/bytecode.h"

namespace flex {

class ASTPrinter : public ASTVisitor {
public:
    int indent = 0;
    
    void print(const std::string& s);
    
    void visit(IntegerLiteral& n) override;
    void visit(FloatLiteral& n) override;
    void visit(StringLiteral& n) override;
    void visit(InterpolatedString& n) override;
    void visit(BoolLiteral& n) override;
    void visit(NilLiteral& n) override;
    void visit(Identifier& n) override;
    void visit(BinaryExpr& n) override;
    void visit(UnaryExpr& n) override;
    void visit(CallExpr& n) override;
    void visit(MemberExpr& n) override;
    void visit(IndexExpr& n) override;
    void visit(ListExpr& n) override;
    void visit(RecordExpr& n) override;
    void visit(MapExpr& n) override;
    void visit(RangeExpr& n) override;
    void visit(LambdaExpr& n) override;
    void visit(TernaryExpr& n) override;
    void visit(ListCompExpr& n) override;
    void visit(AddressOfExpr& n) override;
    void visit(DerefExpr& n) override;
    void visit(NewExpr& n) override;
    void visit(CastExpr& n) override;
    void visit(AwaitExpr& n) override;
    void visit(SpawnExpr& n) override;
    void visit(DSLBlock& n) override;
    void visit(AssignExpr& n) override;
    void visit(PropagateExpr& n) override;
    void visit(ExprStmt& n) override;
    void visit(VarDecl& n) override;
    void visit(DestructuringDecl& n) override;
    void visit(AssignStmt& n) override;
    void visit(Block& n) override;
    void visit(IfStmt& n) override;
    void visit(WhileStmt& n) override;
    void visit(ForStmt& n) override;
    void visit(MatchStmt& n) override;
    void visit(ReturnStmt& n) override;
    void visit(BreakStmt& n) override;
    void visit(ContinueStmt& n) override;
    void visit(TryStmt& n) override;
    void visit(FnDecl& n) override;
    void visit(RecordDecl& n) override;
    void visit(EnumDecl& n) override;
    void visit(TypeAlias& n) override;
    void visit(TraitDecl& n) override;
    void visit(ImplBlock& n) override;
    void visit(UnsafeBlock& n) override;
    void visit(ImportStmt& n) override;
    void visit(ExternDecl& n) override;
    void visit(MacroDecl& n) override;
    void visit(SyntaxMacroDecl& n) override;
    void visit(LayerDecl& n) override;
    void visit(UseStmt& n) override;
    void visit(ModuleDecl& n) override;
    void visit(DeleteStmt& n) override;
    void visit(Program& n) override;
};

void printTokens(const std::vector<Token>& tokens);
void printBytecode(Chunk& chunk);

} // namespace flex

#endif // FLEX_AST_PRINTER_H
