// Flex Compiler - VM Core Implementation
// Constructor, stack operations, run entry point

#include "vm_base.h"
#include <cmath>

namespace flex {

VM::VM() {
    registerBuiltins();
}

void VM::push(Value val) {
    stack.push_back(std::move(val));
}

Value VM::pop() {
    if (stack.empty()) {
        runtimeError("Stack underflow");
    }
    Value val = std::move(stack.back());
    stack.pop_back();
    return val;
}

Value& VM::peek(int distance) {
    return stack[stack.size() - 1 - distance];
}

void VM::runtimeError(const std::string& msg) {
    std::cerr << "Runtime error at instruction " << ip << ": " << msg << std::endl;
    throw FlexError(msg);
}

void VM::run(Chunk& ch) {
    chunk = &ch;
    ip = 0;
    stack.clear();
    frames.clear();
    execute();
}

Value VM::binaryOp(OpCode op, const Value& a, const Value& b) {
    if (op == OpCode::ADD && (a.type == ValueType::STRING || b.type == ValueType::STRING)) {
        return Value(a.toString() + b.toString());
    }
    
    bool useFloat = (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT);
    
    if (useFloat) {
        double av = a.type == ValueType::FLOAT ? a.floatVal : (double)a.intVal;
        double bv = b.type == ValueType::FLOAT ? b.floatVal : (double)b.intVal;
        
        switch (op) {
            case OpCode::ADD: return Value(av + bv);
            case OpCode::SUB: return Value(av - bv);
            case OpCode::MUL: return Value(av * bv);
            case OpCode::DIV: return Value(av / bv);
            case OpCode::MOD: return Value(std::fmod(av, bv));
            case OpCode::LT: return Value(av < bv);
            case OpCode::GT: return Value(av > bv);
            case OpCode::LE: return Value(av <= bv);
            case OpCode::GE: return Value(av >= bv);
            default: break;
        }
    } else if (a.type == ValueType::INT && b.type == ValueType::INT) {
        int64_t av = a.intVal;
        int64_t bv = b.intVal;
        
        switch (op) {
            case OpCode::ADD: return Value(av + bv);
            case OpCode::SUB: return Value(av - bv);
            case OpCode::MUL: return Value(av * bv);
            case OpCode::DIV: return Value(bv != 0 ? av / bv : (int64_t)0);
            case OpCode::MOD: return Value(bv != 0 ? av % bv : (int64_t)0);
            case OpCode::LT: return Value(av < bv);
            case OpCode::GT: return Value(av > bv);
            case OpCode::LE: return Value(av <= bv);
            case OpCode::GE: return Value(av >= bv);
            default: break;
        }
    }
    
    if (op == OpCode::EQ) return Value(a == b);
    if (op == OpCode::NE) return Value(!(a == b));
    if (op == OpCode::AND) return Value(a.isTruthy() && b.isTruthy());
    if (op == OpCode::OR) return Value(a.isTruthy() || b.isTruthy());
    
    runtimeError("Invalid operands for binary operation");
    return Value();
}

void VM::callFunction(std::shared_ptr<FlexFunction> func, int argCount) {
    if (argCount != (int)func->params.size()) {
        runtimeError("Expected " + std::to_string(func->params.size()) + 
                    " arguments but got " + std::to_string(argCount));
    }
    
    CallFrame frame;
    frame.ip = ip;
    frame.stackBase = stack.size() - argCount - 1;
    frame.function = func;
    
    stack.erase(stack.begin() + frame.stackBase);
    frame.stackBase = stack.size() - argCount;
    
    frames.push_back(frame);
    ip = func->codeStart;
}

void VM::returnFromFunction() {
    Value result = pop();
    
    CallFrame& frame = frames.back();
    
    while (stack.size() > frame.stackBase) {
        pop();
    }
    
    ip = frame.ip;
    frames.pop_back();
    
    push(std::move(result));
}

} // namespace flex
