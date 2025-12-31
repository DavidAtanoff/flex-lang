// Flex Compiler - Type Checker and Inference
#ifndef FLEX_TYPE_CHECKER_H
#define FLEX_TYPE_CHECKER_H

#include "frontend/ast/ast.h"
#include "semantic/types/types.h"
#include "semantic/symbols/symbol_table.h"
#include <vector>

namespace flex {

struct TypeDiagnostic {
    enum class Level { ERROR, WARNING, NOTE };
    Level level;
    std::string message;
    SourceLocation location;
    TypeDiagnostic(Level l, std::string msg, SourceLocation loc) : level(l), message(std::move(msg)), location(loc) {}
};

class TypeChecker : public ASTVisitor {
public:
    TypeChecker();
    bool check(Program& program);
    const std::vector<TypeDiagnostic>& diagnostics() const { return diagnostics_; }
    bool hasErrors() const;
    TypePtr getType(Expression* expr);
    SymbolTable& symbols() { return symbols_; }
    
private:
    SymbolTable symbols_;
    std::vector<TypeDiagnostic> diagnostics_;
    TypePtr currentType_;
    TypePtr expectedReturn_;
    
    // Generic type context
    std::unordered_map<std::string, TypePtr> currentTypeParams_;  // Active type parameter bindings
    std::vector<std::string> currentTypeParamNames_;              // Type params in scope
    
    TypePtr inferType(Expression* expr);
    TypePtr unify(TypePtr a, TypePtr b, const SourceLocation& loc);
    TypePtr commonType(TypePtr a, TypePtr b);
    bool isAssignable(TypePtr target, TypePtr source);
    bool isComparable(TypePtr a, TypePtr b);
    void error(const std::string& msg, const SourceLocation& loc);
    void warning(const std::string& msg, const SourceLocation& loc);
    void note(const std::string& msg, const SourceLocation& loc);
    TypePtr parseTypeAnnotation(const std::string& str);
    void registerBuiltins();  // Register built-in functions
    
    // Generic and trait type checking
    TypePtr parseGenericType(const std::string& str);
    TypePtr resolveTypeParam(const std::string& name);
    bool checkTraitBounds(TypePtr type, const std::vector<std::string>& bounds, const SourceLocation& loc);
    TypePtr instantiateGenericFunction(FunctionType* fnType, const std::vector<TypePtr>& typeArgs, const SourceLocation& loc);
    void checkTraitImpl(const std::string& traitName, const std::string& typeName, 
                        const std::vector<std::unique_ptr<FnDecl>>& methods, const SourceLocation& loc);
    
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
    
    std::unordered_map<Expression*, TypePtr> exprTypes_;
};

} // namespace flex

#endif // FLEX_TYPE_CHECKER_H
