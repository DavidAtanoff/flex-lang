// Flex Standard Library - Map/Record Module
// Record/dictionary manipulation

#include "stdlib/flex_stdlib.h"
#include <unordered_set>

namespace flex {
namespace stdlib {

void registerMap(std::unordered_map<std::string, Value>& globals) {
    // keys(record) -> list - Get all keys
    globals["keys"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> result;
        if (args.empty() || args[0].type != ValueType::RECORD) return Value(result);
        for (const auto& [k, v] : args[0].recordVal) {
            result.push_back(Value(k));
        }
        return Value(result);
    });

    // values(record) -> list - Get all values
    globals["values"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> result;
        if (args.empty() || args[0].type != ValueType::RECORD) return Value(result);
        for (const auto& [k, v] : args[0].recordVal) {
            result.push_back(v);
        }
        return Value(result);
    });

    // entries(record) -> list - Get key-value pairs as [[k, v], ...]
    globals["entries"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> result;
        if (args.empty() || args[0].type != ValueType::RECORD) return Value(result);
        for (const auto& [k, v] : args[0].recordVal) {
            result.push_back(Value(std::vector<Value>{Value(k), v}));
        }
        return Value(result);
    });

    // has_key(record, key) -> bool - Check if key exists
    globals["has_key"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::RECORD) return Value::makeBool(false);
        std::string key = args[1].toString();
        return Value(args[0].recordVal.find(key) != args[0].recordVal.end());
    });

    // get_key(record, key, default?) -> value - Get value by key with optional default
    globals["get_key"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::RECORD) {
            return args.size() > 2 ? args[2] : Value();
        }
        std::string key = args[1].toString();
        auto it = args[0].recordVal.find(key);
        if (it != args[0].recordVal.end()) return it->second;
        return args.size() > 2 ? args[2] : Value();
    });

    // set_key(record, key, value) -> record - Set key-value (returns new record)
    globals["set_key"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        if (!args.empty() && args[0].type == ValueType::RECORD) {
            result.recordVal = args[0].recordVal;
        }
        if (args.size() >= 3) {
            result.recordVal[args[1].toString()] = args[2];
        }
        return result;
    });

    // remove_key(record, key) -> record - Remove key (returns new record)
    globals["remove_key"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        if (args.empty() || args[0].type != ValueType::RECORD) return result;
        result.recordVal = args[0].recordVal;
        if (args.size() >= 2) {
            result.recordVal.erase(args[1].toString());
        }
        return result;
    });

    // merge(record1, record2) -> record - Merge records (second overwrites first)
    globals["merge"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        for (const auto& arg : args) {
            if (arg.type == ValueType::RECORD) {
                for (const auto& [k, v] : arg.recordVal) {
                    result.recordVal[k] = v;
                }
            }
        }
        return result;
    });

    // size(collection) -> int - Get size of list, string, or record
    globals["size"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        switch (args[0].type) {
            case ValueType::STRING: return Value((int64_t)args[0].stringVal.length());
            case ValueType::LIST: return Value((int64_t)args[0].listVal.size());
            case ValueType::RECORD: return Value((int64_t)args[0].recordVal.size());
            default: return Value((int64_t)0);
        }
    });

    // is_empty(collection) -> bool - Check if collection is empty
    globals["is_empty"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value::makeBool(true);
        switch (args[0].type) {
            case ValueType::STRING: return Value(args[0].stringVal.empty());
            case ValueType::LIST: return Value(args[0].listVal.empty());
            case ValueType::RECORD: return Value(args[0].recordVal.empty());
            case ValueType::NIL: return Value::makeBool(true);
            default: return Value::makeBool(false);
        }
    });

    // from_entries(list) -> record - Create record from [[k, v], ...] pairs
    globals["from_entries"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        if (args.empty() || args[0].type != ValueType::LIST) return result;
        for (const auto& entry : args[0].listVal) {
            if (entry.type == ValueType::LIST && entry.listVal.size() >= 2) {
                result.recordVal[entry.listVal[0].toString()] = entry.listVal[1];
            }
        }
        return result;
    });

    // pick(record, keys...) -> record - Pick specific keys from record
    globals["pick"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        if (args.empty() || args[0].type != ValueType::RECORD) return result;
        for (size_t i = 1; i < args.size(); i++) {
            std::string key = args[i].toString();
            auto it = args[0].recordVal.find(key);
            if (it != args[0].recordVal.end()) {
                result.recordVal[key] = it->second;
            }
        }
        return result;
    });

    // omit(record, keys...) -> record - Omit specific keys from record
    globals["omit"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        if (args.empty() || args[0].type != ValueType::RECORD) return result;
        
        std::unordered_set<std::string> keysToOmit;
        for (size_t i = 1; i < args.size(); i++) {
            keysToOmit.insert(args[i].toString());
        }
        
        for (const auto& [k, v] : args[0].recordVal) {
            if (keysToOmit.find(k) == keysToOmit.end()) {
                result.recordVal[k] = v;
            }
        }
        return result;
    });
}

} // namespace stdlib
} // namespace flex
