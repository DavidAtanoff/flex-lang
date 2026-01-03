// Flex Compiler - Native Code Generator String Builtin Calls
// Handles: len, upper, lower, trim, starts_with, ends_with, substring, replace, split, join, index_of

#include "backend/codegen/codegen_base.h"

namespace flex {

// String builtin implementations extracted from codegen_call_core.cpp
// These handle string manipulation functions

void NativeCodeGen::emitStringLen(CallExpr& node) {
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
}

void NativeCodeGen::emitStringUpper(CallExpr& node) {
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
}

void NativeCodeGen::emitStringLower(CallExpr& node) {
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
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01);
    
    asm_.test_rax_rax();
    asm_.jz_rel32(doneLabel);
    
    asm_.code.push_back(0x3C); asm_.code.push_back('A');
    asm_.jl_rel32(noConvertLabel);
    asm_.code.push_back(0x3C); asm_.code.push_back('Z');
    asm_.jg_rel32(noConvertLabel);
    
    asm_.code.push_back(0x04); asm_.code.push_back(32);
    
    asm_.label(noConvertLabel);
    asm_.code.push_back(0x88); asm_.code.push_back(0x02);
    
    asm_.inc_rax();
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
    
    asm_.jmp_rel32(loopLabel);
    
    asm_.label(doneLabel);
    asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00);
    
    asm_.lea_rax_rbp(bufOffset);
}

void NativeCodeGen::emitStringTrim(CallExpr& node) {
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
    asm_.mov_rcx_rax();
    
    std::string skipLeadLoop = newLabel("trim_lead");
    std::string skipLeadDone = newLabel("trim_lead_done");
    
    asm_.label(skipLeadLoop);
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01);
    asm_.test_rax_rax();
    asm_.jz_rel32(skipLeadDone);
    asm_.code.push_back(0x3C); asm_.code.push_back(' ');
    asm_.code.push_back(0x74); asm_.code.push_back(0x0C);
    asm_.code.push_back(0x3C); asm_.code.push_back('\t');
    asm_.code.push_back(0x74); asm_.code.push_back(0x08);
    asm_.code.push_back(0x3C); asm_.code.push_back('\n');
    asm_.code.push_back(0x74); asm_.code.push_back(0x04);
    asm_.code.push_back(0x3C); asm_.code.push_back('\r');
    asm_.jnz_rel32(skipLeadDone);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
    asm_.jmp_rel32(skipLeadLoop);
    
    asm_.label(skipLeadDone);
    
    asm_.lea_rax_rbp(bufOffset);
    asm_.mov_rdx_rax();
    asm_.push_rdx();
    
    std::string copyLoop = newLabel("trim_copy");
    std::string copyDone = newLabel("trim_copy_done");
    
    asm_.label(copyLoop);
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01);
    asm_.test_rax_rax();
    asm_.jz_rel32(copyDone);
    asm_.code.push_back(0x88); asm_.code.push_back(0x02);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
    asm_.jmp_rel32(copyLoop);
    
    asm_.label(copyDone);
    asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00);
    
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xCA);
    asm_.pop_rcx();
    
    std::string trimTrailLoop = newLabel("trim_trail");
    std::string trimTrailDone = newLabel("trim_trail_done");
    
    asm_.label(trimTrailLoop);
    asm_.code.push_back(0x48); asm_.code.push_back(0x39); asm_.code.push_back(0xCA);
    asm_.jl_rel32(trimTrailDone);
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02);
    asm_.code.push_back(0x3C); asm_.code.push_back(' ');
    asm_.code.push_back(0x74); asm_.code.push_back(0x0C);
    asm_.code.push_back(0x3C); asm_.code.push_back('\t');
    asm_.code.push_back(0x74); asm_.code.push_back(0x08);
    asm_.code.push_back(0x3C); asm_.code.push_back('\n');
    asm_.code.push_back(0x74); asm_.code.push_back(0x04);
    asm_.code.push_back(0x3C); asm_.code.push_back('\r');
    asm_.jnz_rel32(trimTrailDone);
    asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xCA);
    asm_.jmp_rel32(trimTrailLoop);
    
    asm_.label(trimTrailDone);
    asm_.mov_rax_rcx();
}

void NativeCodeGen::emitStringStartsWith(CallExpr& node) {
    std::string strVal, prefix;
    if (tryEvalConstantString(node.args[0].get(), strVal) && 
        tryEvalConstantString(node.args[1].get(), prefix)) {
        bool result = strVal.size() >= prefix.size() && 
                      strVal.compare(0, prefix.size(), prefix) == 0;
        asm_.mov_rax_imm64(result ? 1 : 0);
        return;
    }
    
    node.args[0]->accept(*this);
    asm_.push_rax();
    node.args[1]->accept(*this);
    asm_.mov_rdx_rax();
    asm_.pop_rcx();
    
    std::string loopLabel = newLabel("starts_loop");
    std::string matchLabel = newLabel("starts_match");
    std::string noMatchLabel = newLabel("starts_nomatch");
    
    asm_.label(loopLabel);
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02);
    asm_.test_rax_rax();
    asm_.jz_rel32(matchLabel);
    
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x39);
    asm_.code.push_back(0x39); asm_.code.push_back(0xC7);
    asm_.jnz_rel32(noMatchLabel);
    
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
    asm_.jmp_rel32(loopLabel);
    
    asm_.label(matchLabel);
    asm_.mov_rax_imm64(1);
    std::string doneLabel = newLabel("starts_done");
    asm_.jmp_rel32(doneLabel);
    
    asm_.label(noMatchLabel);
    asm_.xor_rax_rax();
    
    asm_.label(doneLabel);
}

void NativeCodeGen::emitStringEndsWith(CallExpr& node) {
    std::string strVal, suffix;
    if (tryEvalConstantString(node.args[0].get(), strVal) && 
        tryEvalConstantString(node.args[1].get(), suffix)) {
        bool result = strVal.size() >= suffix.size() && 
                      strVal.compare(strVal.size() - suffix.size(), suffix.size(), suffix) == 0;
        asm_.mov_rax_imm64(result ? 1 : 0);
        return;
    }
    
    node.args[0]->accept(*this);
    asm_.push_rax();
    
    asm_.mov_rcx_rax();
    asm_.xor_rax_rax();
    std::string lenLoop1 = newLabel("ends_len1");
    std::string lenDone1 = newLabel("ends_len1_done");
    asm_.label(lenLoop1);
    asm_.code.push_back(0x80); asm_.code.push_back(0x39); asm_.code.push_back(0x00);
    asm_.jz_rel32(lenDone1);
    asm_.inc_rax();
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
    asm_.jmp_rel32(lenLoop1);
    asm_.label(lenDone1);
    asm_.push_rax();
    
    node.args[1]->accept(*this);
    asm_.push_rax();
    
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
    
    asm_.pop_rdx();
    asm_.pop_rcx();
    asm_.pop_rdi();
    
    std::string noMatchLabel = newLabel("ends_nomatch");
    asm_.code.push_back(0x48); asm_.code.push_back(0x39); asm_.code.push_back(0xC1);
    asm_.jl_rel32(noMatchLabel);
    
    asm_.code.push_back(0x48); asm_.code.push_back(0x29); asm_.code.push_back(0xC1);
    asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xCF);
    asm_.mov_rcx_rdi();
    
    std::string cmpLoop = newLabel("ends_cmp");
    std::string matchLabel = newLabel("ends_match");
    
    asm_.label(cmpLoop);
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x02);
    asm_.test_rax_rax();
    asm_.jz_rel32(matchLabel);
    
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x39);
    asm_.code.push_back(0x39); asm_.code.push_back(0xC7);
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
}

void NativeCodeGen::emitStringSubstring(CallExpr& node) {
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
    
    allocLocal("$substr_buf");
    int32_t bufOffset = locals["$substr_buf"];
    for (int i = 0; i < 63; i++) allocLocal("$substr_pad" + std::to_string(i));
    
    node.args[0]->accept(*this);
    asm_.push_rax();
    node.args[1]->accept(*this);
    asm_.mov_rcx_rax();
    
    if (hasLen) {
        node.args[2]->accept(*this);
        asm_.mov_r8_rax();
    } else {
        asm_.mov_rax_imm64(0x7FFFFFFF);
        asm_.mov_r8_rax();
    }
    
    asm_.pop_rax();
    asm_.code.push_back(0x48); asm_.code.push_back(0x01); asm_.code.push_back(0xC8);
    asm_.mov_rcx_rax();
    
    asm_.lea_rax_rbp(bufOffset);
    asm_.mov_rdx_rax();
    
    std::string copyLoop = newLabel("substr_copy");
    std::string copyDone = newLabel("substr_done");
    
    asm_.label(copyLoop);
    asm_.code.push_back(0x4D); asm_.code.push_back(0x85); asm_.code.push_back(0xC0);
    asm_.jz_rel32(copyDone);
    
    asm_.code.push_back(0x0F); asm_.code.push_back(0xB6); asm_.code.push_back(0x01);
    asm_.test_rax_rax();
    asm_.jz_rel32(copyDone);
    
    asm_.code.push_back(0x88); asm_.code.push_back(0x02);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC1);
    asm_.code.push_back(0x48); asm_.code.push_back(0xFF); asm_.code.push_back(0xC2);
    asm_.code.push_back(0x49); asm_.code.push_back(0xFF); asm_.code.push_back(0xC8);
    asm_.jmp_rel32(copyLoop);
    
    asm_.label(copyDone);
    asm_.code.push_back(0xC6); asm_.code.push_back(0x02); asm_.code.push_back(0x00);
    
    asm_.lea_rax_rbp(bufOffset);
}

void NativeCodeGen::emitStringReplace(CallExpr& node) {
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
    node.args[0]->accept(*this);
}

void NativeCodeGen::emitStringSplit(CallExpr& node) {
    std::string strVal, delim;
    if (tryEvalConstantString(node.args[0].get(), strVal) &&
        tryEvalConstantString(node.args[1].get(), delim)) {
        size_t pos = strVal.find(delim);
        if (pos != std::string::npos) {
            strVal = strVal.substr(0, pos);
        }
        uint32_t rva = addString(strVal);
        asm_.lea_rax_rip_fixup(rva);
        return;
    }
    node.args[0]->accept(*this);
}

void NativeCodeGen::emitStringJoin(CallExpr& node) {
    (void)node;
    uint32_t rva = addString("");
    asm_.lea_rax_rip_fixup(rva);
}

void NativeCodeGen::emitStringIndexOf(CallExpr& node) {
    std::string strVal, substr;
    if (tryEvalConstantString(node.args[0].get(), strVal) &&
        tryEvalConstantString(node.args[1].get(), substr)) {
        size_t pos = strVal.find(substr);
        int64_t result = (pos == std::string::npos) ? -1 : static_cast<int64_t>(pos);
        asm_.mov_rax_imm64(result);
        return;
    }
    
    // Runtime implementation would go here
    asm_.mov_rax_imm64(-1);
}

} // namespace flex
