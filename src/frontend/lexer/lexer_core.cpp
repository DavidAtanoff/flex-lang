// Flex Compiler - Lexer Core
// Keywords, token creation, core methods

#include "lexer_base.h"

namespace flex {

const std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"fn", TokenType::FN}, {"if", TokenType::IF}, {"else", TokenType::ELSE},
    {"elif", TokenType::ELIF}, {"for", TokenType::FOR}, {"while", TokenType::WHILE},
    {"match", TokenType::MATCH}, {"return", TokenType::RETURN},
    {"true", TokenType::TRUE}, {"false", TokenType::FALSE}, {"nil", TokenType::NIL},
    {"and", TokenType::AND}, {"or", TokenType::OR}, {"not", TokenType::NOT},
    {"in", TokenType::IN}, {"to", TokenType::TO}, {"by", TokenType::BY},
    {"try", TokenType::TRY}, {"use", TokenType::USE}, {"layer", TokenType::LAYER},
    {"macro", TokenType::MACRO}, {"import", TokenType::IMPORT}, {"module", TokenType::MODULE}, {"extern", TokenType::EXTERN},
    {"async", TokenType::ASYNC}, {"await", TokenType::AWAIT}, {"spawn", TokenType::SPAWN},
    {"record", TokenType::RECORD}, {"enum", TokenType::ENUM},
    {"let", TokenType::LET}, {"mut", TokenType::MUT}, {"const", TokenType::CONST},
    {"unsafe", TokenType::UNSAFE}, {"ptr", TokenType::PTR}, {"ref", TokenType::REF},
    {"new", TokenType::NEW}, {"delete", TokenType::DELETE},
    {"break", TokenType::BREAK}, {"continue", TokenType::CONTINUE},
    {"type", TokenType::TYPE}, {"alias", TokenType::ALIAS}, {"syntax", TokenType::SYNTAX},
    {"pub", TokenType::PUB}, {"priv", TokenType::PRIV},
    {"self", TokenType::SELF}, {"super", TokenType::SUPER},
    {"trait", TokenType::TRAIT}, {"impl", TokenType::IMPL}
};

Lexer::Lexer(const std::string& src, const std::string& fname)
    : source(src), filename(fname) {
    indentStack.push(0);
}

char Lexer::advance() {
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
        lineStart = current;
    } else {
        column++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[current] != expected) return false;
    advance();
    return true;
}

void Lexer::addToken(TokenType type) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, SourceLocation{filename, line, (int)(start - lineStart + 1)});
}

void Lexer::addToken(TokenType type, int64_t value) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, SourceLocation{filename, line, (int)(start - lineStart + 1)}, value);
}

void Lexer::addToken(TokenType type, double value) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, SourceLocation{filename, line, (int)(start - lineStart + 1)}, value);
}

void Lexer::addToken(TokenType type, const std::string& value) {
    std::string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, SourceLocation{filename, line, (int)(start - lineStart + 1)}, value);
}

} // namespace flex
