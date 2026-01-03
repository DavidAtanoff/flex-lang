// Flex Compiler - Native Code Generator Index Expressions
// Handles: IndexExpr

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(IndexExpr& node) {
    // Handle map access with string key
    if (auto* strKey = dynamic_cast<StringLiteral*>(node.index.get())) {
        emitMapIndexAccess(node, strKey);
        return;
    }
    
    // Check for constant list access (1-based indexing)
    if (auto* ident = dynamic_cast<Identifier*>(node.object.get())) {
        auto constListIt = constListVars.find(ident->name);
        if (constListIt != constListVars.end()) {
            int64_t indexVal;
            if (tryEvalConstant(node.index.get(), indexVal)) {
                int64_t zeroBasedIndex = indexVal - 1;
                if (zeroBasedIndex >= 0 && (size_t)zeroBasedIndex < constListIt->second.size()) {
                    asm_.mov_rax_imm64(constListIt->second[zeroBasedIndex]);
                    lastExprWasFloat_ = false;
                    return;
                }
            }
            // Runtime index into constant list
            node.index->accept(*this);
            asm_.dec_rax();
            asm_.push_rax();
            
            node.object->accept(*this);
            asm_.pop_rcx();
            
            asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
            asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
            
            asm_.add_rax_rcx();
            asm_.mov_rax_mem_rax();
            
            lastExprWasFloat_ = false;
            return;
        }
        
        // Check for fixed-size array access (0-based indexing)
        auto fixedArrayIt = varFixedArrayTypes_.find(ident->name);
        if (fixedArrayIt != varFixedArrayTypes_.end()) {
            emitFixedArrayIndexAccess(node, fixedArrayIt->second);
            return;
        }
    }
    
    // Runtime list indexing (GC-allocated lists have a 16-byte header)
    node.index->accept(*this);
    asm_.dec_rax();
    asm_.push_rax();
    
    node.object->accept(*this);
    asm_.add_rax_imm32(16);
    
    asm_.pop_rcx();
    asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
    asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
    
    asm_.add_rax_rcx();
    asm_.mov_rax_mem_rax();
    
    lastExprWasFloat_ = false;
}

void NativeCodeGen::emitMapIndexAccess(IndexExpr& node, StringLiteral* strKey) {
    uint64_t hash = 5381;
    for (char c : strKey->value) {
        hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
    }
    
    uint32_t keyRva = addString(strKey->value);
    
    node.object->accept(*this);
    
    allocLocal("$map_get_ptr");
    asm_.mov_mem_rbp_rax(locals["$map_get_ptr"]);
    
    asm_.mov_rcx_mem_rax();
    
    asm_.mov_rax_imm64(static_cast<int64_t>(hash));
    asm_.code.push_back(0x48); asm_.code.push_back(0x31); asm_.code.push_back(0xD2);
    asm_.code.push_back(0x48); asm_.code.push_back(0xF7); asm_.code.push_back(0xF1);
    
    asm_.mov_rax_mem_rbp(locals["$map_get_ptr"]);
    asm_.add_rax_imm32(16);
    asm_.code.push_back(0x48); asm_.code.push_back(0xC1); 
    asm_.code.push_back(0xE2); asm_.code.push_back(0x03);
    asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xD0);
    
    asm_.mov_rax_mem_rax();
    
    std::string searchLoop = newLabel("map_search");
    std::string foundLabel = newLabel("map_found");
    std::string notFoundLabel = newLabel("map_notfound");
    
    asm_.label(searchLoop);
    asm_.test_rax_rax();
    asm_.jz_rel32(notFoundLabel);
    
    asm_.push_rax();
    asm_.mov_rcx_mem_rax();
    asm_.mov_rdx_imm64(static_cast<int64_t>(hash));
    asm_.code.push_back(0x48); asm_.code.push_back(0x39); asm_.code.push_back(0xD1);
    asm_.pop_rax();
    asm_.jnz_rel32(searchLoop + "_next");
    
    asm_.push_rax();
    asm_.add_rax_imm32(8);
    asm_.mov_rcx_mem_rax();
    
    asm_.lea_rax_rip_fixup(keyRva);
    asm_.mov_rdx_rax();
    
    std::string cmpLoop = newLabel("strcmp");
    std::string cmpDone = newLabel("strcmp_done");
    std::string cmpNotEqual = newLabel("strcmp_ne");
    
    asm_.label(cmpLoop);
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01);
    asm_.code.push_back(0x44); asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); 
    asm_.code.push_back(0x02);
    
    asm_.code.push_back(0x44); asm_.code.push_back(0x39); asm_.code.push_back(0xC0);
    asm_.jnz_rel32(cmpNotEqual);
    
    asm_.test_rax_rax();
    asm_.jz_rel32(cmpDone);
    
    asm_.inc_rcx();
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
    asm_.jmp_rel32(cmpLoop);
    
    asm_.label(cmpNotEqual);
    asm_.pop_rax();
    asm_.jmp_rel32(searchLoop + "_next");
    
    asm_.label(cmpDone);
    asm_.pop_rax();
    asm_.jmp_rel32(foundLabel);
    
    asm_.label(searchLoop + "_next");
    asm_.add_rax_imm32(24);
    asm_.mov_rax_mem_rax();
    asm_.jmp_rel32(searchLoop);
    
    asm_.label(notFoundLabel);
    asm_.xor_rax_rax();
    std::string endLabel = newLabel("map_get_end");
    asm_.jmp_rel32(endLabel);
    
    asm_.label(foundLabel);
    asm_.add_rax_imm32(16);
    asm_.mov_rax_mem_rax();
    
    asm_.label(endLabel);
    lastExprWasFloat_ = false;
}

void NativeCodeGen::emitFixedArrayIndexAccess(IndexExpr& node, const FixedArrayInfo& info) {
    node.index->accept(*this);
    asm_.push_rax();
    
    node.object->accept(*this);
    asm_.pop_rcx();
    
    if (info.elementSize == 8) {
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
        asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
    } else if (info.elementSize == 4) {
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
        asm_.code.push_back(0xE1); asm_.code.push_back(0x02);
    } else if (info.elementSize == 2) {
        asm_.code.push_back(0x48); asm_.code.push_back(0xD1);
        asm_.code.push_back(0xE1);
    } else if (info.elementSize != 1) {
        asm_.mov_rdx_imm64(info.elementSize);
        asm_.code.push_back(0x48); asm_.code.push_back(0x0F);
        asm_.code.push_back(0xAF); asm_.code.push_back(0xCA);
    }
    
    asm_.add_rax_rcx();
    
    if (info.elementSize == 1) {
        asm_.code.push_back(0x48);
        asm_.code.push_back(0x0F);
        asm_.code.push_back(0xB6);
        asm_.code.push_back(0x00);
    } else if (info.elementSize == 2) {
        asm_.code.push_back(0x48);
        asm_.code.push_back(0x0F);
        asm_.code.push_back(0xB7);
        asm_.code.push_back(0x00);
    } else if (info.elementSize == 4) {
        asm_.code.push_back(0x8B);
        asm_.code.push_back(0x00);
    } else {
        asm_.mov_rax_mem_rax();
    }
    
    lastExprWasFloat_ = (info.elementType == "float" || info.elementType == "f64" || info.elementType == "f32");
}

} // namespace flex
