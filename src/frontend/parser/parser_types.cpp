// Flex Compiler - Parser Type and Helper Implementations
// Handles: type parsing, parameter parsing, call args, DSL blocks

#include "parser_base.h"

namespace flex {

std::string Parser::parseType() {
    std::string type;
    
    // C-style pointer: *int, *str, **int (pointer to pointer)
    // Also handles function pointer: *fn(int, int) -> int
    if (match(TokenType::STAR)) {
        // Check if this is a function pointer type: *fn(...)
        if (check(TokenType::FN)) {
            // Parse the function type and wrap it as a pointer
            type = "*" + parseType();  // parseType will handle fn(...)
        } else {
            type = "*" + parseType();
        }
    }
    // Reference type: &T, &mut T
    else if (match(TokenType::AMP)) {
        bool isMut = match(TokenType::MUT);
        type = (isMut ? "&mut " : "&") + parseType();
    }
    // Verbose pointer: ptr<T>
    else if (match(TokenType::PTR)) {
        consume(TokenType::LT, "Expected '<' after ptr");
        type = "*" + parseType();  // Normalize to *T internally
        consume(TokenType::GT, "Expected '>' after ptr type");
    }
    // Reference: ref<T>
    else if (match(TokenType::REF)) {
        consume(TokenType::LT, "Expected '<' after ref");
        type = "ref<" + parseType() + ">";
        consume(TokenType::GT, "Expected '>' after ref type");
    }
    // Channel type: chan[T] or chan[T, N] for buffered
    else if (match(TokenType::CHAN)) {
        consume(TokenType::LBRACKET, "Expected '[' after chan");
        std::string elemType = parseType();
        type = "chan[" + elemType;
        // Check for buffer size: chan[T, N]
        if (match(TokenType::COMMA)) {
            auto sizeTok = consume(TokenType::INTEGER, "Expected buffer size");
            int64_t bufSize = std::get<int64_t>(sizeTok.literal);
            type += ", " + std::to_string(bufSize);
        }
        type += "]";
        consume(TokenType::RBRACKET, "Expected ']' after channel type");
    }
    // Mutex type: Mutex[T]
    else if (match(TokenType::MUTEX)) {
        consume(TokenType::LBRACKET, "Expected '[' after Mutex");
        std::string elemType = parseType();
        type = "Mutex[" + elemType + "]";
        consume(TokenType::RBRACKET, "Expected ']' after Mutex type");
    }
    // RWLock type: RWLock[T]
    else if (match(TokenType::RWLOCK)) {
        consume(TokenType::LBRACKET, "Expected '[' after RWLock");
        std::string elemType = parseType();
        type = "RWLock[" + elemType + "]";
        consume(TokenType::RBRACKET, "Expected ']' after RWLock type");
    }
    // Cond type: Cond
    else if (match(TokenType::COND)) {
        type = "Cond";
    }
    // Semaphore type: Semaphore
    else if (match(TokenType::SEMAPHORE)) {
        type = "Semaphore";
    }
    // List type: [T] or fixed-size array: [T; N]
    else if (match(TokenType::LBRACKET)) {
        std::string elemType = parseType();
        
        // Check for fixed-size array syntax: [T; N]
        if (match(TokenType::SEMICOLON)) {
            // Parse the size
            auto sizeTok = consume(TokenType::INTEGER, "Expected array size");
            int64_t size = std::get<int64_t>(sizeTok.literal);
            type = "[" + elemType + "; " + std::to_string(size) + "]";
        } else {
            // Regular list type: [T]
            type = "[" + elemType + "]";
        }
        consume(TokenType::RBRACKET, "Expected ']' after array/list type");
    }
    // Function pointer: fn(int, int) -> int
    else if (match(TokenType::FN)) {
        type = "fn(";
        if (match(TokenType::LPAREN)) {
            bool first = true;
            while (!check(TokenType::RPAREN) && !isAtEnd()) {
                if (!first) type += ", ";
                first = false;
                // Check for variadic
                if (match(TokenType::DOTDOT)) {
                    if (match(TokenType::DOT) || check(TokenType::RPAREN)) {
                        type += "...";
                    }
                } else {
                    type += parseType();
                }
                if (!match(TokenType::COMMA)) break;
            }
            consume(TokenType::RPAREN, "Expected ')' after function parameters");
        }
        type += ")";
        if (match(TokenType::ARROW)) {
            type += " -> " + parseType();
        }
    }
    // Named type or generic
    else if (check(TokenType::IDENTIFIER)) {
        type = advance().lexeme;
        if (match(TokenType::LT)) {
            type += "<" + parseType();
            while (match(TokenType::COMMA)) {
                type += ", " + parseType();
            }
            consume(TokenType::GT, "Expected '>' after generic type");
            type += ">";
        }
    }
    
    // Nullable modifier
    if (match(TokenType::QUESTION)) {
        type += "?";
    }
    
    return type;
}

std::vector<std::pair<std::string, std::string>> Parser::parseParams() {
    std::vector<std::pair<std::string, std::string>> params;
    
    while (check(TokenType::IDENTIFIER)) {
        std::string name = advance().lexeme;
        std::string type;
        
        if (match(TokenType::COLON)) {
            if (check(TokenType::IDENTIFIER) || check(TokenType::PTR) || 
                check(TokenType::REF) || check(TokenType::LBRACKET) ||
                check(TokenType::STAR) || check(TokenType::FN)) {
                type = parseType();
            } else {
                current--;
                params.emplace_back(name, "");
                break;
            }
        }
        
        params.emplace_back(name, type);
        if (!match(TokenType::COMMA)) break;
    }
    
    return params;
}

void Parser::parseCallArgs(CallExpr* call) {
    if (check(TokenType::RPAREN)) return;
    
    do {
        skipNewlines();
        if (check(TokenType::IDENTIFIER)) {
            size_t saved = current;
            auto name = advance().lexeme;
            if (match(TokenType::COLON)) {
                auto value = expression();
                call->namedArgs.emplace_back(name, std::move(value));
                continue;
            }
            current = saved;
        }
        call->args.push_back(expression());
    } while (match(TokenType::COMMA));
    skipNewlines();
}

std::string Parser::captureRawBlock() {
    std::string content;
    
    consume(TokenType::INDENT, "Expected indented DSL block");
    
    int depth = 1;
    
    while (depth > 0 && !isAtEnd()) {
        if (check(TokenType::INDENT)) {
            depth++;
            advance();
            content += "\n";
        } else if (check(TokenType::DEDENT)) {
            depth--;
            if (depth > 0) {
                advance();
                content += "\n";
            }
        } else if (check(TokenType::NEWLINE)) {
            advance();
            content += "\n";
        } else {
            if (!content.empty() && content.back() != '\n' && content.back() != ' ') {
                content += " ";
            }
            content += peek().lexeme;
            advance();
        }
    }
    
    if (check(TokenType::DEDENT)) {
        advance();
    }
    
    while (!content.empty() && (content.back() == ' ' || content.back() == '\n')) {
        content.pop_back();
    }
    
    return content;
}

ExprPtr Parser::parseDSLBlock(const std::string& dslName, SourceLocation loc) {
    std::string rawContent = captureRawBlock();
    return std::make_unique<DSLBlock>(dslName, rawContent, loc);
}

} // namespace flex
