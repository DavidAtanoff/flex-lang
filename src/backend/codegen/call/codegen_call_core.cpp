// Flex Compiler - Native Code Generator Call Core
// Main CallExpr visitor and function call handling

#include "backend/codegen/codegen_base.h"
#include "semantic/types/types.h"

namespace flex {

void NativeCodeGen::visit(CallExpr& node) {
    // Handle module function calls (e.g., math.add)
    if (auto* member = dynamic_cast<MemberExpr*>(node.callee.get())) {
        if (auto* moduleId = dynamic_cast<Identifier*>(member->object.get())) {
            std::string mangledName = moduleId->name + "." + member->member;
            
            if (asm_.labels.count(mangledName)) {
                for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                    node.args[i]->accept(*this);
                    asm_.push_rax();
                }
                
                if (node.args.size() >= 1) asm_.pop_rcx();
                if (node.args.size() >= 2) asm_.pop_rdx();
                if (node.args.size() >= 3) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x58);
                }
                if (node.args.size() >= 4) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x59);
                }
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                asm_.call_rel32(mangledName);
                if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                return;
            }
            
            // Check for trait method call (static dispatch)
            // Look for Type.method() pattern where Type has an impl
            for (const auto& [implKey, info] : impls_) {
                if (info.typeName == moduleId->name) {
                    auto methodIt = info.methodLabels.find(member->member);
                    if (methodIt != info.methodLabels.end()) {
                        // Found the method - emit static dispatch call
                        // Push arguments in reverse order
                        for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                            node.args[i]->accept(*this);
                            asm_.push_rax();
                        }
                        
                        // Pop into argument registers (Windows x64 ABI)
                        if (node.args.size() >= 1) asm_.pop_rcx();
                        if (node.args.size() >= 2) asm_.pop_rdx();
                        if (node.args.size() >= 3) {
                            asm_.code.push_back(0x41); asm_.code.push_back(0x58); // pop r8
                        }
                        if (node.args.size() >= 4) {
                            asm_.code.push_back(0x41); asm_.code.push_back(0x59); // pop r9
                        }
                        
                        if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                        asm_.call_rel32(methodIt->second);
                        if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                        return;
                    }
                }
            }
        }
        
        // Check for instance method call (obj.method())
        // This handles both inherent methods and trait methods on concrete types
        for (const auto& [implKey, info] : impls_) {
            auto methodIt = info.methodLabels.find(member->member);
            if (methodIt != info.methodLabels.end()) {
                // Evaluate the object (self) first
                member->object->accept(*this);
                asm_.push_rax();  // Save self
                
                // Push remaining arguments in reverse order
                for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                    node.args[i]->accept(*this);
                    asm_.push_rax();
                }
                
                // Pop into argument registers
                // First arg (RCX) is self, then the rest
                size_t totalArgs = node.args.size() + 1;  // +1 for self
                if (totalArgs >= 1) asm_.pop_rcx();  // self
                if (totalArgs >= 2) asm_.pop_rdx();
                if (totalArgs >= 3) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x58); // pop r8
                }
                if (totalArgs >= 4) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x59); // pop r9
                }
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                asm_.call_rel32(methodIt->second);
                if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                return;
            }
        }
    }
    
    if (auto* id = dynamic_cast<Identifier*>(node.callee.get())) {
        // Check if this is an extern function
        auto externIt = externFunctions_.find(id->name);
        if (externIt != externFunctions_.end()) {
            for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                node.args[i]->accept(*this);
                asm_.push_rax();
            }
            
            if (node.args.size() >= 1) asm_.pop_rcx();
            if (node.args.size() >= 2) asm_.pop_rdx();
            if (node.args.size() >= 3) {
                asm_.code.push_back(0x41); asm_.code.push_back(0x58);
            }
            if (node.args.size() >= 4) {
                asm_.code.push_back(0x41); asm_.code.push_back(0x59);
            }
            
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
            asm_.call_mem_rip(pe_.getImportRVA(id->name));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
            return;
        }
        
        // Handle len()
        if (id->name == "len" && node.args.size() == 1) {
            if (auto* strLit = dynamic_cast<StringLiteral*>(node.args[0].get())) {
                asm_.mov_rax_imm64((int64_t)strLit->value.length());
                return;
            }
            if (auto* ident = dynamic_cast<Identifier*>(node.args[0].get())) {
                auto strIt = constStrVars.find(ident->name);
                if (strIt != constStrVars.end() && !strIt->second.empty()) {
                    asm_.mov_rax_imm64((int64_t)strIt->second.length());
                    return;
                }
                auto listIt = listSizes.find(ident->name);
                if (listIt != listSizes.end()) {
                    asm_.mov_rax_imm64((int64_t)listIt->second);
                    return;
                }
                auto constListIt = constListVars.find(ident->name);
                if (constListIt != constListVars.end()) {
                    asm_.mov_rax_imm64((int64_t)constListIt->second.size());
                    return;
                }
                if (strIt != constStrVars.end()) {
                    node.args[0]->accept(*this);
                    asm_.mov_rcx_rax();
                    asm_.xor_rax_rax();
                    
                    std::string loopLabel = newLabel("strlen_loop");
                    std::string doneLabel = newLabel("strlen_done");
                    
                    asm_.label(loopLabel);
                    asm_.code.push_back(0x48); asm_.code.push_back(0x0F); asm_.code.push_back(0xB6);
                    asm_.code.push_back(0x14); asm_.code.push_back(0x01);
                    asm_.code.push_back(0x84); asm_.code.push_back(0xD2);
                    asm_.jz_rel32(doneLabel);
                    asm_.inc_rax();
                    asm_.jmp_rel32(loopLabel);
                    
                    asm_.label(doneLabel);
                    return;
                }
            }
            if (auto* list = dynamic_cast<ListExpr*>(node.args[0].get())) {
                asm_.mov_rax_imm64((int64_t)list->elements.size());
                return;
            }
            asm_.xor_rax_rax();
            return;
        }
        
        // Handle upper()
        if (id->name == "upper" && node.args.size() == 1) {
            std::string strVal;
            if (tryEvalConstantString(node.args[0].get(), strVal)) {
                for (char& c : strVal) {
                    if (c >= 'a' && c <= 'z') c -= 32;
                }
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            
            allocLocal("$upper_buf");
            int32_t bufOffset = locals["$upper_buf"];
            for (int i = 0; i < 31; i++) allocLocal("$upper_pad" + std::to_string(i));
            
            node.args[0]->accept(*this);
            asm_.mov_rcx_rax();
            
            asm_.lea_rax_rbp(bufOffset);
            asm_.mov_rdx_rax();
            
            std::string loopLabel = newLabel("upper_loop");
            std::string doneLabel = newLabel("upper_done");
            std::string noConvertLabel = newLabel("upper_noconv");
            
            asm_.label(loopLabel);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01);
            
            asm_.test_rax_rax();
            asm_.jz_rel32(doneLabel);
            
            asm_.code.push_back(0x3C); asm_.code.push_back('a');
            asm_.jl_rel32(noConvertLabel);
            asm_.code.push_back(0x3C); asm_.code.push_back('z');
            asm_.jg_rel32(noConvertLabel);
            
            asm_.code.push_back(0x2C); asm_.code.push_back(32);
            
            asm_.label(noConvertLabel);
            asm_.code.push_back(0x88); asm_.code.push_back(0x02);
            
            asm_.inc_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
            
            asm_.jmp_rel32(loopLabel);
            
            asm_.label(doneLabel);
            asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00);
            
            asm_.lea_rax_rbp(bufOffset);
            return;
        }
        
        // Handle lower() - returns lowercase string
        if (id->name == "lower" && node.args.size() == 1) {
            std::string strVal;
            if (tryEvalConstantString(node.args[0].get(), strVal)) {
                for (char& c : strVal) {
                    if (c >= 'A' && c <= 'Z') c += 32;
                }
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            
            allocLocal("$lower_buf");
            int32_t bufOffset = locals["$lower_buf"];
            for (int i = 0; i < 31; i++) allocLocal("$lower_pad" + std::to_string(i));
            
            node.args[0]->accept(*this);
            asm_.mov_rcx_rax();
            
            asm_.lea_rax_rbp(bufOffset);
            asm_.mov_rdx_rax();
            
            std::string loopLabel = newLabel("lower_loop");
            std::string doneLabel = newLabel("lower_done");
            std::string noConvertLabel = newLabel("lower_noconv");
            
            asm_.label(loopLabel);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01); // movzx eax, byte [rcx]
            
            asm_.test_rax_rax();
            asm_.jz_rel32(doneLabel);
            
            asm_.code.push_back(0x3C); asm_.code.push_back('A'); // cmp al, 'A'
            asm_.jl_rel32(noConvertLabel);
            asm_.code.push_back(0x3C); asm_.code.push_back('Z'); // cmp al, 'Z'
            asm_.jg_rel32(noConvertLabel);
            
            asm_.code.push_back(0x04); asm_.code.push_back(32); // add al, 32
            
            asm_.label(noConvertLabel);
            asm_.code.push_back(0x88); asm_.code.push_back(0x02); // mov [rdx], al
            
            asm_.inc_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1); // inc rcx
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2); // inc rdx
            
            asm_.jmp_rel32(loopLabel);
            
            asm_.label(doneLabel);
            asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00); // mov byte [rdx], 0
            
            asm_.lea_rax_rbp(bufOffset);
            return;
        }
        
        // Handle trim() - removes leading and trailing whitespace
        if (id->name == "trim" && node.args.size() == 1) {
            std::string strVal;
            if (tryEvalConstantString(node.args[0].get(), strVal)) {
                size_t start = strVal.find_first_not_of(" \t\n\r");
                size_t end = strVal.find_last_not_of(" \t\n\r");
                if (start == std::string::npos) {
                    strVal = "";
                } else {
                    strVal = strVal.substr(start, end - start + 1);
                }
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            
            allocLocal("$trim_buf");
            int32_t bufOffset = locals["$trim_buf"];
            for (int i = 0; i < 31; i++) allocLocal("$trim_pad" + std::to_string(i));
            
            node.args[0]->accept(*this);
            asm_.mov_rcx_rax(); // rcx = source string
            
            // Skip leading whitespace
            std::string skipLeadLoop = newLabel("trim_lead");
            std::string skipLeadDone = newLabel("trim_lead_done");
            
            asm_.label(skipLeadLoop);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01); // movzx eax, byte [rcx]
            asm_.test_rax_rax();
            asm_.jz_rel32(skipLeadDone);
            asm_.code.push_back(0x3C); asm_.code.push_back(' '); // cmp al, ' '
            asm_.code.push_back(0x74); asm_.code.push_back(0x0C); // je skip (inc and loop)
            asm_.code.push_back(0x3C); asm_.code.push_back('\t');
            asm_.code.push_back(0x74); asm_.code.push_back(0x08);
            asm_.code.push_back(0x3C); asm_.code.push_back('\n');
            asm_.code.push_back(0x74); asm_.code.push_back(0x04);
            asm_.code.push_back(0x3C); asm_.code.push_back('\r');
            asm_.jnz_rel32(skipLeadDone);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1); // inc rcx
            asm_.jmp_rel32(skipLeadLoop);
            
            asm_.label(skipLeadDone);
            // rcx now points to first non-whitespace char (or null)
            
            // Copy to buffer
            asm_.lea_rax_rbp(bufOffset);
            asm_.mov_rdx_rax(); // rdx = dest buffer
            asm_.push_rdx(); // save buffer start
            
            std::string copyLoop = newLabel("trim_copy");
            std::string copyDone = newLabel("trim_copy_done");
            
            asm_.label(copyLoop);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01); // movzx eax, byte [rcx]
            asm_.test_rax_rax();
            asm_.jz_rel32(copyDone);
            asm_.code.push_back(0x88); asm_.code.push_back(0x02); // mov [rdx], al
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1); // inc rcx
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2); // inc rdx
            asm_.jmp_rel32(copyLoop);
            
            asm_.label(copyDone);
            asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00); // null terminate
            
            // Now trim trailing whitespace by walking back from rdx
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xCA); // dec rdx
            asm_.pop_rcx(); // rcx = buffer start
            
            std::string trimTrailLoop = newLabel("trim_trail");
            std::string trimTrailDone = newLabel("trim_trail_done");
            
            asm_.label(trimTrailLoop);
            asm_.code.push_back(0x48); asm_.code.push_back(0x39); asm_.code.push_back(0xCA); // cmp rdx, rcx
            asm_.jl_rel32(trimTrailDone);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02); // movzx eax, byte [rdx]
            asm_.code.push_back(0x3C); asm_.code.push_back(' ');
            asm_.code.push_back(0x74); asm_.code.push_back(0x0C);
            asm_.code.push_back(0x3C); asm_.code.push_back('\t');
            asm_.code.push_back(0x74); asm_.code.push_back(0x08);
            asm_.code.push_back(0x3C); asm_.code.push_back('\n');
            asm_.code.push_back(0x74); asm_.code.push_back(0x04);
            asm_.code.push_back(0x3C); asm_.code.push_back('\r');
            asm_.jnz_rel32(trimTrailDone);
            asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00); // null terminate here
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xCA); // dec rdx
            asm_.jmp_rel32(trimTrailLoop);
            
            asm_.label(trimTrailDone);
            asm_.mov_rax_rcx(); // return buffer start
            return;
        }
        
        // Handle starts_with(str, prefix) - returns 1 if str starts with prefix, 0 otherwise
        if (id->name == "starts_with" && node.args.size() == 2) {
            std::string strVal, prefix;
            if (tryEvalConstantString(node.args[0].get(), strVal) && 
                tryEvalConstantString(node.args[1].get(), prefix)) {
                bool result = strVal.size() >= prefix.size() && 
                              strVal.compare(0, prefix.size(), prefix) == 0;
                asm_.mov_rax_imm64(result ? 1 : 0);
                return;
            }
            
            // Runtime implementation
            node.args[0]->accept(*this);
            asm_.push_rax(); // save string
            node.args[1]->accept(*this);
            asm_.mov_rdx_rax(); // rdx = prefix
            asm_.pop_rcx(); // rcx = string
            
            std::string loopLabel = newLabel("starts_loop");
            std::string matchLabel = newLabel("starts_match");
            std::string noMatchLabel = newLabel("starts_nomatch");
            
            asm_.label(loopLabel);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02); // movzx eax, byte [rdx]
            asm_.test_rax_rax();
            asm_.jz_rel32(matchLabel); // prefix ended = match
            
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x39); // movzx edi, byte [rcx]
            asm_.code.push_back(0x39); asm_.code.push_back(0xC7); // cmp edi, eax
            asm_.jnz_rel32(noMatchLabel);
            
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1); // inc rcx
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2); // inc rdx
            asm_.jmp_rel32(loopLabel);
            
            asm_.label(matchLabel);
            asm_.mov_rax_imm64(1);
            std::string doneLabel = newLabel("starts_done");
            asm_.jmp_rel32(doneLabel);
            
            asm_.label(noMatchLabel);
            asm_.xor_rax_rax();
            
            asm_.label(doneLabel);
            return;
        }
        
        // Handle ends_with(str, suffix) - returns 1 if str ends with suffix, 0 otherwise
        if (id->name == "ends_with" && node.args.size() == 2) {
            std::string strVal, suffix;
            if (tryEvalConstantString(node.args[0].get(), strVal) && 
                tryEvalConstantString(node.args[1].get(), suffix)) {
                bool result = strVal.size() >= suffix.size() && 
                              strVal.compare(strVal.size() - suffix.size(), suffix.size(), suffix) == 0;
                asm_.mov_rax_imm64(result ? 1 : 0);
                return;
            }
            
            // Runtime: need to find string lengths first
            node.args[0]->accept(*this);
            asm_.push_rax(); // save string ptr
            
            // Get string length
            asm_.mov_rcx_rax();
            asm_.xor_rax_rax();
            std::string lenLoop1 = newLabel("ends_len1");
            std::string lenDone1 = newLabel("ends_len1_done");
            asm_.label(lenLoop1);
            asm_.code.push_back(0x80); asm_.code.push_back(0x39); asm_.code.push_back(0x00); // cmp byte [rcx], 0
            asm_.jz_rel32(lenDone1);
            asm_.inc_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1); // inc rcx
            asm_.jmp_rel32(lenLoop1);
            asm_.label(lenDone1);
            asm_.push_rax(); // save string length
            
            node.args[1]->accept(*this);
            asm_.push_rax(); // save suffix ptr
            
            // Get suffix length
            asm_.mov_rcx_rax();
            asm_.xor_rax_rax();
            std::string lenLoop2 = newLabel("ends_len2");
            std::string lenDone2 = newLabel("ends_len2_done");
            asm_.label(lenLoop2);
            asm_.code.push_back(0x80); asm_.code.push_back(0x39); asm_.code.push_back(0x00);
            asm_.jz_rel32(lenDone2);
            asm_.inc_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
            asm_.jmp_rel32(lenLoop2);
            asm_.label(lenDone2);
            // rax = suffix length
            
            asm_.pop_rdx(); // rdx = suffix ptr
            asm_.pop_rcx(); // rcx = string length
            asm_.pop_rdi(); // rdi = string ptr (using rdi as temp)
            
            // Check if string is long enough
            std::string noMatchLabel = newLabel("ends_nomatch");
            asm_.code.push_back(0x48); asm_.code.push_back(0x39); asm_.code.push_back(0xC1); // cmp rcx, rax
            asm_.jl_rel32(noMatchLabel);
            
            // Move string ptr to (string + strlen - suffixlen)
            asm_.code.push_back(0x48); asm_.code.push_back(0x29); asm_.code.push_back(0xC1); // sub rcx, rax
            asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xCF); // add rdi, rcx
            asm_.mov_rcx_rdi(); // rcx = string + offset
            
            // Compare suffix with end of string
            std::string cmpLoop = newLabel("ends_cmp");
            std::string matchLabel = newLabel("ends_match");
            
            asm_.label(cmpLoop);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02); // movzx eax, byte [rdx]
            asm_.test_rax_rax();
            asm_.jz_rel32(matchLabel);
            
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x39); // movzx edi, byte [rcx]
            asm_.code.push_back(0x39); asm_.code.push_back(0xC7); // cmp edi, eax
            asm_.jnz_rel32(noMatchLabel);
            
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
            asm_.jmp_rel32(cmpLoop);
            
            asm_.label(matchLabel);
            asm_.mov_rax_imm64(1);
            std::string doneLabel = newLabel("ends_done");
            asm_.jmp_rel32(doneLabel);
            
            asm_.label(noMatchLabel);
            asm_.xor_rax_rax();
            
            asm_.label(doneLabel);
            return;
        }
        
        // Handle substring(str, start, len) - returns substring
        if (id->name == "substring" && (node.args.size() == 2 || node.args.size() == 3)) {
            std::string strVal;
            int64_t start, len = -1;
            bool hasLen = node.args.size() == 3;
            
            if (tryEvalConstantString(node.args[0].get(), strVal) &&
                tryEvalConstant(node.args[1].get(), start) &&
                (!hasLen || tryEvalConstant(node.args[2].get(), len))) {
                if (start < 0) start = 0;
                if ((size_t)start >= strVal.size()) {
                    strVal = "";
                } else if (hasLen && len >= 0) {
                    strVal = strVal.substr(start, len);
                } else {
                    strVal = strVal.substr(start);
                }
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            
            // Runtime implementation
            allocLocal("$substr_buf");
            int32_t bufOffset = locals["$substr_buf"];
            for (int i = 0; i < 63; i++) allocLocal("$substr_pad" + std::to_string(i));
            
            node.args[0]->accept(*this);
            asm_.push_rax(); // save string
            node.args[1]->accept(*this);
            asm_.mov_rcx_rax(); // rcx = start index
            
            if (hasLen) {
                node.args[2]->accept(*this);
                asm_.mov_r8_rax(); // r8 = length
            } else {
                asm_.mov_rax_imm64(0x7FFFFFFF);
                asm_.mov_r8_rax(); // r8 = max length
            }
            
            asm_.pop_rax(); // rax = string
            asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xC8); // add rax, rcx (skip start chars)
            asm_.mov_rcx_rax(); // rcx = source (string + start)
            
            asm_.lea_rax_rbp(bufOffset);
            asm_.mov_rdx_rax(); // rdx = dest buffer
            
            std::string copyLoop = newLabel("substr_copy");
            std::string copyDone = newLabel("substr_done");
            
            asm_.label(copyLoop);
            asm_.code.push_back(0x4D); asm_.code.push_back(0x85); asm_.code.push_back(0xC0); // test r8, r8
            asm_.jz_rel32(copyDone);
            
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01); // movzx eax, byte [rcx]
            asm_.test_rax_rax();
            asm_.jz_rel32(copyDone);
            
            asm_.code.push_back(0x88); asm_.code.push_back(0x02); // mov [rdx], al
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1); // inc rcx
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2); // inc rdx
            asm_.code.push_back(0x49); asm_.code.push_back(0xFF); asm_.code.push_back(0xC8); // dec r8
            asm_.jmp_rel32(copyLoop);
            
            asm_.label(copyDone);
            asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00); // null terminate
            
            asm_.lea_rax_rbp(bufOffset);
            return;
        }
        
        // Handle replace(str, old, new) - replaces first occurrence
        if (id->name == "replace" && node.args.size() == 3) {
            std::string strVal, oldStr, newStr;
            if (tryEvalConstantString(node.args[0].get(), strVal) &&
                tryEvalConstantString(node.args[1].get(), oldStr) &&
                tryEvalConstantString(node.args[2].get(), newStr)) {
                size_t pos = strVal.find(oldStr);
                if (pos != std::string::npos) {
                    strVal.replace(pos, oldStr.size(), newStr);
                }
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            
            // Runtime: simplified - just return original string for now
            // Full implementation would need dynamic memory allocation
            node.args[0]->accept(*this);
            return;
        }
        
        // Handle split(str, delimiter) - returns list of strings
        // For now, just return the original string as a single-element operation
        // Full implementation would need list allocation
        if (id->name == "split" && node.args.size() == 2) {
            std::string strVal, delim;
            if (tryEvalConstantString(node.args[0].get(), strVal) &&
                tryEvalConstantString(node.args[1].get(), delim)) {
                // For constant case, just return first part before delimiter
                size_t pos = strVal.find(delim);
                if (pos != std::string::npos) {
                    strVal = strVal.substr(0, pos);
                }
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            node.args[0]->accept(*this);
            return;
        }
        
        // Handle join(list, delimiter) - joins list elements with delimiter
        // Simplified: for now just return empty string
        if (id->name == "join" && node.args.size() == 2) {
            uint32_t rva = addString("");
            asm_.lea_rax_rip_fixup(rva);
            return;
        }
        
        // Handle index_of(str, substr) - returns index of first occurrence, -1 if not found
        if (id->name == "index_of" && node.args.size() == 2) {
            std::string strVal, substr;
            if (tryEvalConstantString(node.args[0].get(), strVal) &&
                tryEvalConstantString(node.args[1].get(), substr)) {
                size_t pos = strVal.find(substr);
                int64_t result = (pos == std::string::npos) ? -1 : (int64_t)pos;
                asm_.mov_rax_imm64(result);
                return;
            }
            
            // Runtime implementation
            node.args[0]->accept(*this);
            asm_.push_rax(); // save string
            node.args[1]->accept(*this);
            asm_.mov_rdx_rax(); // rdx = substr
            asm_.pop_rcx(); // rcx = string
            
            asm_.xor_rax_rax(); // rax = current index
            
            std::string outerLoop = newLabel("indexof_outer");
            std::string innerLoop = newLabel("indexof_inner");
            std::string foundLabel = newLabel("indexof_found");
            std::string notFoundLabel = newLabel("indexof_notfound");
            std::string nextOuter = newLabel("indexof_next");
            
            asm_.label(outerLoop);
            asm_.code.push_back(0x80); asm_.code.push_back(0x39); asm_.code.push_back(0x00); // cmp byte [rcx], 0
            asm_.jz_rel32(notFoundLabel);
            
            // Try to match substr at current position
            asm_.push_rcx();
            asm_.push_rdx();
            asm_.push_rax();
            
            asm_.label(innerLoop);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02); // movzx eax, byte [rdx]
            asm_.test_rax_rax();
            asm_.jz_rel32(foundLabel); // substr ended = found
            
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x39); // movzx edi, byte [rcx]
            asm_.code.push_back(0x39); asm_.code.push_back(0xC7); // cmp edi, eax
            asm_.jnz_rel32(nextOuter);
            
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
            asm_.jmp_rel32(innerLoop);
            
            asm_.label(nextOuter);
            asm_.pop_rax();
            asm_.pop_rdx();
            asm_.pop_rcx();
            asm_.inc_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
            asm_.jmp_rel32(outerLoop);
            
            asm_.label(foundLabel);
            asm_.pop_rax(); // restore index
            asm_.add_rsp_imm32(16); // clean up rdx and rcx
            std::string doneLabel = newLabel("indexof_done");
            asm_.jmp_rel32(doneLabel);
            
            asm_.label(notFoundLabel);
            asm_.mov_rax_imm64(-1);
            
            asm_.label(doneLabel);
            return;
        }
        
        // Handle contains()
        if (id->name == "contains" && node.args.size() == 2) {
            std::string haystack, needle;
            bool haystackConst = tryEvalConstantString(node.args[0].get(), haystack);
            bool needleConst = tryEvalConstantString(node.args[1].get(), needle);
            
            if (haystackConst && needleConst) {
                bool found = haystack.find(needle) != std::string::npos;
                asm_.mov_rax_imm64(found ? 1 : 0);
                return;
            }
            asm_.xor_rax_rax();
            return;
        }
        
        // Handle push()
        if (id->name == "push" && node.args.size() == 2) {
            node.args[0]->accept(*this);
            asm_.push_rax();
            
            node.args[1]->accept(*this);
            asm_.push_rax();
            
            std::string listName;
            size_t oldSize = 0;
            bool knownSize = false;
            if (auto* ident = dynamic_cast<Identifier*>(node.args[0].get())) {
                listName = ident->name;
                auto sizeIt = listSizes.find(listName);
                if (sizeIt != listSizes.end()) {
                    oldSize = sizeIt->second;
                    knownSize = true;
                }
            }
            
            if (knownSize && oldSize > 0) {
                size_t newSize = oldSize + 1;
                
                emitGCAllocList(newSize);
                
                allocLocal("$push_newlist");
                asm_.mov_mem_rbp_rax(locals["$push_newlist"]);
                
                for (size_t i = 0; i < oldSize; i++) {
                    asm_.code.push_back(0x48); asm_.code.push_back(0x8B);
                    asm_.code.push_back(0x44); asm_.code.push_back(0x24);
                    asm_.code.push_back(0x08);
                    
                    if (i > 0) {
                        asm_.add_rax_imm32((int32_t)(i * 8));
                    }
                    asm_.mov_rax_mem_rax();
                    
                    asm_.mov_rcx_mem_rbp(locals["$push_newlist"]);
                    if (i > 0) {
                        asm_.add_rcx_imm32((int32_t)(i * 8));
                    }
                    asm_.mov_mem_rcx_rax();
                }
                
                asm_.pop_rax();
                asm_.mov_rcx_mem_rbp(locals["$push_newlist"]);
                asm_.add_rcx_imm32((int32_t)(oldSize * 8));
                asm_.mov_mem_rcx_rax();
                
                asm_.pop_rcx();
                
                asm_.mov_rax_mem_rbp(locals["$push_newlist"]);
                
                if (!listName.empty()) {
                    listSizes[listName] = newSize;
                }
            } else {
                allocLocal("$push_oldlist");
                allocLocal("$push_element");
                allocLocal("$push_oldsize");
                allocLocal("$push_newlist");
                
                asm_.pop_rax();
                asm_.mov_mem_rbp_rax(locals["$push_element"]);
                asm_.pop_rax();
                asm_.mov_mem_rbp_rax(locals["$push_oldlist"]);
                
                asm_.mov_rax_mem_rax();
                asm_.mov_mem_rbp_rax(locals["$push_oldsize"]);
                
                asm_.add_rax_imm32(2);
                asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
                asm_.code.push_back(0xE0); asm_.code.push_back(0x03);
                asm_.push_rax();
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
                asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
                asm_.mov_rcx_rax();
                asm_.xor_rax_rax();
                asm_.mov_rdx_rax();
                asm_.code.push_back(0x41); asm_.code.push_back(0x58);
                asm_.call_mem_rip(pe_.getImportRVA("HeapAlloc"));
                if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
                
                asm_.mov_mem_rbp_rax(locals["$push_newlist"]);
                
                asm_.mov_rcx_mem_rbp(locals["$push_oldsize"]);
                asm_.inc_rcx();
                asm_.mov_mem_rax_rcx();
                
                allocLocal("$push_idx");
                asm_.xor_rax_rax();
                asm_.mov_mem_rbp_rax(locals["$push_idx"]);
                
                std::string copyLoop = newLabel("push_copy");
                std::string copyDone = newLabel("push_done");
                
                asm_.label(copyLoop);
                asm_.mov_rax_mem_rbp(locals["$push_idx"]);
                asm_.cmp_rax_mem_rbp(locals["$push_oldsize"]);
                asm_.jge_rel32(copyDone);
                
                asm_.mov_rcx_mem_rbp(locals["$push_oldlist"]);
                asm_.mov_rax_mem_rbp(locals["$push_idx"]);
                asm_.inc_rax();
                asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
                asm_.code.push_back(0xE0); asm_.code.push_back(0x03);
                asm_.add_rax_rcx();
                asm_.mov_rax_mem_rax();
                asm_.push_rax();
                
                asm_.mov_rcx_mem_rbp(locals["$push_newlist"]);
                asm_.mov_rax_mem_rbp(locals["$push_idx"]);
                asm_.inc_rax();
                asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
                asm_.code.push_back(0xE0); asm_.code.push_back(0x03);
                asm_.add_rax_rcx();
                asm_.pop_rcx();
                asm_.mov_mem_rax_rcx();
                
                asm_.mov_rax_mem_rbp(locals["$push_idx"]);
                asm_.inc_rax();
                asm_.mov_mem_rbp_rax(locals["$push_idx"]);
                asm_.jmp_rel32(copyLoop);
                
                asm_.label(copyDone);
                
                asm_.mov_rcx_mem_rbp(locals["$push_newlist"]);
                asm_.mov_rax_mem_rbp(locals["$push_oldsize"]);
                asm_.inc_rax();
                asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
                asm_.code.push_back(0xE0); asm_.code.push_back(0x03);
                asm_.add_rax_rcx();
                asm_.mov_rcx_mem_rbp(locals["$push_element"]);
                asm_.mov_mem_rax_rcx();
                
                asm_.mov_rax_mem_rbp(locals["$push_newlist"]);
            }
            return;
        }
        
        // Handle pop()
        if (id->name == "pop" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            
            std::string listName;
            size_t listSize = 0;
            bool knownSize = false;
            if (auto* ident = dynamic_cast<Identifier*>(node.args[0].get())) {
                listName = ident->name;
                auto sizeIt = listSizes.find(listName);
                if (sizeIt != listSizes.end()) {
                    listSize = sizeIt->second;
                    knownSize = true;
                }
            }
            
            if (knownSize && listSize > 0) {
                asm_.add_rax_imm32((int32_t)((listSize - 1) * 8));
                asm_.mov_rax_mem_rax();
                
                if (!listName.empty()) {
                    listSizes[listName] = listSize - 1;
                }
            } else {
                allocLocal("$pop_list");
                asm_.mov_mem_rbp_rax(locals["$pop_list"]);
                
                asm_.mov_rcx_mem_rax();
                
                asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
                asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
                asm_.add_rax_rcx();
                asm_.mov_rax_mem_rax();
            }
            return;
        }
        
        // Handle range()
        if (id->name == "range") {
            asm_.xor_rax_rax();
            return;
        }
        
        // Handle platform()
        if (id->name == "platform") {
            uint32_t rva = addString("windows");
            asm_.lea_rax_rip_fixup(rva);
            return;
        }
        
        // Handle arch()
        if (id->name == "arch") {
            uint32_t rva = addString("x64");
            asm_.lea_rax_rip_fixup(rva);
            return;
        }
        
        // Handle str()
        if (id->name == "str" && node.args.size() == 1) {
            std::string strVal;
            if (tryEvalConstantString(node.args[0].get(), strVal)) {
                uint32_t rva = addString(strVal);
                asm_.lea_rax_rip_fixup(rva);
                return;
            }
            
            node.args[0]->accept(*this);
            emitItoa();
            return;
        }
        
        // Handle int() - convert string/float/bool to int
        if (id->name == "int" && node.args.size() == 1) {
            // Try constant evaluation first
            int64_t intVal;
            if (tryEvalConstant(node.args[0].get(), intVal)) {
                asm_.mov_rax_imm64(intVal);
                return;
            }
            
            // Check if it's a string literal - parse at compile time
            if (auto* strLit = dynamic_cast<StringLiteral*>(node.args[0].get())) {
                int64_t result = 0;
                bool negative = false;
                size_t i = 0;
                const std::string& s = strLit->value;
                
                // Skip whitespace
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
                
                // Check for sign
                if (i < s.size() && s[i] == '-') { negative = true; i++; }
                else if (i < s.size() && s[i] == '+') { i++; }
                
                // Parse digits
                while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                    result = result * 10 + (s[i] - '0');
                    i++;
                }
                
                if (negative) result = -result;
                asm_.mov_rax_imm64(result);
                return;
            }
            
            // Check if it's a float expression - convert float to int
            if (isFloatExpression(node.args[0].get())) {
                node.args[0]->accept(*this);
                // Convert float in xmm0 to int in rax
                // cvttsd2si rax, xmm0
                asm_.code.push_back(0xF2); asm_.code.push_back(0x48);
                asm_.code.push_back(0x0F); asm_.code.push_back(0x2C);
                asm_.code.push_back(0xC0);
                return;
            }
            
            // Check if it's a known string variable
            if (auto* ident = dynamic_cast<Identifier*>(node.args[0].get())) {
                auto strIt = constStrVars.find(ident->name);
                if (strIt != constStrVars.end()) {
                    // Parse constant string at compile time
                    int64_t result = 0;
                    bool negative = false;
                    size_t i = 0;
                    const std::string& s = strIt->second;
                    
                    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
                    if (i < s.size() && s[i] == '-') { negative = true; i++; }
                    else if (i < s.size() && s[i] == '+') { i++; }
                    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                        result = result * 10 + (s[i] - '0');
                        i++;
                    }
                    if (negative) result = -result;
                    asm_.mov_rax_imm64(result);
                    return;
                }
            }
            
            // Runtime string to int conversion (atoi)
            node.args[0]->accept(*this);
            
            // Inline atoi implementation
            // Input: rax = string pointer
            // Output: rax = integer value
            asm_.mov_rcx_rax();  // rcx = string pointer
            asm_.xor_rax_rax();  // rax = result = 0
            asm_.code.push_back(0x45); asm_.code.push_back(0x31); asm_.code.push_back(0xC9);  // xor r9d, r9d (negative flag)
            
            // Skip leading whitespace
            std::string skipWsLoop = newLabel("atoi_skipws");
            std::string skipWsDone = newLabel("atoi_skipws_done");
            std::string skipWsInc = newLabel("atoi_skipws_inc");
            asm_.label(skipWsLoop);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x11);  // movzx edx, byte [rcx]
            asm_.code.push_back(0x80); asm_.code.push_back(0xFA); asm_.code.push_back(' ');  // cmp dl, ' '
            asm_.jz_rel32(skipWsInc);
            asm_.code.push_back(0x80); asm_.code.push_back(0xFA); asm_.code.push_back('\t');  // cmp dl, '\t'
            asm_.jz_rel32(skipWsInc);
            asm_.jmp_rel32(skipWsDone);
            asm_.label(skipWsInc);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);  // inc rcx
            asm_.jmp_rel32(skipWsLoop);
            asm_.label(skipWsDone);
            
            // Check for sign
            std::string checkDigits = newLabel("atoi_digits");
            std::string checkPlus = newLabel("atoi_checkplus");
            std::string afterSign = newLabel("atoi_aftersign");
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x11);  // movzx edx, byte [rcx]
            asm_.code.push_back(0x80); asm_.code.push_back(0xFA); asm_.code.push_back('-');  // cmp dl, '-'
            asm_.jnz_rel32(checkPlus);
            asm_.code.push_back(0x41); asm_.code.push_back(0xB9);  // mov r9d, 1
            asm_.code.push_back(0x01); asm_.code.push_back(0x00); asm_.code.push_back(0x00); asm_.code.push_back(0x00);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);  // inc rcx
            asm_.jmp_rel32(checkDigits);
            asm_.label(checkPlus);
            asm_.code.push_back(0x80); asm_.code.push_back(0xFA); asm_.code.push_back('+');  // cmp dl, '+'
            asm_.jnz_rel32(checkDigits);
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);  // inc rcx
            
            // Parse digits
            asm_.label(checkDigits);
            std::string digitLoop = newLabel("atoi_loop");
            std::string digitDone = newLabel("atoi_done");
            std::string applyNeg = newLabel("atoi_neg");
            std::string skipNeg = newLabel("atoi_skipneg");
            
            asm_.label(digitLoop);
            asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x11);  // movzx edx, byte [rcx]
            asm_.code.push_back(0x80); asm_.code.push_back(0xFA); asm_.code.push_back('0');  // cmp dl, '0'
            asm_.jl_rel32(digitDone);
            asm_.code.push_back(0x80); asm_.code.push_back(0xFA); asm_.code.push_back('9');  // cmp dl, '9'
            asm_.jg_rel32(digitDone);
            
            // result = result * 10 + (digit - '0')
            asm_.code.push_back(0x48); asm_.code.push_back(0x6B); asm_.code.push_back(0xC0); asm_.code.push_back(0x0A);  // imul rax, rax, 10
            asm_.code.push_back(0x80); asm_.code.push_back(0xEA); asm_.code.push_back('0');  // sub dl, '0'
            asm_.code.push_back(0x48); asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0xD2);  // movzx rdx, dl
            asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xD0);  // add rax, rdx
            asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);  // inc rcx
            asm_.jmp_rel32(digitLoop);
            
            asm_.label(digitDone);
            // Apply negative sign if needed
            asm_.code.push_back(0x45); asm_.code.push_back(0x85); asm_.code.push_back(0xC9);  // test r9d, r9d
            asm_.jz_rel32(skipNeg);
            asm_.neg_rax();
            asm_.label(skipNeg);
            return;
        }
        
        // Handle float() - convert string/int/bool to float
        if (id->name == "float" && node.args.size() == 1) {
            // Try constant float evaluation first
            double floatVal;
            if (tryEvalConstantFloat(node.args[0].get(), floatVal)) {
                uint32_t rva = addFloatConstant(floatVal);
                asm_.movsd_xmm0_mem_rip(rva);
                // Move to rax for consistency (float values stored in rax as bits)
                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                asm_.code.push_back(0x0F); asm_.code.push_back(0x7E); asm_.code.push_back(0xC0);  // movq rax, xmm0
                lastExprWasFloat_ = true;
                return;
            }
            
            // Try constant int evaluation - convert to float
            int64_t intVal;
            if (tryEvalConstant(node.args[0].get(), intVal)) {
                double result = (double)intVal;
                uint32_t rva = addFloatConstant(result);
                asm_.movsd_xmm0_mem_rip(rva);
                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                asm_.code.push_back(0x0F); asm_.code.push_back(0x7E); asm_.code.push_back(0xC0);
                lastExprWasFloat_ = true;
                return;
            }
            
            // Check if it's a string literal - parse at compile time
            if (auto* strLit = dynamic_cast<StringLiteral*>(node.args[0].get())) {
                double result = std::stod(strLit->value);
                uint32_t rva = addFloatConstant(result);
                asm_.movsd_xmm0_mem_rip(rva);
                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                asm_.code.push_back(0x0F); asm_.code.push_back(0x7E); asm_.code.push_back(0xC0);
                lastExprWasFloat_ = true;
                return;
            }
            
            // Check if it's already a float expression
            if (isFloatExpression(node.args[0].get())) {
                node.args[0]->accept(*this);
                lastExprWasFloat_ = true;
                return;
            }
            
            // Runtime: evaluate expression and convert int to float
            node.args[0]->accept(*this);
            // cvtsi2sd xmm0, rax
            asm_.code.push_back(0xF2); asm_.code.push_back(0x48);
            asm_.code.push_back(0x0F); asm_.code.push_back(0x2A); asm_.code.push_back(0xC0);
            // movq rax, xmm0
            asm_.code.push_back(0x66); asm_.code.push_back(0x48);
            asm_.code.push_back(0x0F); asm_.code.push_back(0x7E); asm_.code.push_back(0xC0);
            lastExprWasFloat_ = true;
            return;
        }
        
        // Handle bool() - convert int/string to bool
        if (id->name == "bool" && node.args.size() == 1) {
            // Try constant evaluation first
            int64_t intVal;
            if (tryEvalConstant(node.args[0].get(), intVal)) {
                asm_.mov_rax_imm64(intVal != 0 ? 1 : 0);
                return;
            }
            
            // Check if it's a string literal
            if (auto* strLit = dynamic_cast<StringLiteral*>(node.args[0].get())) {
                // "true" or non-empty string (except "0" and "false") is true
                const std::string& s = strLit->value;
                bool result = !s.empty() && s != "0" && s != "false" && s != "False" && s != "FALSE";
                asm_.mov_rax_imm64(result ? 1 : 0);
                return;
            }
            
            // Check if it's a bool literal
            if (auto* boolLit = dynamic_cast<BoolLiteral*>(node.args[0].get())) {
                asm_.mov_rax_imm64(boolLit->value ? 1 : 0);
                return;
            }
            
            // Runtime: evaluate and convert to bool (non-zero = true)
            node.args[0]->accept(*this);
            
            // Check if it's a string pointer - need to check if non-empty
            if (isStringReturningExpr(node.args[0].get())) {
                // String: check if first char is non-zero (non-empty string)
                asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x00);  // movzx eax, byte [rax]
                asm_.test_rax_rax();
                asm_.code.push_back(0x0F); asm_.code.push_back(0x95); asm_.code.push_back(0xC0);  // setne al
                asm_.code.push_back(0x48); asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0xC0);  // movzx rax, al
            } else {
                // Integer/other: non-zero is true
                asm_.test_rax_rax();
                asm_.code.push_back(0x0F); asm_.code.push_back(0x95); asm_.code.push_back(0xC0);  // setne al
                asm_.code.push_back(0x48); asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0xC0);  // movzx rax, al
            }
            return;
        }
        
        // Handle print/println
        if (id->name == "print" || id->name == "println") {
            for (auto& arg : node.args) {
                emitPrintExpr(arg.get());
            }
            
            uint32_t nlRVA = addString("\r\n");
            emitWriteConsole(nlRVA, 2);
            
            asm_.xor_rax_rax();
            return;
        }
        
        // Handle Ok/Err/is_ok/is_err/unwrap/unwrap_or - Result type
        if (id->name == "Ok" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.code.push_back(0x48); asm_.code.push_back(0xD1); asm_.code.push_back(0xE0);
            asm_.code.push_back(0x48); asm_.code.push_back(0x83); asm_.code.push_back(0xC8); asm_.code.push_back(0x01);
            return;
        }
        
        if (id->name == "Err" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.code.push_back(0x48); asm_.code.push_back(0xD1); asm_.code.push_back(0xE0);
            return;
        }
        
        if (id->name == "is_ok" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.code.push_back(0x48); asm_.code.push_back(0x83); asm_.code.push_back(0xE0); asm_.code.push_back(0x01);
            return;
        }
        
        if (id->name == "is_err" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.code.push_back(0x48); asm_.code.push_back(0x83); asm_.code.push_back(0xE0); asm_.code.push_back(0x01);
            asm_.code.push_back(0x48); asm_.code.push_back(0x83); asm_.code.push_back(0xF0); asm_.code.push_back(0x01);
            return;
        }
        
        if (id->name == "unwrap" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.code.push_back(0x48); asm_.code.push_back(0xD1); asm_.code.push_back(0xE8);
            return;
        }
        
        if (id->name == "unwrap_or" && node.args.size() == 2) {
            node.args[0]->accept(*this);
            asm_.push_rax();
            
            asm_.code.push_back(0x48); asm_.code.push_back(0x83); asm_.code.push_back(0xE0); asm_.code.push_back(0x01);
            
            std::string okLabel = newLabel("unwrap_ok");
            std::string endLabel = newLabel("unwrap_end");
            
            asm_.test_rax_rax();
            asm_.jnz_rel32(okLabel);
            
            asm_.pop_rax();
            node.args[1]->accept(*this);
            asm_.jmp_rel32(endLabel);
            
            asm_.label(okLabel);
            asm_.pop_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0xD1); asm_.code.push_back(0xE8);
            
            asm_.label(endLabel);
            return;
        }
        
        // Continue to system calls and general function calls in codegen_call_system.cpp
        // For now, include them inline here
        
        // Handle hostname()
        if (id->name == "hostname") {
            allocLocal("$hostname_buf");
            int32_t bufOffset = locals["$hostname_buf"];
            for (int i = 0; i < 31; i++) allocLocal("$hostname_pad" + std::to_string(i));
            
            allocLocal("$hostname_size");
            int32_t sizeOffset = locals["$hostname_size"];
            
            asm_.mov_rax_imm64(256);
            asm_.mov_mem_rbp_rax(sizeOffset);
            
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.lea_rax_rbp(bufOffset);
            asm_.mov_rcx_rax();
            asm_.lea_rax_rbp(sizeOffset);
            asm_.mov_rdx_rax();
            asm_.call_mem_rip(pe_.getImportRVA("GetComputerNameA"));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            
            asm_.lea_rax_rbp(bufOffset);
            return;
        }
        
        // Handle sleep()
        if (id->name == "sleep" && node.args.size() >= 1) {
            node.args[0]->accept(*this);
            asm_.mov_rcx_rax();
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.call_mem_rip(pe_.getImportRVA("Sleep"));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            asm_.xor_rax_rax();
            return;
        }
        
        // Handle now()
        if (id->name == "now") {
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.call_mem_rip(pe_.getImportRVA("GetTickCount64"));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            asm_.mov_rcx_imm64(1000);
            asm_.cqo();
            asm_.idiv_rcx();
            return;
        }
        
        // Handle now_ms()
        if (id->name == "now_ms") {
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.call_mem_rip(pe_.getImportRVA("GetTickCount64"));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            return;
        }
        
        // Handle GC functions
        if (id->name == "gc_collect" && node.args.empty()) {
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.call_rel32(gcCollectLabel_);
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            asm_.xor_rax_rax();
            return;
        }
        
        if (id->name == "gc_stats" && node.args.empty()) {
            asm_.lea_rax_rip_fixup(gcDataRVA_ + 8);
            asm_.mov_rax_mem_rax();
            return;
        }
        
        // gc_count() - Get number of GC collections performed
        if (id->name == "gc_count" && node.args.empty()) {
            asm_.lea_rax_rip_fixup(gcDataRVA_ + 32);  // gc_collections offset
            asm_.mov_rax_mem_rax();
            return;
        }
        
        // gc_pin(ptr) - Pin GC object to prevent collection
        // Sets the pinned flag (bit 0) in the object header flags field
        if (id->name == "gc_pin" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            // rax = pointer to user data
            // Header is at rax - 16, flags field is at offset 7 from header start
            // So flags is at rax - 9
            asm_.code.push_back(0x80);  // or byte [rax-9], 1
            asm_.code.push_back(0x48);
            asm_.code.push_back(0xF7);  // -9 as signed byte
            asm_.code.push_back(0x01);  // OR with 1 (GC_FLAG_PINNED)
            asm_.xor_rax_rax();
            return;
        }
        
        // gc_unpin(ptr) - Unpin GC object
        // Clears the pinned flag (bit 0) in the object header flags field
        if (id->name == "gc_unpin" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            // rax = pointer to user data
            // Clear bit 0 of flags at rax - 9
            asm_.code.push_back(0x80);  // and byte [rax-9], 0xFE
            asm_.code.push_back(0x60);
            asm_.code.push_back(0xF7);  // -9 as signed byte
            asm_.code.push_back(0xFE);  // AND with ~1
            asm_.xor_rax_rax();
            return;
        }
        
        // gc_add_root(ptr) - Register external pointer as GC root
        // For now, this is a no-op since our conservative stack scanning
        // will find pointers on the stack. In a more sophisticated GC,
        // this would add the pointer to a root set.
        if (id->name == "gc_add_root" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            // Store the pointer in a global root list (future enhancement)
            // For now, just return success
            asm_.xor_rax_rax();
            return;
        }
        
        // gc_remove_root(ptr) - Unregister external pointer as GC root
        if (id->name == "gc_remove_root" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            // Remove from global root list (future enhancement)
            // For now, just return success
            asm_.xor_rax_rax();
            return;
        }
        
        // set_allocator(alloc_fn, free_fn) - Set custom allocator functions
        // alloc_fn: function pointer (size, alignment) -> ptr
        // free_fn: function pointer (ptr, size) -> void
        if (id->name == "set_allocator" && node.args.size() == 2) {
            // Evaluate alloc function pointer
            node.args[0]->accept(*this);
            asm_.push_rax();  // Save alloc_fn
            
            // Evaluate free function pointer
            node.args[1]->accept(*this);
            asm_.mov_rdx_rax();  // rdx = free_fn
            
            asm_.pop_rcx();  // rcx = alloc_fn
            
            // Store in GC data section
            // gcDataRVA_ + 48: custom_alloc_fn (8 bytes)
            // gcDataRVA_ + 56: custom_free_fn (8 bytes)
            asm_.lea_rax_rip_fixup(gcDataRVA_ + 48);
            asm_.mov_mem_rax_rcx();  // Store alloc_fn
            
            asm_.lea_rax_rip_fixup(gcDataRVA_ + 56);
            asm_.code.push_back(0x48); asm_.code.push_back(0x89); asm_.code.push_back(0x10);  // mov [rax], rdx
            
            asm_.xor_rax_rax();
            return;
        }
        
        // reset_allocator() - Reset to default system allocator
        if (id->name == "reset_allocator" && node.args.size() == 0) {
            // Clear custom allocator pointers
            asm_.xor_rax_rax();
            asm_.lea_rcx_rip_fixup(gcDataRVA_ + 48);
            asm_.mov_mem_rcx_rax();  // Clear alloc_fn
            
            asm_.lea_rcx_rip_fixup(gcDataRVA_ + 56);
            asm_.mov_mem_rcx_rax();  // Clear free_fn
            
            return;
        }
        
        // allocator_stats() - Get total bytes allocated
        if (id->name == "allocator_stats" && node.args.size() == 0) {
            // Return gc_total_bytes from GC data section
            asm_.lea_rax_rip_fixup(gcDataRVA_ + 8);
            asm_.mov_rax_mem_rax();
            return;
        }
        
        // allocator_peak() - Get peak memory usage
        // For now, return same as total (would need additional tracking)
        if (id->name == "allocator_peak" && node.args.size() == 0) {
            asm_.lea_rax_rip_fixup(gcDataRVA_ + 8);
            asm_.mov_rax_mem_rax();
            return;
        }
        
        // Handle alloc/free
        if (id->name == "alloc" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.mov_r8_rax();
            
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
            asm_.mov_rcx_rax();
            asm_.mov_rdx_imm64(0x08);
            asm_.call_mem_rip(pe_.getImportRVA("HeapAlloc"));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            return;
        }
        
        if (id->name == "free" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            asm_.mov_r8_rax();
            
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
            asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
            asm_.mov_rcx_rax();
            asm_.xor_rax_rax();
            asm_.mov_rdx_rax();
            asm_.call_mem_rip(pe_.getImportRVA("HeapFree"));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
            
            asm_.xor_rax_rax();
            return;
        }
        
        // stackalloc(size) - Allocate memory on the stack
        // Returns pointer to stack-allocated buffer
        // WARNING: This memory is only valid within the current function scope
        if (id->name == "stackalloc" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            // rax = size to allocate
            // Align size to 16 bytes for stack alignment
            asm_.add_rax_imm32(15);
            asm_.code.push_back(0x48);  // and rax, ~15
            asm_.code.push_back(0x83);
            asm_.code.push_back(0xE0);
            asm_.code.push_back(0xF0);
            // sub rsp, rax
            asm_.code.push_back(0x48);
            asm_.code.push_back(0x29);
            asm_.code.push_back(0xC4);  // sub rsp, rax
            // mov rax, rsp (return the stack pointer)
            asm_.code.push_back(0x48);
            asm_.code.push_back(0x89);
            asm_.code.push_back(0xE0);  // mov rax, rsp
            return;
        }
        
        // sizeof(T) - Get byte size of type
        // Returns the size in bytes of the given type
        if (id->name == "sizeof" && node.args.size() == 1) {
            // sizeof takes a type name as argument (passed as identifier)
            if (auto* typeIdent = dynamic_cast<Identifier*>(node.args[0].get())) {
                std::string typeName = typeIdent->name;
                int64_t size = 8;  // Default size for unknown types (pointer size)
                
                // Primitive types
                if (typeName == "int" || typeName == "i64" || typeName == "u64" || 
                    typeName == "float" || typeName == "f64") {
                    size = 8;
                } else if (typeName == "i32" || typeName == "u32" || typeName == "f32") {
                    size = 4;
                } else if (typeName == "i16" || typeName == "u16") {
                    size = 2;
                } else if (typeName == "i8" || typeName == "u8" || typeName == "bool") {
                    size = 1;
                } else if (typeName == "void") {
                    size = 0;
                } else if (typeName == "str" || typeName == "string") {
                    size = 8;  // String is a pointer
                } else {
                    // Check if it's a registered record type
                    auto& reg = TypeRegistry::instance();
                    TypePtr type = reg.lookupType(typeName);
                    if (type) {
                        if (auto* recType = dynamic_cast<RecordType*>(type.get())) {
                            // Calculate record size: sum of field sizes (8-byte aligned)
                            size = recType->fields.size() * 8;
                        } else {
                            size = type->size();
                        }
                    }
                }
                
                asm_.mov_rax_imm64(size);
                return;
            }
            // If not an identifier, evaluate the expression and return pointer size
            asm_.mov_rax_imm64(8);
            return;
        }
        
        // alignof(T) - Get alignment requirement of type
        // Returns the alignment in bytes of the given type
        if (id->name == "alignof" && node.args.size() == 1) {
            if (auto* typeIdent = dynamic_cast<Identifier*>(node.args[0].get())) {
                std::string typeName = typeIdent->name;
                int64_t alignment = 8;  // Default alignment
                
                // Primitive types - alignment equals size for most types
                if (typeName == "int" || typeName == "i64" || typeName == "u64" || 
                    typeName == "float" || typeName == "f64") {
                    alignment = 8;
                } else if (typeName == "i32" || typeName == "u32" || typeName == "f32") {
                    alignment = 4;
                } else if (typeName == "i16" || typeName == "u16") {
                    alignment = 2;
                } else if (typeName == "i8" || typeName == "u8" || typeName == "bool") {
                    alignment = 1;
                } else if (typeName == "void") {
                    alignment = 1;  // void has alignment 1 by convention
                } else if (typeName == "str" || typeName == "string") {
                    alignment = 8;  // Pointer alignment
                } else {
                    // Check if it's a registered record type
                    auto& reg = TypeRegistry::instance();
                    TypePtr type = reg.lookupType(typeName);
                    if (type) {
                        alignment = type->alignment();
                    }
                }
                
                asm_.mov_rax_imm64(alignment);
                return;
            }
            // Default to pointer alignment
            asm_.mov_rax_imm64(8);
            return;
        }
        
        // offsetof(Record, field) - Get byte offset of field in record
        // Returns the byte offset of the specified field within the record type
        if (id->name == "offsetof" && node.args.size() == 2) {
            // First arg is record type name, second is field name
            if (auto* recordIdent = dynamic_cast<Identifier*>(node.args[0].get())) {
                if (auto* fieldIdent = dynamic_cast<Identifier*>(node.args[1].get())) {
                    std::string recordName = recordIdent->name;
                    std::string fieldName = fieldIdent->name;
                    int64_t offset = 0;
                    
                    // Look up the record type
                    auto& reg = TypeRegistry::instance();
                    TypePtr type = reg.lookupType(recordName);
                    if (type) {
                        if (auto* recType = dynamic_cast<RecordType*>(type.get())) {
                            // Find the field and calculate offset
                            // Fields are stored at 8-byte intervals
                            for (size_t i = 0; i < recType->fields.size(); i++) {
                                if (recType->fields[i].name == fieldName) {
                                    offset = i * 8;  // Each field is 8 bytes (64-bit aligned)
                                    break;
                                }
                            }
                        }
                    }
                    
                    asm_.mov_rax_imm64(offset);
                    return;
                }
            }
            // If we can't determine the offset, return 0
            asm_.mov_rax_imm64(0);
            return;
        }
        
        // placement_new(ptr, value) - Construct value at specific address
        // Writes the value to the given memory address and returns the pointer
        if (id->name == "placement_new" && node.args.size() == 2) {
            // Evaluate the pointer first
            node.args[0]->accept(*this);
            asm_.push_rax();  // Save pointer
            
            // Evaluate the value
            node.args[1]->accept(*this);
            asm_.mov_rcx_rax();  // rcx = value
            
            // Pop pointer into rax
            asm_.pop_rax();
            
            // Store value at pointer: mov [rax], rcx
            asm_.code.push_back(0x48);
            asm_.code.push_back(0x89);
            asm_.code.push_back(0x08);  // mov [rax], rcx
            
            // Return the pointer (already in rax)
            return;
        }
        
        // Check if this is a generic function call
        std::string callTarget = id->name;
        bool callReturnsFloat = false;
        bool callReturnsString = false;
        auto genericIt = genericFunctions_.find(id->name);
        if (genericIt != genericFunctions_.end()) {
            // This is a call to a generic function - we need to use the mangled name
            // Infer type arguments from the call arguments
            FnDecl* genericFn = genericIt->second;
            std::vector<TypePtr> typeArgs;
            auto& reg = TypeRegistry::instance();
            
            // Simple type inference from arguments
            std::unordered_map<std::string, TypePtr> inferred;
            for (size_t i = 0; i < node.args.size() && i < genericFn->params.size(); i++) {
                const std::string& paramType = genericFn->params[i].second;
                
                // Check if param type is a type parameter
                for (const auto& tp : genericFn->typeParams) {
                    if (paramType == tp) {
                        TypePtr argType = reg.anyType();
                        
                        // Infer type from argument
                        if (dynamic_cast<IntegerLiteral*>(node.args[i].get())) {
                            argType = reg.intType();
                        } else if (dynamic_cast<FloatLiteral*>(node.args[i].get())) {
                            argType = reg.floatType();
                        } else if (dynamic_cast<StringLiteral*>(node.args[i].get())) {
                            argType = reg.stringType();
                        } else if (dynamic_cast<BoolLiteral*>(node.args[i].get())) {
                            argType = reg.boolType();
                        } else if (auto* ident = dynamic_cast<Identifier*>(node.args[i].get())) {
                            // Check if we know the variable's type
                            if (floatVars.count(ident->name)) {
                                argType = reg.floatType();
                            } else if (constFloatVars.count(ident->name)) {
                                argType = reg.floatType();
                            } else if (constVars.count(ident->name)) {
                                argType = reg.intType();
                            } else if (constStrVars.count(ident->name)) {
                                argType = reg.stringType();
                            }
                        } else if (isFloatExpression(node.args[i].get())) {
                            argType = reg.floatType();
                        } else if (isStringReturningExpr(node.args[i].get())) {
                            argType = reg.stringType();
                        }
                        
                        if (inferred.find(tp) == inferred.end()) {
                            inferred[tp] = argType;
                        }
                        break;
                    }
                }
            }
            
            // Build type args in order
            for (const auto& tp : genericFn->typeParams) {
                auto it = inferred.find(tp);
                if (it != inferred.end()) {
                    typeArgs.push_back(it->second);
                } else {
                    typeArgs.push_back(reg.anyType());
                }
            }
            
            // Get the mangled name
            callTarget = monomorphizer_.getMangledName(id->name, typeArgs);
            
            // Make sure the instantiation is recorded
            if (!monomorphizer_.hasInstantiation(id->name, typeArgs)) {
                monomorphizer_.recordFunctionInstantiation(id->name, typeArgs, genericFn);
            }
            
            // Check if this specialized function returns float or string
            callReturnsFloat = monomorphizer_.functionReturnsFloat(callTarget);
            callReturnsString = monomorphizer_.functionReturnsString(callTarget);
            
            // Fallback: if monomorphizer doesn't know, check if any arg is a float literal
            // and the return type is a type parameter
            if (!callReturnsFloat && !typeArgs.empty()) {
                for (const auto& arg : typeArgs) {
                    if (arg->toString() == "float") {
                        // Check if return type is a type parameter that maps to float
                        std::string returnType = genericFn->returnType;
                        for (size_t i = 0; i < genericFn->typeParams.size() && i < typeArgs.size(); i++) {
                            if (returnType == genericFn->typeParams[i] && typeArgs[i]->toString() == "float") {
                                callReturnsFloat = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        // Direct function call
        if (asm_.labels.count(callTarget) || asm_.labels.count(id->name)) {
            // For float-returning generic functions, we need special handling
            if (callReturnsFloat) {
                // Evaluate arguments in reverse order and push to stack
                // This preserves registers across argument evaluations
                for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                    // Check if this arg should be treated as float
                    if (isFloatExpression(node.args[i].get())) {
                        node.args[i]->accept(*this);
                        if (!lastExprWasFloat_) {
                            // Should be float but isn't marked? Force simple conversion if needed
                            // But usually accept() handles it.
                        }
                        // Move float bits to RAX for pushing
                        asm_.movq_rax_xmm0();
                        asm_.push_rax();
                    } else {
                        node.args[i]->accept(*this);
                        asm_.push_rax();
                    }
                }
                
                // Pop arguments into correct registers (XMM or GP)
                // Pop in reverse order to get args in correct registers
                // Stack has: [arg0, arg1, arg2, arg3] (top to bottom)
                // We need: arg0->XMM0/RCX, arg1->XMM1/RDX, etc.
                for (size_t i = 0; i < node.args.size() && i < 4; i++) {
                    bool isFloat = isFloatExpression(node.args[i].get());
                    
                    if (isFloat) {
                        // Pop into RAX first
                        asm_.pop_rax();
                        
                        // Move directly to target XMM register (not through XMM0)
                        switch (i) {
                            case 0:
                                // movq xmm0, rax
                                asm_.movq_xmm0_rax();
                                break;
                            case 1:
                                // movq xmm1, rax
                                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                                asm_.code.push_back(0x0F); asm_.code.push_back(0x6E); asm_.code.push_back(0xC8);
                                break;
                            case 2:
                                // movq xmm2, rax
                                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                                asm_.code.push_back(0x0F); asm_.code.push_back(0x6E); asm_.code.push_back(0xD0);
                                break;
                            case 3:
                                // movq xmm3, rax
                                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                                asm_.code.push_back(0x0F); asm_.code.push_back(0x6E); asm_.code.push_back(0xD8);
                                break;
                        }
                    } else {
                        // Pop into GP register
                        switch (i) {
                            case 0: asm_.pop_rcx(); break;
                            case 1: asm_.pop_rdx(); break;
                            case 2: asm_.code.push_back(0x41); asm_.code.push_back(0x58); break; // pop r8
                            case 3: asm_.code.push_back(0x41); asm_.code.push_back(0x59); break; // pop r9
                        }
                    }
                }
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                asm_.call_rel32(callTarget);
                if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                
                // Result is in xmm0 for float-returning functions
                // Move to rax as bit pattern for storage, but mark as float
                asm_.code.push_back(0x66); asm_.code.push_back(0x48);
                asm_.code.push_back(0x0F); asm_.code.push_back(0x7E); asm_.code.push_back(0xC0);  // movq rax, xmm0
                lastExprWasFloat_ = true;
                return;
            }
            
            // Standard integer/pointer call
            for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                node.args[i]->accept(*this);
                asm_.push_rax();
            }
            
            if (node.args.size() >= 1) asm_.pop_rcx();
            if (node.args.size() >= 2) asm_.pop_rdx();
            if (node.args.size() >= 3) {
                asm_.code.push_back(0x41); asm_.code.push_back(0x58);
            }
            if (node.args.size() >= 4) {
                asm_.code.push_back(0x41); asm_.code.push_back(0x59);
            }
            
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
            asm_.call_rel32(callTarget);
            if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
            
            // Mark if this returns a string
            if (callReturnsString) {
                // Result is a string pointer in rax
            }
            return;
        }
    }
    
    // Indirect call through closure
    node.callee->accept(*this);
    asm_.push_rax();
    
    for (int i = (int)node.args.size() - 1; i >= 0; i--) {
        node.args[i]->accept(*this);
        asm_.push_rax();
    }
    
    if (node.args.size() >= 1) asm_.pop_rdx();
    if (node.args.size() >= 2) {
        asm_.code.push_back(0x41); asm_.code.push_back(0x58);
    }
    if (node.args.size() >= 3) {
        asm_.code.push_back(0x41); asm_.code.push_back(0x59);
    }
    for (size_t i = 3; i < node.args.size(); i++) {
        asm_.pop_rax();
    }
    
    asm_.pop_rcx();
    
    asm_.mov_rax_mem_rcx();
    
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
    asm_.call_rax();
    if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
}

} // namespace flex
