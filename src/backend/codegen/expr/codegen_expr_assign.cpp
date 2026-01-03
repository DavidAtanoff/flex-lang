// Flex Compiler - Native Code Generator Assignment Expressions
// Handles: AssignExpr

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(AssignExpr& node) {
    // Check if this is a float expression BEFORE evaluating
    bool isFloat = isFloatExpression(node.value.get());
    
    // Additional check for generic function calls with float arguments
    if (!isFloat) {
        if (auto* call = dynamic_cast<CallExpr*>(node.value.get())) {
            if (auto* callId = dynamic_cast<Identifier*>(call->callee.get())) {
                auto it = genericFunctions_.find(callId->name);
                if (it != genericFunctions_.end()) {
                    for (auto& arg : call->args) {
                        if (dynamic_cast<FloatLiteral*>(arg.get())) {
                            isFloat = true;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    // Handle pointer dereference assignment: *ptr = value
    if (auto* deref = dynamic_cast<DerefExpr*>(node.target.get())) {
        node.value->accept(*this);
        asm_.push_rax();
        deref->operand->accept(*this);
        asm_.mov_rcx_rax();
        asm_.pop_rax();
        asm_.mov_mem_rcx_rax();
        return;
    }
    
    // Handle record field assignment: p.x = value
    if (auto* member = dynamic_cast<MemberExpr*>(node.target.get())) {
        if (auto* objId = dynamic_cast<Identifier*>(member->object.get())) {
            auto varTypeIt = varRecordTypes_.find(objId->name);
            if (varTypeIt != varRecordTypes_.end()) {
                auto typeIt = recordTypes_.find(varTypeIt->second);
                if (typeIt != recordTypes_.end()) {
                    int fieldIndex = -1;
                    for (size_t i = 0; i < typeIt->second.fieldNames.size(); i++) {
                        if (typeIt->second.fieldNames[i] == member->member) {
                            fieldIndex = static_cast<int>(i);
                            break;
                        }
                    }
                    
                    if (fieldIndex >= 0) {
                        bool isBitfield = false;
                        int bitWidth = 0;
                        if (fieldIndex < static_cast<int>(typeIt->second.fieldBitWidths.size())) {
                            bitWidth = typeIt->second.fieldBitWidths[fieldIndex];
                            isBitfield = (bitWidth > 0);
                        }
                        
                        if (isBitfield) {
                            node.value->accept(*this);
                            asm_.mov_rcx_rax();
                            member->object->accept(*this);
                            emitBitfieldWrite(varTypeIt->second, fieldIndex);
                            return;
                        }
                        
                        node.value->accept(*this);
                        asm_.push_rax();
                        member->object->accept(*this);
                        
                        int32_t offset = getRecordFieldOffset(varTypeIt->second, fieldIndex);
                        if (offset > 0) {
                            asm_.add_rax_imm32(offset);
                        }
                        
                        const std::string& fieldType = typeIt->second.fieldTypes[fieldIndex];
                        int32_t fieldSize = getTypeSize(fieldType);
                        
                        asm_.mov_rcx_rax();
                        asm_.pop_rax();
                        
                        if (fieldSize == 1) {
                            asm_.code.push_back(0x88);
                            asm_.code.push_back(0x01);
                        } else if (fieldSize == 2) {
                            asm_.code.push_back(0x66);
                            asm_.code.push_back(0x89);
                            asm_.code.push_back(0x01);
                        } else if (fieldSize == 4) {
                            asm_.code.push_back(0x89);
                            asm_.code.push_back(0x01);
                        } else {
                            asm_.mov_mem_rcx_rax();
                        }
                        return;
                    }
                }
            }
        }
        node.value->accept(*this);
        asm_.push_rax();
        member->object->accept(*this);
        asm_.mov_rcx_rax();
        asm_.pop_rax();
        asm_.mov_mem_rcx_rax();
        return;
    }
    
    node.value->accept(*this);
    
    if (lastExprWasFloat_) {
        isFloat = true;
    }
    
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        bool isReassignment = locals.count(id->name) > 0 || 
                              varRegisters_.count(id->name) > 0 ||
                              globalVarRegisters_.count(id->name) > 0;
        
        if (isReassignment) {
            constVars.erase(id->name);
            constStrVars.erase(id->name);
            constFloatVars.erase(id->name);
        }
        
        if (isFloat && node.op == TokenType::ASSIGN) {
            floatVars.insert(id->name);
        }
        
        if (node.op == TokenType::ASSIGN) {
            if (isStringReturningExpr(node.value.get())) {
                std::string strVal;
                if (tryEvalConstantString(node.value.get(), strVal)) {
                    constStrVars[id->name] = strVal;
                } else {
                    constStrVars[id->name] = "";
                }
            }
        }
        
        auto regIt = varRegisters_.find(id->name);
        auto globalRegIt = globalVarRegisters_.find(id->name);
        
        VarRegister reg = VarRegister::NONE;
        if (regIt != varRegisters_.end() && regIt->second != VarRegister::NONE) {
            reg = regIt->second;
        } else if (globalRegIt != globalVarRegisters_.end() && globalRegIt->second != VarRegister::NONE) {
            reg = globalRegIt->second;
        }
        
        if (reg != VarRegister::NONE) {
            if (isFloat && lastExprWasFloat_) {
                asm_.movq_rax_xmm0();
            }
            
            if (node.op != TokenType::ASSIGN && !isFloat) {
                if (node.op == TokenType::SLASH_ASSIGN) {
                    asm_.mov_rcx_rax();
                    switch (reg) {
                        case VarRegister::RBX: asm_.mov_rax_rbx(); break;
                        case VarRegister::R12: asm_.mov_rax_r12(); break;
                        case VarRegister::R13: asm_.mov_rax_r13(); break;
                        case VarRegister::R14: asm_.mov_rax_r14(); break;
                        case VarRegister::R15: asm_.mov_rax_r15(); break;
                        default: break;
                    }
                    asm_.cqo();
                    asm_.idiv_rcx();
                } else {
                    asm_.push_rax();
                    switch (reg) {
                        case VarRegister::RBX: asm_.mov_rax_rbx(); break;
                        case VarRegister::R12: asm_.mov_rax_r12(); break;
                        case VarRegister::R13: asm_.mov_rax_r13(); break;
                        case VarRegister::R14: asm_.mov_rax_r14(); break;
                        case VarRegister::R15: asm_.mov_rax_r15(); break;
                        default: break;
                    }
                    asm_.pop_rcx();
                    
                    if (node.op == TokenType::PLUS_ASSIGN) asm_.add_rax_rcx();
                    else if (node.op == TokenType::MINUS_ASSIGN) asm_.sub_rax_rcx();
                    else if (node.op == TokenType::STAR_ASSIGN) asm_.imul_rax_rcx();
                }
            }
            
            switch (reg) {
                case VarRegister::RBX: asm_.mov_rbx_rax(); break;
                case VarRegister::R12: asm_.mov_r12_rax(); break;
                case VarRegister::R13: asm_.mov_r13_rax(); break;
                case VarRegister::R14: asm_.mov_r14_rax(); break;
                case VarRegister::R15: asm_.mov_r15_rax(); break;
                default: break;
            }
        } else {
            auto it = locals.find(id->name);
            if (it == locals.end()) {
                allocLocal(id->name);
                it = locals.find(id->name);
            }
            
            if (node.op != TokenType::ASSIGN) {
                if (node.op == TokenType::SLASH_ASSIGN) {
                    asm_.mov_rcx_rax();
                    asm_.mov_rax_mem_rbp(it->second);
                    asm_.cqo();
                    asm_.idiv_rcx();
                } else if (node.op == TokenType::STAR_ASSIGN) {
                    asm_.mov_rcx_mem_rbp(it->second);
                    asm_.imul_rax_rcx();
                } else {
                    asm_.push_rax();
                    asm_.mov_rax_mem_rbp(it->second);
                    asm_.pop_rcx();
                    if (node.op == TokenType::PLUS_ASSIGN) asm_.add_rax_rcx();
                    else if (node.op == TokenType::MINUS_ASSIGN) asm_.sub_rax_rcx();
                }
            }
            
            if (isFloat && lastExprWasFloat_) {
                asm_.movsd_mem_rbp_xmm0(it->second);
            } else {
                asm_.mov_mem_rbp_rax(it->second);
            }
        }
    }
    else if (auto* indexExpr = dynamic_cast<IndexExpr*>(node.target.get())) {
        emitIndexAssignment(indexExpr, node);
    }
}

// Helper for index assignment (extracted for clarity)
void NativeCodeGen::emitIndexAssignment(IndexExpr* indexExpr, AssignExpr& node) {
    asm_.push_rax();
    
    if (auto* strKey = dynamic_cast<StringLiteral*>(indexExpr->index.get())) {
        // Map index assignment with string key
        uint64_t hash = 5381;
        for (char c : strKey->value) {
            hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
        }
        uint32_t keyRva = addString(strKey->value);
        
        indexExpr->object->accept(*this);
        allocLocal("$map_set_ptr");
        asm_.mov_mem_rbp_rax(locals["$map_set_ptr"]);
        
        asm_.mov_rcx_mem_rax();
        
        asm_.mov_rax_imm64(static_cast<int64_t>(hash));
        asm_.code.push_back(0x48); asm_.code.push_back(0x31); asm_.code.push_back(0xD2);
        asm_.code.push_back(0x48); asm_.code.push_back(0xF7); asm_.code.push_back(0xF1);
        
        asm_.mov_rax_mem_rbp(locals["$map_set_ptr"]);
        asm_.add_rax_imm32(16);
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1); 
        asm_.code.push_back(0xE2); asm_.code.push_back(0x03);
        asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xD0);
        
        allocLocal("$bucket_addr");
        asm_.mov_mem_rbp_rax(locals["$bucket_addr"]);
        
        asm_.mov_rax_mem_rax();
        
        std::string searchLoop = newLabel("map_set_search");
        std::string foundLabel = newLabel("map_set_found");
        std::string insertNew = newLabel("map_set_insert");
        
        asm_.label(searchLoop);
        asm_.test_rax_rax();
        asm_.jz_rel32(insertNew);
        
        asm_.push_rax();
        asm_.mov_rcx_mem_rax();
        asm_.mov_rdx_imm64(static_cast<int64_t>(hash));
        asm_.code.push_back(0x48); asm_.code.push_back(0x39); asm_.code.push_back(0xD1);
        asm_.pop_rax();
        
        std::string nextEntry = newLabel("map_set_next");
        asm_.jnz_rel32(nextEntry);
        
        asm_.jmp_rel32(foundLabel);
        
        asm_.label(nextEntry);
        asm_.add_rax_imm32(24);
        asm_.mov_rax_mem_rax();
        asm_.jmp_rel32(searchLoop);
        
        asm_.label(insertNew);
        emitGCAllocMapEntry();
        
        allocLocal("$new_entry");
        asm_.mov_mem_rbp_rax(locals["$new_entry"]);
        
        asm_.mov_rcx_imm64(static_cast<int64_t>(hash));
        asm_.mov_mem_rax_rcx();
        
        asm_.mov_rcx_mem_rbp(locals["$new_entry"]);
        asm_.add_rcx_imm32(8);
        asm_.lea_rax_rip_fixup(keyRva);
        asm_.mov_mem_rcx_rax();
        
        asm_.mov_rax_mem_rbp(locals["$bucket_addr"]);
        asm_.mov_rcx_mem_rax();
        asm_.mov_rax_mem_rbp(locals["$new_entry"]);
        asm_.add_rax_imm32(24);
        asm_.mov_mem_rax_rcx();
        
        asm_.mov_rax_mem_rbp(locals["$bucket_addr"]);
        asm_.mov_rcx_mem_rbp(locals["$new_entry"]);
        asm_.mov_mem_rax_rcx();
        
        asm_.mov_rax_mem_rbp(locals["$new_entry"]);
        std::string setValueLabel = newLabel("map_set_value");
        asm_.jmp_rel32(setValueLabel);
        
        asm_.label(foundLabel);
        
        asm_.label(setValueLabel);
        asm_.add_rax_imm32(16);
        asm_.pop_rcx();
        asm_.mov_mem_rax_rcx();
        asm_.mov_rax_rcx();
    } else {
        // Check for fixed-size array
        if (auto* objId = dynamic_cast<Identifier*>(indexExpr->object.get())) {
            auto fixedArrayIt = varFixedArrayTypes_.find(objId->name);
            if (fixedArrayIt != varFixedArrayTypes_.end()) {
                const FixedArrayInfo& info = fixedArrayIt->second;
                
                // Value is already on stack from push_rax above
                
                // Evaluate the index (0-based for fixed arrays)
                indexExpr->index->accept(*this);
                asm_.push_rax();  // Save index
                
                // Load the array pointer
                indexExpr->object->accept(*this);
                
                asm_.pop_rcx();  // rcx = index
                
                // Calculate element offset: index * elementSize
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
                
                asm_.pop_rcx();  // rcx = value
                
                if (info.elementSize == 1) {
                    asm_.code.push_back(0x88);
                    asm_.code.push_back(0x08);
                } else if (info.elementSize == 2) {
                    asm_.code.push_back(0x66);
                    asm_.code.push_back(0x89);
                    asm_.code.push_back(0x08);
                } else if (info.elementSize == 4) {
                    asm_.code.push_back(0x89);
                    asm_.code.push_back(0x08);
                } else {
                    asm_.code.push_back(0x48);
                    asm_.code.push_back(0x89);
                    asm_.code.push_back(0x08);
                }
                asm_.mov_rax_rcx();
                return;
            }
        }
        
        // List index assignment (1-based indexing)
        indexExpr->index->accept(*this);
        asm_.dec_rax();
        asm_.push_rax();
        
        indexExpr->object->accept(*this);
        asm_.add_rax_imm32(16);
        
        asm_.pop_rcx();
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
        asm_.code.push_back(0xE1); asm_.code.push_back(0x03);
        asm_.add_rax_rcx();
        
        asm_.pop_rcx();
        asm_.mov_mem_rax_rcx();
        asm_.mov_rax_rcx();
    }
}

} // namespace flex
