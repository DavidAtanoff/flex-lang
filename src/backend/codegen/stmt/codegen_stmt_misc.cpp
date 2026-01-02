// Flex Compiler - Native Code Generator Miscellaneous Statements
// Handles: Block, Return, Break, Continue, Try, Delete

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(Block& node) {
    for (auto& stmt : node.statements) {
        if (dynamic_cast<FnDecl*>(stmt.get())) {
            continue;
        }
        stmt->accept(*this);
    }
}

void NativeCodeGen::visit(ReturnStmt& node) {
    if (node.value) {
        node.value->accept(*this);
    } else {
        asm_.xor_rax_rax();
    }
    
    // Function epilogue - must match the prologue condition in FnDecl::visit
    // The prologue uses: isLeafFunction_ && varRegisters_.size() == params.size() && params.size() <= 4
    // Since we don't have access to params here, we use stackAllocated_ which tracks if sub rsp was done
    if (!stackAllocated_) {
        // Simplified epilogue for leaf functions without stack allocation
        emitRestoreCalleeSavedRegs();
    } else {
        // Full epilogue with stack cleanup
        asm_.add_rsp_imm32(functionStackSize_);
        emitRestoreCalleeSavedRegs();
        asm_.pop_rbp();
    }
    
    asm_.ret();
}

void NativeCodeGen::visit(BreakStmt& node) {
    (void)node;
    if (!loopStack.empty()) {
        asm_.jmp_rel32(loopStack.back().breakLabel);
    }
}

void NativeCodeGen::visit(ContinueStmt& node) {
    (void)node;
    if (!loopStack.empty()) {
        asm_.jmp_rel32(loopStack.back().continueLabel);
    }
}

void NativeCodeGen::visit(TryStmt& node) {
    // TryStmt in Flex is a try-else expression (like Rust's ? operator)
    // tryExpr is evaluated, and if it fails, elseExpr is used
    // For now, just evaluate the try expression
    node.tryExpr->accept(*this);
    
    // If we had proper error handling, we'd check for error and use elseExpr
    // For now, this is a simplified implementation
    if (node.elseExpr) {
        // The else expression would be used if tryExpr fails
        // This would require proper Result/Option type support
        (void)node.elseExpr;
    }
}

void NativeCodeGen::visit(DeleteStmt& node) {
    // Delete: free the memory pointed to by the expression
    node.expr->accept(*this);
    asm_.mov_r8_rax();
    
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
    asm_.mov_rcx_rax();
    asm_.xor_rax_rax();
    asm_.mov_rdx_rax();
    asm_.call_mem_rip(pe_.getImportRVA("HeapFree"));
    if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
}

} // namespace flex
