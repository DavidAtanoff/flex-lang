// Flex Compiler - Monomorphization for Generics
// Generates specialized code for each concrete type instantiation
#ifndef FLEX_MONOMORPHIZER_H
#define FLEX_MONOMORPHIZER_H

#include "frontend/ast/ast.h"
#include "semantic/types/types.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace flex {

// Represents a specific instantiation of a generic function/type
struct GenericInstantiation {
    std::string baseName;                          // Original generic name
    std::vector<TypePtr> typeArgs;                 // Concrete type arguments
    std::string mangledName;                       // Mangled name for this instantiation
    std::string returnType;                        // Concrete return type after substitution
    
    bool operator==(const GenericInstantiation& other) const;
    
    // Check if this instantiation returns a float
    bool returnsFloat() const { return returnType == "float"; }
    
    // Check if this instantiation returns a string
    bool returnsString() const { return returnType == "string" || returnType == "str"; }
};

// Hash function for GenericInstantiation
struct GenericInstantiationHash {
    size_t operator()(const GenericInstantiation& inst) const;
};

// Tracks all generic instantiations needed during compilation
class Monomorphizer {
public:
    Monomorphizer() = default;
    
    // Record a generic function instantiation
    void recordFunctionInstantiation(const std::string& fnName, 
                                     const std::vector<TypePtr>& typeArgs,
                                     FnDecl* originalDecl);
    
    // Record a generic record instantiation
    void recordRecordInstantiation(const std::string& recordName,
                                   const std::vector<TypePtr>& typeArgs,
                                   RecordDecl* originalDecl);
    
    // Get mangled name for an instantiation
    std::string getMangledName(const std::string& baseName, 
                               const std::vector<TypePtr>& typeArgs) const;
    
    // Check if an instantiation already exists
    bool hasInstantiation(const std::string& baseName,
                          const std::vector<TypePtr>& typeArgs) const;
    
    // Get all function instantiations
    const std::vector<std::pair<GenericInstantiation, FnDecl*>>& getFunctionInstantiations() const {
        return functionInstantiations_;
    }
    
    // Get all record instantiations
    const std::vector<std::pair<GenericInstantiation, RecordDecl*>>& getRecordInstantiations() const {
        return recordInstantiations_;
    }
    
    // Check if a mangled function name returns float
    bool functionReturnsFloat(const std::string& mangledName) const;
    
    // Check if a mangled function name returns string
    bool functionReturnsString(const std::string& mangledName) const;
    
    // Get the return type for a mangled function name
    std::string getFunctionReturnType(const std::string& mangledName) const;
    
    // Create a specialized copy of a function declaration
    std::unique_ptr<FnDecl> specializeFunction(FnDecl* original,
                                                const std::vector<TypePtr>& typeArgs);
    
    // Create a specialized copy of a record declaration
    std::unique_ptr<RecordDecl> specializeRecord(RecordDecl* original,
                                                  const std::vector<TypePtr>& typeArgs);
    
    // Substitute type parameters in a type string
    std::string substituteTypeString(const std::string& typeStr,
                                     const std::vector<std::string>& typeParams,
                                     const std::vector<TypePtr>& typeArgs) const;
    
    // Clear all recorded instantiations
    void clear();
    
private:
    std::vector<std::pair<GenericInstantiation, FnDecl*>> functionInstantiations_;
    std::vector<std::pair<GenericInstantiation, RecordDecl*>> recordInstantiations_;
    std::unordered_set<std::string> instantiatedNames_;  // Set of mangled names already created
    
    // Helper to create mangled name from type args
    std::string mangleTypeArgs(const std::vector<TypePtr>& typeArgs) const;
};

// Expression visitor to collect generic instantiations
class GenericCollector : public ASTVisitor {
public:
    GenericCollector(Monomorphizer& mono, 
                     std::unordered_map<std::string, FnDecl*>& genericFunctions,
                     std::unordered_map<std::string, RecordDecl*>& genericRecords);
    
    void collect(Program& program);
    
    // Visitor methods
    void visit(IntegerLiteral& node) override {}
    void visit(FloatLiteral& node) override {}
    void visit(StringLiteral& node) override {}
    void visit(InterpolatedString& node) override;
    void visit(BoolLiteral& node) override {}
    void visit(NilLiteral& node) override {}
    void visit(Identifier& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(MemberExpr& node) override;
    void visit(IndexExpr& node) override;
    void visit(ListExpr& node) override;
    void visit(RecordExpr& node) override;
    void visit(MapExpr& node) override;
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
    void visit(DSLBlock& node) override {}
    void visit(AssignExpr& node) override;
    void visit(PropagateExpr& node) override;
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
    void visit(BreakStmt& node) override {}
    void visit(ContinueStmt& node) override {}
    void visit(TryStmt& node) override;
    void visit(FnDecl& node) override;
    void visit(RecordDecl& node) override;
    void visit(EnumDecl& node) override {}
    void visit(TypeAlias& node) override {}
    void visit(TraitDecl& node) override {}
    void visit(ImplBlock& node) override;
    void visit(UnsafeBlock& node) override;
    void visit(ImportStmt& node) override {}
    void visit(ExternDecl& node) override {}
    void visit(MacroDecl& node) override {}
    void visit(SyntaxMacroDecl& node) override {}
    void visit(LayerDecl& node) override {}
    void visit(UseStmt& node) override {}
    void visit(ModuleDecl& node) override;
    void visit(DeleteStmt& node) override;
    void visit(Program& node) override;
    
private:
    Monomorphizer& mono_;
    std::unordered_map<std::string, FnDecl*>& genericFunctions_;
    std::unordered_map<std::string, RecordDecl*>& genericRecords_;
    
    // Current type parameter context for inference
    std::unordered_map<std::string, TypePtr> currentTypeBindings_;
    
    // Infer type arguments from call arguments
    std::vector<TypePtr> inferTypeArgs(FnDecl* fn, CallExpr& call);
    
    // Parse type from string (simplified)
    TypePtr parseType(const std::string& typeStr);
};

} // namespace flex

#endif // FLEX_MONOMORPHIZER_H
