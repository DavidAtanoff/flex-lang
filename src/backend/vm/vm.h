// Flex Compiler - Virtual Machine
#ifndef FLEX_VM_H
#define FLEX_VM_H

#include "backend/bytecode/bytecode.h"
#include <stack>

namespace flex {

struct CallFrame {
    size_t ip;
    size_t stackBase;
    std::shared_ptr<FlexFunction> function;
};

class VM {
public:
    VM();
    void run(Chunk& chunk);
    void setDebug(bool d) { debug = d; }
    
private:
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> globals;
    std::vector<CallFrame> frames;
    Chunk* chunk = nullptr;
    size_t ip = 0;
    bool debug = false;
    
    void push(Value val);
    Value pop();
    Value& peek(int distance = 0);
    void execute();
    void callFunction(std::shared_ptr<FlexFunction> func, int argCount);
    void returnFromFunction();
    void registerBuiltins();
    Value binaryOp(OpCode op, const Value& a, const Value& b);
    void runtimeError(const std::string& msg);
};

} // namespace flex

#endif // FLEX_VM_H
