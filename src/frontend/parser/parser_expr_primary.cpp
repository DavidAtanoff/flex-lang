// Flex Compiler - Pratt Parser Primary Expressions
// Handles literals, identifiers, grouping, and compound expressions

#include "parser_base.h"
#include "frontend/macro/syntax_macro.h"
#include "frontend/lexer/lexer.h"
#include "common/errors.h"
#include <unordered_set>

namespace flex {

ExprPtr Parser::primary() {
    auto loc = peek().location;
    
    if (match(TokenType::INTEGER)) {
        return std::make_unique<IntegerLiteral>(std::get<int64_t>(previous().literal), loc);
    }
    
    if (match(TokenType::FLOAT)) {
        return std::make_unique<FloatLiteral>(std::get<double>(previous().literal), loc);
    }
    
    if (match(TokenType::STRING)) {
        std::string value = std::get<std::string>(previous().literal);
        
        if (value.find('\x01') != std::string::npos) {
            auto interp = std::make_unique<InterpolatedString>(loc);
            
            std::string currentPart;
            size_t i = 0;
            while (i < value.length()) {
                if (value[i] == '\x01') {
                    if (!currentPart.empty()) {
                        interp->parts.push_back(currentPart);
                        currentPart.clear();
                    }
                    i++;
                    std::string exprStr;
                    while (i < value.length() && value[i] != '\x02') {
                        exprStr += value[i++];
                    }
                    if (i < value.length()) i++;
                    
                    Lexer exprLexer(exprStr, "<interpolation>");
                    auto exprTokens = exprLexer.tokenize();
                    Parser exprParser(exprTokens);
                    auto parsedExpr = exprParser.expression();
                    interp->parts.push_back(std::move(parsedExpr));
                } else {
                    currentPart += value[i++];
                }
            }
            if (!currentPart.empty()) {
                interp->parts.push_back(currentPart);
            }
            return interp;
        }
        
        return std::make_unique<StringLiteral>(value, loc);
    }
    
    if (match(TokenType::TRUE)) {
        return std::make_unique<BoolLiteral>(true, loc);
    }
    if (match(TokenType::FALSE)) {
        return std::make_unique<BoolLiteral>(false, loc);
    }
    
    if (match(TokenType::NIL)) {
        return std::make_unique<NilLiteral>(loc);
    }
    
    // Channel creation: chan[T] or chan[T, N]
    if (match(TokenType::CHAN)) {
        consume(TokenType::LBRACKET, "Expected '[' after chan");
        std::string elemType = parseType();
        int64_t bufSize = 0;
        if (match(TokenType::COMMA)) {
            auto sizeTok = consume(TokenType::INTEGER, "Expected buffer size");
            bufSize = std::get<int64_t>(sizeTok.literal);
        }
        consume(TokenType::RBRACKET, "Expected ']' after channel type");
        return std::make_unique<MakeChanExpr>(elemType, bufSize, loc);
    }
    
    // Mutex creation: Mutex[T]
    if (match(TokenType::MUTEX)) {
        consume(TokenType::LBRACKET, "Expected '[' after Mutex");
        std::string elemType = parseType();
        consume(TokenType::RBRACKET, "Expected ']' after Mutex type");
        return std::make_unique<MakeMutexExpr>(elemType, loc);
    }
    
    // RWLock creation: RWLock[T]
    if (match(TokenType::RWLOCK)) {
        consume(TokenType::LBRACKET, "Expected '[' after RWLock");
        std::string elemType = parseType();
        consume(TokenType::RBRACKET, "Expected ']' after RWLock type");
        return std::make_unique<MakeRWLockExpr>(elemType, loc);
    }
    
    // Cond creation: Cond()
    if (match(TokenType::COND)) {
        if (match(TokenType::LPAREN)) {
            consume(TokenType::RPAREN, "Expected ')' after Cond");
        }
        return std::make_unique<MakeCondExpr>(loc);
    }
    
    // Semaphore creation: Semaphore(initial, max)
    if (match(TokenType::SEMAPHORE)) {
        consume(TokenType::LPAREN, "Expected '(' after Semaphore");
        auto initTok = consume(TokenType::INTEGER, "Expected initial count");
        int64_t initialCount = std::get<int64_t>(initTok.literal);
        int64_t maxCount = initialCount;  // Default max = initial
        if (match(TokenType::COMMA)) {
            auto maxTok = consume(TokenType::INTEGER, "Expected max count");
            maxCount = std::get<int64_t>(maxTok.literal);
        }
        consume(TokenType::RPAREN, "Expected ')' after Semaphore arguments");
        return std::make_unique<MakeSemaphoreExpr>(initialCount, maxCount, loc);
    }
    
    if (match(TokenType::IDENTIFIER)) {
        std::string name = previous().lexeme;
        auto idLoc = previous().location;
        
        static const std::unordered_set<std::string> exprBuiltins = {
            "str", "len", "int", "float", "bool", "type", "abs", "not"
        };
        
        if (exprBuiltins.count(name) && !check(TokenType::LPAREN) && !isAtStatementBoundary() &&
            !check(TokenType::ASSIGN) && !check(TokenType::COLON) && !check(TokenType::NEWLINE) &&
            !check(TokenType::COMMA) && !check(TokenType::RPAREN) && !check(TokenType::RBRACKET) &&
            !check(TokenType::PLUS) && !check(TokenType::MINUS) && !check(TokenType::STAR) &&
            !check(TokenType::SLASH) && !check(TokenType::PERCENT)) {
            auto callee = std::make_unique<Identifier>(name, idLoc);
            auto call = std::make_unique<CallExpr>(std::move(callee), idLoc);
            call->args.push_back(parsePrecedence(Precedence::UNARY));
            return call;
        }
        
        if (check(TokenType::COLON)) {
            size_t saved = current;
            advance();
            
            if (check(TokenType::NEWLINE)) {
                advance();
                skipNewlines();
                
                if (check(TokenType::INDENT)) {
                    bool isDSL = SyntaxMacroRegistry::instance().isDSLName(name) ||
                                 name == "sql" || name == "html" || name == "json" || 
                                 name == "regex" || name == "asm" || name == "css" ||
                                 name == "xml" || name == "yaml" || name == "toml" ||
                                 name == "graphql" || name == "markdown" || name == "query";
                    
                    if (isDSL) {
                        return parseDSLBlock(name, idLoc);
                    }
                }
            }
            current = saved;
        }
        
        return std::make_unique<Identifier>(name, idLoc);
    }
    
    if (match(TokenType::LBRACKET)) {
        return listLiteral();
    }
    
    if (match(TokenType::LBRACE)) {
        return recordLiteral();
    }
    
    if (match(TokenType::LPAREN)) {
        auto expr = expression();
        
        if (match(TokenType::COMMA)) {
            auto list = std::make_unique<ListExpr>(loc);
            list->elements.push_back(std::move(expr));
            do {
                list->elements.push_back(expression());
            } while (match(TokenType::COMMA));
            consume(TokenType::RPAREN, "Expected ')' after tuple elements");
            return list;
        }
        
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    if (match(TokenType::PIPE)) {
        return lambda();
    }
    
    if (match(TokenType::TRY)) {
        auto tryExpr = expression();
        consume(TokenType::ELSE, "Expected 'else' after try expression");
        auto elseExpr = expression();
        return std::make_unique<TernaryExpr>(std::move(tryExpr), std::move(tryExpr), 
                                              std::move(elseExpr), loc);
    }
    
    auto diag = errors::expectedExpression(tokenTypeToString(peek().type), loc);
    throw FlexDiagnosticError(diag);
}

ExprPtr Parser::listLiteral() {
    auto loc = previous().location;
    auto list = std::make_unique<ListExpr>(loc);
    
    skipNewlines();
    if (!check(TokenType::RBRACKET)) {
        auto first = expression();
        
        if (match(TokenType::FOR)) {
            auto var = consume(TokenType::IDENTIFIER, "Expected variable in comprehension").lexeme;
            consume(TokenType::IN, "Expected 'in' in comprehension");
            auto iterable = expression();
            ExprPtr condition = nullptr;
            if (match(TokenType::IF)) {
                condition = expression();
            }
            skipNewlines();
            consume(TokenType::RBRACKET, "Expected ']' after list comprehension");
            return std::make_unique<ListCompExpr>(std::move(first), var, std::move(iterable), 
                                                   std::move(condition), loc);
        }
        
        list->elements.push_back(std::move(first));
        
        while (match(TokenType::COMMA)) {
            skipNewlines();
            if (check(TokenType::RBRACKET)) break;
            list->elements.push_back(expression());
        }
    }
    
    skipNewlines();
    consume(TokenType::RBRACKET, "Expected ']' after list");
    return list;
}

ExprPtr Parser::recordLiteral() {
    auto loc = previous().location;
    
    skipNewlines();
    
    if (check(TokenType::RBRACE)) {
        advance();
        return std::make_unique<RecordExpr>(loc);
    }
    
    if (check(TokenType::STRING)) {
        auto map = std::make_unique<MapExpr>(loc);
        do {
            skipNewlines();
            if (check(TokenType::RBRACE)) break;
            
            auto keyToken = consume(TokenType::STRING, "Expected string key in map");
            auto key = std::make_unique<StringLiteral>(std::get<std::string>(keyToken.literal), keyToken.location);
            
            consume(TokenType::COLON, "Expected ':' after map key");
            auto value = expression();
            map->entries.emplace_back(std::move(key), std::move(value));
        } while (match(TokenType::COMMA));
        
        skipNewlines();
        consume(TokenType::RBRACE, "Expected '}' after map");
        return map;
    }
    
    auto rec = std::make_unique<RecordExpr>(loc);
    do {
        skipNewlines();
        if (check(TokenType::RBRACE)) break;
        
        auto name = consume(TokenType::IDENTIFIER, "Expected field name").lexeme;
        consume(TokenType::COLON, "Expected ':' after field name");
        auto value = expression();
        rec->fields.emplace_back(name, std::move(value));
    } while (match(TokenType::COMMA));
    
    skipNewlines();
    consume(TokenType::RBRACE, "Expected '}' after record");
    return rec;
}

ExprPtr Parser::lambda() {
    auto loc = previous().location;
    auto lam = std::make_unique<LambdaExpr>(loc);
    
    if (!check(TokenType::PIPE)) {
        do {
            auto name = consume(TokenType::IDENTIFIER, "Expected parameter name").lexeme;
            std::string type;
            if (match(TokenType::COLON)) {
                type = parseType();
            }
            lam->params.emplace_back(name, type);
        } while (match(TokenType::COMMA));
    }
    
    consume(TokenType::PIPE, "Expected '|' after lambda parameters");
    match(TokenType::DOUBLE_ARROW);
    
    lam->body = expression();
    return lam;
}

} // namespace flex
