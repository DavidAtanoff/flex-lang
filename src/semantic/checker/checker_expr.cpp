// Flex Compiler - Type Checker Expression Visitors
// Expression type checking

#include "checker_base.h"

namespace flex {

TypePtr TypeChecker::commonType(TypePtr a, TypePtr b) {
    auto& reg = TypeRegistry::instance();
    if (a->kind == TypeKind::UNKNOWN) return b;
    if (b->kind == TypeKind::UNKNOWN) return a;
    if (a->equals(b.get())) return a;
    if (a->isNumeric() && b->isNumeric()) {
        if (a->isFloat() || b->isFloat()) return reg.floatType();
        return reg.intType();
    }
    return reg.anyType();
}

bool TypeChecker::isAssignable(TypePtr target, TypePtr source) {
    if (target->kind == TypeKind::UNKNOWN || source->kind == TypeKind::UNKNOWN) return true;
    if (target->kind == TypeKind::ANY) return true;
    if (target->equals(source.get())) return true;
    if (target->isNumeric() && source->isNumeric()) {
        if (target->isFloat() && source->isInteger()) return true;
        if (target->size() >= source->size()) return true;
    }
    return false;
}

bool TypeChecker::isComparable(TypePtr a, TypePtr b) {
    if (a->kind == TypeKind::ANY || b->kind == TypeKind::ANY) return true;
    if (a->isNumeric() && b->isNumeric()) return true;
    if (a->kind == TypeKind::STRING && b->kind == TypeKind::STRING) return true;
    if (a->kind == TypeKind::BOOL && b->kind == TypeKind::BOOL) return true;
    return a->equals(b.get());
}

void TypeChecker::visit(IntegerLiteral&) { currentType_ = TypeRegistry::instance().intType(); }
void TypeChecker::visit(FloatLiteral&) { currentType_ = TypeRegistry::instance().floatType(); }
void TypeChecker::visit(StringLiteral&) { currentType_ = TypeRegistry::instance().stringType(); }
void TypeChecker::visit(BoolLiteral&) { currentType_ = TypeRegistry::instance().boolType(); }
void TypeChecker::visit(NilLiteral&) {
    auto nilType = TypeRegistry::instance().unknownType();
    nilType->isNullable = true;
    currentType_ = nilType;
}

void TypeChecker::visit(Identifier& node) {
    Symbol* sym = symbols_.lookup(node.name);
    if (!sym) {
        error("Undefined identifier '" + node.name + "'", node.location);
        currentType_ = TypeRegistry::instance().errorType();
        return;
    }
    currentType_ = sym->type;
}


void TypeChecker::visit(BinaryExpr& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr leftType = inferType(node.left.get());
    TypePtr rightType = inferType(node.right.get());
    
    // If either operand is ANY, allow the operation and return ANY
    if (leftType->kind == TypeKind::ANY || rightType->kind == TypeKind::ANY) {
        switch (node.op) {
            case TokenType::PLUS:
            case TokenType::MINUS:
            case TokenType::STAR:
            case TokenType::SLASH:
            case TokenType::PERCENT:
                currentType_ = reg.anyType();
                return;
            case TokenType::EQ: case TokenType::NE:
            case TokenType::LT: case TokenType::GT:
            case TokenType::LE: case TokenType::GE:
            case TokenType::AND: case TokenType::OR:
                currentType_ = reg.boolType();
                return;
            default:
                currentType_ = reg.anyType();
                return;
        }
    }
    
    switch (node.op) {
        case TokenType::PLUS:
            if (leftType->kind == TypeKind::STRING || rightType->kind == TypeKind::STRING) {
                currentType_ = reg.stringType();
            } else if (leftType->isNumeric() && rightType->isNumeric()) {
                currentType_ = commonType(leftType, rightType);
            } else {
                error("Invalid operands for '+'", node.location);
                currentType_ = reg.errorType();
            }
            break;
        case TokenType::MINUS: case TokenType::STAR: case TokenType::SLASH: case TokenType::PERCENT:
            if (leftType->isNumeric() && rightType->isNumeric()) {
                currentType_ = commonType(leftType, rightType);
            } else {
                error("Arithmetic operators require numeric operands", node.location);
                currentType_ = reg.errorType();
            }
            break;
        case TokenType::EQ: case TokenType::NE:
            if (!isComparable(leftType, rightType)) warning("Comparing incompatible types", node.location);
            currentType_ = reg.boolType();
            break;
        case TokenType::LT: case TokenType::GT: case TokenType::LE: case TokenType::GE:
            currentType_ = reg.boolType();
            break;
        case TokenType::AND: case TokenType::OR:
            currentType_ = reg.boolType();
            break;
        default:
            currentType_ = reg.unknownType();
            break;
    }
}

void TypeChecker::visit(UnaryExpr& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr operandType = inferType(node.operand.get());
    switch (node.op) {
        case TokenType::MINUS:
            currentType_ = operandType->isNumeric() ? operandType : reg.errorType();
            break;
        case TokenType::NOT: case TokenType::BANG:
            currentType_ = reg.boolType();
            break;
        default:
            currentType_ = reg.unknownType();
            break;
    }
}

void TypeChecker::visit(CallExpr& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr calleeType = inferType(node.callee.get());
    
    if (calleeType->kind == TypeKind::FUNCTION) {
        auto* fnType = static_cast<FunctionType*>(calleeType.get());
        
        // Handle generic function instantiation
        if (!fnType->typeParams.empty()) {
            // Try to infer type arguments from call arguments
            std::vector<TypePtr> inferredTypeArgs;
            std::unordered_map<std::string, TypePtr> typeArgMap;
            
            // Infer type arguments from argument types
            for (size_t i = 0; i < node.args.size() && i < fnType->params.size(); i++) {
                TypePtr argType = inferType(node.args[i].get());
                TypePtr paramType = fnType->params[i].second;
                
                // If param is a type parameter, bind it
                if (paramType->kind == TypeKind::TYPE_PARAM) {
                    auto* tp = static_cast<TypeParamType*>(paramType.get());
                    auto it = typeArgMap.find(tp->name);
                    if (it == typeArgMap.end()) {
                        typeArgMap[tp->name] = argType;
                    } else {
                        // Unify with existing binding
                        typeArgMap[tp->name] = unify(it->second, argType, node.location);
                    }
                }
            }
            
            // Build type args in order
            for (const auto& tpName : fnType->typeParams) {
                auto it = typeArgMap.find(tpName);
                if (it != typeArgMap.end()) {
                    inferredTypeArgs.push_back(it->second);
                } else {
                    inferredTypeArgs.push_back(reg.anyType());  // Couldn't infer
                }
            }
            
            // Instantiate the generic function
            TypePtr instantiated = instantiateGenericFunction(fnType, inferredTypeArgs, node.location);
            if (instantiated->kind == TypeKind::FUNCTION) {
                auto* instFn = static_cast<FunctionType*>(instantiated.get());
                currentType_ = instFn->returnType ? instFn->returnType : reg.voidType();
                return;
            }
        }
        
        // Non-generic function call
        for (size_t i = 0; i < node.args.size() && i < fnType->params.size(); i++) {
            TypePtr argType = inferType(node.args[i].get());
            TypePtr paramType = fnType->params[i].second;
            
            if (!isAssignable(paramType, argType)) {
                error("Argument type mismatch: expected '" + paramType->toString() + 
                      "', got '" + argType->toString() + "'", node.args[i]->location);
            }
        }
        currentType_ = fnType->returnType ? fnType->returnType : reg.voidType();
    } else if (calleeType->kind == TypeKind::ANY) {
        for (auto& arg : node.args) inferType(arg.get());
        currentType_ = reg.anyType();
    } else {
        currentType_ = reg.errorType();
    }
}

void TypeChecker::visit(MemberExpr& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr objType = inferType(node.object.get());
    if (objType->kind == TypeKind::RECORD) {
        auto* recType = static_cast<RecordType*>(objType.get());
        TypePtr fieldType = recType->getField(node.member);
        currentType_ = fieldType ? fieldType : reg.errorType();
    } else {
        currentType_ = reg.anyType();
    }
}

void TypeChecker::visit(IndexExpr& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr objType = inferType(node.object.get());
    inferType(node.index.get());
    if (objType->kind == TypeKind::LIST) {
        currentType_ = static_cast<ListType*>(objType.get())->element;
    } else if (objType->kind == TypeKind::STRING) {
        currentType_ = reg.stringType();
    } else {
        currentType_ = reg.anyType();
    }
}

void TypeChecker::visit(ListExpr& node) {
    auto& reg = TypeRegistry::instance();
    if (node.elements.empty()) {
        currentType_ = reg.listType(reg.unknownType());
        return;
    }
    TypePtr elemType = inferType(node.elements[0].get());
    for (size_t i = 1; i < node.elements.size(); i++) {
        elemType = commonType(elemType, inferType(node.elements[i].get()));
    }
    currentType_ = reg.listType(elemType);
}

void TypeChecker::visit(RecordExpr& node) {
    auto recType = std::make_shared<RecordType>();
    for (auto& field : node.fields) {
        TypePtr fieldType = inferType(field.second.get());
        recType->fields.push_back({field.first, fieldType, false});
    }
    currentType_ = recType;
}

void TypeChecker::visit(RangeExpr& node) {
    auto& reg = TypeRegistry::instance();
    inferType(node.start.get());
    inferType(node.end.get());
    if (node.step) inferType(node.step.get());
    currentType_ = reg.listType(reg.intType());
}

void TypeChecker::visit(LambdaExpr& node) {
    auto fnType = std::make_shared<FunctionType>();
    auto& reg = TypeRegistry::instance();
    symbols_.pushScope(Scope::Kind::FUNCTION);
    for (auto& p : node.params) {
        TypePtr ptype = parseTypeAnnotation(p.second);
        if (ptype->kind == TypeKind::UNKNOWN) ptype = reg.anyType();
        fnType->params.push_back(std::make_pair(p.first, ptype));
        Symbol sym(p.first, SymbolKind::PARAMETER, ptype);
        symbols_.define(sym);
    }
    TypePtr bodyType = inferType(node.body.get());
    fnType->returnType = bodyType;
    symbols_.popScope();
    currentType_ = fnType;
}

void TypeChecker::visit(TernaryExpr& node) {
    inferType(node.condition.get());
    TypePtr thenType = inferType(node.thenExpr.get());
    TypePtr elseType = inferType(node.elseExpr.get());
    currentType_ = commonType(thenType, elseType);
}

void TypeChecker::visit(ListCompExpr& node) {
    auto& reg = TypeRegistry::instance();
    symbols_.pushScope(Scope::Kind::BLOCK);
    TypePtr iterType = inferType(node.iterable.get());
    TypePtr elemType = reg.anyType();
    if (iterType->kind == TypeKind::LIST) {
        elemType = static_cast<ListType*>(iterType.get())->element;
    }
    Symbol varSym(node.var, SymbolKind::VARIABLE, elemType);
    symbols_.define(varSym);
    if (node.condition) inferType(node.condition.get());
    TypePtr exprType = inferType(node.expr.get());
    symbols_.popScope();
    currentType_ = reg.listType(exprType);
}

void TypeChecker::visit(AddressOfExpr& node) {
    auto& reg = TypeRegistry::instance();
    if (!symbols_.inUnsafe()) error("Address-of operator requires unsafe block", node.location);
    TypePtr operandType = inferType(node.operand.get());
    currentType_ = reg.ptrType(operandType, true);
}

void TypeChecker::visit(DerefExpr& node) {
    auto& reg = TypeRegistry::instance();
    if (!symbols_.inUnsafe()) error("Dereference operator requires unsafe block", node.location);
    TypePtr operandType = inferType(node.operand.get());
    if (operandType->isPointer()) {
        currentType_ = static_cast<PtrType*>(operandType.get())->pointee;
    } else {
        error("Cannot dereference non-pointer type", node.location);
        currentType_ = reg.errorType();
    }
}

void TypeChecker::visit(NewExpr& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr allocType = symbols_.lookupType(node.typeName);
    if (!allocType) allocType = reg.fromString(node.typeName);
    for (auto& arg : node.args) inferType(arg.get());
    currentType_ = reg.ptrType(allocType, symbols_.inUnsafe());
}

void TypeChecker::visit(CastExpr& node) {
    inferType(node.expr.get());
    TypePtr targetType = parseTypeAnnotation(node.targetType);
    currentType_ = targetType;
}

void TypeChecker::visit(InterpolatedString& node) {
    auto& reg = TypeRegistry::instance();
    for (auto& part : node.parts) {
        if (auto* exprPtr = std::get_if<ExprPtr>(&part)) {
            inferType(exprPtr->get());
        }
    }
    currentType_ = reg.stringType();
}

void TypeChecker::visit(AwaitExpr& node) {
    TypePtr operandType = inferType(node.operand.get());
    // For now, await just returns the operand type
    currentType_ = operandType;
}

void TypeChecker::visit(SpawnExpr& node) {
    TypePtr operandType = inferType(node.operand.get());
    // Spawn returns a future/task type - for now use any
    currentType_ = TypeRegistry::instance().anyType();
}

void TypeChecker::visit(DSLBlock& node) {
    (void)node;
    // DSL blocks are opaque - return string type
    currentType_ = TypeRegistry::instance().stringType();
}

void TypeChecker::visit(AssignExpr& node) {
    auto& reg = TypeRegistry::instance();
    
    // In Flex, x = value can be a variable declaration if x is not yet defined
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        Symbol* existing = symbols_.lookup(id->name);
        if (!existing) {
            // This is a new variable declaration via assignment
            TypePtr valueType = inferType(node.value.get());
            Symbol sym(id->name, SymbolKind::VARIABLE, valueType);
            sym.isInitialized = true;
            sym.isMutable = true;  // Variables declared via = are mutable by default
            symbols_.define(sym);
            currentType_ = valueType;
            return;
        }
    }
    
    // Existing variable assignment
    TypePtr targetType = inferType(node.target.get());
    TypePtr valueType = inferType(node.value.get());
    
    if (!isAssignable(targetType, valueType)) {
        error("Type mismatch in assignment: cannot assign '" + valueType->toString() + 
              "' to '" + targetType->toString() + "'", node.location);
    }
    currentType_ = targetType;
}

} // namespace flex
