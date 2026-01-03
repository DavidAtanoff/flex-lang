// Flex Compiler - Type Checker Statement Visitors
// Statement type checking

#include "checker_base.h"

namespace flex {

void TypeChecker::visit(ExprStmt& node) { inferType(node.expr.get()); }

void TypeChecker::visit(VarDecl& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr declaredType = parseTypeAnnotation(node.typeName);
    TypePtr initType = reg.unknownType();
    if (node.initializer) initType = inferType(node.initializer.get());
    TypePtr varType = (declaredType->kind != TypeKind::UNKNOWN) ? declaredType : initType;
    Symbol sym(node.name, SymbolKind::VARIABLE, varType);
    sym.isInitialized = node.initializer != nullptr;
    sym.isMutable = node.isMutable;
    sym.storage = symbols_.currentScope()->isGlobal() ? StorageClass::GLOBAL : StorageClass::LOCAL;
    sym.location = node.location;  // Store location for unused variable warnings
    sym.isUsed = false;            // Initialize as unused
    symbols_.define(sym);
}

void TypeChecker::visit(AssignStmt& node) {
    // Check for pointer dereference assignment (*ptr = value) - requires unsafe block
    if (dynamic_cast<DerefExpr*>(node.target.get())) {
        if (!symbols_.inUnsafe()) {
            error("Pointer dereference assignment requires unsafe block", node.location);
        }
    }
    
    TypePtr targetType = inferType(node.target.get());
    TypePtr valueType = inferType(node.value.get());
    if (auto* id = dynamic_cast<Identifier*>(node.target.get())) {
        Symbol* sym = symbols_.lookup(id->name);
        if (sym && !sym->isMutable) error("Cannot assign to immutable variable", node.location);
    }
}

void TypeChecker::visit(Block& node) {
    symbols_.pushScope(Scope::Kind::BLOCK);
    for (auto& stmt : node.statements) stmt->accept(*this);
    checkUnusedVariables(symbols_.currentScope());  // Check for unused variables before popping
    symbols_.popScope();
}

void TypeChecker::visit(IfStmt& node) {
    inferType(node.condition.get());
    node.thenBranch->accept(*this);
    for (auto& elif : node.elifBranches) {
        inferType(elif.first.get());
        elif.second->accept(*this);
    }
    if (node.elseBranch) node.elseBranch->accept(*this);
}

void TypeChecker::visit(WhileStmt& node) {
    inferType(node.condition.get());
    symbols_.pushScope(Scope::Kind::LOOP);
    node.body->accept(*this);
    checkUnusedVariables(symbols_.currentScope());
    symbols_.popScope();
}

void TypeChecker::visit(ForStmt& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr iterType = inferType(node.iterable.get());
    TypePtr elemType = reg.anyType();
    if (iterType->kind == TypeKind::LIST) elemType = static_cast<ListType*>(iterType.get())->element;
    symbols_.pushScope(Scope::Kind::LOOP);
    Symbol varSym(node.var, SymbolKind::VARIABLE, elemType);
    varSym.location = node.location;
    symbols_.define(varSym);
    node.body->accept(*this);
    checkUnusedVariables(symbols_.currentScope());
    symbols_.popScope();
}

void TypeChecker::visit(MatchStmt& node) {
    inferType(node.value.get());
    for (auto& c : node.cases) {
        // Check if pattern is the wildcard '_' - don't try to infer its type
        if (auto* ident = dynamic_cast<Identifier*>(c.pattern.get())) {
            if (ident->name == "_") {
                // Wildcard pattern - skip type inference, check guard and body
                if (c.guard) inferType(c.guard.get());
                c.body->accept(*this);
                continue;
            }
            // Variable binding pattern - define the variable in scope
            if (ident->name.length() > 0 && std::islower(ident->name[0])) {
                symbols_.define(Symbol(ident->name, SymbolKind::VARIABLE, getType(node.value.get())));
            }
        }
        inferType(c.pattern.get());
        if (c.guard) inferType(c.guard.get());
        c.body->accept(*this);
    }
    if (node.defaultCase) node.defaultCase->accept(*this);
}

void TypeChecker::visit(ReturnStmt& node) {
    if (node.value) inferType(node.value.get());
}

void TypeChecker::visit(BreakStmt& node) {
    if (!symbols_.inLoop()) error("Break statement outside of loop", node.location);
}

void TypeChecker::visit(ContinueStmt& node) {
    if (!symbols_.inLoop()) error("Continue statement outside of loop", node.location);
}

void TypeChecker::visit(TryStmt& node) {
    TypePtr tryType = inferType(node.tryExpr.get());
    TypePtr elseType = inferType(node.elseExpr.get());
    currentType_ = commonType(tryType, elseType);
}

void TypeChecker::visit(UnsafeBlock& node) {
    symbols_.pushScope(Scope::Kind::UNSAFE);
    node.body->accept(*this);
    symbols_.popScope();
}

void TypeChecker::visit(DeleteStmt& node) {
    if (!symbols_.inUnsafe()) error("Delete requires unsafe block", node.location);
    inferType(node.expr.get());
}

void TypeChecker::visit(LockStmt& node) {
    // Type check the mutex expression
    TypePtr mutexType = inferType(node.mutex.get());
    
    // Verify it's a mutex type
    if (mutexType->kind != TypeKind::MUTEX) {
        error("lock statement requires a Mutex type, got '" + mutexType->toString() + "'", node.location);
    }
    
    // Type check the body
    node.body->accept(*this);
}

void TypeChecker::visit(AsmStmt& node) {
    // Inline assembly requires unsafe block
    if (!symbols_.inUnsafe()) {
        error("Inline assembly requires unsafe block", node.location);
    }
    // No type checking needed for assembly code itself
}

void TypeChecker::visit(DestructuringDecl& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr initType = inferType(node.initializer.get());
    
    // For tuple destructuring, try to get element types
    if (node.kind == DestructuringDecl::Kind::TUPLE) {
        if (initType->kind == TypeKind::LIST) {
            auto* listType = static_cast<ListType*>(initType.get());
            for (const auto& name : node.names) {
                Symbol sym(name, SymbolKind::VARIABLE, listType->element);
                sym.isMutable = node.isMutable;
                symbols_.define(sym);
            }
        } else {
            // Unknown tuple type - use any
            for (const auto& name : node.names) {
                Symbol sym(name, SymbolKind::VARIABLE, reg.anyType());
                sym.isMutable = node.isMutable;
                symbols_.define(sym);
            }
        }
    } else if (node.kind == DestructuringDecl::Kind::RECORD) {
        if (initType->kind == TypeKind::RECORD) {
            auto* recType = static_cast<RecordType*>(initType.get());
            for (const auto& name : node.names) {
                TypePtr fieldType = recType->getField(name);
                Symbol sym(name, SymbolKind::VARIABLE, fieldType ? fieldType : reg.anyType());
                sym.isMutable = node.isMutable;
                symbols_.define(sym);
            }
        } else {
            for (const auto& name : node.names) {
                Symbol sym(name, SymbolKind::VARIABLE, reg.anyType());
                sym.isMutable = node.isMutable;
                symbols_.define(sym);
            }
        }
    }
}

void TypeChecker::visit(SyntaxMacroDecl& node) {
    // Syntax macros are compile-time constructs
    Symbol macroSym(node.name, SymbolKind::MACRO, TypeRegistry::instance().anyType());
    symbols_.define(macroSym);
}

void TypeChecker::visit(Program& node) {
    for (auto& stmt : node.statements) stmt->accept(*this);
}

} // namespace flex
