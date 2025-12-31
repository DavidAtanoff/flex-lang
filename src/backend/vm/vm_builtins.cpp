// Flex Compiler - VM Built-in Functions
// Core builtins + standard library

#include "vm_base.h"
#include "stdlib/flex_stdlib.h"
#include <cmath>

namespace flex {

void VM::registerBuiltins() {
    // Register standard library
    stdlib::registerAll(globals);
    
    // Core builtins (these override stdlib if needed)
    globals["print"] = Value([](const std::vector<Value>& args) -> Value {
        for (size_t i = 0; i < args.size(); i++) {
            if (i > 0) std::cout << " ";
            std::cout << args[i].toString();
        }
        std::cout << std::endl;
        return Value();
    });
    
    globals["len"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        const Value& v = args[0];
        if (v.type == ValueType::STRING) return Value((int64_t)v.stringVal.length());
        if (v.type == ValueType::LIST) return Value((int64_t)v.listVal.size());
        return Value((int64_t)0);
    });
    
    globals["type"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value("nil");
        switch (args[0].type) {
            case ValueType::NIL: return Value("nil");
            case ValueType::BOOL: return Value("bool");
            case ValueType::INT: return Value("int");
            case ValueType::FLOAT: return Value("float");
            case ValueType::STRING: return Value("string");
            case ValueType::LIST: return Value("list");
            case ValueType::RECORD: return Value("record");
            case ValueType::FUNCTION: return Value("function");
            case ValueType::NATIVE_FN: return Value("native_fn");
            case ValueType::RANGE: return Value("range");
        }
        return Value("unknown");
    });
    
    globals["str"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value("");
        return Value(args[0].toString());
    });
    
    globals["int"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        const Value& v = args[0];
        if (v.type == ValueType::INT) return v;
        if (v.type == ValueType::FLOAT) return Value((int64_t)v.floatVal);
        if (v.type == ValueType::STRING) {
            try { return Value((int64_t)std::stoll(v.stringVal)); }
            catch (...) { return Value((int64_t)0); }
        }
        return Value((int64_t)0);
    });
    
    globals["float"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        const Value& v = args[0];
        if (v.type == ValueType::FLOAT) return v;
        if (v.type == ValueType::INT) return Value((double)v.intVal);
        if (v.type == ValueType::STRING) {
            try { return Value(std::stod(v.stringVal)); }
            catch (...) { return Value(0.0); }
        }
        return Value(0.0);
    });
    
    globals["abs"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        const Value& v = args[0];
        if (v.type == ValueType::INT) return Value(std::abs(v.intVal));
        if (v.type == ValueType::FLOAT) return Value(std::abs(v.floatVal));
        return Value((int64_t)0);
    });
    
    globals["min"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value();
        Value result = args[0];
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i].type == ValueType::INT && result.type == ValueType::INT) {
                if (args[i].intVal < result.intVal) result = args[i];
            } else if (args[i].type == ValueType::FLOAT || result.type == ValueType::FLOAT) {
                double a = result.type == ValueType::FLOAT ? result.floatVal : (double)result.intVal;
                double b = args[i].type == ValueType::FLOAT ? args[i].floatVal : (double)args[i].intVal;
                if (b < a) result = args[i];
            }
        }
        return result;
    });
    
    globals["max"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value();
        Value result = args[0];
        for (size_t i = 1; i < args.size(); i++) {
            if (args[i].type == ValueType::INT && result.type == ValueType::INT) {
                if (args[i].intVal > result.intVal) result = args[i];
            } else if (args[i].type == ValueType::FLOAT || result.type == ValueType::FLOAT) {
                double a = result.type == ValueType::FLOAT ? result.floatVal : (double)result.intVal;
                double b = args[i].type == ValueType::FLOAT ? args[i].floatVal : (double)args[i].intVal;
                if (b > a) result = args[i];
            }
        }
        return result;
    });
    
    globals["range"] = Value([](const std::vector<Value>& args) -> Value {
        int64_t start = 0, end = 0, step = 1;
        if (args.size() >= 1 && args[0].type == ValueType::INT) end = args[0].intVal;
        if (args.size() >= 2 && args[1].type == ValueType::INT) { start = end; end = args[1].intVal; }
        if (args.size() >= 3 && args[2].type == ValueType::INT) step = args[2].intVal;
        return Value(FlexRange{start, end, step});
    });
    
    globals["input"] = Value([](const std::vector<Value>& args) -> Value {
        if (!args.empty()) std::cout << args[0].toString();
        std::string line;
        std::getline(std::cin, line);
        return Value(line);
    });
}

} // namespace flex
