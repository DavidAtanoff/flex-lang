// Flex Compiler - Abstract Syntax Tree
#ifndef FLEX_AST_H
#define FLEX_AST_H

#include "common/common.h"
#include "frontend/token/token.h"

namespace flex {

struct ASTVisitor;
struct ASTNode { SourceLocation location; virtual ~ASTNode() = default; virtual void accept(ASTVisitor& visitor) = 0; };
using ASTPtr = std::unique_ptr<ASTNode>;

struct Expression : ASTNode {};
using ExprPtr = std::unique_ptr<Expression>;

struct IntegerLiteral : Expression { int64_t value; IntegerLiteral(int64_t v, SourceLocation loc) : value(v) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct FloatLiteral : Expression { double value; FloatLiteral(double v, SourceLocation loc) : value(v) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct StringLiteral : Expression { std::string value; StringLiteral(std::string v, SourceLocation loc) : value(std::move(v)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct InterpolatedString : Expression { std::vector<std::variant<std::string, ExprPtr>> parts; InterpolatedString(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct BoolLiteral : Expression { bool value; BoolLiteral(bool v, SourceLocation loc) : value(v) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct NilLiteral : Expression { NilLiteral(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct Identifier : Expression { std::string name; Identifier(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct BinaryExpr : Expression { ExprPtr left; TokenType op; ExprPtr right; BinaryExpr(ExprPtr l, TokenType o, ExprPtr r, SourceLocation loc) : left(std::move(l)), op(o), right(std::move(r)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct UnaryExpr : Expression { TokenType op; ExprPtr operand; UnaryExpr(TokenType o, ExprPtr e, SourceLocation loc) : op(o), operand(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct CallExpr : Expression { ExprPtr callee; std::vector<ExprPtr> args; std::vector<std::pair<std::string, ExprPtr>> namedArgs; std::vector<std::string> typeArgs; bool isHotCallSite = false; CallExpr(ExprPtr c, SourceLocation loc) : callee(std::move(c)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MemberExpr : Expression { ExprPtr object; std::string member; MemberExpr(ExprPtr obj, std::string m, SourceLocation loc) : object(std::move(obj)), member(std::move(m)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct IndexExpr : Expression { ExprPtr object; ExprPtr index; IndexExpr(ExprPtr obj, ExprPtr idx, SourceLocation loc) : object(std::move(obj)), index(std::move(idx)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ListExpr : Expression { std::vector<ExprPtr> elements; ListExpr(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct RecordExpr : Expression { std::string typeName; std::vector<std::pair<std::string, ExprPtr>> fields; RecordExpr(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MapExpr : Expression { std::vector<std::pair<ExprPtr, ExprPtr>> entries; MapExpr(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct RangeExpr : Expression { ExprPtr start; ExprPtr end; ExprPtr step; RangeExpr(ExprPtr s, ExprPtr e, ExprPtr st, SourceLocation loc) : start(std::move(s)), end(std::move(e)), step(std::move(st)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct LambdaExpr : Expression { std::vector<std::pair<std::string, std::string>> params; ExprPtr body; LambdaExpr(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct TernaryExpr : Expression { ExprPtr condition; ExprPtr thenExpr; ExprPtr elseExpr; TernaryExpr(ExprPtr c, ExprPtr t, ExprPtr e, SourceLocation loc) : condition(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ListCompExpr : Expression { ExprPtr expr; std::string var; ExprPtr iterable; ExprPtr condition; ListCompExpr(ExprPtr e, std::string v, ExprPtr it, ExprPtr cond, SourceLocation loc) : expr(std::move(e)), var(std::move(v)), iterable(std::move(it)), condition(std::move(cond)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct AddressOfExpr : Expression { ExprPtr operand; AddressOfExpr(ExprPtr e, SourceLocation loc) : operand(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct DerefExpr : Expression { ExprPtr operand; DerefExpr(ExprPtr e, SourceLocation loc) : operand(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct NewExpr : Expression { std::string typeName; std::vector<ExprPtr> args; NewExpr(std::string t, SourceLocation loc) : typeName(std::move(t)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct CastExpr : Expression { ExprPtr expr; std::string targetType; CastExpr(ExprPtr e, std::string t, SourceLocation loc) : expr(std::move(e)), targetType(std::move(t)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct AwaitExpr : Expression { ExprPtr operand; AwaitExpr(ExprPtr e, SourceLocation loc) : operand(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct SpawnExpr : Expression { ExprPtr operand; SpawnExpr(ExprPtr e, SourceLocation loc) : operand(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct DSLBlock : Expression { std::string dslName; std::string rawContent; DSLBlock(std::string name, std::string content, SourceLocation loc) : dslName(std::move(name)), rawContent(std::move(content)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct AssignExpr : Expression { ExprPtr target; TokenType op; ExprPtr value; AssignExpr(ExprPtr t, TokenType o, ExprPtr v, SourceLocation loc) : target(std::move(t)), op(o), value(std::move(v)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct PropagateExpr : Expression { ExprPtr operand; PropagateExpr(ExprPtr e, SourceLocation loc) : operand(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };

// Channel expressions for inter-thread communication
struct ChanSendExpr : Expression { ExprPtr channel; ExprPtr value; ChanSendExpr(ExprPtr ch, ExprPtr v, SourceLocation loc) : channel(std::move(ch)), value(std::move(v)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ChanRecvExpr : Expression { ExprPtr channel; ChanRecvExpr(ExprPtr ch, SourceLocation loc) : channel(std::move(ch)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MakeChanExpr : Expression { std::string elementType; int64_t bufferSize; MakeChanExpr(std::string t, int64_t sz, SourceLocation loc) : elementType(std::move(t)), bufferSize(sz) { location = loc; } void accept(ASTVisitor& visitor) override; };

// Synchronization primitive expressions
struct MakeMutexExpr : Expression { std::string elementType; MakeMutexExpr(std::string t, SourceLocation loc) : elementType(std::move(t)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MakeRWLockExpr : Expression { std::string elementType; MakeRWLockExpr(std::string t, SourceLocation loc) : elementType(std::move(t)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MakeCondExpr : Expression { MakeCondExpr(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MakeSemaphoreExpr : Expression { int64_t initialCount; int64_t maxCount; MakeSemaphoreExpr(int64_t init, int64_t max, SourceLocation loc) : initialCount(init), maxCount(max) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MutexLockExpr : Expression { ExprPtr mutex; MutexLockExpr(ExprPtr m, SourceLocation loc) : mutex(std::move(m)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MutexUnlockExpr : Expression { ExprPtr mutex; MutexUnlockExpr(ExprPtr m, SourceLocation loc) : mutex(std::move(m)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct RWLockReadExpr : Expression { ExprPtr rwlock; RWLockReadExpr(ExprPtr r, SourceLocation loc) : rwlock(std::move(r)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct RWLockWriteExpr : Expression { ExprPtr rwlock; RWLockWriteExpr(ExprPtr r, SourceLocation loc) : rwlock(std::move(r)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct RWLockUnlockExpr : Expression { ExprPtr rwlock; RWLockUnlockExpr(ExprPtr r, SourceLocation loc) : rwlock(std::move(r)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct CondWaitExpr : Expression { ExprPtr cond; ExprPtr mutex; CondWaitExpr(ExprPtr c, ExprPtr m, SourceLocation loc) : cond(std::move(c)), mutex(std::move(m)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct CondSignalExpr : Expression { ExprPtr cond; CondSignalExpr(ExprPtr c, SourceLocation loc) : cond(std::move(c)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct CondBroadcastExpr : Expression { ExprPtr cond; CondBroadcastExpr(ExprPtr c, SourceLocation loc) : cond(std::move(c)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct SemAcquireExpr : Expression { ExprPtr sem; SemAcquireExpr(ExprPtr s, SourceLocation loc) : sem(std::move(s)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct SemReleaseExpr : Expression { ExprPtr sem; SemReleaseExpr(ExprPtr s, SourceLocation loc) : sem(std::move(s)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct SemTryAcquireExpr : Expression { ExprPtr sem; SemTryAcquireExpr(ExprPtr s, SourceLocation loc) : sem(std::move(s)) { location = loc; } void accept(ASTVisitor& visitor) override; };

struct Statement : ASTNode {};
using StmtPtr = std::unique_ptr<Statement>;

struct ExprStmt : Statement { ExprPtr expr; ExprStmt(ExprPtr e, SourceLocation loc) : expr(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct VarDecl : Statement { std::string name; std::string typeName; ExprPtr initializer; bool isMutable = true; bool isConst = false; VarDecl(std::string n, std::string t, ExprPtr init, SourceLocation loc) : name(std::move(n)), typeName(std::move(t)), initializer(std::move(init)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct DestructuringDecl : Statement { enum class Kind { TUPLE, RECORD }; Kind kind; std::vector<std::string> names; ExprPtr initializer; bool isMutable = true; DestructuringDecl(Kind k, std::vector<std::string> n, ExprPtr init, SourceLocation loc) : kind(k), names(std::move(n)), initializer(std::move(init)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct AssignStmt : Statement { ExprPtr target; TokenType op; ExprPtr value; AssignStmt(ExprPtr t, TokenType o, ExprPtr v, SourceLocation loc) : target(std::move(t)), op(o), value(std::move(v)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct Block : Statement { std::vector<StmtPtr> statements; Block(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct IfStmt : Statement { ExprPtr condition; StmtPtr thenBranch; std::vector<std::pair<ExprPtr, StmtPtr>> elifBranches; StmtPtr elseBranch; IfStmt(ExprPtr c, StmtPtr t, SourceLocation loc) : condition(std::move(c)), thenBranch(std::move(t)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct WhileStmt : Statement { ExprPtr condition; StmtPtr body; WhileStmt(ExprPtr c, StmtPtr b, SourceLocation loc) : condition(std::move(c)), body(std::move(b)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ForStmt : Statement { std::string var; ExprPtr iterable; StmtPtr body; int unrollHint = 0; ForStmt(std::string v, ExprPtr it, StmtPtr b, SourceLocation loc) : var(std::move(v)), iterable(std::move(it)), body(std::move(b)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MatchCase { ExprPtr pattern; ExprPtr guard; StmtPtr body; MatchCase(ExprPtr p, ExprPtr g, StmtPtr b) : pattern(std::move(p)), guard(std::move(g)), body(std::move(b)) {} };
struct MatchStmt : Statement { ExprPtr value; std::vector<MatchCase> cases; StmtPtr defaultCase; MatchStmt(ExprPtr v, SourceLocation loc) : value(std::move(v)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ReturnStmt : Statement { ExprPtr value; ReturnStmt(ExprPtr v, SourceLocation loc) : value(std::move(v)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct BreakStmt : Statement { BreakStmt(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ContinueStmt : Statement { ContinueStmt(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct TryStmt : Statement { ExprPtr tryExpr; ExprPtr elseExpr; TryStmt(ExprPtr t, ExprPtr e, SourceLocation loc) : tryExpr(std::move(t)), elseExpr(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
// Calling convention for FFI
enum class CallingConvention {
    Default,    // Platform default (win64 on Windows)
    Cdecl,      // C calling convention
    Stdcall,    // Windows stdcall
    Fastcall,   // Fastcall convention
    Win64       // Windows x64 ABI
};

struct FnDecl : Statement { std::string name; std::vector<std::string> typeParams; std::vector<std::pair<std::string, std::string>> params; std::string returnType; StmtPtr body; bool isPublic = false; bool isExtern = false; bool isAsync = false; bool isHot = false; bool isCold = false; bool isVariadic = false; bool isNaked = false; CallingConvention callingConv = CallingConvention::Default; FnDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; bool hasVariadicParams() const { for (const auto& p : params) { if (p.second == "...") return true; } return false; } };
// Bitfield specification for a record field
struct BitfieldSpec {
    int bitWidth = 0;          // Number of bits (0 = not a bitfield)
    bool isBitfield() const { return bitWidth > 0; }
};

struct RecordDecl : Statement { 
    std::string name; 
    std::vector<std::string> typeParams; 
    std::vector<std::pair<std::string, std::string>> fields; 
    std::vector<BitfieldSpec> bitfields;  // Bitfield specs for each field (parallel to fields)
    bool isPublic = false;
    // Attributes for FFI/layout control
    bool reprC = false;        // #[repr(C)] - C-compatible layout
    bool reprPacked = false;   // #[repr(packed)] - no padding
    int reprAlign = 0;         // #[repr(align(N))] - explicit alignment
    RecordDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } 
    void accept(ASTVisitor& visitor) override; 
};
struct UnionDecl : Statement { 
    std::string name; 
    std::vector<std::string> typeParams; 
    std::vector<std::pair<std::string, std::string>> fields; 
    bool isPublic = false;
    // Attributes for FFI/layout control
    bool reprC = false;        // #[repr(C)] - C-compatible layout
    int reprAlign = 0;         // #[repr(align(N))] - explicit alignment
    UnionDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } 
    void accept(ASTVisitor& visitor) override; 
};
struct EnumDecl : Statement { std::string name; std::vector<std::string> typeParams; std::vector<std::pair<std::string, std::optional<int64_t>>> variants; EnumDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct TypeAlias : Statement { std::string name; std::string targetType; TypeAlias(std::string n, std::string t, SourceLocation loc) : name(std::move(n)), targetType(std::move(t)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct TraitDecl : Statement { std::string name; std::vector<std::string> typeParams; std::vector<std::string> superTraits; std::vector<std::unique_ptr<FnDecl>> methods; TraitDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ImplBlock : Statement { std::string traitName; std::string typeName; std::vector<std::string> typeParams; std::vector<std::unique_ptr<FnDecl>> methods; ImplBlock(std::string trait, std::string type, SourceLocation loc) : traitName(std::move(trait)), typeName(std::move(type)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct UnsafeBlock : Statement { StmtPtr body; UnsafeBlock(StmtPtr b, SourceLocation loc) : body(std::move(b)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ImportStmt : Statement { std::string path; std::string alias; std::vector<std::string> items; ImportStmt(std::string p, SourceLocation loc) : path(std::move(p)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct ExternDecl : Statement { std::string abi; std::string library; std::vector<std::unique_ptr<FnDecl>> functions; ExternDecl(std::string a, std::string lib, SourceLocation loc) : abi(std::move(a)), library(std::move(lib)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct MacroDecl : Statement { std::string name; std::vector<std::string> params; std::vector<StmtPtr> body; bool isOperator = false; std::string operatorSymbol; int precedence = 0; bool isInfix = false; bool isPrefix = false; bool isPostfix = false; MacroDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct SyntaxMacroDecl : Statement { std::string name; std::vector<StmtPtr> body; std::string transformExpr; SyntaxMacroDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct LayerDecl : Statement { std::string name; std::vector<StmtPtr> declarations; LayerDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct UseStmt : Statement { 
    std::string layerName; 
    bool isLayer = false; 
    bool isFileImport = false; 
    std::vector<std::string> importItems;  // For selective imports: use math::{sin, cos}
    std::string alias;  // For aliased imports: use math as m
    UseStmt(std::string n, SourceLocation loc) : layerName(std::move(n)) { location = loc; } 
    void accept(ASTVisitor& visitor) override; 
};
struct ModuleDecl : Statement {
    std::string name;
    bool isPublic = true;
    std::vector<StmtPtr> body;
    ModuleDecl(std::string n, SourceLocation loc) : name(std::move(n)) { location = loc; }
    void accept(ASTVisitor& visitor) override;
};
struct DeleteStmt : Statement { ExprPtr expr; DeleteStmt(ExprPtr e, SourceLocation loc) : expr(std::move(e)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct LockStmt : Statement { ExprPtr mutex; StmtPtr body; LockStmt(ExprPtr m, StmtPtr b, SourceLocation loc) : mutex(std::move(m)), body(std::move(b)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct AsmStmt : Statement { std::string code; std::vector<std::string> outputs; std::vector<std::string> inputs; std::vector<std::string> clobbers; AsmStmt(std::string c, SourceLocation loc) : code(std::move(c)) { location = loc; } void accept(ASTVisitor& visitor) override; };
struct Program : ASTNode { std::vector<StmtPtr> statements; Program(SourceLocation loc) { location = loc; } void accept(ASTVisitor& visitor) override; };

struct ASTVisitor {
    virtual ~ASTVisitor() = default;
    virtual void visit(IntegerLiteral& node) = 0; virtual void visit(FloatLiteral& node) = 0;
    virtual void visit(StringLiteral& node) = 0; virtual void visit(InterpolatedString& node) = 0;
    virtual void visit(BoolLiteral& node) = 0; virtual void visit(NilLiteral& node) = 0;
    virtual void visit(Identifier& node) = 0; virtual void visit(BinaryExpr& node) = 0;
    virtual void visit(UnaryExpr& node) = 0; virtual void visit(CallExpr& node) = 0;
    virtual void visit(MemberExpr& node) = 0; virtual void visit(IndexExpr& node) = 0;
    virtual void visit(ListExpr& node) = 0; virtual void visit(RecordExpr& node) = 0;
    virtual void visit(MapExpr& node) = 0;
    virtual void visit(RangeExpr& node) = 0; virtual void visit(LambdaExpr& node) = 0;
    virtual void visit(TernaryExpr& node) = 0; virtual void visit(ListCompExpr& node) = 0;
    virtual void visit(AddressOfExpr& node) = 0; virtual void visit(DerefExpr& node) = 0;
    virtual void visit(NewExpr& node) = 0; virtual void visit(CastExpr& node) = 0;
    virtual void visit(AwaitExpr& node) = 0; virtual void visit(SpawnExpr& node) = 0;
    virtual void visit(DSLBlock& node) = 0; virtual void visit(AssignExpr& node) = 0;
    virtual void visit(PropagateExpr& node) = 0;
    virtual void visit(ChanSendExpr& node) = 0;
    virtual void visit(ChanRecvExpr& node) = 0;
    virtual void visit(MakeChanExpr& node) = 0;
    virtual void visit(MakeMutexExpr& node) = 0;
    virtual void visit(MakeRWLockExpr& node) = 0;
    virtual void visit(MakeCondExpr& node) = 0;
    virtual void visit(MakeSemaphoreExpr& node) = 0;
    virtual void visit(MutexLockExpr& node) = 0;
    virtual void visit(MutexUnlockExpr& node) = 0;
    virtual void visit(RWLockReadExpr& node) = 0;
    virtual void visit(RWLockWriteExpr& node) = 0;
    virtual void visit(RWLockUnlockExpr& node) = 0;
    virtual void visit(CondWaitExpr& node) = 0;
    virtual void visit(CondSignalExpr& node) = 0;
    virtual void visit(CondBroadcastExpr& node) = 0;
    virtual void visit(SemAcquireExpr& node) = 0;
    virtual void visit(SemReleaseExpr& node) = 0;
    virtual void visit(SemTryAcquireExpr& node) = 0;
    virtual void visit(ExprStmt& node) = 0;
    virtual void visit(VarDecl& node) = 0; virtual void visit(DestructuringDecl& node) = 0;
    virtual void visit(AssignStmt& node) = 0; virtual void visit(Block& node) = 0;
    virtual void visit(IfStmt& node) = 0; virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ForStmt& node) = 0; virtual void visit(MatchStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0; virtual void visit(BreakStmt& node) = 0;
    virtual void visit(ContinueStmt& node) = 0; virtual void visit(TryStmt& node) = 0;
    virtual void visit(FnDecl& node) = 0; virtual void visit(RecordDecl& node) = 0;
    virtual void visit(UnionDecl& node) = 0;
    virtual void visit(EnumDecl& node) = 0; virtual void visit(TypeAlias& node) = 0;
    virtual void visit(TraitDecl& node) = 0; virtual void visit(ImplBlock& node) = 0;
    virtual void visit(UnsafeBlock& node) = 0; virtual void visit(ImportStmt& node) = 0;
    virtual void visit(ExternDecl& node) = 0; virtual void visit(MacroDecl& node) = 0;
    virtual void visit(SyntaxMacroDecl& node) = 0; virtual void visit(LayerDecl& node) = 0;
    virtual void visit(UseStmt& node) = 0; 
    virtual void visit(ModuleDecl& node) = 0;
    virtual void visit(DeleteStmt& node) = 0;
    virtual void visit(LockStmt& node) = 0;
    virtual void visit(AsmStmt& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace flex

#endif // FLEX_AST_H
