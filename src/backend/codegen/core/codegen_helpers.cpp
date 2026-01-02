// Flex Compiler - Native Code Generator Helpers
// Constant evaluation, stack calculations, and utility functions

#include "backend/codegen/codegen_base.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace flex {

std::string NativeCodeGen::newLabel(const std::string& prefix) {
    return prefix + std::to_string(labelCounter++);
}

uint32_t NativeCodeGen::addString(const std::string& str) {
    auto it = stringOffsets.find(str);
    if (it != stringOffsets.end()) return it->second;
    
    uint32_t rva = pe_.addString(str);
    stringOffsets[str] = rva;
    return rva;
}

uint32_t NativeCodeGen::addFloatConstant(double value) {
    // Store float as 8 bytes in data section
    union { double d; uint8_t bytes[8]; } u;
    u.d = value;
    return pe_.addData(u.bytes, 8);
}

void NativeCodeGen::allocLocal(const std::string& name) {
    stackOffset -= 8;
    locals[name] = stackOffset;
}

// Calculate the maximum stack space needed for a function body
int32_t NativeCodeGen::calculateFunctionStackSize(Statement* body) {
    if (!body) return 0;
    
    int32_t maxStack = 0;
    
    std::function<void(Statement*)> scanStmt = [&](Statement* stmt) {
        if (!stmt) return;
        
        if (auto* block = dynamic_cast<Block*>(stmt)) {
            for (auto& s : block->statements) {
                scanStmt(s.get());
            }
        }
        else if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(exprStmt->expr.get()));
        }
        else if (auto* varDecl = dynamic_cast<VarDecl*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(varDecl->initializer.get()));
        }
        else if (auto* assignStmt = dynamic_cast<AssignStmt*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(assignStmt->value.get()));
        }
        else if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(ifStmt->condition.get()));
            scanStmt(ifStmt->thenBranch.get());
            for (auto& elif : ifStmt->elifBranches) {
                maxStack = std::max(maxStack, calculateExprStackSize(elif.first.get()));
                scanStmt(elif.second.get());
            }
            scanStmt(ifStmt->elseBranch.get());
        }
        else if (auto* whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(whileStmt->condition.get()));
            scanStmt(whileStmt->body.get());
        }
        else if (auto* forStmt = dynamic_cast<ForStmt*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(forStmt->iterable.get()));
            scanStmt(forStmt->body.get());
        }
        else if (auto* returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
            maxStack = std::max(maxStack, calculateExprStackSize(returnStmt->value.get()));
        }
    };
    
    scanStmt(body);
    return maxStack;
}

// Calculate stack space needed for an expression (mainly for calls)
int32_t NativeCodeGen::calculateExprStackSize(Expression* expr) {
    if (!expr) return 0;
    
    int32_t maxStack = 0;
    
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
            if (id->name == "print" || id->name == "println") {
                maxStack = std::max(maxStack, (int32_t)0x38);
            } else if (id->name == "hostname" || id->name == "username" || 
                       id->name == "cpu_count" || id->name == "year" ||
                       id->name == "month" || id->name == "day" ||
                       id->name == "hour" || id->name == "minute" ||
                       id->name == "second" || id->name == "now" ||
                       id->name == "now_ms" || id->name == "sleep") {
                maxStack = std::max(maxStack, (int32_t)0x28);
            } else {
                maxStack = std::max(maxStack, (int32_t)0x20);
            }
        } else {
            maxStack = std::max(maxStack, (int32_t)0x20);
        }
        
        for (auto& arg : call->args) {
            maxStack = std::max(maxStack, calculateExprStackSize(arg.get()));
        }
    }
    else if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        maxStack = std::max(maxStack, calculateExprStackSize(binary->left.get()));
        maxStack = std::max(maxStack, calculateExprStackSize(binary->right.get()));
    }
    else if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        maxStack = std::max(maxStack, calculateExprStackSize(unary->operand.get()));
    }
    else if (auto* ternary = dynamic_cast<TernaryExpr*>(expr)) {
        maxStack = std::max(maxStack, calculateExprStackSize(ternary->condition.get()));
        maxStack = std::max(maxStack, calculateExprStackSize(ternary->thenExpr.get()));
        maxStack = std::max(maxStack, calculateExprStackSize(ternary->elseExpr.get()));
    }
    
    return maxStack;
}

void NativeCodeGen::emitCallWithOptimizedStack(uint32_t importRVA) {
    asm_.call_mem_rip(importRVA);
}

void NativeCodeGen::emitCallRelWithOptimizedStack(const std::string& label) {
    asm_.call_rel32(label);
}

// Check if a statement ends with a terminator (return, break, continue)
bool NativeCodeGen::endsWithTerminator(Statement* stmt) {
    if (!stmt) return false;
    
    if (dynamic_cast<ReturnStmt*>(stmt)) return true;
    if (dynamic_cast<BreakStmt*>(stmt)) return true;
    if (dynamic_cast<ContinueStmt*>(stmt)) return true;
    
    if (auto* block = dynamic_cast<Block*>(stmt)) {
        if (block->statements.empty()) return false;
        return endsWithTerminator(block->statements.back().get());
    }
    
    if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
        if (!ifStmt->elseBranch) return false;
        if (!endsWithTerminator(ifStmt->thenBranch.get())) return false;
        for (auto& elif : ifStmt->elifBranches) {
            if (!endsWithTerminator(elif.second.get())) return false;
        }
        return endsWithTerminator(ifStmt->elseBranch.get());
    }
    
    return false;
}

bool NativeCodeGen::tryEvalConstant(Expression* expr, int64_t& outValue) {
    if (auto* intLit = dynamic_cast<IntegerLiteral*>(expr)) {
        outValue = intLit->value;
        return true;
    }
    if (auto* boolLit = dynamic_cast<BoolLiteral*>(expr)) {
        outValue = boolLit->value ? 1 : 0;
        return true;
    }
    if (auto* ident = dynamic_cast<Identifier*>(expr)) {
        auto it = constVars.find(ident->name);
        if (it != constVars.end()) {
            outValue = it->second;
            return true;
        }
        return false;
    }
    // Handle constant list indexing with constant index (1-based indexing)
    if (auto* indexExpr = dynamic_cast<IndexExpr*>(expr)) {
        if (auto* ident = dynamic_cast<Identifier*>(indexExpr->object.get())) {
            auto constListIt = constListVars.find(ident->name);
            if (constListIt != constListVars.end()) {
                int64_t indexVal;
                if (tryEvalConstant(indexExpr->index.get(), indexVal)) {
                    // Convert 1-based index to 0-based for internal access
                    int64_t zeroBasedIndex = indexVal - 1;
                    if (zeroBasedIndex >= 0 && (size_t)zeroBasedIndex < constListIt->second.size()) {
                        outValue = constListIt->second[zeroBasedIndex];
                        return true;
                    }
                }
            }
        }
        return false;
    }
    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        int64_t left, right;
        if (tryEvalConstant(binary->left.get(), left) && tryEvalConstant(binary->right.get(), right)) {
            switch (binary->op) {
                case TokenType::PLUS: outValue = left + right; return true;
                case TokenType::MINUS: outValue = left - right; return true;
                case TokenType::STAR: outValue = left * right; return true;
                case TokenType::SLASH: if (right != 0) { outValue = left / right; return true; } break;
                case TokenType::PERCENT: if (right != 0) { outValue = left % right; return true; } break;
                case TokenType::LT: outValue = left < right ? 1 : 0; return true;
                case TokenType::GT: outValue = left > right ? 1 : 0; return true;
                case TokenType::LE: outValue = left <= right ? 1 : 0; return true;
                case TokenType::GE: outValue = left >= right ? 1 : 0; return true;
                case TokenType::EQ: outValue = left == right ? 1 : 0; return true;
                case TokenType::NE: outValue = left != right ? 1 : 0; return true;
                default: break;
            }
        }
    }
    if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        int64_t val;
        if (tryEvalConstant(unary->operand.get(), val)) {
            switch (unary->op) {
                case TokenType::MINUS: outValue = -val; return true;
                case TokenType::NOT: outValue = !val ? 1 : 0; return true;
                default: break;
            }
        }
    }
    // Handle int() conversion at compile time
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
            if (id->name == "int" && call->args.size() == 1) {
                // Try to evaluate argument as int first
                int64_t intVal;
                if (tryEvalConstant(call->args[0].get(), intVal)) {
                    outValue = intVal;
                    return true;
                }
                // Try to evaluate argument as string and parse
                std::string strVal;
                if (tryEvalConstantString(call->args[0].get(), strVal)) {
                    int64_t result = 0;
                    bool negative = false;
                    size_t i = 0;
                    while (i < strVal.size() && (strVal[i] == ' ' || strVal[i] == '\t')) i++;
                    if (i < strVal.size() && strVal[i] == '-') { negative = true; i++; }
                    else if (i < strVal.size() && strVal[i] == '+') { i++; }
                    while (i < strVal.size() && strVal[i] >= '0' && strVal[i] <= '9') {
                        result = result * 10 + (strVal[i] - '0');
                        i++;
                    }
                    if (negative) result = -result;
                    outValue = result;
                    return true;
                }
                // Try to evaluate argument as float and truncate
                double floatVal;
                if (tryEvalConstantFloat(call->args[0].get(), floatVal)) {
                    outValue = (int64_t)floatVal;
                    return true;
                }
            }
            if (id->name == "bool" && call->args.size() == 1) {
                int64_t intVal;
                if (tryEvalConstant(call->args[0].get(), intVal)) {
                    outValue = intVal != 0 ? 1 : 0;
                    return true;
                }
                std::string strVal;
                if (tryEvalConstantString(call->args[0].get(), strVal)) {
                    bool result = !strVal.empty() && strVal != "0" && strVal != "false" && strVal != "False" && strVal != "FALSE";
                    outValue = result ? 1 : 0;
                    return true;
                }
            }
            // Handle sizeof(T) at compile time
            if (id->name == "sizeof" && call->args.size() == 1) {
                if (auto* typeIdent = dynamic_cast<Identifier*>(call->args[0].get())) {
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
                                size = recType->fields.size() * 8;
                            } else {
                                size = type->size();
                            }
                        }
                    }
                    outValue = size;
                    return true;
                }
            }
            // Handle alignof(T) at compile time
            if (id->name == "alignof" && call->args.size() == 1) {
                if (auto* typeIdent = dynamic_cast<Identifier*>(call->args[0].get())) {
                    std::string typeName = typeIdent->name;
                    int64_t alignment = 8;  // Default alignment
                    
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
                        alignment = 1;
                    } else if (typeName == "str" || typeName == "string") {
                        alignment = 8;
                    } else {
                        auto& reg = TypeRegistry::instance();
                        TypePtr type = reg.lookupType(typeName);
                        if (type) {
                            alignment = type->alignment();
                        }
                    }
                    outValue = alignment;
                    return true;
                }
            }
            // Handle offsetof(Record, field) at compile time
            if (id->name == "offsetof" && call->args.size() == 2) {
                if (auto* recordIdent = dynamic_cast<Identifier*>(call->args[0].get())) {
                    if (auto* fieldIdent = dynamic_cast<Identifier*>(call->args[1].get())) {
                        std::string recordName = recordIdent->name;
                        std::string fieldName = fieldIdent->name;
                        int64_t offset = 0;
                        
                        auto& reg = TypeRegistry::instance();
                        TypePtr type = reg.lookupType(recordName);
                        if (type) {
                            if (auto* recType = dynamic_cast<RecordType*>(type.get())) {
                                for (size_t i = 0; i < recType->fields.size(); i++) {
                                    if (recType->fields[i].name == fieldName) {
                                        offset = i * 8;
                                        break;
                                    }
                                }
                            }
                        }
                        outValue = offset;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool NativeCodeGen::tryEvalConstantFloat(Expression* expr, double& outValue) {
    if (auto* floatLit = dynamic_cast<FloatLiteral*>(expr)) {
        outValue = floatLit->value;
        return true;
    }
    if (auto* intLit = dynamic_cast<IntegerLiteral*>(expr)) {
        outValue = static_cast<double>(intLit->value);
        return true;
    }
    if (auto* ident = dynamic_cast<Identifier*>(expr)) {
        auto it = constFloatVars.find(ident->name);
        if (it != constFloatVars.end()) {
            outValue = it->second;
            return true;
        }
        auto intIt = constVars.find(ident->name);
        if (intIt != constVars.end()) {
            outValue = static_cast<double>(intIt->second);
            return true;
        }
        return false;
    }
    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        double left, right;
        if (tryEvalConstantFloat(binary->left.get(), left) && tryEvalConstantFloat(binary->right.get(), right)) {
            switch (binary->op) {
                case TokenType::PLUS: outValue = left + right; return true;
                case TokenType::MINUS: outValue = left - right; return true;
                case TokenType::STAR: outValue = left * right; return true;
                case TokenType::SLASH: if (right != 0.0) { outValue = left / right; return true; } break;
                default: break;
            }
        }
    }
    if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        double val;
        if (tryEvalConstantFloat(unary->operand.get(), val)) {
            switch (unary->op) {
                case TokenType::MINUS: outValue = -val; return true;
                default: break;
            }
        }
    }
    return false;
}

bool NativeCodeGen::isFloatExpression(Expression* expr) {
    if (dynamic_cast<FloatLiteral*>(expr)) return true;
    
    if (auto* ident = dynamic_cast<Identifier*>(expr)) {
        if (floatVars.count(ident->name)) return true;
        if (constFloatVars.count(ident->name)) return true;
    }
    
    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        return isFloatExpression(binary->left.get()) || isFloatExpression(binary->right.get());
    }
    
    if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        return isFloatExpression(unary->operand.get());
    }
    
    if (auto* ternary = dynamic_cast<TernaryExpr*>(expr)) {
        return isFloatExpression(ternary->thenExpr.get()) || isFloatExpression(ternary->elseExpr.get());
    }
    
    // Check for float() conversion function
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
            if (id->name == "float") return true;
            
            // Check if this is a generic function call that returns float
            auto it = genericFunctions_.find(id->name);
            if (it != genericFunctions_.end() && !call->args.empty()) {
                // Infer type arguments to determine return type
                FnDecl* genericFn = it->second;
                auto& reg = TypeRegistry::instance();
                
                std::unordered_map<std::string, TypePtr> inferred;
                for (size_t i = 0; i < call->args.size() && i < genericFn->params.size(); i++) {
                    const std::string& paramType = genericFn->params[i].second;
                    for (const auto& tp : genericFn->typeParams) {
                        if (paramType == tp) {
                            TypePtr argType = reg.anyType();
                            // Check for float literal first
                            if (dynamic_cast<FloatLiteral*>(call->args[i].get())) {
                                argType = reg.floatType();
                            } else if (isFloatExpression(call->args[i].get())) {
                                argType = reg.floatType();
                            } else if (auto* argId = dynamic_cast<Identifier*>(call->args[i].get())) {
                                if (floatVars.count(argId->name) || constFloatVars.count(argId->name)) {
                                    argType = reg.floatType();
                                }
                            }
                            if (inferred.find(tp) == inferred.end()) {
                                inferred[tp] = argType;
                            }
                            break;
                        }
                    }
                }
                
                // Build type args and check return type
                std::vector<TypePtr> typeArgs;
                for (const auto& tp : genericFn->typeParams) {
                    auto typeIt = inferred.find(tp);
                    if (typeIt != inferred.end()) {
                        typeArgs.push_back(typeIt->second);
                    } else {
                        typeArgs.push_back(reg.anyType());
                    }
                }
                
                std::string mangledName = monomorphizer_.getMangledName(id->name, typeArgs);
                if (monomorphizer_.functionReturnsFloat(mangledName)) {
                    return true;
                }
                
                // Also check by substituting return type directly
                // This handles cases where the monomorphizer hasn't recorded the instantiation yet
                std::string returnType = genericFn->returnType;
                for (size_t i = 0; i < genericFn->typeParams.size() && i < typeArgs.size(); i++) {
                    if (returnType == genericFn->typeParams[i]) {
                        if (typeArgs[i]->toString() == "float") {
                            return true;
                        }
                    }
                }
            }
        }
    }
    
    return false;
}

bool NativeCodeGen::isStringReturningExpr(Expression* expr) {
    if (!expr) return false;
    
    if (dynamic_cast<StringLiteral*>(expr)) return true;
    if (dynamic_cast<InterpolatedString*>(expr)) return true;
    
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
            if (id->name == "platform" || id->name == "arch" ||
                id->name == "upper" || id->name == "lower" ||
                id->name == "trim" || id->name == "substring" ||
                id->name == "replace" || id->name == "split" ||
                id->name == "join" || id->name == "hostname" ||
                id->name == "username" || id->name == "str" ||
                id->name == "read") {
                return true;
            }
            
            // Check if this is a generic function call with string argument
            auto it = genericFunctions_.find(id->name);
            if (it != genericFunctions_.end() && !call->args.empty()) {
                // Check if first argument is a string
                if (isStringReturningExpr(call->args[0].get())) {
                    return true;
                }
                // Also check for string variable
                if (auto* argId = dynamic_cast<Identifier*>(call->args[0].get())) {
                    if (constStrVars.count(argId->name)) {
                        return true;
                    }
                }
            }
        }
    }
    
    if (auto* ternary = dynamic_cast<TernaryExpr*>(expr)) {
        return isStringReturningExpr(ternary->thenExpr.get()) || 
               isStringReturningExpr(ternary->elseExpr.get());
    }
    
    if (auto* ident = dynamic_cast<Identifier*>(expr)) {
        if (constStrVars.count(ident->name)) return true;
    }
    
    return false;
}

bool NativeCodeGen::tryEvalConstantString(Expression* expr, std::string& outValue) {
    if (auto* strLit = dynamic_cast<StringLiteral*>(expr)) {
        outValue = strLit->value;
        return true;
    }
    if (auto* interp = dynamic_cast<InterpolatedString*>(expr)) {
        std::string result;
        for (auto& part : interp->parts) {
            if (auto* str = std::get_if<std::string>(&part)) {
                result += *str;
            } else if (auto* exprPtr = std::get_if<ExprPtr>(&part)) {
                std::string strVal;
                int64_t intVal;
                if (tryEvalConstantString(exprPtr->get(), strVal)) {
                    result += strVal;
                } else if (tryEvalConstant(exprPtr->get(), intVal)) {
                    result += std::to_string(intVal);
                } else {
                    return false;
                }
            }
        }
        outValue = result;
        return true;
    }
    if (auto* ident = dynamic_cast<Identifier*>(expr)) {
        auto it = constStrVars.find(ident->name);
        if (it != constStrVars.end() && !it->second.empty()) {
            outValue = it->second;
            return true;
        }
        return false;
    }
    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        if (binary->op == TokenType::PLUS) {
            std::string left, right;
            if (tryEvalConstantString(binary->left.get(), left) && tryEvalConstantString(binary->right.get(), right)) {
                outValue = left + right;
                return true;
            }
            int64_t intVal;
            if (dynamic_cast<StringLiteral*>(binary->left.get()) || 
                (dynamic_cast<Identifier*>(binary->left.get()) && 
                 constStrVars.count(dynamic_cast<Identifier*>(binary->left.get())->name))) {
                if (tryEvalConstantString(binary->left.get(), left) && tryEvalConstant(binary->right.get(), intVal)) {
                    outValue = left + std::to_string(intVal);
                    return true;
                }
            }
            if (dynamic_cast<StringLiteral*>(binary->right.get()) || 
                (dynamic_cast<Identifier*>(binary->right.get()) && 
                 constStrVars.count(dynamic_cast<Identifier*>(binary->right.get())->name))) {
                if (tryEvalConstant(binary->left.get(), intVal) && tryEvalConstantString(binary->right.get(), right)) {
                    outValue = std::to_string(intVal) + right;
                    return true;
                }
            }
        }
    }
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        if (auto* id = dynamic_cast<Identifier*>(call->callee.get())) {
            if (id->name == "str" && call->args.size() == 1) {
                int64_t intVal;
                if (tryEvalConstant(call->args[0].get(), intVal)) {
                    outValue = std::to_string(intVal);
                    return true;
                }
                std::string strVal;
                if (tryEvalConstantString(call->args[0].get(), strVal)) {
                    outValue = strVal;
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace flex
