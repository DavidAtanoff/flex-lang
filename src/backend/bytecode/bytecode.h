// Flex Compiler - Bytecode definitions
#ifndef FLEX_BYTECODE_H
#define FLEX_BYTECODE_H

#include "backend/runtime/value.h"

namespace flex {

enum class OpCode : uint8_t {
    CONST, POP, DUP,
    LOAD_GLOBAL, STORE_GLOBAL, LOAD_LOCAL, STORE_LOCAL,
    ADD, SUB, MUL, DIV, MOD, NEG,
    EQ, NE, LT, GT, LE, GE,
    NOT, AND, OR,
    JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE, LOOP,
    CALL, RETURN,
    MAKE_LIST, MAKE_RECORD, MAKE_RANGE, GET_INDEX, SET_INDEX, GET_MEMBER, SET_MEMBER,
    GET_ITER, ITER_NEXT,
    PRINT, HALT
};

struct Instruction {
    OpCode op;
    int32_t operand = 0;
    Instruction(OpCode o) : op(o) {}
    Instruction(OpCode o, int32_t arg) : op(o), operand(arg) {}
};

struct Chunk {
    std::vector<Instruction> code;
    std::vector<Value> constants;
    std::vector<int> lines;
    size_t addConstant(Value val) { constants.push_back(std::move(val)); return constants.size() - 1; }
    size_t emit(OpCode op) { code.emplace_back(op); lines.push_back(0); return code.size() - 1; }
    size_t emitOp(OpCode op, int32_t operand) { code.emplace_back(op, operand); lines.push_back(0); return code.size() - 1; }
    void patch(size_t offset, int32_t value) { code[offset].operand = value; }
    size_t currentOffset() const { return code.size(); }
};

inline std::string opCodeToString(OpCode op) {
    switch (op) {
        case OpCode::CONST: return "CONST"; case OpCode::POP: return "POP"; case OpCode::DUP: return "DUP";
        case OpCode::LOAD_GLOBAL: return "LOAD_GLOBAL"; case OpCode::STORE_GLOBAL: return "STORE_GLOBAL";
        case OpCode::LOAD_LOCAL: return "LOAD_LOCAL"; case OpCode::STORE_LOCAL: return "STORE_LOCAL";
        case OpCode::ADD: return "ADD"; case OpCode::SUB: return "SUB"; case OpCode::MUL: return "MUL";
        case OpCode::DIV: return "DIV"; case OpCode::MOD: return "MOD"; case OpCode::NEG: return "NEG";
        case OpCode::EQ: return "EQ"; case OpCode::NE: return "NE"; case OpCode::LT: return "LT";
        case OpCode::GT: return "GT"; case OpCode::LE: return "LE"; case OpCode::GE: return "GE";
        case OpCode::NOT: return "NOT"; case OpCode::AND: return "AND"; case OpCode::OR: return "OR";
        case OpCode::JUMP: return "JUMP"; case OpCode::JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE: return "JUMP_IF_TRUE"; case OpCode::LOOP: return "LOOP";
        case OpCode::CALL: return "CALL"; case OpCode::RETURN: return "RETURN";
        case OpCode::MAKE_LIST: return "MAKE_LIST"; case OpCode::MAKE_RECORD: return "MAKE_RECORD";
        case OpCode::MAKE_RANGE: return "MAKE_RANGE"; case OpCode::GET_INDEX: return "GET_INDEX";
        case OpCode::SET_INDEX: return "SET_INDEX"; case OpCode::GET_MEMBER: return "GET_MEMBER";
        case OpCode::SET_MEMBER: return "SET_MEMBER"; case OpCode::GET_ITER: return "GET_ITER";
        case OpCode::ITER_NEXT: return "ITER_NEXT"; case OpCode::PRINT: return "PRINT"; case OpCode::HALT: return "HALT";
    }
    return "UNKNOWN";
}

} // namespace flex

#endif // FLEX_BYTECODE_H
