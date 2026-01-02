// Flex Compiler - Native Code Generator
#ifndef FLEX_NATIVE_CODEGEN_H
#define FLEX_NATIVE_CODEGEN_H

#include "frontend/ast/ast.h"
#include "backend/x64/x64_assembler.h"
#include "backend/x64/pe_generator.h"
#include "backend/codegen/register_allocator.h"
#include "backend/codegen/global_register_allocator.h"
#include "backend/gc/gc.h"
#include "semantic/generics/monomorphizer.h"
#include <map>
#include <set>

namespace flex {

// Optimization level for code generation (LLVM/Clang compatible)
enum class CodeGenOptLevel {
    O0,    // No optimization - debug friendly, no inlining
    O1,    // Basic optimization - shared runtime routines
    O2,    // Standard optimization - selective inlining
    O3,    // Aggressive optimization - more inlining, speed over size
    Os,    // Optimize for size - shared routines, minimal inlining
    Oz,    // Aggressive size - maximum code sharing
    Ofast  // Maximum optimization - full inlining, unsafe opts
};

class NativeCodeGen : public ASTVisitor {
public:
    NativeCodeGen();
    bool compile(Program& program, const std::string& outputFile);
    
    // Set optimization level
    void setOptLevel(CodeGenOptLevel level) { optLevel_ = level; }
    CodeGenOptLevel optLevel() const { return optLevel_; }
    
    // Dump generated assembly (for debugging)
    void dumpAssembly(std::ostream& out) const;
    
    // Get the assembler for inspection
    const X64Assembler& getAssembler() const { return asm_; }
    
private:
    X64Assembler asm_;
    PEGenerator pe_;
    std::map<std::string, int32_t> locals;
    std::map<std::string, uint32_t> dataOffsets;
    int32_t stackOffset = 0;
    int labelCounter = 0;
    bool inFunction = false;
    int currentArgCount = 0;
    std::map<std::string, uint32_t> stringOffsets;
    uint32_t itoaBufferRVA_ = 0;
    std::map<std::string, int64_t> constVars;
    std::map<std::string, std::string> constStrVars;
    std::map<std::string, std::vector<int64_t>> constListVars;  // Track constant list values
    std::map<std::string, size_t> listSizes;  // Track list sizes
    
    // Float support
    std::set<std::string> floatVars;           // Variables that are floats
    std::map<std::string, double> constFloatVars;  // Constant float values
    uint32_t negZeroRVA_ = 0;                  // RVA for -0.0 constant (for negation)
    bool lastExprWasFloat_ = false;            // Track if last expression result is float
    
    // Loop context for break/continue
    struct LoopLabels {
        std::string continueLabel;  // Jump here for continue
        std::string breakLabel;     // Jump here for break
    };
    std::vector<LoopLabels> loopStack;
    
    // Stack frame optimization - allocate stack once per function
    bool useOptimizedStackFrame_ = true;       // Enable stack frame optimization
    int32_t functionStackSize_ = 0;            // Total stack size for current function
    bool stackAllocated_ = false;              // Whether stack is already allocated
    
    // Register allocation
    RegisterAllocator regAlloc_;               // Register allocator instance
    bool useRegisterAllocation_ = true;        // Enable register allocation
    std::map<std::string, VarRegister> varRegisters_;  // Variable -> register mapping for current function
    
    // Global register allocation (top-level)
    GlobalRegisterAllocator globalRegAlloc_;   // Global register allocator
    bool useGlobalRegisterAllocation_ = true;  // Enable global register allocation
    std::map<std::string, VarRegister> globalVarRegisters_;  // Global variable -> register mapping
    
    // Leaf function optimization
    bool isLeafFunction_ = false;              // Current function is a leaf (no calls)
    bool useLeafOptimization_ = true;          // Enable leaf function optimization
    
    // Stdout handle caching - avoid redundant GetStdHandle calls
    bool stdoutHandleCached_ = false;          // Whether stdout handle is cached in RDI
    bool useStdoutCaching_ = true;             // Enable stdout handle caching
    
    // Optimization level
    CodeGenOptLevel optLevel_ = CodeGenOptLevel::O2;  // Default to O2
    
    // Shared runtime routines (for O1/O2 - reduces code size)
    bool runtimeRoutinesEmitted_ = false;      // Whether runtime routines have been emitted
    std::string itoaRoutineLabel_;             // Label for shared itoa routine
    std::string ftoaRoutineLabel_;             // Label for shared ftoa routine
    std::string printIntRoutineLabel_;         // Label for shared print_int routine
    
    std::string newLabel(const std::string& prefix = "L");
    uint32_t addString(const std::string& str);
    uint32_t addFloatConstant(double value);    // Add float constant to data section
    void allocLocal(const std::string& name);
    void emitPrintInt(int32_t localOffset);
    void emitPrintString(uint32_t dataOffset);
    void emitPrintNewline();
    void emitItoa();
    void emitPrintRuntimeValue();
    void emitPrintFloat();                      // Print float from xmm0
    void emitFtoa();                            // Float to ASCII conversion
    
    // Shared runtime routines (for O1/O2 code size optimization)
    void emitRuntimeRoutines();                 // Emit shared runtime routines at end of code
    void emitItoaCall();                        // Call shared itoa routine (O1/O2)
    void emitFtoaCall();                        // Call shared ftoa routine (O1/O2)
    void emitPrintIntCall();                    // Call shared print_int routine (O1/O2)
    bool shouldInlineItoa() const;              // Check if itoa should be inlined based on opt level
    bool shouldInlineFtoa() const;              // Check if ftoa should be inlined based on opt level
    
    bool tryEvalConstant(Expression* expr, int64_t& outValue);
    bool tryEvalConstantFloat(Expression* expr, double& outValue);  // Evaluate float constants
    bool tryEvalConstantString(Expression* expr, std::string& outValue);
    void emitPrintExpr(Expression* expr);  // Helper to print a single expression
    bool isFloatExpression(Expression* expr);  // Check if expression is float type
    bool isStringReturningExpr(Expression* expr);  // Check if expression returns a string pointer
    void emitPrintStringPtr();  // Print string from pointer in rax (calculates length at runtime)
    void emitWriteConsole(uint32_t strRVA, size_t len);  // Emit WriteConsoleA with cached stdout handle
    void emitWriteConsoleBuffer();  // Emit WriteConsoleA for buffer in rdx with length in r8, uses cached handle
    
    // Stack frame optimization helpers
    int32_t calculateFunctionStackSize(Statement* body);  // Pre-scan to calculate stack needs
    int32_t calculateExprStackSize(Expression* expr);     // Calculate stack needs for expression
    void emitCallWithOptimizedStack(uint32_t importRVA);  // Emit call without stack adjustment
    void emitCallRelWithOptimizedStack(const std::string& label);  // Emit relative call
    
    // Dead code elimination helper - check if statement ends with terminator
    bool endsWithTerminator(Statement* stmt);  // Returns true if stmt ends with return/break/continue
    
    // Register allocation helpers
    void emitLoadVarToRax(const std::string& name);       // Load variable to RAX (from register or stack)
    void emitStoreRaxToVar(const std::string& name);      // Store RAX to variable (to register or stack)
    void emitSaveCalleeSavedRegs();                       // Save used callee-saved registers
    void emitRestoreCalleeSavedRegs();                    // Restore used callee-saved registers
    void emitMoveParamToVar(int paramIndex, const std::string& name, const std::string& type);  // Move param register to variable location
    
    // Leaf function optimization helpers
    bool checkIsLeafFunction(Statement* body);            // Check if function makes no calls
    bool statementHasCall(Statement* stmt);               // Check if statement contains a call
    bool expressionHasCall(Expression* expr);             // Check if expression contains a call
    
    // Closure capture analysis
    void collectCapturedVariables(Expression* expr, const std::set<std::string>& params, std::set<std::string>& captured);
    void collectCapturedVariablesStmt(Statement* stmt, const std::set<std::string>& params, std::set<std::string>& captured);
    
    // Module support
    std::string currentModule_;                           // Current module name (empty if top-level)
    std::map<std::string, std::vector<std::string>> moduleFunctions_;  // Module -> function names
    
    // Extern/FFI support
    std::map<std::string, uint32_t> externFunctions_;     // Extern function name -> import RVA
    
    // Trait/vtable support
    struct TraitInfo {
        std::string name;
        std::vector<std::string> methodNames;             // Method names in order
        std::vector<std::string> superTraits;             // Super traits (inheritance)
    };
    struct ImplInfo {
        std::string traitName;
        std::string typeName;
        std::map<std::string, std::string> methodLabels;  // Method name -> label
    };
    std::map<std::string, TraitInfo> traits_;             // Trait name -> info
    std::map<std::string, ImplInfo> impls_;               // "trait:type" -> impl info
    std::map<std::string, uint32_t> vtables_;             // "trait:type" -> vtable RVA
    std::map<std::string, std::vector<uint32_t>> vtableFixups_;  // "trait:type" -> list of fixup offsets
    
    // Trait dispatch helpers
    void finalizeVtables();                               // Generate vtables with actual function pointers
    void emitTraitMethodCall(const std::string& traitName, const std::string& methodName, 
                             int argCount);               // Emit dynamic dispatch call
    int getMethodIndex(const std::string& traitName, const std::string& methodName);  // Get method index in vtable
    std::string resolveTraitMethod(const std::string& typeName, const std::string& traitName, 
                                   const std::string& methodName);  // Resolve to concrete method label
    
    // Garbage collection support
    bool useGC_ = true;                                    // Enable GC for allocations
    bool gcInitEmitted_ = false;                           // Whether GC init code has been emitted
    uint32_t gcDataRVA_ = 0;                               // RVA of GC data section globals
    std::string gcCollectLabel_;                           // Label for GC collection routine
    
    // Generics / Monomorphization support
    Monomorphizer monomorphizer_;                          // Tracks generic instantiations
    std::unordered_map<std::string, FnDecl*> genericFunctions_;    // Generic function declarations
    std::unordered_map<std::string, RecordDecl*> genericRecords_;  // Generic record declarations
    std::vector<std::unique_ptr<FnDecl>> specializedFunctions_;    // Specialized function copies
    std::vector<std::unique_ptr<RecordDecl>> specializedRecords_;  // Specialized record copies
    
    // Generics helper methods
    void collectGenericInstantiations(Program& program);   // Collect all generic instantiations
    void emitSpecializedFunctions();                       // Emit code for specialized functions
    std::string resolveGenericCall(const std::string& fnName, const std::vector<TypePtr>& typeArgs);
    
    // GC helper methods
    void emitGCInit();                                     // Emit GC initialization at program start
    void emitGCShutdown();                                 // Emit GC shutdown at program end
    void emitGCAlloc(size_t size, GCObjectType type);      // Emit GC allocation call
    void emitGCAllocList(size_t capacity);                 // Emit list allocation via GC
    void emitGCAllocRecord(size_t fieldCount);             // Emit record allocation via GC
    void emitGCAllocClosure(size_t captureCount);          // Emit closure allocation via GC
    void emitGCAllocString(size_t len);                    // Emit string allocation via GC
    void emitGCAllocMap(size_t capacity);                  // Emit map allocation via GC
    void emitGCAllocMapEntry();                            // Emit map entry allocation via GC
    void emitGCAllocRaw(size_t size);                      // Emit raw allocation via GC
    void emitGCPushFrame();                                // Emit stack frame push for GC
    void emitGCPopFrame();                                 // Emit stack frame pop for GC
    void emitGCCollectRoutine();                           // Emit the GC collection routine (mark-and-sweep)
    
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
    void visit(DSLBlock& node) override;
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
};

} // namespace flex

#endif // FLEX_NATIVE_CODEGEN_H
