// Flex Compiler - Token definitions
#ifndef FLEX_TOKEN_H
#define FLEX_TOKEN_H

#include "common/common.h"

namespace flex {

enum class TokenType {
    INTEGER, FLOAT, STRING, IDENTIFIER,
    FN, IF, ELSE, ELIF, FOR, WHILE, MATCH, RETURN,
    TRUE, FALSE, NIL, AND, OR, NOT, IN, TO, BY,
    TRY, ELSE_KW, USE, LAYER, MACRO, IMPORT, MODULE,
    EXTERN, ASYNC, AWAIT, SPAWN, RECORD, ENUM,
    LET, MUT, CONST, VAR,
    UNSAFE, PTR, REF, NEW, DELETE,
    BREAK, CONTINUE,
    TYPE, ALIAS, SYNTAX,
    PUB, PRIV, SELF, SUPER, TRAIT, IMPL,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EQ, NE, LT, GT, LE, GE,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN,
    DOT, DOTDOT, ARROW, DOUBLE_ARROW,
    AMP, PIPE, CARET, TILDE,
    AMP_AMP, PIPE_PIPE,
    QUESTION, BANG, AT, DOUBLE_COLON, PIPE_GT, QUESTION_QUESTION, DOLLAR, SPACESHIP,
    COLON, COMMA, SEMICOLON, LPAREN, RPAREN,
    LBRACKET, RBRACKET, LBRACE, RBRACE,
    NEWLINE, INDENT, DEDENT,
    CUSTOM_OP,
    END_OF_FILE, ERROR
};

inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INTEGER: return "INTEGER"; case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING"; case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::FN: return "FN"; case TokenType::IF: return "IF"; case TokenType::ELSE: return "ELSE";
        case TokenType::ELIF: return "ELIF"; case TokenType::FOR: return "FOR"; case TokenType::WHILE: return "WHILE";
        case TokenType::MATCH: return "MATCH"; case TokenType::RETURN: return "RETURN";
        case TokenType::TRUE: return "TRUE"; case TokenType::FALSE: return "FALSE"; case TokenType::NIL: return "NIL";
        case TokenType::AND: return "AND"; case TokenType::OR: return "OR"; case TokenType::NOT: return "NOT";
        case TokenType::IN: return "IN"; case TokenType::TO: return "TO"; case TokenType::BY: return "BY";
        case TokenType::TRY: return "TRY"; case TokenType::ELSE_KW: return "ELSE_KW"; case TokenType::USE: return "USE";
        case TokenType::LAYER: return "LAYER"; case TokenType::MACRO: return "MACRO"; case TokenType::IMPORT: return "IMPORT";
        case TokenType::MODULE: return "MODULE";
        case TokenType::EXTERN: return "EXTERN"; case TokenType::ASYNC: return "ASYNC"; case TokenType::AWAIT: return "AWAIT";
        case TokenType::SPAWN: return "SPAWN"; case TokenType::RECORD: return "RECORD"; case TokenType::ENUM: return "ENUM";
        case TokenType::LET: return "LET"; case TokenType::MUT: return "MUT"; case TokenType::CONST: return "CONST";
        case TokenType::VAR: return "VAR"; case TokenType::UNSAFE: return "UNSAFE"; case TokenType::PTR: return "PTR";
        case TokenType::REF: return "REF"; case TokenType::NEW: return "NEW"; case TokenType::DELETE: return "DELETE";
        case TokenType::BREAK: return "BREAK"; case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::TYPE: return "TYPE"; case TokenType::ALIAS: return "ALIAS"; case TokenType::SYNTAX: return "SYNTAX";
        case TokenType::PUB: return "PUB"; case TokenType::PRIV: return "PRIV"; case TokenType::SELF: return "SELF";
        case TokenType::SUPER: return "SUPER"; case TokenType::TRAIT: return "TRAIT"; case TokenType::IMPL: return "IMPL";
        case TokenType::PLUS: return "PLUS"; case TokenType::MINUS: return "MINUS"; case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH"; case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQ: return "EQ"; case TokenType::NE: return "NE"; case TokenType::LT: return "LT";
        case TokenType::GT: return "GT"; case TokenType::LE: return "LE"; case TokenType::GE: return "GE";
        case TokenType::ASSIGN: return "ASSIGN"; case TokenType::PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN: return "MINUS_ASSIGN"; case TokenType::STAR_ASSIGN: return "STAR_ASSIGN";
        case TokenType::SLASH_ASSIGN: return "SLASH_ASSIGN"; case TokenType::DOT: return "DOT";
        case TokenType::DOTDOT: return "DOTDOT"; case TokenType::ARROW: return "ARROW";
        case TokenType::DOUBLE_ARROW: return "DOUBLE_ARROW"; case TokenType::AMP: return "AMP";
        case TokenType::PIPE: return "PIPE"; case TokenType::CARET: return "CARET"; case TokenType::TILDE: return "TILDE";
        case TokenType::AMP_AMP: return "AMP_AMP"; case TokenType::PIPE_PIPE: return "PIPE_PIPE";
        case TokenType::QUESTION: return "QUESTION"; case TokenType::BANG: return "BANG"; case TokenType::AT: return "AT";
        case TokenType::DOUBLE_COLON: return "DOUBLE_COLON"; case TokenType::PIPE_GT: return "PIPE_GT";
        case TokenType::QUESTION_QUESTION: return "QUESTION_QUESTION"; case TokenType::DOLLAR: return "DOLLAR";
        case TokenType::SPACESHIP: return "SPACESHIP"; case TokenType::COLON: return "COLON";
        case TokenType::COMMA: return "COMMA"; case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::LPAREN: return "LPAREN"; case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACKET: return "LBRACKET"; case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LBRACE: return "LBRACE"; case TokenType::RBRACE: return "RBRACE";
        case TokenType::NEWLINE: return "NEWLINE"; case TokenType::INDENT: return "INDENT";
        case TokenType::DEDENT: return "DEDENT"; case TokenType::CUSTOM_OP: return "CUSTOM_OP";
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ERROR: return "ERROR"; default: return "UNKNOWN";
    }
}

class Token {
public:
    TokenType type;
    std::string lexeme;
    SourceLocation location;
    std::variant<std::monostate, int64_t, double, std::string> literal;
    
    Token(TokenType t, std::string lex, SourceLocation loc) : type(t), lexeme(std::move(lex)), location(loc) {}
    Token(TokenType t, std::string lex, SourceLocation loc, int64_t val) : type(t), lexeme(std::move(lex)), location(loc), literal(val) {}
    Token(TokenType t, std::string lex, SourceLocation loc, double val) : type(t), lexeme(std::move(lex)), location(loc), literal(val) {}
    Token(TokenType t, std::string lex, SourceLocation loc, std::string val) : type(t), lexeme(std::move(lex)), location(loc), literal(std::move(val)) {}
    
    std::string toString() const { return tokenTypeToString(type) + " '" + lexeme + "' at " + location.toString(); }
};

} // namespace flex

#endif // FLEX_TOKEN_H
