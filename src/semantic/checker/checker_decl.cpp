// Flex Compiler - Type Checker Declaration Visitors
// Declaration type checking

#include "checker_base.h"

namespace flex {

void TypeChecker::visit(FnDecl& node) {
    auto& reg = TypeRegistry::instance();
    auto fnType = std::make_shared<FunctionType>();
    
    // Handle generic type parameters
    fnType->typeParams = node.typeParams;
    
    // Push type parameters into scope
    std::vector<std::string> savedTypeParamNames = currentTypeParamNames_;
    for (const auto& tp : node.typeParams) {
        currentTypeParamNames_.push_back(tp);
        // Create type parameter type
        auto tpType = std::make_shared<TypeParamType>(tp);
        currentTypeParams_[tp] = tpType;
    }
    
    for (auto& p : node.params) {
        TypePtr paramType = parseTypeAnnotation(p.second);
        if (paramType->kind == TypeKind::UNKNOWN) paramType = reg.anyType();
        fnType->params.push_back(std::make_pair(p.first, paramType));
    }
    fnType->returnType = parseTypeAnnotation(node.returnType);
    if (fnType->returnType->kind == TypeKind::UNKNOWN) fnType->returnType = reg.anyType();
    
    Symbol fnSym(node.name, SymbolKind::FUNCTION, fnType);
    symbols_.define(fnSym);
    
    symbols_.pushScope(Scope::Kind::FUNCTION);
    for (size_t i = 0; i < node.params.size(); i++) {
        Symbol paramSym(node.params[i].first, SymbolKind::PARAMETER, fnType->params[i].second);
        symbols_.define(paramSym);
    }
    expectedReturn_ = fnType->returnType;
    if (node.body) {
        node.body->accept(*this);
    }
    symbols_.popScope();
    
    // Restore type parameter scope
    for (const auto& tp : node.typeParams) {
        currentTypeParams_.erase(tp);
    }
    currentTypeParamNames_ = savedTypeParamNames;
}

void TypeChecker::visit(RecordDecl& node) {
    auto& reg = TypeRegistry::instance();
    auto recType = std::make_shared<RecordType>(node.name);
    
    // Handle generic type parameters
    std::vector<std::string> savedTypeParamNames = currentTypeParamNames_;
    for (const auto& tp : node.typeParams) {
        currentTypeParamNames_.push_back(tp);
        auto tpType = std::make_shared<TypeParamType>(tp);
        currentTypeParams_[tp] = tpType;
    }
    
    for (auto& f : node.fields) {
        TypePtr fieldType = parseTypeAnnotation(f.second);
        recType->fields.push_back({f.first, fieldType, false});
    }
    
    symbols_.registerType(node.name, recType);
    Symbol typeSym(node.name, SymbolKind::TYPE, recType);
    symbols_.define(typeSym);
    
    // Restore type parameter scope
    for (const auto& tp : node.typeParams) {
        currentTypeParams_.erase(tp);
    }
    currentTypeParamNames_ = savedTypeParamNames;
}

void TypeChecker::visit(EnumDecl& node) {
    auto& reg = TypeRegistry::instance();
    auto enumType = std::make_shared<Type>(TypeKind::INT);
    symbols_.registerType(node.name, enumType);
    int64_t nextValue = 0;
    for (auto& v : node.variants) {
        int64_t actualValue = v.second.value_or(nextValue);
        Symbol variantSym(node.name + "." + v.first, SymbolKind::VARIABLE, reg.intType());
        variantSym.isMutable = false;
        symbols_.define(variantSym);
        nextValue = actualValue + 1;
    }
}

void TypeChecker::visit(TypeAlias& node) {
    TypePtr targetType = parseTypeAnnotation(node.targetType);
    symbols_.registerType(node.name, targetType);
}

void TypeChecker::visit(TraitDecl& node) {
    auto& reg = TypeRegistry::instance();
    auto trait = std::make_shared<TraitType>(node.name);
    trait->typeParams = node.typeParams;
    
    // Push type parameters into scope for method parsing
    std::vector<std::string> savedTypeParamNames = currentTypeParamNames_;
    for (const auto& tp : node.typeParams) {
        currentTypeParamNames_.push_back(tp);
        auto tpType = std::make_shared<TypeParamType>(tp);
        currentTypeParams_[tp] = tpType;
    }
    
    // Add implicit Self type parameter
    currentTypeParamNames_.push_back("Self");
    currentTypeParams_["Self"] = reg.typeParamType("Self");
    
    // Process trait methods
    for (auto& method : node.methods) {
        TraitMethod tm;
        tm.name = method->name;
        tm.signature = std::make_shared<FunctionType>();
        tm.hasDefaultImpl = (method->body != nullptr);
        
        for (auto& p : method->params) {
            TypePtr paramType = parseTypeAnnotation(p.second);
            if (paramType->kind == TypeKind::UNKNOWN) paramType = reg.anyType();
            tm.signature->params.push_back({p.first, paramType});
        }
        tm.signature->returnType = parseTypeAnnotation(method->returnType);
        if (tm.signature->returnType->kind == TypeKind::UNKNOWN) {
            tm.signature->returnType = reg.voidType();
        }
        
        trait->methods.push_back(tm);
    }
    
    // Register the trait
    reg.registerTrait(node.name, trait);
    
    // Also register as a type for trait object usage
    Symbol traitSym(node.name, SymbolKind::TYPE, trait);
    symbols_.define(traitSym);
    
    // Restore type parameter scope
    currentTypeParams_.erase("Self");
    for (const auto& tp : node.typeParams) {
        currentTypeParams_.erase(tp);
    }
    currentTypeParamNames_ = savedTypeParamNames;
}

void TypeChecker::visit(ImplBlock& node) {
    auto& reg = TypeRegistry::instance();
    
    // Push type parameters into scope
    std::vector<std::string> savedTypeParamNames = currentTypeParamNames_;
    for (const auto& tp : node.typeParams) {
        currentTypeParamNames_.push_back(tp);
        auto tpType = std::make_shared<TypeParamType>(tp);
        currentTypeParams_[tp] = tpType;
    }
    
    // Resolve the implementing type
    TypePtr implType = parseTypeAnnotation(node.typeName);
    
    // Add Self as alias for the implementing type
    currentTypeParams_["Self"] = implType;
    
    if (!node.traitName.empty()) {
        // This is a trait implementation
        checkTraitImpl(node.traitName, node.typeName, node.methods, node.location);
    }
    
    // Type check all methods
    for (auto& method : node.methods) {
        // Create qualified method name for the type
        std::string qualifiedName = node.typeName + "." + method->name;
        
        auto fnType = std::make_shared<FunctionType>();
        for (auto& p : method->params) {
            TypePtr paramType = parseTypeAnnotation(p.second);
            if (paramType->kind == TypeKind::UNKNOWN) paramType = reg.anyType();
            fnType->params.push_back({p.first, paramType});
        }
        fnType->returnType = parseTypeAnnotation(method->returnType);
        if (fnType->returnType->kind == TypeKind::UNKNOWN) {
            fnType->returnType = reg.anyType();
        }
        
        Symbol methodSym(qualifiedName, SymbolKind::FUNCTION, fnType);
        symbols_.define(methodSym);
        
        // Type check method body
        symbols_.pushScope(Scope::Kind::FUNCTION);
        
        // Add 'self' parameter if present
        for (size_t i = 0; i < method->params.size(); i++) {
            TypePtr paramType = fnType->params[i].second;
            if (method->params[i].first == "self") {
                paramType = implType;  // self has the implementing type
            }
            Symbol paramSym(method->params[i].first, SymbolKind::PARAMETER, paramType);
            symbols_.define(paramSym);
        }
        
        expectedReturn_ = fnType->returnType;
        if (method->body) {
            method->body->accept(*this);
        }
        symbols_.popScope();
    }
    
    // Restore type parameter scope
    currentTypeParams_.erase("Self");
    for (const auto& tp : node.typeParams) {
        currentTypeParams_.erase(tp);
    }
    currentTypeParamNames_ = savedTypeParamNames;
}

void TypeChecker::visit(ImportStmt&) {}

void TypeChecker::visit(ExternDecl& node) {
    auto& reg = TypeRegistry::instance();
    for (auto& fn : node.functions) {
        auto fnType = std::make_shared<FunctionType>();
        for (auto& p : fn->params) {
            TypePtr paramType = parseTypeAnnotation(p.second);
            if (paramType->kind == TypeKind::UNKNOWN) paramType = reg.anyType();
            fnType->params.push_back(std::make_pair(p.first, paramType));
        }
        fnType->returnType = parseTypeAnnotation(fn->returnType);
        if (fnType->returnType->kind == TypeKind::UNKNOWN) fnType->returnType = reg.voidType();
        Symbol fnSym(fn->name, SymbolKind::FUNCTION, fnType);
        symbols_.define(fnSym);
    }
}

void TypeChecker::visit(MacroDecl& node) {
    Symbol macroSym(node.name, SymbolKind::MACRO, TypeRegistry::instance().anyType());
    symbols_.define(macroSym);
}

void TypeChecker::visit(LayerDecl& node) {
    Symbol layerSym(node.name, SymbolKind::LAYER, TypeRegistry::instance().anyType());
    symbols_.define(layerSym);
}

void TypeChecker::visit(UseStmt&) {}
void TypeChecker::visit(ModuleDecl& node) {
    // Type check all declarations in the module
    for (auto& stmt : node.body) {
        stmt->accept(*this);
    }
}

} // namespace flex
