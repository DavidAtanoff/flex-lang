// Flex Compiler - Bytecode Compiler
#ifndef FLEX_COMPILER_H
#define FLEX_COMPILER_H

#include "frontend/ast/ast.h"
#include "bytecode.h"

namespace flex {

struct Local { std::string name; int depth; };

// Loop context for break/continue
struct LoopContext {
    size_t loopStart;                    // Where to jump for continue
    std::vector<size_t> breakJumps;      // Jumps to patch for break
};

class Compiler : public ASTVisitor {
public:
    Compiler();
    Chunk compile(Program& program);
    
private:
    Chunk chunk;
    std::vector<Local> locals;
    int scopeDepth = 0;
    bool inFunction = false;
    std::unordered_map<std::string, int> globalNames;
    int globalCount = 0;
    std::vector<LoopContext> loopStack;  // Stack of active loops
    
    void visit(IntegerLiteral& node) override;
    void visit(FloatLiteral& node) override;
    void visit(StringLiteral& node) override;
    void visit(InterpolatedString& node) override;
    void visit(BoolLiteral& node) override;
    void visit(NilLiteral& node) override;
    void visit(Identifier& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(MemberExpr& node) override;
    void visit(IndexExpr& node) override;
    void visit(ListExpr& node) override;
    void visit(RecordExpr& node) override;
    void visit(RangeExpr& node) override;
    void visit(LambdaExpr& node) override;
    void visit(TernaryExpr& node) override;
    void visit(ListCompExpr& node) override;
    void visit(AddressOfExpr& node) override;
    void visit(DerefExpr& node) override;
    void visit(NewExpr& node) override;
    void visit(CastExpr& node) override;
    void visit(AwaitExpr& node) override;
    void visit(SpawnExpr& node) override;
    void visit(DSLBlock& node) override;
    void visit(AssignExpr& node) override;
    void visit(ExprStmt& node) override;
    void visit(VarDecl& node) override;
    void visit(DestructuringDecl& node) override;
    void visit(AssignStmt& node) override;
    void visit(Block& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ForStmt& node) override;
    void visit(MatchStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(BreakStmt& node) override;
    void visit(ContinueStmt& node) override;
    void visit(TryStmt& node) override;
    void visit(FnDecl& node) override;
    void visit(RecordDecl& node) override;
    void visit(EnumDecl& node) override;
    void visit(TypeAlias& node) override;
    void visit(TraitDecl& node) override;
    void visit(ImplBlock& node) override;
    void visit(UnsafeBlock& node) override;
    void visit(ImportStmt& node) override;
    void visit(ExternDecl& node) override;
    void visit(MacroDecl& node) override;
    void visit(SyntaxMacroDecl& node) override;
    void visit(LayerDecl& node) override;
    void visit(UseStmt& node) override;
    void visit(ModuleDecl& node) override;
    void visit(DeleteStmt& node) override;
    void visit(Program& node) override;
    
    void emit(OpCode op);
    void emitOp(OpCode op, int32_t operand);
    size_t emitJump(OpCode op);
    void patchJump(size_t offset);
    void emitLoop(size_t loopStart);
    int addConstant(Value val);
    int resolveLocal(const std::string& name);
    int resolveGlobal(const std::string& name);
    void declareLocal(const std::string& name);
    void beginScope();
    void endScope();
};

} // namespace flex

#endif // FLEX_COMPILER_H
