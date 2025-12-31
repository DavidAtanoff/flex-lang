// Flex Compiler - Parser Statement Implementations
// Handles: if, while, for, match, return, break, continue, delete, block, expression statements

#include "parser_base.h"
#include <unordered_set>

namespace flex {

StmtPtr Parser::statement() {
    if (match(TokenType::IF)) return ifStatement();
    if (match(TokenType::WHILE)) return whileStatement();
    if (match(TokenType::FOR)) return forStatement();
    if (match(TokenType::MATCH)) return matchStatement();
    if (match(TokenType::RETURN)) return returnStatement();
    if (match(TokenType::BREAK)) return breakStatement();
    if (match(TokenType::CONTINUE)) return continueStatement();
    if (match(TokenType::DELETE)) return deleteStatement();
    
    return expressionStatement();
}

StmtPtr Parser::expressionStatement() {
    auto loc = peek().location;
    auto expr = expression();
    
    if (auto* id = dynamic_cast<Identifier*>(expr.get())) {
        static const std::unordered_set<std::string> builtins = {"print", "println", "input", "exit"};
        
        // Compile-time constant: NAME :: value
        if (match(TokenType::DOUBLE_COLON)) {
            auto init = expression();
            match(TokenType::NEWLINE);
            auto decl = std::make_unique<VarDecl>(id->name, "", std::move(init), loc);
            decl->isMutable = false;
            decl->isConst = true;
            return decl;
        }
        
        // Built-in function call without parentheses
        if (builtins.count(id->name) && !isAtStatementBoundary() && 
            !check(TokenType::ASSIGN) && !check(TokenType::COLON)) {
            auto call = std::make_unique<CallExpr>(std::move(expr), loc);
            call->args.push_back(expression());
            match(TokenType::NEWLINE);
            return std::make_unique<ExprStmt>(std::move(call), loc);
        }
        
        // name value  OR  name: type = value
        if (!isAtStatementBoundary() && !check(TokenType::ASSIGN) && 
            !check(TokenType::PLUS_ASSIGN) && !check(TokenType::MINUS_ASSIGN)) {
            std::string name = id->name;
            std::string typeName;
            
            if (match(TokenType::COLON)) {
                typeName = parseType();
                if (match(TokenType::ASSIGN)) {
                    auto init = expression();
                    match(TokenType::NEWLINE);
                    return std::make_unique<VarDecl>(name, typeName, std::move(init), loc);
                }
            } else {
                auto init = expression();
                match(TokenType::NEWLINE);
                return std::make_unique<VarDecl>(name, "", std::move(init), loc);
            }
        }
    }
    
    match(TokenType::NEWLINE);
    return std::make_unique<ExprStmt>(std::move(expr), loc);
}

StmtPtr Parser::block() {
    auto blk = std::make_unique<Block>(peek().location);
    
    consume(TokenType::INDENT, "Expected indented block");
    skipNewlines();
    
    while (!check(TokenType::DEDENT) && !isAtEnd()) {
        blk->statements.push_back(declaration());
        skipNewlines();
    }
    
    consume(TokenType::DEDENT, "Expected end of block");
    return blk;
}

StmtPtr Parser::ifStatement() {
    auto loc = previous().location;
    auto condition = expression();
    consume(TokenType::COLON, "Expected ':' after if condition");
    match(TokenType::NEWLINE);
    
    auto thenBranch = block();
    auto stmt = std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), loc);
    
    skipNewlines();
    while (match(TokenType::ELIF)) {
        auto elifCond = expression();
        consume(TokenType::COLON, "Expected ':' after elif condition");
        match(TokenType::NEWLINE);
        auto elifBody = block();
        stmt->elifBranches.emplace_back(std::move(elifCond), std::move(elifBody));
        skipNewlines();
    }
    
    if (match(TokenType::ELSE)) {
        consume(TokenType::COLON, "Expected ':' after else");
        match(TokenType::NEWLINE);
        stmt->elseBranch = block();
    }
    
    return stmt;
}

StmtPtr Parser::whileStatement() {
    auto loc = previous().location;
    auto condition = expression();
    consume(TokenType::COLON, "Expected ':' after while condition");
    match(TokenType::NEWLINE);
    auto body = block();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), loc);
}

StmtPtr Parser::forStatement() {
    auto loc = previous().location;
    auto varName = consume(TokenType::IDENTIFIER, "Expected variable name").lexeme;
    consume(TokenType::IN, "Expected 'in' after for variable");
    auto iterable = expression();
    consume(TokenType::COLON, "Expected ':' after for iterable");
    match(TokenType::NEWLINE);
    auto body = block();
    return std::make_unique<ForStmt>(varName, std::move(iterable), std::move(body), loc);
}

StmtPtr Parser::matchStatement() {
    auto loc = previous().location;
    auto value = expression();
    consume(TokenType::COLON, "Expected ':' after match value");
    match(TokenType::NEWLINE);
    
    auto stmt = std::make_unique<MatchStmt>(std::move(value), loc);
    consume(TokenType::INDENT, "Expected indented match cases");
    skipNewlines();
    
    while (!check(TokenType::DEDENT) && !isAtEnd()) {
        auto pattern = expression();
        if (!match(TokenType::ARROW)) {
            consume(TokenType::COLON, "Expected '->' or ':' after match pattern");
        }
        
        if (check(TokenType::NEWLINE)) {
            match(TokenType::NEWLINE);
            auto body = block();
            stmt->cases.emplace_back(std::move(pattern), std::move(body));
        } else if (check(TokenType::RETURN)) {
            advance();
            auto retStmt = returnStatement();
            stmt->cases.emplace_back(std::move(pattern), std::move(retStmt));
        } else if (check(TokenType::IDENTIFIER)) {
            static const std::unordered_set<std::string> builtins = {"print", "println", "input", "exit"};
            std::string name = peek().lexeme;
            if (builtins.count(name)) {
                auto loc = peek().location;
                advance();
                auto callee = std::make_unique<Identifier>(name, loc);
                auto call = std::make_unique<CallExpr>(std::move(callee), loc);
                call->args.push_back(expression());
                auto exprStmt = std::make_unique<ExprStmt>(std::move(call), loc);
                stmt->cases.emplace_back(std::move(pattern), std::move(exprStmt));
                match(TokenType::NEWLINE);
            } else {
                auto expr = expression();
                auto exprStmt = std::make_unique<ExprStmt>(std::move(expr), peek().location);
                stmt->cases.emplace_back(std::move(pattern), std::move(exprStmt));
                match(TokenType::NEWLINE);
            }
        } else {
            auto expr = expression();
            auto exprStmt = std::make_unique<ExprStmt>(std::move(expr), peek().location);
            stmt->cases.emplace_back(std::move(pattern), std::move(exprStmt));
            match(TokenType::NEWLINE);
        }
        skipNewlines();
    }
    
    consume(TokenType::DEDENT, "Expected end of match block");
    return stmt;
}

StmtPtr Parser::returnStatement() {
    auto loc = previous().location;
    ExprPtr value = nullptr;
    if (!isAtStatementBoundary()) {
        value = expression();
    }
    match(TokenType::NEWLINE);
    return std::make_unique<ReturnStmt>(std::move(value), loc);
}

StmtPtr Parser::breakStatement() {
    auto loc = previous().location;
    match(TokenType::NEWLINE);
    return std::make_unique<BreakStmt>(loc);
}

StmtPtr Parser::continueStatement() {
    auto loc = previous().location;
    match(TokenType::NEWLINE);
    return std::make_unique<ContinueStmt>(loc);
}

StmtPtr Parser::deleteStatement() {
    auto loc = previous().location;
    auto expr = expression();
    match(TokenType::NEWLINE);
    return std::make_unique<DeleteStmt>(std::move(expr), loc);
}

} // namespace flex
