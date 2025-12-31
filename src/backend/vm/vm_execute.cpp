// Flex Compiler - VM Execute Loop
// Main instruction dispatch loop

#include "vm_base.h"

namespace flex {

void VM::execute() {
    while (ip < chunk->code.size()) {
        Instruction& instr = chunk->code[ip];
        
        if (debug) {
            std::cout << "[" << ip << "] " << opCodeToString(instr.op);
            if (instr.operand != 0) std::cout << " " << instr.operand;
            std::cout << " | stack: ";
            for (auto& v : stack) std::cout << v.toString() << " ";
            std::cout << std::endl;
        }
        
        ip++;
        
        switch (instr.op) {
            case OpCode::CONST:
                push(chunk->constants[instr.operand]);
                break;
                
            case OpCode::POP:
                pop();
                break;
                
            case OpCode::DUP:
                push(peek());
                break;
                
            case OpCode::LOAD_GLOBAL: {
                std::string name = chunk->constants[instr.operand].stringVal;
                auto it = globals.find(name);
                if (it != globals.end()) {
                    push(it->second);
                } else {
                    push(Value());
                }
                break;
            }
                
            case OpCode::STORE_GLOBAL: {
                std::string name = chunk->constants[instr.operand].stringVal;
                globals[name] = peek();
                break;
            }
                
            case OpCode::LOAD_LOCAL: {
                size_t base = frames.empty() ? 0 : frames.back().stackBase;
                push(stack[base + instr.operand]);
                break;
            }
                
            case OpCode::STORE_LOCAL: {
                size_t base = frames.empty() ? 0 : frames.back().stackBase;
                stack[base + instr.operand] = peek();
                break;
            }
                
            case OpCode::ADD: case OpCode::SUB: case OpCode::MUL:
            case OpCode::DIV: case OpCode::MOD:
            case OpCode::LT: case OpCode::GT: case OpCode::LE: case OpCode::GE:
            case OpCode::EQ: case OpCode::NE:
            case OpCode::AND: case OpCode::OR: {
                Value b = pop();
                Value a = pop();
                push(binaryOp(instr.op, a, b));
                break;
            }
                
            case OpCode::NEG: {
                Value v = pop();
                if (v.type == ValueType::INT) push(Value(-v.intVal));
                else if (v.type == ValueType::FLOAT) push(Value(-v.floatVal));
                else runtimeError("Cannot negate non-numeric value");
                break;
            }
                
            case OpCode::NOT:
                push(Value(!pop().isTruthy()));
                break;
                
            case OpCode::JUMP:
                ip += instr.operand;
                break;
                
            case OpCode::JUMP_IF_FALSE:
                if (!peek().isTruthy()) ip += instr.operand;
                pop();
                break;
                
            case OpCode::JUMP_IF_TRUE:
                if (peek().isTruthy()) ip += instr.operand;
                pop();
                break;
                
            case OpCode::LOOP:
                ip -= instr.operand;
                break;
                
            case OpCode::CALL: {
                int argCount = instr.operand;
                Value callee = peek(argCount);
                
                if (callee.type == ValueType::FUNCTION) {
                    callFunction(callee.funcVal, argCount);
                } else if (callee.type == ValueType::NATIVE_FN) {
                    std::vector<Value> args;
                    for (int i = argCount - 1; i >= 0; i--) {
                        args.insert(args.begin(), pop());
                    }
                    pop();
                    Value result = callee.nativeVal(args);
                    push(std::move(result));
                } else {
                    runtimeError("Cannot call non-function value");
                }
                break;
            }
                
            case OpCode::RETURN:
                if (frames.empty()) {
                    return;
                }
                returnFromFunction();
                break;
                
            case OpCode::MAKE_LIST: {
                std::vector<Value> elements;
                for (int i = 0; i < instr.operand; i++) {
                    elements.insert(elements.begin(), pop());
                }
                push(Value(std::move(elements)));
                break;
            }
                
            case OpCode::MAKE_RECORD: {
                Value rec = Value::makeRecord();
                for (int i = 0; i < instr.operand; i++) {
                    Value val = pop();
                    Value key = pop();
                    rec.recordVal[key.stringVal] = std::move(val);
                }
                push(std::move(rec));
                break;
            }
                
            case OpCode::MAKE_RANGE: {
                Value step = pop();
                Value end = pop();
                Value start = pop();
                FlexRange r{start.intVal, end.intVal, step.intVal};
                push(Value(r));
                break;
            }
                
            case OpCode::GET_INDEX: {
                Value index = pop();
                Value obj = pop();
                if (obj.type == ValueType::LIST) {
                    int64_t idx = index.intVal;
                    if (idx < 0) idx += obj.listVal.size();
                    if (idx >= 0 && idx < (int64_t)obj.listVal.size()) {
                        push(obj.listVal[idx]);
                    } else {
                        push(Value());
                    }
                } else if (obj.type == ValueType::STRING) {
                    int64_t idx = index.intVal;
                    if (idx < 0) idx += obj.stringVal.size();
                    if (idx >= 0 && idx < (int64_t)obj.stringVal.size()) {
                        push(Value(std::string(1, obj.stringVal[idx])));
                    } else {
                        push(Value(""));
                    }
                } else if (obj.type == ValueType::RECORD) {
                    auto it = obj.recordVal.find(index.stringVal);
                    push(it != obj.recordVal.end() ? it->second : Value());
                } else {
                    runtimeError("Cannot index this type");
                }
                break;
            }
                
            case OpCode::SET_INDEX: {
                Value val = pop();
                Value index = pop();
                Value& obj = peek();
                if (obj.type == ValueType::LIST) {
                    int64_t idx = index.intVal;
                    if (idx >= 0 && idx < (int64_t)obj.listVal.size()) {
                        obj.listVal[idx] = std::move(val);
                    }
                } else if (obj.type == ValueType::RECORD) {
                    obj.recordVal[index.stringVal] = std::move(val);
                }
                break;
            }
                
            case OpCode::GET_MEMBER: {
                std::string member = chunk->constants[instr.operand].stringVal;
                Value obj = pop();
                if (obj.type == ValueType::RECORD) {
                    auto it = obj.recordVal.find(member);
                    push(it != obj.recordVal.end() ? it->second : Value());
                } else {
                    runtimeError("Cannot access member of non-record");
                }
                break;
            }
                
            case OpCode::SET_MEMBER: {
                std::string member = chunk->constants[instr.operand].stringVal;
                Value val = pop();
                Value& obj = peek();
                if (obj.type == ValueType::RECORD) {
                    obj.recordVal[member] = std::move(val);
                }
                break;
            }
                
            case OpCode::GET_ITER: {
                Value obj = pop();
                if (obj.type == ValueType::RANGE) {
                    std::vector<Value> iter;
                    iter.push_back(Value(obj.rangeVal.start));
                    iter.push_back(Value(obj.rangeVal.end));
                    iter.push_back(Value(obj.rangeVal.step));
                    push(Value(std::move(iter)));
                } else if (obj.type == ValueType::LIST) {
                    std::vector<Value> iter;
                    iter.push_back(Value((int64_t)0));
                    iter.push_back(obj);
                    push(Value(std::move(iter)));
                } else {
                    runtimeError("Cannot iterate over this type");
                }
                break;
            }
                
            case OpCode::ITER_NEXT: {
                Value& iter = peek();
                if (iter.type == ValueType::LIST && iter.listVal.size() == 3) {
                    int64_t current = iter.listVal[0].intVal;
                    int64_t end = iter.listVal[1].intVal;
                    int64_t step = iter.listVal[2].intVal;
                    
                    if ((step > 0 && current < end) || (step < 0 && current > end)) {
                        iter.listVal[0] = Value(current + step);
                        push(Value(current));
                        push(Value::makeBool(true));
                    } else {
                        push(Value());
                        push(Value::makeBool(false));
                    }
                } else if (iter.type == ValueType::LIST && iter.listVal.size() == 2) {
                    int64_t idx = iter.listVal[0].intVal;
                    Value& list = iter.listVal[1];
                    
                    if (idx < (int64_t)list.listVal.size()) {
                        Value val = list.listVal[idx];
                        iter.listVal[0] = Value(idx + 1);
                        push(std::move(val));
                        push(Value::makeBool(true));
                    } else {
                        push(Value());
                        push(Value::makeBool(false));
                    }
                } else {
                    push(Value());
                    push(Value::makeBool(false));
                }
                break;
            }
                
            case OpCode::PRINT:
                std::cout << pop().toString() << std::endl;
                break;
                
            case OpCode::HALT:
                return;
        }
    }
}

} // namespace flex
