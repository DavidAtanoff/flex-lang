// Flex Compiler - Macro Expander Clone Functions
// Expression and statement cloning with parameter substitution

#include "expander_base.h"

namespace flex {

ExprPtr MacroExpander::cloneExpr(Expression* expr, const std::unordered_map<std::string, Expression*>& params) {
    if (!expr) return nullptr;
    
    if (auto* ident = dynamic_cast<Identifier*>(expr)) {
        auto it = params.find(ident->name);
        if (it != params.end()) {
            return cloneExpr(it->second, {});
        }
        return std::make_unique<Identifier>(ident->name, ident->location);
    }
    
    if (auto* intLit = dynamic_cast<IntegerLiteral*>(expr)) {
        return std::make_unique<IntegerLiteral>(intLit->value, intLit->location);
    }
    if (auto* floatLit = dynamic_cast<FloatLiteral*>(expr)) {
        return std::make_unique<FloatLiteral>(floatLit->value, floatLit->location);
    }
    if (auto* strLit = dynamic_cast<StringLiteral*>(expr)) {
        return std::make_unique<StringLiteral>(strLit->value, strLit->location);
    }
    if (auto* boolLit = dynamic_cast<BoolLiteral*>(expr)) {
        return std::make_unique<BoolLiteral>(boolLit->value, boolLit->location);
    }
    if (auto* nilLit = dynamic_cast<NilLiteral*>(expr)) {
        return std::make_unique<NilLiteral>(nilLit->location);
    }
    
    if (auto* binary = dynamic_cast<BinaryExpr*>(expr)) {
        return std::make_unique<BinaryExpr>(
            cloneExpr(binary->left.get(), params), binary->op,
            cloneExpr(binary->right.get(), params), binary->location);
    }
    
    if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        return std::make_unique<UnaryExpr>(
            unary->op, cloneExpr(unary->operand.get(), params), unary->location);
    }
    
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        auto newCall = std::make_unique<CallExpr>(cloneExpr(call->callee.get(), params), call->location);
        for (auto& arg : call->args) newCall->args.push_back(cloneExpr(arg.get(), params));
        return newCall;
    }
    
    if (auto* member = dynamic_cast<MemberExpr*>(expr)) {
        return std::make_unique<MemberExpr>(
            cloneExpr(member->object.get(), params), member->member, member->location);
    }
    
    if (auto* index = dynamic_cast<IndexExpr*>(expr)) {
        return std::make_unique<IndexExpr>(
            cloneExpr(index->object.get(), params),
            cloneExpr(index->index.get(), params), index->location);
    }
    
    if (auto* ternary = dynamic_cast<TernaryExpr*>(expr)) {
        return std::make_unique<TernaryExpr>(
            cloneExpr(ternary->condition.get(), params),
            cloneExpr(ternary->thenExpr.get(), params),
            cloneExpr(ternary->elseExpr.get(), params), ternary->location);
    }
    
    return nullptr;
}


StmtPtr MacroExpander::cloneStmt(Statement* stmt, const std::unordered_map<std::string, Expression*>& params) {
    if (!stmt) return nullptr;
    
    if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
        return std::make_unique<ExprStmt>(cloneExpr(exprStmt->expr.get(), params), exprStmt->location);
    }
    
    if (auto* retStmt = dynamic_cast<ReturnStmt*>(stmt)) {
        return std::make_unique<ReturnStmt>(
            retStmt->value ? cloneExpr(retStmt->value.get(), params) : nullptr, retStmt->location);
    }
    
    if (auto* varDecl = dynamic_cast<VarDecl*>(stmt)) {
        auto newDecl = std::make_unique<VarDecl>(
            varDecl->name, varDecl->typeName,
            varDecl->initializer ? cloneExpr(varDecl->initializer.get(), params) : nullptr,
            varDecl->location);
        newDecl->isMutable = varDecl->isMutable;
        newDecl->isConst = varDecl->isConst;
        return newDecl;
    }
    
    if (auto* assignStmt = dynamic_cast<AssignStmt*>(stmt)) {
        return std::make_unique<AssignStmt>(
            cloneExpr(assignStmt->target.get(), params), assignStmt->op,
            cloneExpr(assignStmt->value.get(), params), assignStmt->location);
    }
    
    if (auto* ifStmt = dynamic_cast<IfStmt*>(stmt)) {
        auto newIf = std::make_unique<IfStmt>(
            cloneExpr(ifStmt->condition.get(), params),
            cloneStmt(ifStmt->thenBranch.get(), params), ifStmt->location);
        for (auto& [cond, branch] : ifStmt->elifBranches) {
            newIf->elifBranches.emplace_back(
                cloneExpr(cond.get(), params), cloneStmt(branch.get(), params));
        }
        if (ifStmt->elseBranch) newIf->elseBranch = cloneStmt(ifStmt->elseBranch.get(), params);
        return newIf;
    }
    
    if (auto* whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
        return std::make_unique<WhileStmt>(
            cloneExpr(whileStmt->condition.get(), params),
            cloneStmt(whileStmt->body.get(), params), whileStmt->location);
    }
    
    if (auto* forStmt = dynamic_cast<ForStmt*>(stmt)) {
        return std::make_unique<ForStmt>(
            forStmt->var, cloneExpr(forStmt->iterable.get(), params),
            cloneStmt(forStmt->body.get(), params), forStmt->location);
    }
    
    if (auto* block = dynamic_cast<Block*>(stmt)) {
        auto newBlock = std::make_unique<Block>(block->location);
        for (auto& s : block->statements) newBlock->statements.push_back(cloneStmt(s.get(), params));
        return newBlock;
    }
    
    if (auto* breakStmt = dynamic_cast<BreakStmt*>(stmt)) {
        return std::make_unique<BreakStmt>(breakStmt->location);
    }
    
    if (auto* contStmt = dynamic_cast<ContinueStmt*>(stmt)) {
        return std::make_unique<ContinueStmt>(contStmt->location);
    }
    
    return nullptr;
}

std::vector<StmtPtr> MacroExpander::cloneStmts(const std::vector<StmtPtr>& stmts,
                                                const std::unordered_map<std::string, Expression*>& params,
                                                Statement* blockParam) {
    std::vector<StmtPtr> result;
    for (auto& stmt : stmts) {
        if (auto* exprStmt = dynamic_cast<ExprStmt*>(stmt.get())) {
            if (auto* ident = dynamic_cast<Identifier*>(exprStmt->expr.get())) {
                if (blockParam && (ident->name == "body" || ident->name == "block" || ident->name == "content")) {
                    result.push_back(cloneStmt(blockParam, params));
                    continue;
                }
            }
        }
        result.push_back(cloneStmt(stmt.get(), params));
    }
    return result;
}

ExprPtr MacroExpander::convertIfToTernary(IfStmt* ifStmt, 
                                           const std::unordered_map<std::string, Expression*>& params,
                                           SourceLocation loc) {
    auto condition = cloneExpr(ifStmt->condition.get(), params);
    
    ExprPtr thenValue = nullptr;
    if (auto* block = dynamic_cast<Block*>(ifStmt->thenBranch.get())) {
        for (auto& stmt : block->statements) {
            if (auto* ret = dynamic_cast<ReturnStmt*>(stmt.get())) {
                thenValue = cloneExpr(ret->value.get(), params);
                break;
            }
        }
    } else if (auto* ret = dynamic_cast<ReturnStmt*>(ifStmt->thenBranch.get())) {
        thenValue = cloneExpr(ret->value.get(), params);
    }
    
    if (!thenValue) thenValue = std::make_unique<NilLiteral>(loc);
    
    ExprPtr elseValue = nullptr;
    
    if (!ifStmt->elifBranches.empty()) {
        for (auto it = ifStmt->elifBranches.rbegin(); it != ifStmt->elifBranches.rend(); ++it) {
            auto& [elifCond, elifBody] = *it;
            auto elifCondClone = cloneExpr(elifCond.get(), params);
            
            ExprPtr elifValue = nullptr;
            if (auto* block = dynamic_cast<Block*>(elifBody.get())) {
                for (auto& stmt : block->statements) {
                    if (auto* ret = dynamic_cast<ReturnStmt*>(stmt.get())) {
                        elifValue = cloneExpr(ret->value.get(), params);
                        break;
                    }
                }
            }
            if (!elifValue) elifValue = std::make_unique<NilLiteral>(loc);
            
            if (!elseValue) {
                if (ifStmt->elseBranch) {
                    if (auto* block = dynamic_cast<Block*>(ifStmt->elseBranch.get())) {
                        for (auto& stmt : block->statements) {
                            if (auto* ret = dynamic_cast<ReturnStmt*>(stmt.get())) {
                                elseValue = cloneExpr(ret->value.get(), params);
                                break;
                            }
                        }
                    }
                }
                if (!elseValue) elseValue = std::make_unique<NilLiteral>(loc);
            }
            
            elseValue = std::make_unique<TernaryExpr>(
                std::move(elifCondClone), std::move(elifValue), std::move(elseValue), loc);
        }
    } else if (ifStmt->elseBranch) {
        if (auto* block = dynamic_cast<Block*>(ifStmt->elseBranch.get())) {
            for (auto& stmt : block->statements) {
                if (auto* ret = dynamic_cast<ReturnStmt*>(stmt.get())) {
                    elseValue = cloneExpr(ret->value.get(), params);
                    break;
                }
            }
        }
    }
    
    if (!elseValue) elseValue = std::make_unique<NilLiteral>(loc);
    
    return std::make_unique<TernaryExpr>(std::move(condition), std::move(thenValue), std::move(elseValue), loc);
}

} // namespace flex
