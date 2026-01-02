// Flex Compiler - Pratt Parser Expression Implementation
// Uses Pratt parsing (top-down operator precedence) for clean, extensible expression parsing

#include "parser_base.h"
#include "frontend/macro/syntax_macro.h"
#include "frontend/lexer/lexer.h"
#include "common/errors.h"
#include <unordered_set>

namespace flex {

static Parser::Precedence getInfixPrecedence(TokenType type) {
    switch (type) {
        case TokenType::ASSIGN:
        case TokenType::PLUS_ASSIGN:
        case TokenType::MINUS_ASSIGN:
        case TokenType::STAR_ASSIGN:
        case TokenType::SLASH_ASSIGN: return Parser::Precedence::ASSIGNMENT;
        case TokenType::QUESTION_QUESTION: return Parser::Precedence::NULL_COALESCE;
        case TokenType::OR:
        case TokenType::PIPE_PIPE: return Parser::Precedence::OR;
        case TokenType::AND:
        case TokenType::AMP_AMP: return Parser::Precedence::AND;
        case TokenType::PIPE: return Parser::Precedence::BIT_OR;
        case TokenType::CARET: return Parser::Precedence::BIT_XOR;
        case TokenType::AMP: return Parser::Precedence::BIT_AND;
        case TokenType::EQ:
        case TokenType::NE: return Parser::Precedence::EQUALITY;
        case TokenType::LT:
        case TokenType::GT:
        case TokenType::LE:
        case TokenType::GE:
        case TokenType::SPACESHIP: return Parser::Precedence::COMPARISON;
        case TokenType::DOTDOT: return Parser::Precedence::RANGE;
        case TokenType::PLUS:
        case TokenType::MINUS: return Parser::Precedence::TERM;
        case TokenType::STAR:
        case TokenType::SLASH:
        case TokenType::PERCENT: return Parser::Precedence::FACTOR;
        case TokenType::CUSTOM_OP: return Parser::Precedence::FACTOR; // Custom ops get factor precedence by default
        case TokenType::PIPE_GT: return Parser::Precedence::PIPE;
        case TokenType::QUESTION: return Parser::Precedence::TERNARY;
        case TokenType::DOT:
        case TokenType::LBRACKET:
        case TokenType::LPAREN: return Parser::Precedence::POSTFIX;
        default: return Parser::Precedence::NONE;
    }
}

// Check if token is an infix operator
static bool isInfixOperator(TokenType type) {
    return getInfixPrecedence(type) != Parser::Precedence::NONE;
}

// Main Pratt parser entry point
ExprPtr Parser::expression() {
    return parsePrecedence(Precedence::ASSIGNMENT);
}

// Core Pratt parsing loop
ExprPtr Parser::parsePrecedence(Precedence minPrec) {
    // Parse prefix (includes primary expressions)
    ExprPtr left = parsePrefix();
    
    // Parse infix operators while they have higher precedence
    while (!isAtEnd()) {
        Precedence prec = getInfixPrecedence(peek().type);
        
        // Check for user-defined infix operators
        if (check(TokenType::IDENTIFIER)) {
            auto& registry = SyntaxMacroRegistry::instance();
            if (registry.isUserInfixOperator(peek().lexeme)) {
                prec = Precedence::COMPARISON; // User operators get comparison precedence
            }
        }
        
        // Check for ternary (expr if cond else other)
        if (check(TokenType::IF) && minPrec <= Precedence::TERNARY) {
            left = parseTernary(std::move(left));
            continue;
        }
        
        // Check for 'as' cast
        if (check(TokenType::IDENTIFIER) && peek().lexeme == "as") {
            left = parseCast(std::move(left));
            continue;
        }
        
        if (prec == Precedence::NONE || prec < minPrec) break;
        
        left = parseInfix(std::move(left), prec);
    }
    
    return left;
}

// Parse prefix expressions (unary operators and primary)
ExprPtr Parser::parsePrefix() {
    auto loc = peek().location;
    
    // Unary operators
    if (match({TokenType::MINUS, TokenType::NOT, TokenType::BANG, TokenType::TILDE})) {
        auto op = previous().type;
        auto operand = parsePrecedence(Precedence::UNARY);
        return std::make_unique<UnaryExpr>(op, std::move(operand), loc);
    }
    
    // Address-of
    if (match(TokenType::AMP)) {
        auto operand = parsePrecedence(Precedence::UNARY);
        return std::make_unique<AddressOfExpr>(std::move(operand), loc);
    }
    
    // Dereference
    if (match(TokenType::STAR)) {
        auto operand = parsePrecedence(Precedence::UNARY);
        return std::make_unique<DerefExpr>(std::move(operand), loc);
    }
    
    // Await
    if (match(TokenType::AWAIT)) {
        auto operand = parsePrecedence(Precedence::UNARY);
        return std::make_unique<AwaitExpr>(std::move(operand), loc);
    }
    
    // Spawn
    if (match(TokenType::SPAWN)) {
        auto operand = parsePrecedence(Precedence::UNARY);
        return std::make_unique<SpawnExpr>(std::move(operand), loc);
    }
    
    // New expression
    if (match(TokenType::NEW)) {
        return parseNew(loc);
    }
    
    return primary();
}

// Parse infix expressions
ExprPtr Parser::parseInfix(ExprPtr left, Precedence prec) {
    auto loc = peek().location;
    TokenType op = peek().type;
    
    // Handle CUSTOM_OP tokens (like **)
    if (check(TokenType::CUSTOM_OP)) {
        std::string opSymbol = peek().lexeme;
        advance();
        auto right = parsePrecedence(static_cast<Precedence>(static_cast<int>(prec) + 1));
        
        // Check if there's a user-defined infix macro for this operator
        auto& registry = SyntaxMacroRegistry::instance();
        if (registry.isUserInfixOperator(opSymbol)) {
            auto opIdent = std::make_unique<Identifier>("__infix_" + opSymbol, loc);
            auto call = std::make_unique<CallExpr>(std::move(opIdent), loc);
            call->args.push_back(std::move(left));
            call->args.push_back(std::move(right));
            return call;
        }
        
        // No macro defined - create a call to a function named after the operator
        // e.g., ** becomes __op_starstar
        std::string funcName = "__op_";
        for (char c : opSymbol) {
            switch (c) {
                case '*': funcName += "star"; break;
                case '+': funcName += "plus"; break;
                case '-': funcName += "minus"; break;
                case '/': funcName += "slash"; break;
                case '%': funcName += "percent"; break;
                case '<': funcName += "lt"; break;
                case '>': funcName += "gt"; break;
                case '=': funcName += "eq"; break;
                case '!': funcName += "bang"; break;
                case '&': funcName += "amp"; break;
                case '|': funcName += "pipe"; break;
                case '^': funcName += "caret"; break;
                case '~': funcName += "tilde"; break;
                case '@': funcName += "at"; break;
                default: funcName += c; break;
            }
        }
        auto opIdent = std::make_unique<Identifier>(funcName, loc);
        auto call = std::make_unique<CallExpr>(std::move(opIdent), loc);
        call->args.push_back(std::move(left));
        call->args.push_back(std::move(right));
        return call;
    }
    
    // Handle user-defined infix operators (identifier-based)
    if (check(TokenType::IDENTIFIER)) {
        auto& registry = SyntaxMacroRegistry::instance();
        std::string opSymbol = peek().lexeme;
        if (registry.isUserInfixOperator(opSymbol)) {
            advance();
            auto right = parsePrecedence(static_cast<Precedence>(static_cast<int>(prec) + 1));
            auto opIdent = std::make_unique<Identifier>("__infix_" + opSymbol, loc);
            auto call = std::make_unique<CallExpr>(std::move(opIdent), loc);
            call->args.push_back(std::move(left));
            call->args.push_back(std::move(right));
            return call;
        }
    }
    
    advance(); // consume operator
    
    // Postfix operators (left-associative, special handling)
    if (op == TokenType::DOT) {
        return parseMemberAccess(std::move(left), loc);
    }
    if (op == TokenType::LBRACKET) {
        return parseIndexAccess(std::move(left), loc);
    }
    if (op == TokenType::LPAREN) {
        return parseCall(std::move(left), loc);
    }
    
    // Standard Ternary: condition ? then : else
    // OR Postfix error propagation: expr?
    if (op == TokenType::QUESTION) {
        // Check if this is postfix ? (error propagation) or ternary ?:
        // If the next token cannot start an expression, it's postfix ?
        // Common cases: newline, ), ], }, ;, ,, operators, EOF
        TokenType next = peek().type;
        bool isPostfix = (next == TokenType::NEWLINE || next == TokenType::RPAREN ||
                          next == TokenType::RBRACKET || next == TokenType::RBRACE ||
                          next == TokenType::SEMICOLON || next == TokenType::COMMA ||
                          next == TokenType::END_OF_FILE || next == TokenType::DEDENT ||
                          next == TokenType::COLON);  // : alone means postfix, not ternary
        
        if (isPostfix) {
            // Postfix ? - error propagation operator
            return std::make_unique<PropagateExpr>(std::move(left), loc);
        }
        
        // Ternary: condition ? then : else
        auto thenExpr = parsePrecedence(Precedence::TERNARY);
        consume(TokenType::COLON, "Expected ':' in ternary expression");
        auto elseExpr = parsePrecedence(Precedence::TERNARY);
        return std::make_unique<TernaryExpr>(std::move(left), std::move(thenExpr), 
                                              std::move(elseExpr), loc);
    }
    
    // Postfix !
    if (op == TokenType::BANG) {
        return std::make_unique<UnaryExpr>(op, std::move(left), loc);
    }
    
    // Compound Assignment
    if (op == TokenType::ASSIGN || op == TokenType::PLUS_ASSIGN || op == TokenType::MINUS_ASSIGN ||
        op == TokenType::STAR_ASSIGN || op == TokenType::SLASH_ASSIGN) {
        auto right = parsePrecedence(Precedence::ASSIGNMENT);
        return std::make_unique<AssignExpr>(std::move(left), op, std::move(right), loc);
    }
    
    // Pipe operator (special: transforms into function call)
    if (op == TokenType::PIPE_GT) {
        auto right = parsePrecedence(static_cast<Precedence>(static_cast<int>(prec) + 1));
        return parsePipe(std::move(left), std::move(right), loc);
    }
    
    // Range operator (can have optional 'by' step)
    if (op == TokenType::DOTDOT) {
        auto end = parsePrecedence(static_cast<Precedence>(static_cast<int>(Precedence::RANGE) + 1));
        ExprPtr step = nullptr;
        if (match(TokenType::BY)) {
            step = parsePrecedence(static_cast<Precedence>(static_cast<int>(Precedence::RANGE) + 1));
        }
        return std::make_unique<RangeExpr>(std::move(left), std::move(end), std::move(step), loc);
    }
    
    // Spaceship operator with user override check
    if (op == TokenType::SPACESHIP) {
        auto right = parsePrecedence(static_cast<Precedence>(static_cast<int>(prec) + 1));
        auto& registry = SyntaxMacroRegistry::instance();
        if (registry.isUserInfixOperator("<=>")) {
            auto opIdent = std::make_unique<Identifier>("__infix_<=>", loc);
            auto call = std::make_unique<CallExpr>(std::move(opIdent), loc);
            call->args.push_back(std::move(left));
            call->args.push_back(std::move(right));
            return call;
        }
        return std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), loc);
    }
    
    // Standard binary operators (left-associative)
    auto right = parsePrecedence(static_cast<Precedence>(static_cast<int>(prec) + 1));
    
    // Normalize OR/AND variants
    if (op == TokenType::PIPE_PIPE) op = TokenType::OR;
    if (op == TokenType::AMP_AMP) op = TokenType::AND;
    
    return std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), loc);
}

// Parse ternary: value if condition else other
ExprPtr Parser::parseTernary(ExprPtr thenExpr) {
    advance(); // consume 'if'
    auto condition = parsePrecedence(Precedence::TERNARY);
    consume(TokenType::ELSE, "Expected 'else' in ternary expression");
    auto elseExpr = parsePrecedence(Precedence::TERNARY);
    return std::make_unique<TernaryExpr>(std::move(condition), std::move(thenExpr), 
                                          std::move(elseExpr), thenExpr->location);
}

// Parse cast: expr as Type
ExprPtr Parser::parseCast(ExprPtr expr) {
    auto loc = peek().location;
    advance(); // consume 'as'
    auto targetType = parseType();
    return std::make_unique<CastExpr>(std::move(expr), targetType, loc);
}

// Parse member access: expr.member or expr.method()
ExprPtr Parser::parseMemberAccess(ExprPtr object, SourceLocation loc) {
    auto member = consume(TokenType::IDENTIFIER, "Expected member name after '.'").lexeme;
    
    // Check for method call
    if (match(TokenType::LPAREN)) {
        auto memberExpr = std::make_unique<MemberExpr>(std::move(object), member, loc);
        auto call = std::make_unique<CallExpr>(std::move(memberExpr), loc);
        parseCallArgs(call.get());
        consume(TokenType::RPAREN, "Expected ')' after method arguments");
        return call;
    }
    
    return std::make_unique<MemberExpr>(std::move(object), member, loc);
}

// Parse function call: expr(args) or expr[TypeArgs](args)
ExprPtr Parser::parseCall(ExprPtr callee, SourceLocation loc) {
    auto call = std::make_unique<CallExpr>(std::move(callee), loc);
    parseCallArgs(call.get());
    consume(TokenType::RPAREN, "Expected ')' after arguments");
    return call;
}

// Parse index access or explicit type arguments followed by call
ExprPtr Parser::parseIndexAccess(ExprPtr object, SourceLocation loc) {
    // Check if this might be explicit type arguments for a generic call
    // Pattern: identifier[Type1, Type2](args)
    // We need to look ahead to see if there's a ( after the ]
    
    // First, check if the object is an identifier (potential generic function)
    bool mightBeTypeArgs = dynamic_cast<Identifier*>(object.get()) != nullptr;
    
    if (mightBeTypeArgs) {
        // Save position to potentially backtrack
        size_t savedPos = current;
        
        // Try to parse as type arguments
        std::vector<std::string> typeArgs;
        bool validTypeArgs = true;
        
        // Parse type arguments
        do {
            skipNewlines();
            if (check(TokenType::IDENTIFIER)) {
                std::string typeArg = parseType();
                if (!typeArg.empty()) {
                    typeArgs.push_back(typeArg);
                } else {
                    validTypeArgs = false;
                    break;
                }
            } else {
                validTypeArgs = false;
                break;
            }
        } while (match(TokenType::COMMA));
        
        if (validTypeArgs && check(TokenType::RBRACKET)) {
            advance(); // consume ]
            
            // Check if followed by (
            if (check(TokenType::LPAREN)) {
                advance(); // consume (
                auto call = std::make_unique<CallExpr>(std::move(object), loc);
                call->typeArgs = std::move(typeArgs);
                parseCallArgs(call.get());
                consume(TokenType::RPAREN, "Expected ')' after arguments");
                return call;
            }
        }
        
        // Not type arguments - restore position and parse as index
        current = savedPos;
    }
    
    // Regular index access
    auto index = expression();
    consume(TokenType::RBRACKET, "Expected ']' after index");
    return std::make_unique<IndexExpr>(std::move(object), std::move(index), loc);
}

// Parse pipe: left |> right  ->  right(left) or right.call(left, existing_args...)
ExprPtr Parser::parsePipe(ExprPtr left, ExprPtr right, SourceLocation loc) {
    if (auto* existingCall = dynamic_cast<CallExpr*>(right.get())) {
        // Insert left as first argument
        std::vector<ExprPtr> newArgs;
        newArgs.push_back(std::move(left));
        for (auto& arg : existingCall->args) {
            newArgs.push_back(std::move(arg));
        }
        existingCall->args = std::move(newArgs);
        return right;
    }
    
    // Create new call with left as argument
    auto call = std::make_unique<CallExpr>(std::move(right), loc);
    call->args.push_back(std::move(left));
    return call;
}

// Parse new expression: new Type(args) or new Type{args}
ExprPtr Parser::parseNew(SourceLocation loc) {
    auto typeName = consume(TokenType::IDENTIFIER, "Expected type name after 'new'").lexeme;
    auto newExpr = std::make_unique<NewExpr>(typeName, loc);
    
    if (match(TokenType::LPAREN)) {
        if (!check(TokenType::RPAREN)) {
            do {
                newExpr->args.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expected ')' after new arguments");
    } else if (match(TokenType::LBRACE)) {
        if (!check(TokenType::RBRACE)) {
            do {
                newExpr->args.push_back(expression());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RBRACE, "Expected '}' after new initializer");
    }
    
    return newExpr;
}


// Primary expressions (literals, identifiers, grouping)
ExprPtr Parser::primary() {
    auto loc = peek().location;
    
    // Integer literal
    if (match(TokenType::INTEGER)) {
        return std::make_unique<IntegerLiteral>(std::get<int64_t>(previous().literal), loc);
    }
    
    // Float literal
    if (match(TokenType::FLOAT)) {
        return std::make_unique<FloatLiteral>(std::get<double>(previous().literal), loc);
    }
    
    // String literal (may be interpolated)
    if (match(TokenType::STRING)) {
        std::string value = std::get<std::string>(previous().literal);
        
        // Check for interpolation markers
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
                    
                    // Parse the expression string properly instead of just creating an identifier
                    // This handles function calls, binary expressions, etc.
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
    
    // Boolean literals
    if (match(TokenType::TRUE)) {
        return std::make_unique<BoolLiteral>(true, loc);
    }
    if (match(TokenType::FALSE)) {
        return std::make_unique<BoolLiteral>(false, loc);
    }
    
    // Nil literal
    if (match(TokenType::NIL)) {
        return std::make_unique<NilLiteral>(loc);
    }
    
    // Identifier (may be DSL block or builtin function)
    if (match(TokenType::IDENTIFIER)) {
        std::string name = previous().lexeme;
        auto idLoc = previous().location;
        
        // Check for builtin functions that can be called without parentheses in expressions
        // These are functions like str, len, int, float, bool, type, etc.
        static const std::unordered_set<std::string> exprBuiltins = {
            "str", "len", "int", "float", "bool", "type", "abs", "not"
        };
        
        if (exprBuiltins.count(name) && !check(TokenType::LPAREN) && !isAtStatementBoundary() &&
            !check(TokenType::ASSIGN) && !check(TokenType::COLON) && !check(TokenType::NEWLINE) &&
            !check(TokenType::COMMA) && !check(TokenType::RPAREN) && !check(TokenType::RBRACKET) &&
            !check(TokenType::PLUS) && !check(TokenType::MINUS) && !check(TokenType::STAR) &&
            !check(TokenType::SLASH) && !check(TokenType::PERCENT)) {
            // Parse as function call with single argument
            auto callee = std::make_unique<Identifier>(name, idLoc);
            auto call = std::make_unique<CallExpr>(std::move(callee), idLoc);
            call->args.push_back(parsePrecedence(Precedence::UNARY));
            return call;
        }
        
        // Check for DSL block: name:\n INDENT content DEDENT
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
    
    // List literal or comprehension
    if (match(TokenType::LBRACKET)) {
        return listLiteral();
    }
    
    // Record literal
    if (match(TokenType::LBRACE)) {
        return recordLiteral();
    }
    
    // Grouped expression or tuple
    if (match(TokenType::LPAREN)) {
        auto expr = expression();
        
        // Tuple: (a, b, c)
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
    
    // Lambda: |params| => body
    if (match(TokenType::PIPE)) {
        return lambda();
    }
    
    // Try expression: try expr else default
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

// List literal or list comprehension
ExprPtr Parser::listLiteral() {
    auto loc = previous().location;
    auto list = std::make_unique<ListExpr>(loc);
    
    skipNewlines();
    if (!check(TokenType::RBRACKET)) {
        auto first = expression();
        
        // List comprehension: [expr for var in iterable if condition]
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

// Record literal: {field: value, ...} or Map literal: {"key": value, ...}
ExprPtr Parser::recordLiteral() {
    auto loc = previous().location;
    
    skipNewlines();
    
    // Empty braces - return empty record
    if (check(TokenType::RBRACE)) {
        advance();
        return std::make_unique<RecordExpr>(loc);
    }
    
    // Peek at first token to determine if this is a map (string key) or record (identifier key)
    if (check(TokenType::STRING)) {
        // This is a map literal: {"key": value, ...}
        auto map = std::make_unique<MapExpr>(loc);
        do {
            skipNewlines();
            if (check(TokenType::RBRACE)) break;
            
            // Parse key (must be a string for maps)
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
    
    // This is a record literal: {field: value, ...}
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

// Lambda expression: |params| => body
ExprPtr Parser::lambda() {
    auto loc = previous().location;
    auto lam = std::make_unique<LambdaExpr>(loc);
    
    // Parse parameters
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
    match(TokenType::DOUBLE_ARROW); // Optional =>
    
    lam->body = expression();
    return lam;
}

// Legacy compatibility wrappers (redirect to Pratt parser)
ExprPtr Parser::assignment() { return parsePrecedence(Precedence::ASSIGNMENT); }
ExprPtr Parser::ternary() { return parsePrecedence(Precedence::TERNARY); }
ExprPtr Parser::nullCoalesce() { return parsePrecedence(Precedence::NULL_COALESCE); }
ExprPtr Parser::userInfixExpr() { return parsePrecedence(Precedence::COMPARISON); }
ExprPtr Parser::pipeExpr() { return parsePrecedence(Precedence::PIPE); }
ExprPtr Parser::logicalOr() { return parsePrecedence(Precedence::OR); }
ExprPtr Parser::logicalAnd() { return parsePrecedence(Precedence::AND); }
ExprPtr Parser::bitwiseOr() { return parsePrecedence(Precedence::BIT_OR); }
ExprPtr Parser::bitwiseXor() { return parsePrecedence(Precedence::BIT_XOR); }
ExprPtr Parser::bitwiseAnd() { return parsePrecedence(Precedence::BIT_AND); }
ExprPtr Parser::equality() { return parsePrecedence(Precedence::EQUALITY); }
ExprPtr Parser::comparison() { return parsePrecedence(Precedence::COMPARISON); }
ExprPtr Parser::range() { return parsePrecedence(Precedence::RANGE); }
ExprPtr Parser::term() { return parsePrecedence(Precedence::TERM); }
ExprPtr Parser::factor() { return parsePrecedence(Precedence::FACTOR); }
ExprPtr Parser::unary() { return parsePrefix(); }
ExprPtr Parser::postfix() { return parsePrecedence(Precedence::POSTFIX); }

} // namespace flex
