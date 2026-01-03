// Flex Compiler - Native Code Generator List Expressions
// Handles: ListExpr, ListCompExpr, RangeExpr

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(RangeExpr& node) {
    // RangeExpr is typically used in for loops and list comprehensions
    // When evaluated directly, we create a list from the range
    int64_t startVal, endVal;
    bool startConst = tryEvalConstant(node.start.get(), startVal);
    bool endConst = tryEvalConstant(node.end.get(), endVal);
    
    if (startConst && endConst) {
        // Constant range - create a list
        int64_t size = endVal - startVal + 1;
        if (size <= 0) {
            emitGCAllocList(4);
            return;
        }
        
        emitGCAllocList(static_cast<size_t>(size));
        
        allocLocal("$range_ptr");
        asm_.mov_mem_rbp_rax(locals["$range_ptr"]);
        
        // Set count
        asm_.mov_rcx_imm64(size);
        asm_.mov_rax_mem_rbp(locals["$range_ptr"]);
        asm_.mov_mem_rax_rcx();
        
        // Fill elements
        for (int64_t i = 0; i < size; i++) {
            asm_.mov_rax_mem_rbp(locals["$range_ptr"]);
            asm_.add_rax_imm32(16 + static_cast<int32_t>(i * 8));
            asm_.mov_rcx_imm64(startVal + i);
            asm_.mov_mem_rax_rcx();
        }
        
        asm_.mov_rax_mem_rbp(locals["$range_ptr"]);
    } else {
        // Dynamic range - evaluate at runtime
        node.start->accept(*this);
        asm_.push_rax();
        node.end->accept(*this);
        // For now, just return the end value (ranges are typically used in for loops)
        asm_.pop_rcx();
    }
    
    lastExprWasFloat_ = false;
}

void NativeCodeGen::visit(ListExpr& node) {
    if (node.elements.empty()) {
        emitGCAllocList(4);
        return;
    }
    
    std::vector<int64_t> values;
    bool allConstant = true;
    for (auto& elem : node.elements) {
        int64_t val;
        if (tryEvalConstant(elem.get(), val)) {
            values.push_back(val);
        } else {
            allConstant = false;
            break;
        }
    }
    
    if (allConstant) {
        std::vector<uint8_t> data;
        for (int64_t val : values) {
            for (int i = 0; i < 8; i++) {
                data.push_back((val >> (i * 8)) & 0xFF);
            }
        }
        uint32_t rva = pe_.addData(data.data(), data.size());
        asm_.lea_rax_rip_fixup(rva);
    } else {
        size_t capacity = node.elements.size();
        if (capacity < 4) capacity = 4;
        
        emitGCAllocList(capacity);
        
        std::string listPtrName = "$list_ptr_" + std::to_string(labelCounter++);
        allocLocal(listPtrName);
        asm_.mov_mem_rbp_rax(locals[listPtrName]);
        
        asm_.mov_rcx_imm64(static_cast<int64_t>(node.elements.size()));
        asm_.mov_rax_mem_rbp(locals[listPtrName]);
        asm_.code.push_back(0x48);
        asm_.code.push_back(0x89);
        asm_.code.push_back(0x08);
        
        for (size_t i = 0; i < node.elements.size(); i++) {
            node.elements[i]->accept(*this);
            
            asm_.mov_rcx_mem_rbp(locals[listPtrName]);
            
            int32_t offset = 16 + static_cast<int32_t>(i * 8);
            asm_.add_rcx_imm32(offset);
            asm_.mov_mem_rcx_rax();
        }
        
        asm_.mov_rax_mem_rbp(locals[listPtrName]);
    }
}

void NativeCodeGen::visit(ListCompExpr& node) {
    int64_t listSize = 0;
    bool sizeKnown = false;
    
    if (auto* range = dynamic_cast<RangeExpr*>(node.iterable.get())) {
        int64_t startVal, endVal;
        if (tryEvalConstant(range->start.get(), startVal) && 
            tryEvalConstant(range->end.get(), endVal)) {
            listSize = endVal - startVal + 1;
            if (listSize < 0) listSize = 0;
            sizeKnown = true;
        }
    } else if (auto* call = dynamic_cast<CallExpr*>(node.iterable.get())) {
        if (auto* calleeId = dynamic_cast<Identifier*>(call->callee.get())) {
            if (calleeId->name == "range") {
                if (call->args.size() == 1) {
                    int64_t endVal;
                    if (tryEvalConstant(call->args[0].get(), endVal)) {
                        listSize = endVal;
                        sizeKnown = true;
                    }
                } else if (call->args.size() >= 2) {
                    int64_t startVal, endVal;
                    if (tryEvalConstant(call->args[0].get(), startVal) &&
                        tryEvalConstant(call->args[1].get(), endVal)) {
                        listSize = endVal - startVal;
                        if (listSize < 0) listSize = 0;
                        sizeKnown = true;
                    }
                }
            }
        }
    }
    
    if (!sizeKnown || listSize <= 0) {
        asm_.xor_rax_rax();
        return;
    }
    
    emitGCAllocList(static_cast<size_t>(listSize));
    
    allocLocal("$listcomp_ptr");
    asm_.mov_mem_rbp_rax(locals["$listcomp_ptr"]);
    
    allocLocal("$listcomp_idx");
    asm_.xor_rax_rax();
    asm_.mov_mem_rbp_rax(locals["$listcomp_idx"]);
    
    allocLocal(node.var);
    
    if (auto* range = dynamic_cast<RangeExpr*>(node.iterable.get())) {
        range->start->accept(*this);
    } else if (auto* call = dynamic_cast<CallExpr*>(node.iterable.get())) {
        if (call->args.size() == 1) {
            asm_.xor_rax_rax();
        } else {
            call->args[0]->accept(*this);
        }
    }
    asm_.mov_mem_rbp_rax(locals[node.var]);
    
    allocLocal("$listcomp_end");
    if (auto* range = dynamic_cast<RangeExpr*>(node.iterable.get())) {
        range->end->accept(*this);
    } else if (auto* call = dynamic_cast<CallExpr*>(node.iterable.get())) {
        if (call->args.size() == 1) {
            call->args[0]->accept(*this);
        } else {
            call->args[1]->accept(*this);
        }
    }
    asm_.mov_mem_rbp_rax(locals["$listcomp_end"]);
    
    std::string loopLabel = newLabel("listcomp_loop");
    std::string endLabel = newLabel("listcomp_end");
    
    asm_.label(loopLabel);
    
    asm_.mov_rax_mem_rbp(locals[node.var]);
    asm_.cmp_rax_mem_rbp(locals["$listcomp_end"]);
    
    if (dynamic_cast<RangeExpr*>(node.iterable.get())) {
        asm_.jg_rel32(endLabel);
    } else {
        asm_.jge_rel32(endLabel);
    }
    
    if (node.condition) {
        std::string skipLabel = newLabel("listcomp_skip");
        node.condition->accept(*this);
        asm_.test_rax_rax();
        asm_.jz_rel32(skipLabel);
        
        node.expr->accept(*this);
        
        asm_.mov_rcx_mem_rbp(locals["$listcomp_ptr"]);
        asm_.add_rcx_imm32(16);
        asm_.mov_rdx_mem_rbp(locals["$listcomp_idx"]);
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
        asm_.code.push_back(0xE2); asm_.code.push_back(0x03);
        asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xD1);
        asm_.mov_mem_rcx_rax();
        
        asm_.mov_rax_mem_rbp(locals["$listcomp_idx"]);
        asm_.inc_rax();
        asm_.mov_mem_rbp_rax(locals["$listcomp_idx"]);
        
        asm_.label(skipLabel);
    } else {
        node.expr->accept(*this);
        
        asm_.push_rax();
        asm_.mov_rcx_mem_rbp(locals["$listcomp_ptr"]);
        asm_.add_rcx_imm32(16);
        asm_.mov_rdx_mem_rbp(locals["$listcomp_idx"]);
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
        asm_.code.push_back(0xE2); asm_.code.push_back(0x03);
        asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xD1);
        asm_.pop_rax();
        asm_.mov_mem_rcx_rax();
        
        asm_.mov_rax_mem_rbp(locals["$listcomp_idx"]);
        asm_.inc_rax();
        asm_.mov_mem_rbp_rax(locals["$listcomp_idx"]);
    }
    
    asm_.mov_rax_mem_rbp(locals[node.var]);
    asm_.inc_rax();
    asm_.mov_mem_rbp_rax(locals[node.var]);
    
    asm_.jmp_rel32(loopLabel);
    
    asm_.label(endLabel);
    
    asm_.mov_rax_mem_rbp(locals["$listcomp_ptr"]);
    
    listSizes["$listcomp_result"] = (size_t)listSize;
    lastExprWasFloat_ = false;
}

} // namespace flex
