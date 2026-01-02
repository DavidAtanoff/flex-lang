// Flex Compiler - Native Code Generator Collection Expressions
// Handles: ListExpr, RecordExpr, MapExpr, IndexExpr, ListCompExpr

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(IndexExpr& node) {
    // Handle map access with string key
    if (auto* strKey = dynamic_cast<StringLiteral*>(node.index.get())) {
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
        return;
    }
    
    // Check for constant list access (1-based indexing)
    if (auto* ident = dynamic_cast<Identifier*>(node.object.get())) {
        auto constListIt = constListVars.find(ident->name);
        if (constListIt != constListVars.end()) {
            int64_t indexVal;
            if (tryEvalConstant(node.index.get(), indexVal)) {
                // Convert 1-based index to 0-based for internal access
                int64_t zeroBasedIndex = indexVal - 1;
                if (zeroBasedIndex >= 0 && (size_t)zeroBasedIndex < constListIt->second.size()) {
                    asm_.mov_rax_imm64(constListIt->second[zeroBasedIndex]);
                    lastExprWasFloat_ = false;
                    return;
                }
            }
            // Runtime index into constant list - no header to skip
            // Convert 1-based to 0-based: subtract 1 from index
            node.index->accept(*this);
            asm_.dec_rax();  // Convert 1-based to 0-based
            asm_.push_rax();
            
            node.object->accept(*this);
            
            asm_.pop_rcx();
            
            // Calculate element offset: index * 8 (no header for constant lists)
            asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
            asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
            
            asm_.add_rax_rcx();
            asm_.mov_rax_mem_rax();
            
            lastExprWasFloat_ = false;
            return;
        }
    }
    
    // Runtime list indexing (GC-allocated lists have a 16-byte header)
    // Convert 1-based to 0-based: subtract 1 from index
    node.index->accept(*this);
    asm_.dec_rax();  // Convert 1-based to 0-based
    asm_.push_rax();
    
    node.object->accept(*this);
    
    // Skip the 16-byte list header (count + capacity) to get to elements
    asm_.add_rax_imm32(16);
    
    asm_.pop_rcx();
    
    // Calculate element offset: index * 8
    asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
    asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
    
    asm_.add_rax_rcx();
    asm_.mov_rax_mem_rax();
    
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

void NativeCodeGen::visit(RecordExpr& node) {
    if (node.fields.empty()) {
        asm_.xor_rax_rax();
        return;
    }
    
    size_t fieldCount = node.fields.size();
    
    emitGCAllocRecord(fieldCount);
    
    allocLocal("$record_ptr");
    asm_.mov_mem_rbp_rax(locals["$record_ptr"]);
    
    for (size_t i = 0; i < node.fields.size(); i++) {
        node.fields[i].second->accept(*this);
        
        asm_.mov_rcx_mem_rbp(locals["$record_ptr"]);
        
        int32_t offset = 8 + static_cast<int32_t>(i * 8);
        if (offset > 0) {
            asm_.add_rcx_imm32(offset);
        }
        asm_.mov_mem_rcx_rax();
    }
    
    asm_.mov_rax_mem_rbp(locals["$record_ptr"]);
}

void NativeCodeGen::visit(MapExpr& node) {
    if (node.entries.empty()) {
        emitGCAllocMap(16);
        return;
    }
    
    size_t capacity = 16;
    while (capacity < node.entries.size() * 2) capacity *= 2;
    
    emitGCAllocMap(capacity);
    
    allocLocal("$map_ptr");
    asm_.mov_mem_rbp_rax(locals["$map_ptr"]);
    
    asm_.mov_rcx_imm64(static_cast<int64_t>(node.entries.size()));
    asm_.mov_rax_mem_rbp(locals["$map_ptr"]);
    asm_.add_rax_imm32(8);
    asm_.mov_mem_rax_rcx();
    
    for (size_t i = 0; i < node.entries.size(); i++) {
        auto* keyStr = dynamic_cast<StringLiteral*>(node.entries[i].first.get());
        if (!keyStr) continue;
        
        uint32_t keyRva = addString(keyStr->value);
        
        uint64_t hash = 5381;
        for (char c : keyStr->value) {
            hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
        }
        
        emitGCAllocMapEntry();
        
        allocLocal("$entry_ptr");
        asm_.mov_mem_rbp_rax(locals["$entry_ptr"]);
        
        asm_.mov_rcx_imm64(static_cast<int64_t>(hash));
        asm_.mov_mem_rax_rcx();
        
        asm_.mov_rcx_mem_rbp(locals["$entry_ptr"]);
        asm_.add_rcx_imm32(8);
        asm_.lea_rax_rip_fixup(keyRva);
        asm_.mov_mem_rcx_rax();
        
        node.entries[i].second->accept(*this);
        asm_.mov_rcx_mem_rbp(locals["$entry_ptr"]);
        asm_.add_rcx_imm32(16);
        asm_.mov_mem_rcx_rax();
        
        asm_.mov_rcx_mem_rbp(locals["$entry_ptr"]);
        asm_.add_rcx_imm32(24);
        asm_.xor_rax_rax();
        asm_.mov_mem_rcx_rax();
        
        size_t bucketIdx = hash % capacity;
        
        asm_.mov_rax_mem_rbp(locals["$map_ptr"]);
        asm_.add_rax_imm32(16 + static_cast<int32_t>(bucketIdx * 8));
        
        asm_.mov_rcx_mem_rax();
        
        asm_.push_rax();
        asm_.mov_rax_mem_rbp(locals["$entry_ptr"]);
        asm_.add_rax_imm32(24);
        asm_.mov_mem_rax_rcx();
        
        asm_.pop_rax();
        asm_.mov_rcx_mem_rbp(locals["$entry_ptr"]);
        asm_.mov_mem_rax_rcx();
    }
    
    asm_.mov_rax_mem_rbp(locals["$map_ptr"]);
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
        // Skip the 16-byte list header (count + capacity) to get to elements
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
        // Skip the 16-byte list header (count + capacity) to get to elements
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

void NativeCodeGen::visit(MemberExpr& node) {
    // Check if this is a trait method call on a known type
    // For static dispatch, we resolve the method at compile time
    if (auto* id = dynamic_cast<Identifier*>(node.object.get())) {
        // Check if we have an impl for this type
        for (const auto& [implKey, info] : impls_) {
            if (info.typeName == id->name || implKey.find(":" + id->name) != std::string::npos) {
                // Found an impl for this type - check if it has the method
                auto methodIt = info.methodLabels.find(node.member);
                if (methodIt != info.methodLabels.end()) {
                    // This is a static method call - the label is stored for later use
                    // The actual call will be handled by CallExpr
                    // For now, just evaluate the object
                    node.object->accept(*this);
                    return;
                }
            }
        }
    }
    
    // Default: evaluate the object (for record field access, etc.)
    node.object->accept(*this);
    
    // TODO: Add record field access by offset
    // For now, records are accessed through the RecordExpr visitor
}

void NativeCodeGen::visit(RangeExpr& node) { (void)node; asm_.xor_rax_rax(); }

} // namespace flex
