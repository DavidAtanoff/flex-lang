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
    symbols_.define(sym);
}

void TypeChecker::visit(AssignStmt& node) {
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
    symbols_.popScope();
}

void TypeChecker::visit(ForStmt& node) {
    auto& reg = TypeRegistry::instance();
    TypePtr iterType = inferType(node.iterable.get());
    TypePtr elemType = reg.anyType();
    if (iterType->kind == TypeKind::LIST) elemType = static_cast<ListType*>(iterType.get())->element;
    symbols_.pushScope(Scope::Kind::LOOP);
    Symbol varSym(node.var, SymbolKind::VARIABLE, elemType);
    symbols_.define(varSym);
    node.body->accept(*this);
    symbols_.popScope();
}

void TypeChecker::visit(MatchStmt& node) {
    inferType(node.value.get());
    for (auto& c : node.cases) {
        // Check if pattern is the wildcard '_' - don't try to infer its type
        if (auto* ident = dynamic_cast<Identifier*>(c.first.get())) {
            if (ident->name == "_") {
                // Wildcard pattern - skip type inference, just check the body
                c.second->accept(*this);
                continue;
            }
        }
        inferType(c.first.get());
        c.second->accept(*this);
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
