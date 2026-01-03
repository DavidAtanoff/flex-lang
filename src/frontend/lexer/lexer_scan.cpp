// Flex Compiler - Lexer Scanning
// Token scanning methods

#include "lexer_base.h"
#include "common/errors.h"

namespace flex {

void Lexer::handleIndentation() {
    int indent = 0;
    while (!isAtEnd() && (peek() == ' ' || peek() == '\t')) {
        if (peek() == ' ') indent++;
        else indent += 4; // Tab = 4 spaces
        advance();
    }
    
    if (peek() == '\n' || (peek() == '/' && peekNext() == '/')) return;
    
    int currentIndent = indentStack.top();
    
    if (indent > currentIndent) {
        indentStack.push(indent);
        tokens.emplace_back(TokenType::INDENT, "", SourceLocation{filename, line, 1});
    } else {
        while (indent < indentStack.top()) {
            indentStack.pop();
            tokens.emplace_back(TokenType::DEDENT, "", SourceLocation{filename, line, 1});
        }
        if (indent != indentStack.top()) {
            auto diag = errors::inconsistentIndentation(SourceLocation{filename, line, 1});
            throw FlexDiagnosticError(diag);
        }
    }
    atLineStart = false;
}

void Lexer::scanComment() {
    while (peek() != '\n' && !isAtEnd()) advance();
}

void Lexer::scanString() {
    char quote = source[current - 1];
    std::string value;
    bool hasInterpolation = false;
    std::vector<std::pair<size_t, std::string>> interpolations;
    
    while (peek() != quote && !isAtEnd()) {
        if (peek() == '\n') {
            auto diag = errors::unterminatedString(currentLocation());
            throw FlexDiagnosticError(diag);
        }
        if (peek() == '\\') {
            advance();
            switch (peek()) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '{': value += '{'; break;
                default: value += peek(); break;
            }
            advance();
        } else if (peek() == '{') {
            advance();
            hasInterpolation = true;
            size_t exprStart = value.length();
            std::string expr;
            int braceDepth = 1;
            
            while (!isAtEnd() && braceDepth > 0) {
                if (peek() == '{') braceDepth++;
                else if (peek() == '}') braceDepth--;
                if (braceDepth > 0) expr += advance();
            }
            
            if (isAtEnd() && braceDepth > 0) {
                auto diag = errors::unterminatedInterpolation(currentLocation());
                throw FlexDiagnosticError(diag);
            }
            advance();
            interpolations.push_back({exprStart, expr});
            value += '\x00';
        } else {
            value += advance();
        }
    }
    
    if (isAtEnd()) {
        auto diag = errors::unterminatedString(currentLocation());
        throw FlexDiagnosticError(diag);
    }
    advance();
    
    if (hasInterpolation) {
        std::string encoded;
        size_t lastPos = 0;
        for (auto& [pos, expr] : interpolations) {
            for (size_t i = lastPos; i < pos && i < value.length(); i++) {
                if (value[i] != '\x00') encoded += value[i];
            }
            encoded += '\x01';
            encoded += expr;
            encoded += '\x02';
            lastPos = pos + 1;
        }
        for (size_t i = lastPos; i < value.length(); i++) {
            if (value[i] != '\x00') encoded += value[i];
        }
        addToken(TokenType::STRING, encoded);
    } else {
        addToken(TokenType::STRING, value);
    }
}


void Lexer::scanNumber() {
    while (isDigit(peek())) advance();
    
    bool isFloat = false;
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance();
        while (isDigit(peek())) advance();
    }
    
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        while (isDigit(peek())) advance();
    }
    
    std::string numStr = source.substr(start, current - start);
    if (isFloat) addToken(TokenType::FLOAT, std::stod(numStr));
    else addToken(TokenType::INTEGER, std::stoll(numStr));
}

void Lexer::scanIdentifier() {
    while (isAlphaNumeric(peek())) advance();
    std::string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    if (it != keywords.end()) addToken(it->second);
    else addToken(TokenType::IDENTIFIER);
}

void Lexer::scanTemplateVar() {
    while (isAlphaNumeric(peek())) advance();
    std::string text = source.substr(start, current - start);
    addToken(TokenType::IDENTIFIER, text);
}

void Lexer::scanToken() {
    char c = advance();
    
    switch (c) {
        case '(': addToken(TokenType::LPAREN); break;
        case ')': addToken(TokenType::RPAREN); break;
        case '[': addToken(TokenType::LBRACKET); break;
        case ']': addToken(TokenType::RBRACKET); break;
        case '{': addToken(TokenType::LBRACE); break;
        case '}': addToken(TokenType::RBRACE); break;
        case ',': addToken(TokenType::COMMA); break;
        case ';': addToken(TokenType::SEMICOLON); break;
        case '%':
            if (peek() == '%') {
                std::string opStr = "%";
                while (!isAtEnd() && peek() == '%') {
                    opStr += advance();
                }
                addToken(TokenType::CUSTOM_OP, opStr);
            } else {
                addToken(TokenType::PERCENT);
            }
            break;
        case '~': addToken(TokenType::TILDE); break;
        case '^':
            if (peek() == '^') {
                std::string opStr = "^";
                while (!isAtEnd() && peek() == '^') {
                    opStr += advance();
                }
                addToken(TokenType::CUSTOM_OP, opStr);
            } else {
                addToken(TokenType::CARET);
            }
            break;
        case '?':
            if (match('?')) addToken(TokenType::QUESTION_QUESTION);
            else addToken(TokenType::QUESTION);
            break;
        case '@':
            if (peek() == '@') {
                std::string opStr = "@";
                while (!isAtEnd() && peek() == '@') {
                    opStr += advance();
                }
                addToken(TokenType::CUSTOM_OP, opStr);
            } else {
                addToken(TokenType::AT);
            }
            break;
        case '$':
            if (isAlpha(peek())) scanTemplateVar();
            else addToken(TokenType::DOLLAR);
            break;
        case ':':
            if (match(':')) addToken(TokenType::DOUBLE_COLON);
            else addToken(TokenType::COLON);
            break;
        case '+':
            if (peek() == '+') {
                std::string opStr = "+";
                while (!isAtEnd() && peek() == '+') {
                    opStr += advance();
                }
                addToken(TokenType::CUSTOM_OP, opStr);
            } else {
                addToken(match('=') ? TokenType::PLUS_ASSIGN : TokenType::PLUS);
            }
            break;
        case '-':
            if (match('>')) addToken(TokenType::ARROW);
            else if (peek() == '-') {
                std::string opStr = "-";
                while (!isAtEnd() && peek() == '-') {
                    opStr += advance();
                }
                addToken(TokenType::CUSTOM_OP, opStr);
            } else {
                addToken(match('=') ? TokenType::MINUS_ASSIGN : TokenType::MINUS);
            }
            break;
        case '*':
            if (match('*')) {
                // Check for more operator chars to form custom op like *** or **=
                std::string opStr = "**";
                while (!isAtEnd() && isOperatorChar(peek())) {
                    opStr += advance();
                }
                addToken(TokenType::CUSTOM_OP, opStr);
            } else {
                addToken(match('=') ? TokenType::STAR_ASSIGN : TokenType::STAR);
            }
            break;
        case '/':
            if (match('/')) scanComment();
            else if (match('=')) addToken(TokenType::SLASH_ASSIGN);
            else addToken(TokenType::SLASH);
            break;
        case '.':
            if (match('.')) addToken(TokenType::DOTDOT);
            else addToken(TokenType::DOT);
            break;
        case '=':
            if (match('=')) addToken(TokenType::EQ);
            else if (match('>')) addToken(TokenType::DOUBLE_ARROW);
            else addToken(TokenType::ASSIGN);
            break;
        case '!':
            if (match('=')) addToken(TokenType::NE);
            else addToken(TokenType::BANG);
            break;
        case '<':
            if (match('-')) addToken(TokenType::CHAN_SEND);  // <- for channel send/receive
            else if (match('=')) {
                if (match('>')) addToken(TokenType::SPACESHIP);
                else addToken(TokenType::LE);
            } else addToken(TokenType::LT);
            break;
        case '>':
            addToken(match('=') ? TokenType::GE : TokenType::GT);
            break;
        case '&':
            if (match('&')) addToken(TokenType::AMP_AMP);
            else addToken(TokenType::AMP);
            break;
        case '|':
            if (match('|')) addToken(TokenType::PIPE_PIPE);
            else if (match('>')) addToken(TokenType::PIPE_GT);
            else addToken(TokenType::PIPE);
            break;
        case ' ': case '\t': case '\r': break;
        case '\n':
            if (!tokens.empty() && tokens.back().type != TokenType::NEWLINE &&
                tokens.back().type != TokenType::INDENT) {
                addToken(TokenType::NEWLINE);
            }
            atLineStart = true;
            break;
        case '"': case '\'': scanString(); break;
        case '#':
            // Check for attribute: #[...]
            if (peek() == '[') {
                advance();  // consume '['
                std::string attrContent;
                while (!isAtEnd() && peek() != ']') {
                    attrContent += advance();
                }
                if (!isAtEnd()) advance();  // consume ']'
                addToken(TokenType::ATTRIBUTE, attrContent);
            } else {
                // Single-line comment (Python style)
                scanComment();
            }
            break;
        default:
            if (isDigit(c)) scanNumber();
            else if (isAlpha(c)) scanIdentifier();
            else {
                auto diag = errors::unexpectedChar(c, currentLocation());
                throw FlexDiagnosticError(diag);
            }
            break;
    }
}

std::vector<Token> Lexer::tokenize() {
    while (!isAtEnd()) {
        if (atLineStart) {
            handleIndentation();
            if (isAtEnd()) break;
        }
        start = current;
        scanToken();
    }
    
    while (indentStack.size() > 1) {
        indentStack.pop();
        tokens.emplace_back(TokenType::DEDENT, "", SourceLocation{filename, line, column});
    }
    
    tokens.emplace_back(TokenType::END_OF_FILE, "", SourceLocation{filename, line, column});
    return tokens;
}

} // namespace flex
