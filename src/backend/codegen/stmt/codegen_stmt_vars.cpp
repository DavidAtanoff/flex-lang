// Flex Compiler - Native Code Generator Variable Statements
// Handles: VarDecl, DestructuringDecl, AssignStmt

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(ExprStmt& node) {
    node.expr->accept(*this);
}

void NativeCodeGen::visit(VarDecl& node) {
    if (node.initializer) {
        bool isFloat = isFloatExpression(node.initializer.get());
        
        // Check if this is a generic function call with float arguments
        // This handles the case where isFloatExpression might not detect it
        if (!isFloat) {
            if (auto* call = dynamic_cast<CallExpr*>(node.initializer.get())) {
                if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
                    auto it = genericFunctions_.find(id->name);
                    if (it != genericFunctions_.end()) {
                        // This is a generic function call
                        // Check if any argument is a float literal
                        for (auto& arg : call->args) {
                            if (dynamic_cast<FloatLiteral*>(arg.get())) {
                                isFloat = true;
                                break;
                            }
                            // Also check for float variables
                            if (auto* argId = dynamic_cast<Identifier*>(arg.get())) {
                                if (floatVars.count(argId->name) || constFloatVars.count(argId->name)) {
                                    isFloat = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (!node.isMutable) {
            if (isFloat) {
                double floatVal;
                if (tryEvalConstantFloat(node.initializer.get(), floatVal)) {
                    constFloatVars[node.name] = floatVal;
                }
            } else {
                int64_t intVal;
                if (tryEvalConstant(node.initializer.get(), intVal)) {
                    constVars[node.name] = intVal;
                }
            }
            std::string strVal;
            if (tryEvalConstantString(node.initializer.get(), strVal)) {
                constStrVars[node.name] = strVal;
            }
        }
        
        if (isFloat) {
            floatVars.insert(node.name);
        }
        
        if (dynamic_cast<StringLiteral*>(node.initializer.get()) ||
            dynamic_cast<InterpolatedString*>(node.initializer.get()) ||
            isStringReturningExpr(node.initializer.get())) {
            if (constStrVars.find(node.name) == constStrVars.end()) {
                constStrVars[node.name] = "";
            }
        }
        
        if (auto* list = dynamic_cast<ListExpr*>(node.initializer.get())) {
            listSizes[node.name] = list->elements.size();
            std::vector<int64_t> values;
            bool allConst = true;
            for (auto& elem : list->elements) {
                int64_t val;
                if (tryEvalConstant(elem.get(), val)) {
                    values.push_back(val);
                } else {
                    allConst = false;
                    break;
                }
            }
            if (allConst) {
                constListVars[node.name] = values;
            }
        }
        node.initializer->accept(*this);
        
        // After evaluating the initializer, check if it was a float expression
        // This catches cases where the expression evaluation sets lastExprWasFloat_
        if (lastExprWasFloat_) {
            isFloat = true;
            floatVars.insert(node.name);
        }
        
        // Also check if it might be in the register allocator directly
        VarRegister allocatedReg = regAlloc_.getRegister(node.name);
        auto regIt = varRegisters_.find(node.name);
        if (allocatedReg != VarRegister::NONE && (regIt == varRegisters_.end() || regIt->second == VarRegister::NONE)) {
            // Register allocator has a register but varRegisters_ doesn't - fix it
            varRegisters_[node.name] = allocatedReg;
            regIt = varRegisters_.find(node.name);
        }
        
        if (regIt != varRegisters_.end() && regIt->second != VarRegister::NONE) {
            if (isFloat && lastExprWasFloat_) {
                asm_.movq_rax_xmm0();
            }
            switch (regIt->second) {
                case VarRegister::RBX: asm_.mov_rbx_rax(); break;
                case VarRegister::R12: asm_.mov_r12_rax(); break;
                case VarRegister::R13: asm_.mov_r13_rax(); break;
                case VarRegister::R14: asm_.mov_r14_rax(); break;
                case VarRegister::R15: asm_.mov_r15_rax(); break;
                default: break;
            }
            return;
        }
        
        auto globalRegIt = globalVarRegisters_.find(node.name);
        if (globalRegIt != globalVarRegisters_.end() && globalRegIt->second != VarRegister::NONE) {
            if (isFloat && lastExprWasFloat_) {
                asm_.movq_rax_xmm0();
            }
            switch (globalRegIt->second) {
                case VarRegister::RBX: asm_.mov_rbx_rax(); break;
                case VarRegister::R12: asm_.mov_r12_rax(); break;
                case VarRegister::R13: asm_.mov_r13_rax(); break;
                case VarRegister::R14: asm_.mov_r14_rax(); break;
                case VarRegister::R15: asm_.mov_r15_rax(); break;
                default: break;
            }
            return;
        }
        
        if (isFloat && lastExprWasFloat_) {
            allocLocal(node.name);
            asm_.movsd_mem_rbp_xmm0(locals[node.name]);
            return;
        }
        
        // For non-float, non-register-allocated variables, store on stack
        allocLocal(node.name);
        asm_.mov_mem_rbp_rax(locals[node.name]);
        return;
    } else {
        asm_.xor_rax_rax();
    }
    
    auto regIt = varRegisters_.find(node.name);
    if (regIt != varRegisters_.end() && regIt->second != VarRegister::NONE) {
        switch (regIt->second) {
            case VarRegister::RBX: asm_.mov_rbx_rax(); break;
            case VarRegister::R12: asm_.mov_r12_rax(); break;
            case VarRegister::R13: asm_.mov_r13_rax(); break;
            case VarRegister::R14: asm_.mov_r14_rax(); break;
            case VarRegister::R15: asm_.mov_r15_rax(); break;
            default: break;
        }
        return;
    }
    
    auto globalRegIt = globalVarRegisters_.find(node.name);
    if (globalRegIt != globalVarRegisters_.end() && globalRegIt->second != VarRegister::NONE) {
        switch (globalRegIt->second) {
            case VarRegister::RBX: asm_.mov_rbx_rax(); break;
            case VarRegister::R12: asm_.mov_r12_rax(); break;
            case VarRegister::R13: asm_.mov_r13_rax(); break;
            case VarRegister::R14: asm_.mov_r14_rax(); break;
            case VarRegister::R15: asm_.mov_r15_rax(); break;
            default: break;
        }
        return;
    }
    
    allocLocal(node.name);
    asm_.mov_mem_rbp_rax(locals[node.name]);
}

void NativeCodeGen::visit(DestructuringDecl& node) {
    if (node.kind == DestructuringDecl::Kind::TUPLE) {
        if (auto* list = dynamic_cast<ListExpr*>(node.initializer.get())) {
            for (size_t i = 0; i < node.names.size() && i < list->elements.size(); i++) {
                list->elements[i]->accept(*this);
                allocLocal(node.names[i]);
                asm_.mov_mem_rbp_rax(locals[node.names[i]]);
                
                int64_t val;
                if (tryEvalConstant(list->elements[i].get(), val)) {
                    constVars[node.names[i]] = val;
                }
            }
            return;
        }
    }
    
    if (node.kind == DestructuringDecl::Kind::RECORD) {
        if (auto* rec = dynamic_cast<RecordExpr*>(node.initializer.get())) {
            std::map<std::string, Expression*> fieldMap;
            for (auto& [name, expr] : rec->fields) {
                fieldMap[name] = expr.get();
            }
            
            for (const std::string& name : node.names) {
                auto it = fieldMap.find(name);
                if (it != fieldMap.end()) {
                    it->second->accept(*this);
                    
                    int64_t val;
                    if (tryEvalConstant(it->second, val)) {
                        constVars[name] = val;
                    }
                    std::string strVal;
                    if (tryEvalConstantString(it->second, strVal)) {
                        constStrVars[name] = strVal;
                    } else if (dynamic_cast<StringLiteral*>(it->second) ||
                               dynamic_cast<InterpolatedString*>(it->second)) {
                        constStrVars[name] = "";
                    }
                } else {
                    asm_.xor_rax_rax();
                }
                allocLocal(name);
                asm_.mov_mem_rbp_rax(locals[name]);
            }
            return;
        }
    }
    
    node.initializer->accept(*this);
    
    allocLocal("$destruct_base");
    asm_.mov_mem_rbp_rax(locals["$destruct_base"]);
    
    for (size_t i = 0; i < node.names.size(); i++) {
        asm_.mov_rax_mem_rbp(locals["$destruct_base"]);
        
        if (i > 0) {
            asm_.mov_rcx_imm64(i * 8);
            asm_.add_rax_rcx();
        }
        
        asm_.mov_rax_mem_rax();
        
        allocLocal(node.names[i]);
        asm_.mov_mem_rbp_rax(locals[node.names[i]]);
    }
}


void NativeCodeGen::visit(AssignStmt& node) {
    bool isFloat = false;
    
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        isFloat = floatVars.count(id->name) > 0 || isFloatExpression(node.value.get());
        
        if (node.op == TokenType::ASSIGN) {
            if (isFloat) {
                double floatVal;
                if (tryEvalConstantFloat(node.value.get(), floatVal)) {
                    constFloatVars[id->name] = floatVal;
                } else {
                    constFloatVars.erase(id->name);
                }
                floatVars.insert(id->name);
            } else {
                int64_t intVal;
                if (tryEvalConstant(node.value.get(), intVal)) {
                    constVars[id->name] = intVal;
                } else {
                    constVars.erase(id->name);
                }
            }
            std::string strVal;
            if (tryEvalConstantString(node.value.get(), strVal)) {
                constStrVars[id->name] = strVal;
            } else if (isStringReturningExpr(node.value.get())) {
                constStrVars[id->name] = "";
            } else {
                constStrVars.erase(id->name);
            }
        } else {
            constVars.erase(id->name);
            constStrVars.erase(id->name);
            constFloatVars.erase(id->name);
        }
    }
    
    int64_t constVal;
    bool valueIsConst = tryEvalConstant(node.value.get(), constVal);
    bool valueIsSmall = valueIsConst && constVal >= INT32_MIN && constVal <= INT32_MAX;
    
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        auto regIt = varRegisters_.find(id->name);
        auto globalRegIt = globalVarRegisters_.find(id->name);
        
        VarRegister reg = VarRegister::NONE;
        if (regIt != varRegisters_.end() && regIt->second != VarRegister::NONE) {
            reg = regIt->second;
        } else if (globalRegIt != globalVarRegisters_.end() && globalRegIt->second != VarRegister::NONE) {
            reg = globalRegIt->second;
        }
        
        if (reg != VarRegister::NONE) {
            if (!isFloat) {
                if (valueIsSmall && (node.op == TokenType::PLUS_ASSIGN || node.op == TokenType::MINUS_ASSIGN)) {
                    switch (reg) {
                        case VarRegister::RBX: asm_.mov_rax_rbx(); break;
                        case VarRegister::R12: asm_.mov_rax_r12(); break;
                        case VarRegister::R13: asm_.mov_rax_r13(); break;
                        case VarRegister::R14: asm_.mov_rax_r14(); break;
                        case VarRegister::R15: asm_.mov_rax_r15(); break;
                        default: break;
                    }
                    
                    if (node.op == TokenType::PLUS_ASSIGN) {
                        asm_.code.push_back(0x48); asm_.code.push_back(0x05);
                    } else {
                        asm_.code.push_back(0x48); asm_.code.push_back(0x2D);
                    }
                    asm_.code.push_back(constVal & 0xFF);
                    asm_.code.push_back((constVal >> 8) & 0xFF);
                    asm_.code.push_back((constVal >> 16) & 0xFF);
                    asm_.code.push_back((constVal >> 24) & 0xFF);
                    
                    switch (reg) {
                        case VarRegister::RBX: asm_.mov_rbx_rax(); break;
                        case VarRegister::R12: asm_.mov_r12_rax(); break;
                        case VarRegister::R13: asm_.mov_r13_rax(); break;
                        case VarRegister::R14: asm_.mov_r14_rax(); break;
                        case VarRegister::R15: asm_.mov_r15_rax(); break;
                        default: break;
                    }
                    return;
                }
                
                node.value->accept(*this);
                
                if (node.op == TokenType::PLUS_ASSIGN) {
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
                    asm_.add_rax_rcx();
                } else if (node.op == TokenType::MINUS_ASSIGN) {
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
                    asm_.sub_rax_rcx();
                } else if (node.op == TokenType::STAR_ASSIGN) {
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
                    asm_.imul_rax_rcx();
                } else if (node.op == TokenType::SLASH_ASSIGN) {
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
                }
                
                switch (reg) {
                    case VarRegister::RBX: asm_.mov_rbx_rax(); break;
                    case VarRegister::R12: asm_.mov_r12_rax(); break;
                    case VarRegister::R13: asm_.mov_r13_rax(); break;
                    case VarRegister::R14: asm_.mov_r14_rax(); break;
                    case VarRegister::R15: asm_.mov_r15_rax(); break;
                    default: break;
                }
                return;
            }
            
            node.value->accept(*this);
            if (isFloat && lastExprWasFloat_) {
                asm_.movq_rax_xmm0();
                
                switch (reg) {
                    case VarRegister::RBX: asm_.mov_rbx_rax(); break;
                    case VarRegister::R12: asm_.mov_r12_rax(); break;
                    case VarRegister::R13: asm_.mov_r13_rax(); break;
                    case VarRegister::R14: asm_.mov_r14_rax(); break;
                    case VarRegister::R15: asm_.mov_r15_rax(); break;
                    default: break;
                }
            }
            return;
        }
        
        auto it = locals.find(id->name);
        
        if (it != locals.end() && !isFloat && valueIsSmall && 
            (node.op == TokenType::PLUS_ASSIGN || node.op == TokenType::MINUS_ASSIGN)) {
            asm_.mov_rax_mem_rbp(it->second);
            if (node.op == TokenType::PLUS_ASSIGN) {
                asm_.code.push_back(0x48); asm_.code.push_back(0x05);
            } else {
                asm_.code.push_back(0x48); asm_.code.push_back(0x2D);
            }
            asm_.code.push_back(constVal & 0xFF);
            asm_.code.push_back((constVal >> 8) & 0xFF);
            asm_.code.push_back((constVal >> 16) & 0xFF);
            asm_.code.push_back((constVal >> 24) & 0xFF);
            asm_.mov_mem_rbp_rax(it->second);
            return;
        }
        
        node.value->accept(*this);
        
        if (it != locals.end()) {
            if (isFloat && lastExprWasFloat_) {
                if (node.op == TokenType::PLUS_ASSIGN) {
                    asm_.movsd_xmm1_mem_rbp(it->second);
                    asm_.addsd_xmm0_xmm1();
                } else if (node.op == TokenType::MINUS_ASSIGN) {
                    asm_.movsd_xmm1_xmm0();
                    asm_.movsd_xmm0_mem_rbp(it->second);
                    asm_.subsd_xmm0_xmm1();
                } else if (node.op == TokenType::STAR_ASSIGN) {
                    asm_.movsd_xmm1_mem_rbp(it->second);
                    asm_.mulsd_xmm0_xmm1();
                } else if (node.op == TokenType::SLASH_ASSIGN) {
                    asm_.movsd_xmm1_xmm0();
                    asm_.movsd_xmm0_mem_rbp(it->second);
                    asm_.divsd_xmm0_xmm1();
                }
                asm_.movsd_mem_rbp_xmm0(it->second);
            } else {
                if (node.op == TokenType::PLUS_ASSIGN) {
                    asm_.mov_rcx_mem_rbp(it->second);
                    asm_.add_rax_rcx();
                } else if (node.op == TokenType::MINUS_ASSIGN) {
                    asm_.mov_rcx_rax();
                    asm_.mov_rax_mem_rbp(it->second);
                    asm_.sub_rax_rcx();
                } else if (node.op == TokenType::STAR_ASSIGN) {
                    asm_.mov_rcx_mem_rbp(it->second);
                    asm_.imul_rax_rcx();
                } else if (node.op == TokenType::SLASH_ASSIGN) {
                    asm_.mov_rcx_rax();
                    asm_.mov_rax_mem_rbp(it->second);
                    asm_.cqo();
                    asm_.idiv_rcx();
                }
                asm_.mov_mem_rbp_rax(it->second);
            }
        } else {
            allocLocal(id->name);
            if (isFloat && lastExprWasFloat_) {
                asm_.movsd_mem_rbp_xmm0(locals[id->name]);
            } else {
                asm_.mov_mem_rbp_rax(locals[id->name]);
            }
        }
    } else if (auto* deref = dynamic_cast<DerefExpr*>(node.target.get())) {
        // Handle pointer dereference assignment: *ptr = value
        // First evaluate the value
        node.value->accept(*this);
        asm_.push_rax();  // Save value
        
        // Then evaluate the pointer (not dereferencing it)
        deref->operand->accept(*this);
        asm_.mov_rcx_rax();  // rcx = pointer address
        
        // Store value at pointer address
        asm_.pop_rax();  // rax = value
        asm_.mov_mem_rcx_rax();  // [rcx] = rax
    } else {
        node.target->accept(*this);
        asm_.push_rax();
        
        node.value->accept(*this);
        asm_.pop_rcx();
        
        asm_.mov_mem_rcx_rax();
    }
}

} // namespace flex
