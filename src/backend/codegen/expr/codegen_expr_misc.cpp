// Flex Compiler - Native Code Generator Miscellaneous Expressions
// Handles: Lambda, Assign, Await, Spawn, DSL, Propagate, New, Cast, AddressOf, Deref

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
        // Evaluate the value first
        node.value->accept(*this);
        asm_.push_rax();  // Save value
        
        // Evaluate the pointer
        deref->operand->accept(*this);
        asm_.mov_rcx_rax();  // rcx = pointer
        
        // Store value at pointer address
        asm_.pop_rax();  // rax = value
        asm_.mov_mem_rcx_rax();  // [rcx] = rax
        return;
    }
    
    node.value->accept(*this);
    
    // Update isFloat based on lastExprWasFloat_ after evaluation
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
        
        // Track float variables
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
                // Float value - move from xmm0 to rax for register storage
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
            
            // Store value - use float instruction if needed
            if (isFloat && lastExprWasFloat_) {
                asm_.movsd_mem_rbp_xmm0(it->second);
            } else {
                asm_.mov_mem_rbp_rax(it->second);
            }
        }
    }
    else if (auto* indexExpr = dynamic_cast<IndexExpr*>(node.target.get())) {
        asm_.push_rax();
        
        if (auto* strKey = dynamic_cast<StringLiteral*>(indexExpr->index.get())) {
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
            // List index assignment (1-based indexing)
            indexExpr->index->accept(*this);
            asm_.dec_rax();  // Convert 1-based to 0-based
            asm_.push_rax();
            
            indexExpr->object->accept(*this);
            
            // Skip the 16-byte list header (count + capacity) to get to elements
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
}

void NativeCodeGen::visit(LambdaExpr& node) {
    std::string lambdaLabel = newLabel("lambda");
    std::string afterLambda = newLabel("after_lambda");
    
    std::set<std::string> paramNames;
    for (const auto& param : node.params) {
        paramNames.insert(param.first);
    }
    
    std::vector<std::string> capturedVars;
    std::set<std::string> capturedSet;
    collectCapturedVariables(node.body.get(), paramNames, capturedSet);
    
    for (const auto& varName : capturedSet) {
        bool inOuterScope = locals.count(varName) > 0 ||
                           varRegisters_.count(varName) > 0 ||
                           globalVarRegisters_.count(varName) > 0 ||
                           constVars.count(varName) > 0 ||
                           constFloatVars.count(varName) > 0;
        if (inOuterScope) {
            capturedVars.push_back(varName);
        }
    }
    
    bool hasCaptures = !capturedVars.empty();
    
    asm_.jmp_rel32(afterLambda);
    
    asm_.label(lambdaLabel);
    
    std::map<std::string, int32_t> savedLocals = locals;
    int32_t savedStackOffset = stackOffset;
    bool savedInFunction = inFunction;
    int32_t savedFunctionStackSize = functionStackSize_;
    bool savedStackAllocated = stackAllocated_;
    std::map<std::string, VarRegister> savedVarRegisters = varRegisters_;
    
    inFunction = true;
    locals.clear();
    stackOffset = 0;
    varRegisters_.clear();
    
    asm_.push_rbp();
    asm_.mov_rbp_rsp();
    
    functionStackSize_ = 0x40 + (hasCaptures ? (int32_t)(capturedVars.size() * 8 + 8) : 0);
    asm_.sub_rsp_imm32(functionStackSize_);
    stackAllocated_ = true;
    
    if (hasCaptures) {
        allocLocal("$closure_ptr");
        asm_.mov_mem_rbp_rcx(locals["$closure_ptr"]);
        
        for (size_t i = 0; i < capturedVars.size(); i++) {
            const std::string& varName = capturedVars[i];
            allocLocal(varName);
            int32_t off = locals[varName];
            
            asm_.mov_rax_mem_rbp(locals["$closure_ptr"]);
            int32_t captureOffset = 16 + static_cast<int32_t>(i * 8);
            asm_.add_rax_imm32(captureOffset);
            asm_.mov_rax_mem_rax();
            asm_.mov_mem_rbp_rax(off);
        }
    }
    
    for (size_t i = 0; i < node.params.size() && i < 3; i++) {
        const std::string& paramName = node.params[i].first;
        allocLocal(paramName);
        int32_t off = locals[paramName];
        
        switch (i) {
            case 0:
                asm_.code.push_back(0x48); asm_.code.push_back(0x89);
                asm_.code.push_back(0x95);
                asm_.code.push_back(off & 0xFF);
                asm_.code.push_back((off >> 8) & 0xFF);
                asm_.code.push_back((off >> 16) & 0xFF);
                asm_.code.push_back((off >> 24) & 0xFF);
                break;
            case 1:
                asm_.code.push_back(0x4C); asm_.code.push_back(0x89);
                asm_.code.push_back(0x85);
                asm_.code.push_back(off & 0xFF);
                asm_.code.push_back((off >> 8) & 0xFF);
                asm_.code.push_back((off >> 16) & 0xFF);
                asm_.code.push_back((off >> 24) & 0xFF);
                break;
            case 2:
                asm_.code.push_back(0x4C); asm_.code.push_back(0x89);
                asm_.code.push_back(0x8D);
                asm_.code.push_back(off & 0xFF);
                asm_.code.push_back((off >> 8) & 0xFF);
                asm_.code.push_back((off >> 16) & 0xFF);
                asm_.code.push_back((off >> 24) & 0xFF);
                break;
        }
    }
    
    node.body->accept(*this);
    
    asm_.add_rsp_imm32(functionStackSize_);
    asm_.pop_rbp();
    asm_.ret();
    
    locals = savedLocals;
    stackOffset = savedStackOffset;
    inFunction = savedInFunction;
    functionStackSize_ = savedFunctionStackSize;
    stackAllocated_ = savedStackAllocated;
    varRegisters_ = savedVarRegisters;
    
    asm_.label(afterLambda);
    
    emitGCAllocClosure(capturedVars.size());
    
    asm_.push_rax();
    
    asm_.code.push_back(0x48); asm_.code.push_back(0x8D); asm_.code.push_back(0x0D);
    asm_.fixupLabel(lambdaLabel);
    asm_.code.push_back(0x48);
    asm_.code.push_back(0x89);
    asm_.code.push_back(0x08);
    
    for (size_t i = 0; i < capturedVars.size(); i++) {
        const std::string& varName = capturedVars[i];
        
        auto regIt = varRegisters_.find(varName);
        auto globalRegIt = globalVarRegisters_.find(varName);
        auto localIt = locals.find(varName);
        auto constIt = constVars.find(varName);
        auto constFloatIt = constFloatVars.find(varName);
        
        if (constIt != constVars.end()) {
            asm_.mov_rcx_imm64(constIt->second);
        } else if (constFloatIt != constFloatVars.end()) {
            union { double d; int64_t i; } u;
            u.d = constFloatIt->second;
            asm_.mov_rcx_imm64(u.i);
        } else if (regIt != varRegisters_.end() && regIt->second != VarRegister::NONE) {
            switch (regIt->second) {
                case VarRegister::RBX: asm_.mov_rcx_rbx(); break;
                case VarRegister::R12: asm_.mov_rcx_r12(); break;
                case VarRegister::R13: asm_.mov_rcx_r13(); break;
                case VarRegister::R14: asm_.mov_rcx_r14(); break;
                case VarRegister::R15: asm_.mov_rcx_r15(); break;
                default: asm_.xor_ecx_ecx(); break;
            }
        } else if (globalRegIt != globalVarRegisters_.end() && globalRegIt->second != VarRegister::NONE) {
            switch (globalRegIt->second) {
                case VarRegister::RBX: asm_.mov_rcx_rbx(); break;
                case VarRegister::R12: asm_.mov_rcx_r12(); break;
                case VarRegister::R13: asm_.mov_rcx_r13(); break;
                case VarRegister::R14: asm_.mov_rcx_r14(); break;
                case VarRegister::R15: asm_.mov_rcx_r15(); break;
                default: asm_.xor_ecx_ecx(); break;
            }
        } else if (localIt != locals.end()) {
            asm_.mov_rcx_mem_rbp(localIt->second);
        } else {
            asm_.xor_ecx_ecx();
        }
        
        asm_.code.push_back(0x48); asm_.code.push_back(0x8B);
        asm_.code.push_back(0x04); asm_.code.push_back(0x24);
        
        int32_t captureOffset = 16 + static_cast<int32_t>(i * 8);
        asm_.code.push_back(0x48);
        asm_.code.push_back(0x89);
        asm_.code.push_back(0x48);
        asm_.code.push_back(static_cast<uint8_t>(captureOffset));
    }
    
    asm_.pop_rax();
    
    lastExprWasFloat_ = false;
}

void NativeCodeGen::visit(AddressOfExpr& node) {
    // Get the address of the operand
    if (auto* id = dynamic_cast<Identifier*>(node.operand.get())) {
        // When taking address of a variable, it can no longer be treated as constant
        // because it might be modified through the pointer
        constVars.erase(id->name);
        constFloatVars.erase(id->name);
        
        // Get address of a variable
        auto regIt = varRegisters_.find(id->name);
        auto globalRegIt = globalVarRegisters_.find(id->name);
        auto localIt = locals.find(id->name);
        
        if (regIt != varRegisters_.end() && regIt->second != VarRegister::NONE) {
            // Variable is in a register - need to spill to stack first to get address
            // Allocate a permanent stack slot for this variable (not a temp)
            if (localIt == locals.end()) {
                allocLocal(id->name);
                localIt = locals.find(id->name);
            }
            int32_t off = localIt->second;
            
            // Spill register value to stack
            switch (regIt->second) {
                case VarRegister::RBX: asm_.mov_rax_rbx(); break;
                case VarRegister::R12: asm_.mov_rax_r12(); break;
                case VarRegister::R13: asm_.mov_rax_r13(); break;
                case VarRegister::R14: asm_.mov_rax_r14(); break;
                case VarRegister::R15: asm_.mov_rax_r15(); break;
                default: break;
            }
            asm_.mov_mem_rbp_rax(off);
            
            // Mark this variable as no longer register-allocated
            // (it now lives on the stack because its address was taken)
            varRegisters_[id->name] = VarRegister::NONE;
            
            // Return address of stack slot
            asm_.lea_rax_rbp(off);
        } else if (globalRegIt != globalVarRegisters_.end() && globalRegIt->second != VarRegister::NONE) {
            // Same for global register variables
            if (localIt == locals.end()) {
                allocLocal(id->name);
                localIt = locals.find(id->name);
            }
            int32_t off = localIt->second;
            
            switch (globalRegIt->second) {
                case VarRegister::RBX: asm_.mov_rax_rbx(); break;
                case VarRegister::R12: asm_.mov_rax_r12(); break;
                case VarRegister::R13: asm_.mov_rax_r13(); break;
                case VarRegister::R14: asm_.mov_rax_r14(); break;
                case VarRegister::R15: asm_.mov_rax_r15(); break;
                default: break;
            }
            asm_.mov_mem_rbp_rax(off);
            
            // Mark as no longer register-allocated
            globalVarRegisters_[id->name] = VarRegister::NONE;
            
            asm_.lea_rax_rbp(off);
        } else if (localIt != locals.end()) {
            // Variable is on stack - get its address
            asm_.lea_rax_rbp(localIt->second);
        } else {
            // Variable not found - allocate it
            allocLocal(id->name);
            asm_.lea_rax_rbp(locals[id->name]);
        }
    } else if (auto* indexExpr = dynamic_cast<IndexExpr*>(node.operand.get())) {
        // Get address of array element: &arr[i] (1-based indexing)
        indexExpr->index->accept(*this);
        asm_.dec_rax();  // Convert 1-based to 0-based
        asm_.push_rax();
        indexExpr->object->accept(*this);
        // Skip the 16-byte list header (count + capacity) to get to elements
        asm_.add_rax_imm32(16);
        asm_.pop_rcx();
        // Calculate address: base + index * 8
        asm_.code.push_back(0x48); asm_.code.push_back(0xC1);
        asm_.code.push_back(0xE1); asm_.code.push_back(0x03);  // shl rcx, 3
        asm_.add_rax_rcx();
    } else if (auto* memberExpr = dynamic_cast<MemberExpr*>(node.operand.get())) {
        // Get address of record field: &rec.field
        memberExpr->object->accept(*this);
        // For now, assume fields are at 8-byte offsets
        // A proper implementation would look up the field offset
        // This is a simplified version
    } else {
        // For other expressions, just evaluate them
        // (taking address of a temporary doesn't make sense, but we handle it)
        node.operand->accept(*this);
    }
    
    lastExprWasFloat_ = false;
}

void NativeCodeGen::visit(DerefExpr& node) {
    // Dereference a pointer: *ptr
    node.operand->accept(*this);
    
    // Check if this is being used as an lvalue (assignment target)
    // For now, just load the value at the address
    // rax contains the pointer, load the value it points to
    asm_.mov_rax_mem_rax();  // rax = [rax]
    
    lastExprWasFloat_ = false;
}

void NativeCodeGen::visit(NewExpr& node) {
    size_t size = 8;
    if (!node.args.empty()) {
        size = node.args.size() * 8;
    }
    
    emitGCAllocRaw(size);
    
    if (!node.args.empty()) {
        asm_.push_rax();
        for (size_t i = 0; i < node.args.size(); i++) {
            node.args[i]->accept(*this);
            asm_.push_rax();
            asm_.code.push_back(0x48); asm_.code.push_back(0x8B);
            asm_.code.push_back(0x4C); asm_.code.push_back(0x24);
            asm_.code.push_back((uint8_t)((node.args.size() - i) * 8));
            if (i > 0) {
                asm_.mov_rax_imm64(i * 8);
                asm_.add_rax_rcx();
                asm_.mov_rcx_rax();
            }
            asm_.pop_rax();
            asm_.mov_mem_rcx_rax();
        }
        asm_.pop_rax();
    }
}

void NativeCodeGen::visit(CastExpr& node) {
    node.expr->accept(*this);
    
    // Handle pointer casting - the value is already a pointer (address)
    // For pointer-to-pointer casts, no actual code change is needed at runtime
    // The type system handles the semantic difference
    // For int-to-pointer or pointer-to-int, the value is already in the right format (64-bit)
    
    // Check if this is a float-to-int or int-to-float cast
    bool sourceIsFloat = lastExprWasFloat_;
    bool targetIsFloat = (node.targetType == "float" || node.targetType == "f32" || 
                          node.targetType == "f64");
    bool targetIsInt = (node.targetType == "int" || node.targetType == "i8" || 
                        node.targetType == "i16" || node.targetType == "i32" || 
                        node.targetType == "i64" || node.targetType == "u8" ||
                        node.targetType == "u16" || node.targetType == "u32" ||
                        node.targetType == "u64");
    
    if (sourceIsFloat && targetIsInt) {
        // Float to int conversion
        asm_.cvttsd2si_rax_xmm0();
        lastExprWasFloat_ = false;
    } else if (!sourceIsFloat && targetIsFloat) {
        // Int to float conversion
        asm_.cvtsi2sd_xmm0_rax();
        lastExprWasFloat_ = true;
    } else if (targetIsFloat) {
        lastExprWasFloat_ = true;
    } else {
        lastExprWasFloat_ = false;
    }
}

void NativeCodeGen::visit(AwaitExpr& node) {
    node.operand->accept(*this);
    
    asm_.cmp_rax_imm32(0x1000);
    std::string notHandle = newLabel("await_not_handle");
    std::string done = newLabel("await_done");
    asm_.jl_rel32(notHandle);
    
    allocLocal("$await_handle");
    asm_.mov_mem_rbp_rax(locals["$await_handle"]);
    
    asm_.mov_rcx_rax();
    asm_.mov_rdx_imm64(0xFFFFFFFF);
    
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("WaitForSingleObject"));
    if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
    
    allocLocal("$await_result");
    asm_.mov_rcx_mem_rbp(locals["$await_handle"]);
    asm_.lea_rdx_rbp_offset(locals["$await_result"]);
    
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("GetExitCodeThread"));
    if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
    
    asm_.mov_rcx_mem_rbp(locals["$await_handle"]);
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("CloseHandle"));
    if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
    
    asm_.mov_rax_mem_rbp(locals["$await_result"]);
    asm_.jmp_rel32(done);
    
    asm_.label(notHandle);
    
    asm_.label(done);
}

void NativeCodeGen::visit(SpawnExpr& node) {
    if (auto* call = dynamic_cast<CallExpr*>(node.operand.get())) {
        if (auto* ident = dynamic_cast<Identifier*>(call->callee.get())) {
            if (asm_.labels.count(ident->name)) {
                std::string thunkLabel = newLabel("spawn_thunk_" + ident->name);
                std::string afterThunk = newLabel("spawn_after_thunk");
                
                asm_.jmp_rel32(afterThunk);
                
                asm_.label(thunkLabel);
                
                asm_.push_rbp();
                asm_.mov_rbp_rsp();
                asm_.push_rdi();
                asm_.sub_rsp_imm32(0x30);
                
                if (call->args.size() == 1) {
                    asm_.mov_mem_rbp_rcx(-0x10);
                }
                
                asm_.mov_ecx_imm32(-11);
                asm_.call_mem_rip(pe_.getImportRVA("GetStdHandle"));
                asm_.mov_rdi_rax();
                
                if (call->args.size() == 1) {
                    asm_.mov_rcx_mem_rbp(-0x10);
                }
                
                asm_.call_rel32(ident->name);
                
                asm_.add_rsp_imm32(0x30);
                asm_.pop_rdi();
                asm_.pop_rbp();
                asm_.ret();
                
                asm_.label(afterThunk);
                
                if (call->args.size() == 1) {
                    call->args[0]->accept(*this);
                    asm_.code.push_back(0x49); asm_.code.push_back(0x89); asm_.code.push_back(0xC1);
                } else {
                    asm_.code.push_back(0x4D); asm_.code.push_back(0x31); asm_.code.push_back(0xC9);
                }
                
                asm_.code.push_back(0x4C); asm_.code.push_back(0x8D); asm_.code.push_back(0x05);
                asm_.fixupLabel(thunkLabel);
                
                asm_.xor_rax_rax();
                asm_.mov_rcx_rax();
                asm_.mov_rdx_rax();
                
                asm_.code.push_back(0x48); asm_.code.push_back(0x89);
                asm_.code.push_back(0x44); asm_.code.push_back(0x24); asm_.code.push_back(0x20);
                
                asm_.code.push_back(0x48); asm_.code.push_back(0x89);
                asm_.code.push_back(0x44); asm_.code.push_back(0x24); asm_.code.push_back(0x28);
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x30);
                asm_.call_mem_rip(pe_.getImportRVA("CreateThread"));
                if (!stackAllocated_) asm_.add_rsp_imm32(0x30);
                
                return;
            }
        }
    }
    
    node.operand->accept(*this);
}

void NativeCodeGen::visit(DSLBlock& node) {
    uint32_t offset = addString(node.rawContent);
    asm_.lea_rax_rip_fixup(offset);
}

void NativeCodeGen::visit(PropagateExpr& node) {
    node.operand->accept(*this);
    
    asm_.push_rax();
    
    asm_.code.push_back(0x48); asm_.code.push_back(0x83); 
    asm_.code.push_back(0xE0); asm_.code.push_back(0x01);
    
    std::string okLabel = newLabel("propagate_ok");
    
    asm_.test_rax_rax();
    asm_.jnz_rel32(okLabel);
    
    asm_.pop_rax();
    
    asm_.code.push_back(0x48); asm_.code.push_back(0x89); asm_.code.push_back(0xEC);
    asm_.code.push_back(0x5D);
    asm_.code.push_back(0xC3);
    
    asm_.label(okLabel);
    asm_.pop_rax();
    asm_.code.push_back(0x48); asm_.code.push_back(0xD1); asm_.code.push_back(0xE8);
}

} // namespace flex
