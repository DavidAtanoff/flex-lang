// Flex Compiler - Type Checker Core
// Core methods, type utilities, diagnostics

#include "checker_base.h"
#include <regex>

namespace flex {

TypeChecker::TypeChecker() : currentType_(nullptr), expectedReturn_(nullptr) {
    // Register built-in functions
    registerBuiltins();
}

void TypeChecker::registerBuiltins() {
    auto& reg = TypeRegistry::instance();
    
    // print/println - variadic, returns void
    auto printFn = std::make_shared<FunctionType>();
    printFn->isVariadic = true;
    printFn->returnType = reg.voidType();
    symbols_.define(Symbol("print", SymbolKind::FUNCTION, printFn));
    symbols_.define(Symbol("println", SymbolKind::FUNCTION, printFn));
    
    // len(x) -> int
    auto lenFn = std::make_shared<FunctionType>();
    lenFn->params.push_back({"x", reg.anyType()});
    lenFn->returnType = reg.intType();
    symbols_.define(Symbol("len", SymbolKind::FUNCTION, lenFn));
    
    // str(x) -> string
    auto strFn = std::make_shared<FunctionType>();
    strFn->params.push_back({"x", reg.anyType()});
    strFn->returnType = reg.stringType();
    symbols_.define(Symbol("str", SymbolKind::FUNCTION, strFn));
    
    // upper(s) -> string
    auto upperFn = std::make_shared<FunctionType>();
    upperFn->params.push_back({"s", reg.stringType()});
    upperFn->returnType = reg.stringType();
    symbols_.define(Symbol("upper", SymbolKind::FUNCTION, upperFn));
    
    // contains(s, sub) -> bool
    auto containsFn = std::make_shared<FunctionType>();
    containsFn->params.push_back({"s", reg.stringType()});
    containsFn->params.push_back({"sub", reg.stringType()});
    containsFn->returnType = reg.boolType();
    symbols_.define(Symbol("contains", SymbolKind::FUNCTION, containsFn));
    
    // range(n) or range(start, end) -> list[int]
    auto rangeFn = std::make_shared<FunctionType>();
    rangeFn->params.push_back({"n", reg.intType()});
    rangeFn->isVariadic = true;  // Can take 1-3 args
    rangeFn->returnType = reg.listType(reg.intType());
    symbols_.define(Symbol("range", SymbolKind::FUNCTION, rangeFn));
    
    // push(list, elem) -> list
    auto pushFn = std::make_shared<FunctionType>();
    pushFn->params.push_back({"list", reg.anyType()});
    pushFn->params.push_back({"elem", reg.anyType()});
    pushFn->returnType = reg.anyType();
    symbols_.define(Symbol("push", SymbolKind::FUNCTION, pushFn));
    
    // platform() -> string
    auto platformFn = std::make_shared<FunctionType>();
    platformFn->returnType = reg.stringType();
    symbols_.define(Symbol("platform", SymbolKind::FUNCTION, platformFn));
    
    // arch() -> string
    auto archFn = std::make_shared<FunctionType>();
    archFn->returnType = reg.stringType();
    symbols_.define(Symbol("arch", SymbolKind::FUNCTION, archFn));
    
    // hostname() -> string
    auto hostnameFn = std::make_shared<FunctionType>();
    hostnameFn->returnType = reg.stringType();
    symbols_.define(Symbol("hostname", SymbolKind::FUNCTION, hostnameFn));
    
    // username() -> string
    auto usernameFn = std::make_shared<FunctionType>();
    usernameFn->returnType = reg.stringType();
    symbols_.define(Symbol("username", SymbolKind::FUNCTION, usernameFn));
    
    // cpu_count() -> int
    auto cpuCountFn = std::make_shared<FunctionType>();
    cpuCountFn->returnType = reg.intType();
    symbols_.define(Symbol("cpu_count", SymbolKind::FUNCTION, cpuCountFn));
    
    // sleep(ms) -> void
    auto sleepFn = std::make_shared<FunctionType>();
    sleepFn->params.push_back({"ms", reg.intType()});
    sleepFn->returnType = reg.voidType();
    symbols_.define(Symbol("sleep", SymbolKind::FUNCTION, sleepFn));
    
    // now() -> int (seconds)
    auto nowFn = std::make_shared<FunctionType>();
    nowFn->returnType = reg.intType();
    symbols_.define(Symbol("now", SymbolKind::FUNCTION, nowFn));
    
    // now_ms() -> int (milliseconds)
    auto nowMsFn = std::make_shared<FunctionType>();
    nowMsFn->returnType = reg.intType();
    symbols_.define(Symbol("now_ms", SymbolKind::FUNCTION, nowMsFn));
    
    // year(), month(), day(), hour(), minute(), second() -> int
    auto timeFn = std::make_shared<FunctionType>();
    timeFn->returnType = reg.intType();
    symbols_.define(Symbol("year", SymbolKind::FUNCTION, timeFn));
    symbols_.define(Symbol("month", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("day", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("hour", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("minute", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("second", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
}

bool TypeChecker::check(Program& program) {
    diagnostics_.clear();
    exprTypes_.clear();
    currentTypeParams_.clear();
    currentTypeParamNames_.clear();
    program.accept(*this);
    return !hasErrors();
}

bool TypeChecker::hasErrors() const {
    for (auto& d : diagnostics_) {
        if (d.level == TypeDiagnostic::Level::ERROR) return true;
    }
    return false;
}

TypePtr TypeChecker::getType(Expression* expr) {
    auto it = exprTypes_.find(expr);
    return it != exprTypes_.end() ? it->second : TypeRegistry::instance().unknownType();
}

TypePtr TypeChecker::inferType(Expression* expr) {
    expr->accept(*this);
    exprTypes_[expr] = currentType_;
    return currentType_;
}

void TypeChecker::error(const std::string& msg, const SourceLocation& loc) {
    diagnostics_.emplace_back(TypeDiagnostic::Level::ERROR, msg, loc);
}

void TypeChecker::warning(const std::string& msg, const SourceLocation& loc) {
    diagnostics_.emplace_back(TypeDiagnostic::Level::WARNING, msg, loc);
}

void TypeChecker::note(const std::string& msg, const SourceLocation& loc) {
    diagnostics_.emplace_back(TypeDiagnostic::Level::NOTE, msg, loc);
}

TypePtr TypeChecker::parseTypeAnnotation(const std::string& str) {
    if (str.empty()) return TypeRegistry::instance().unknownType();
    
    // Check for generic type syntax: Name[T, U, ...]
    auto genericType = parseGenericType(str);
    if (genericType) return genericType;
    
    // Check if it's a type parameter in scope
    auto typeParam = resolveTypeParam(str);
    if (typeParam) return typeParam;
    
    return TypeRegistry::instance().fromString(str);
}

TypePtr TypeChecker::parseGenericType(const std::string& str) {
    auto& reg = TypeRegistry::instance();
    
    // Match pattern: Name[Type1, Type2, ...]
    size_t bracketPos = str.find('[');
    if (bracketPos == std::string::npos) return nullptr;
    
    std::string baseName = str.substr(0, bracketPos);
    size_t endBracket = str.rfind(']');
    if (endBracket == std::string::npos || endBracket <= bracketPos) return nullptr;
    
    std::string argsStr = str.substr(bracketPos + 1, endBracket - bracketPos - 1);
    
    // Parse type arguments (simple comma-separated for now)
    std::vector<TypePtr> typeArgs;
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i <= argsStr.size(); i++) {
        if (i == argsStr.size() || (argsStr[i] == ',' && depth == 0)) {
            std::string arg = argsStr.substr(start, i - start);
            // Trim whitespace
            size_t first = arg.find_first_not_of(" \t");
            size_t last = arg.find_last_not_of(" \t");
            if (first != std::string::npos) {
                arg = arg.substr(first, last - first + 1);
            }
            if (!arg.empty()) {
                typeArgs.push_back(parseTypeAnnotation(arg));
            }
            start = i + 1;
        } else if (argsStr[i] == '[') {
            depth++;
        } else if (argsStr[i] == ']') {
            depth--;
        }
    }
    
    // Check for built-in generic types
    if (baseName == "List" || baseName == "list") {
        if (typeArgs.size() == 1) {
            return reg.listType(typeArgs[0]);
        }
    } else if (baseName == "Map" || baseName == "map") {
        if (typeArgs.size() == 2) {
            return reg.mapType(typeArgs[0], typeArgs[1]);
        }
    } else if (baseName == "Result") {
        // Result[T, E] - for now treat as generic
        return reg.genericType(baseName, typeArgs);
    }
    
    // Look up user-defined generic type
    TypePtr baseType = reg.lookupType(baseName);
    if (baseType) {
        return reg.instantiateGeneric(baseType, typeArgs);
    }
    
    // Return as unresolved generic
    return reg.genericType(baseName, typeArgs);
}

TypePtr TypeChecker::resolveTypeParam(const std::string& name) {
    // Check if name is a type parameter in current scope
    auto it = currentTypeParams_.find(name);
    if (it != currentTypeParams_.end()) {
        return it->second;
    }
    
    // Check if it's in the list of type param names (unbound)
    for (const auto& paramName : currentTypeParamNames_) {
        if (paramName == name) {
            return TypeRegistry::instance().typeParamType(name);
        }
    }
    
    return nullptr;
}

bool TypeChecker::checkTraitBounds(TypePtr type, const std::vector<std::string>& bounds, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    
    for (const auto& bound : bounds) {
        if (!reg.typeImplementsTrait(type, bound)) {
            error("Type '" + type->toString() + "' does not implement trait '" + bound + "'", loc);
            return false;
        }
    }
    return true;
}

TypePtr TypeChecker::instantiateGenericFunction(FunctionType* fnType, const std::vector<TypePtr>& typeArgs, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    
    if (fnType->typeParams.size() != typeArgs.size()) {
        error("Wrong number of type arguments: expected " + std::to_string(fnType->typeParams.size()) +
              ", got " + std::to_string(typeArgs.size()), loc);
        return reg.errorType();
    }
    
    // Build substitution map
    std::unordered_map<std::string, TypePtr> substitutions;
    for (size_t i = 0; i < fnType->typeParams.size(); i++) {
        substitutions[fnType->typeParams[i]] = typeArgs[i];
    }
    
    // Create instantiated function type
    auto newFn = std::make_shared<FunctionType>();
    for (const auto& param : fnType->params) {
        newFn->params.push_back({param.first, reg.substituteTypeParams(param.second, substitutions)});
    }
    newFn->returnType = reg.substituteTypeParams(fnType->returnType, substitutions);
    newFn->isVariadic = fnType->isVariadic;
    
    return newFn;
}

void TypeChecker::checkTraitImpl(const std::string& traitName, const std::string& typeName,
                                  const std::vector<std::unique_ptr<FnDecl>>& methods, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    
    TraitPtr trait = reg.lookupTrait(traitName);
    if (!trait) {
        error("Unknown trait '" + traitName + "'", loc);
        return;
    }
    
    // Check that all required methods are implemented
    for (const auto& traitMethod : trait->methods) {
        if (traitMethod.hasDefaultImpl) continue;  // Has default, not required
        
        bool found = false;
        for (const auto& implMethod : methods) {
            if (implMethod->name == traitMethod.name) {
                found = true;
                
                // Check method signature matches
                auto implFnType = std::make_shared<FunctionType>();
                for (const auto& p : implMethod->params) {
                    implFnType->params.push_back({p.first, parseTypeAnnotation(p.second)});
                }
                implFnType->returnType = parseTypeAnnotation(implMethod->returnType);
                
                // Compare signatures (simplified - just check param count and return type)
                if (implFnType->params.size() != traitMethod.signature->params.size()) {
                    error("Method '" + traitMethod.name + "' has wrong number of parameters", implMethod->location);
                }
                
                break;
            }
        }
        
        if (!found) {
            error("Missing implementation of method '" + traitMethod.name + "' for trait '" + traitName + "'", loc);
        }
    }
    
    // Register the implementation
    TraitImpl impl;
    impl.traitName = traitName;
    impl.typeName = typeName;
    for (const auto& method : methods) {
        auto fnType = std::make_shared<FunctionType>();
        for (const auto& p : method->params) {
            fnType->params.push_back({p.first, parseTypeAnnotation(p.second)});
        }
        fnType->returnType = parseTypeAnnotation(method->returnType);
        impl.methods[method->name] = fnType;
    }
    reg.registerTraitImpl(impl);
}

TypePtr TypeChecker::unify(TypePtr a, TypePtr b, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    if (a->kind == TypeKind::UNKNOWN) return b;
    if (b->kind == TypeKind::UNKNOWN) return a;
    if (a->kind == TypeKind::ANY || b->kind == TypeKind::ANY) return reg.anyType();
    if (a->equals(b.get())) return a;
    
    // Handle type parameter unification
    if (a->kind == TypeKind::TYPE_PARAM) {
        auto* tp = static_cast<TypeParamType*>(a.get());
        // Check bounds
        if (!tp->bounds.empty() && !reg.checkTraitBounds(b, tp->bounds)) {
            error("Type '" + b->toString() + "' does not satisfy bounds of '" + tp->name + "'", loc);
            return reg.errorType();
        }
        return b;
    }
    if (b->kind == TypeKind::TYPE_PARAM) {
        auto* tp = static_cast<TypeParamType*>(b.get());
        if (!tp->bounds.empty() && !reg.checkTraitBounds(a, tp->bounds)) {
            error("Type '" + a->toString() + "' does not satisfy bounds of '" + tp->name + "'", loc);
            return reg.errorType();
        }
        return a;
    }
    
    if (a->isNumeric() && b->isNumeric()) {
        if (a->isFloat() || b->isFloat()) return reg.floatType();
        if (a->size() >= b->size()) return a;
        return b;
    }
    error("Cannot unify types '" + a->toString() + "' and '" + b->toString() + "'", loc);
    return reg.errorType();
}

} // namespace flex
