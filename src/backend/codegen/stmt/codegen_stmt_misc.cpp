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

// Helper to parse a register name and return its encoding
static int parseRegister(const std::string& reg) {
    if (reg == "rax" || reg == "eax" || reg == "ax" || reg == "al") return 0;
    if (reg == "rcx" || reg == "ecx" || reg == "cx" || reg == "cl") return 1;
    if (reg == "rdx" || reg == "edx" || reg == "dx" || reg == "dl") return 2;
    if (reg == "rbx" || reg == "ebx" || reg == "bx" || reg == "bl") return 3;
    if (reg == "rsp" || reg == "esp" || reg == "sp" || reg == "spl") return 4;
    if (reg == "rbp" || reg == "ebp" || reg == "bp" || reg == "bpl") return 5;
    if (reg == "rsi" || reg == "esi" || reg == "si" || reg == "sil") return 6;
    if (reg == "rdi" || reg == "edi" || reg == "di" || reg == "dil") return 7;
    if (reg == "r8" || reg == "r8d" || reg == "r8w" || reg == "r8b") return 8;
    if (reg == "r9" || reg == "r9d" || reg == "r9w" || reg == "r9b") return 9;
    if (reg == "r10" || reg == "r10d" || reg == "r10w" || reg == "r10b") return 10;
    if (reg == "r11" || reg == "r11d" || reg == "r11w" || reg == "r11b") return 11;
    if (reg == "r12" || reg == "r12d" || reg == "r12w" || reg == "r12b") return 12;
    if (reg == "r13" || reg == "r13d" || reg == "r13w" || reg == "r13b") return 13;
    if (reg == "r14" || reg == "r14d" || reg == "r14w" || reg == "r14b") return 14;
    if (reg == "r15" || reg == "r15d" || reg == "r15w" || reg == "r15b") return 15;
    return -1;
}

static bool is64BitReg(const std::string& reg) {
    return reg[0] == 'r' && reg != "rsp" && reg != "rbp" && reg != "rsi" && reg != "rdi" && 
           (reg.size() == 3 || (reg.size() > 1 && reg[1] >= '0' && reg[1] <= '9'));
}

static bool isExtReg(const std::string& reg) {
    return reg.size() >= 2 && reg[0] == 'r' && reg[1] >= '8' && reg[1] <= '9';
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            if (!current.empty()) result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) result.push_back(trim(current));
    return result;
}

void NativeCodeGen::visit(AsmStmt& node) {
    // Parse and emit inline assembly
    // Split by newlines and process each instruction
    std::vector<std::string> lines = split(node.code, '\n');
    
    for (const std::string& line : lines) {
        std::string instr = trim(line);
        if (instr.empty() || instr[0] == ';') continue;  // Skip empty lines and comments
        
        // Convert to lowercase for comparison
        std::string instrLower;
        for (char c : instr) instrLower += (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        
        // Parse instruction and operands
        size_t spacePos = instrLower.find(' ');
        std::string mnemonic = (spacePos != std::string::npos) ? instrLower.substr(0, spacePos) : instrLower;
        std::string operands = (spacePos != std::string::npos) ? trim(instrLower.substr(spacePos + 1)) : "";
        
        // Handle common instructions
        if (mnemonic == "ret") {
            asm_.ret();
        }
        else if (mnemonic == "nop") {
            asm_.code.push_back(0x90);
        }
        else if (mnemonic == "push") {
            int reg = parseRegister(operands);
            if (reg >= 0) {
                if (reg >= 8) {
                    asm_.code.push_back(0x41);
                    asm_.code.push_back(0x50 + (reg - 8));
                } else {
                    asm_.code.push_back(0x50 + reg);
                }
            }
        }
        else if (mnemonic == "pop") {
            int reg = parseRegister(operands);
            if (reg >= 0) {
                if (reg >= 8) {
                    asm_.code.push_back(0x41);
                    asm_.code.push_back(0x58 + (reg - 8));
                } else {
                    asm_.code.push_back(0x58 + reg);
                }
            }
        }
        else if (mnemonic == "mov") {
            auto parts = split(operands, ',');
            if (parts.size() == 2) {
                int dst = parseRegister(parts[0]);
                int src = parseRegister(parts[1]);
                
                if (dst >= 0 && src >= 0) {
                    // mov reg, reg
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;  // REX.B
                    if (src >= 8) rex |= 0x04;  // REX.R
                    asm_.code.push_back(rex);
                    asm_.code.push_back(0x89);
                    asm_.code.push_back(0xC0 | ((src & 7) << 3) | (dst & 7));
                }
                else if (dst >= 0) {
                    // mov reg, imm
                    int64_t imm = 0;
                    try {
                        if (parts[1].size() > 2 && parts[1][0] == '0' && parts[1][1] == 'x') {
                            imm = std::stoll(parts[1], nullptr, 16);
                        } else {
                            imm = std::stoll(parts[1]);
                        }
                    } catch (...) {
                        continue;  // Skip invalid immediate
                    }
                    
                    // mov r64, imm64
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;
                    asm_.code.push_back(rex);
                    asm_.code.push_back(0xB8 + (dst & 7));
                    for (int i = 0; i < 8; i++) {
                        asm_.code.push_back((imm >> (i * 8)) & 0xFF);
                    }
                }
            }
        }
        else if (mnemonic == "xor") {
            auto parts = split(operands, ',');
            if (parts.size() == 2) {
                int dst = parseRegister(parts[0]);
                int src = parseRegister(parts[1]);
                if (dst >= 0 && src >= 0) {
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;
                    if (src >= 8) rex |= 0x04;
                    asm_.code.push_back(rex);
                    asm_.code.push_back(0x31);
                    asm_.code.push_back(0xC0 | ((src & 7) << 3) | (dst & 7));
                }
            }
        }
        else if (mnemonic == "add") {
            auto parts = split(operands, ',');
            if (parts.size() == 2) {
                int dst = parseRegister(parts[0]);
                int src = parseRegister(parts[1]);
                if (dst >= 0 && src >= 0) {
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;
                    if (src >= 8) rex |= 0x04;
                    asm_.code.push_back(rex);
                    asm_.code.push_back(0x01);
                    asm_.code.push_back(0xC0 | ((src & 7) << 3) | (dst & 7));
                }
                else if (dst >= 0) {
                    // add reg, imm
                    int64_t imm = 0;
                    try {
                        if (parts[1].size() > 2 && parts[1][0] == '0' && parts[1][1] == 'x') {
                            imm = std::stoll(parts[1], nullptr, 16);
                        } else {
                            imm = std::stoll(parts[1]);
                        }
                    } catch (...) { continue; }
                    
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;
                    asm_.code.push_back(rex);
                    if (imm >= -128 && imm <= 127) {
                        asm_.code.push_back(0x83);
                        asm_.code.push_back(0xC0 | (dst & 7));
                        asm_.code.push_back((int8_t)imm);
                    } else {
                        asm_.code.push_back(0x81);
                        asm_.code.push_back(0xC0 | (dst & 7));
                        for (int i = 0; i < 4; i++) {
                            asm_.code.push_back((imm >> (i * 8)) & 0xFF);
                        }
                    }
                }
            }
        }
        else if (mnemonic == "sub") {
            auto parts = split(operands, ',');
            if (parts.size() == 2) {
                int dst = parseRegister(parts[0]);
                int src = parseRegister(parts[1]);
                if (dst >= 0 && src >= 0) {
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;
                    if (src >= 8) rex |= 0x04;
                    asm_.code.push_back(rex);
                    asm_.code.push_back(0x29);
                    asm_.code.push_back(0xC0 | ((src & 7) << 3) | (dst & 7));
                }
                else if (dst >= 0) {
                    int64_t imm = 0;
                    try {
                        if (parts[1].size() > 2 && parts[1][0] == '0' && parts[1][1] == 'x') {
                            imm = std::stoll(parts[1], nullptr, 16);
                        } else {
                            imm = std::stoll(parts[1]);
                        }
                    } catch (...) { continue; }
                    
                    uint8_t rex = 0x48;
                    if (dst >= 8) rex |= 0x01;
                    asm_.code.push_back(rex);
                    if (imm >= -128 && imm <= 127) {
                        asm_.code.push_back(0x83);
                        asm_.code.push_back(0xE8 | (dst & 7));
                        asm_.code.push_back((int8_t)imm);
                    } else {
                        asm_.code.push_back(0x81);
                        asm_.code.push_back(0xE8 | (dst & 7));
                        for (int i = 0; i < 4; i++) {
                            asm_.code.push_back((imm >> (i * 8)) & 0xFF);
                        }
                    }
                }
            }
        }
        else if (mnemonic == "inc") {
            int reg = parseRegister(operands);
            if (reg >= 0) {
                uint8_t rex = 0x48;
                if (reg >= 8) rex |= 0x01;
                asm_.code.push_back(rex);
                asm_.code.push_back(0xFF);
                asm_.code.push_back(0xC0 | (reg & 7));
            }
        }
        else if (mnemonic == "dec") {
            int reg = parseRegister(operands);
            if (reg >= 0) {
                uint8_t rex = 0x48;
                if (reg >= 8) rex |= 0x01;
                asm_.code.push_back(rex);
                asm_.code.push_back(0xFF);
                asm_.code.push_back(0xC8 | (reg & 7));
            }
        }
        else if (mnemonic == "imul") {
            auto parts = split(operands, ',');
            if (parts.size() == 2) {
                int dst = parseRegister(parts[0]);
                int src = parseRegister(parts[1]);
                if (dst >= 0 && src >= 0) {
                    uint8_t rex = 0x48;
                    if (src >= 8) rex |= 0x01;
                    if (dst >= 8) rex |= 0x04;
                    asm_.code.push_back(rex);
                    asm_.code.push_back(0x0F);
                    asm_.code.push_back(0xAF);
                    asm_.code.push_back(0xC0 | ((dst & 7) << 3) | (src & 7));
                }
            }
        }
        else if (mnemonic == "syscall") {
            asm_.code.push_back(0x0F);
            asm_.code.push_back(0x05);
        }
        else if (mnemonic == "int3") {
            asm_.code.push_back(0xCC);
        }
        // Add more instructions as needed
    }
}

} // namespace flex
