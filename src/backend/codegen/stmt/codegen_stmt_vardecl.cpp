// Flex Compiler - Native Code Generator Variable Declaration
// Handles: VarDecl

#include "backend/codegen/codegen_base.h"

namespace flex {

void NativeCodeGen::visit(VarDecl& node) {
    if (node.initializer) {
        bool isFloat = isFloatExpression(node.initializer.get());
        
        // Check for function pointer type
        bool isFnPtr = false;
        if (!node.typeName.empty() && node.typeName.size() > 3 && 
            node.typeName.substr(0, 3) == "*fn") {
            isFnPtr = true;
            fnPtrVars_.insert(node.name);
        }
        if (auto* addrOf = dynamic_cast<AddressOfExpr*>(node.initializer.get())) {
            if (auto* fnId = dynamic_cast<Identifier*>(addrOf->operand.get())) {
                if (asm_.labels.find(fnId->name) != asm_.labels.end()) {
                    isFnPtr = true;
                    fnPtrVars_.insert(node.name);
                }
            }
        }
        if (auto* fnId = dynamic_cast<Identifier*>(node.initializer.get())) {
            if (asm_.labels.count(fnId->name)) {
                isFnPtr = true;
                fnPtrVars_.insert(node.name);
            }
        }
        
        // Check for generic function call with float arguments
        if (!isFloat) {
            if (auto* call = dynamic_cast<CallExpr*>(node.initializer.get())) {
                if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
                    auto it = genericFunctions_.find(id->name);
                    if (it != genericFunctions_.end()) {
                        for (auto& arg : call->args) {
                            if (dynamic_cast<FloatLiteral*>(arg.get())) {
                                isFloat = true;
                                break;
                            }
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
        
        // Track constants
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
        
        // Track list sizes
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
        
        // Track record type
        if (auto* call = dynamic_cast<CallExpr*>(node.initializer.get())) {
            if (auto* calleeId = dynamic_cast<Identifier*>(call->callee.get())) {
                if (recordTypes_.find(calleeId->name) != recordTypes_.end()) {
                    varRecordTypes_[node.name] = calleeId->name;
                }
            }
        }
        if (!node.typeName.empty() && recordTypes_.find(node.typeName) != recordTypes_.end()) {
            varRecordTypes_[node.name] = node.typeName;
        }
        
        if (auto* recExpr = dynamic_cast<RecordExpr*>(node.initializer.get())) {
            if (!recExpr->typeName.empty()) {
                varRecordTypes_[node.name] = recExpr->typeName;
            }
        }
        
        node.initializer->accept(*this);
        
        // Post-evaluation type inference
        if (auto* recExpr = dynamic_cast<RecordExpr*>(node.initializer.get())) {
            if (!recExpr->typeName.empty()) {
                varRecordTypes_[node.name] = recExpr->typeName;
            } else if (!recExpr->fields.empty()) {
                for (auto& [typeName, typeInfo] : recordTypes_) {
                    if (typeInfo.fieldNames.size() == recExpr->fields.size()) {
                        bool match = true;
                        for (size_t i = 0; i < recExpr->fields.size(); i++) {
                            if (recExpr->fields[i].first != typeInfo.fieldNames[i]) {
                                match = false;
                                break;
                            }
                        }
                        if (match) {
                            varRecordTypes_[node.name] = typeName;
                            break;
                        }
                    }
                }
            }
        }
        
        if (lastExprWasFloat_) {
            isFloat = true;
            floatVars.insert(node.name);
        }
        
        // Check register allocation
        VarRegister allocatedReg = regAlloc_.getRegister(node.name);
        auto regIt = varRegisters_.find(node.name);
        if (allocatedReg != VarRegister::NONE && (regIt == varRegisters_.end() || regIt->second == VarRegister::NONE)) {
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
        
        allocLocal(node.name);
        asm_.mov_mem_rbp_rax(locals[node.name]);
        return;
    } else {
        // No initializer - handle type-based allocation
        emitUninitializedVarDecl(node);
    }
}

void NativeCodeGen::emitUninitializedVarDecl(VarDecl& node) {
    // Track record type from type annotation
    if (!node.typeName.empty() && recordTypes_.find(node.typeName) != recordTypes_.end()) {
        varRecordTypes_[node.name] = node.typeName;
        size_t recordSize = static_cast<size_t>(getRecordSize(node.typeName));
        
        if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
        
        asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
        asm_.mov_rcx_rax();
        asm_.mov_rdx_imm64(0x08);
        asm_.mov_r8_imm64(recordSize);
        asm_.call_mem_rip(pe_.getImportRVA("HeapAlloc"));
        
        if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
        
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
        } else {
            allocLocal(node.name);
            asm_.mov_mem_rbp_rax(locals[node.name]);
        }
        return;
    }
    
    // Check for fixed-size array type
    if (!node.typeName.empty() && node.typeName.size() > 2 && 
        node.typeName.front() == '[' && node.typeName.back() == ']') {
        emitFixedArrayDecl(node);
        return;
    }
    
    asm_.xor_rax_rax();
    
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

void NativeCodeGen::emitFixedArrayDecl(VarDecl& node) {
    std::string inner = node.typeName.substr(1, node.typeName.size() - 2);
    
    int bracketDepth = 0;
    size_t semicolonPos = std::string::npos;
    for (size_t i = 0; i < inner.size(); i++) {
        if (inner[i] == '[') bracketDepth++;
        else if (inner[i] == ']') bracketDepth--;
        else if (inner[i] == ';' && bracketDepth == 0) {
            semicolonPos = i;
            break;
        }
    }
    
    if (semicolonPos != std::string::npos) {
        std::string elemType = inner.substr(0, semicolonPos);
        std::string sizeStr = inner.substr(semicolonPos + 1);
        while (!sizeStr.empty() && (sizeStr[0] == ' ' || sizeStr[0] == '\t')) sizeStr = sizeStr.substr(1);
        while (!sizeStr.empty() && (sizeStr.back() == ' ' || sizeStr.back() == '\t')) sizeStr.pop_back();
        
        size_t arrayCount = std::stoull(sizeStr);
        int32_t elemSize = getTypeSize(elemType);
        int32_t arraySize = elemSize * static_cast<int32_t>(arrayCount);
        
        FixedArrayInfo info;
        info.elementType = elemType;
        info.size = arrayCount;
        info.elementSize = elemSize;
        varFixedArrayTypes_[node.name] = info;
        
        if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
        
        asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
        asm_.mov_rcx_rax();
        asm_.mov_rdx_imm64(0x08);
        asm_.mov_r8_imm64(static_cast<size_t>(arraySize));
        asm_.call_mem_rip(pe_.getImportRVA("HeapAlloc"));
        
        if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
        
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
        } else {
            allocLocal(node.name);
            asm_.mov_mem_rbp_rax(locals[node.name]);
        }
    }
}

} // namespace flex
