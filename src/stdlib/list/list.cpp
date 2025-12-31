// Flex Standard Library - List Module
// List manipulation, functional operations

#include "stdlib/flex_stdlib.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <unordered_set>

namespace flex {
namespace stdlib {

void registerList(std::unordered_map<std::string, Value>& globals) {
    // push(list, item) -> list - Add item to end (returns new list)
    globals["push"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return args.size() > 1 ? Value(std::vector<Value>{args[1]}) : Value(std::vector<Value>{});
        }
        std::vector<Value> result = args[0].listVal;
        if (args.size() > 1) result.push_back(args[1]);
        return Value(result);
    });

    // pop(list) -> list - Remove last item (returns new list)
    globals["pop"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST || args[0].listVal.empty()) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result = args[0].listVal;
        result.pop_back();
        return Value(result);
    });

    // first(list) -> value - Get first element
    globals["first"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST || args[0].listVal.empty()) {
            return Value();
        }
        return args[0].listVal.front();
    });

    // last(list) -> value - Get last element
    globals["last"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST || args[0].listVal.empty()) {
            return Value();
        }
        return args[0].listVal.back();
    });

    // get(list, index) -> value - Get element at index
    globals["get"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::LIST || args[1].type != ValueType::INT) {
            return Value();
        }
        int64_t idx = args[1].intVal;
        const auto& list = args[0].listVal;
        if (idx < 0) idx = list.size() + idx;
        if (idx < 0 || idx >= (int64_t)list.size()) return Value();
        return list[idx];
    });

    // set(list, index, value) -> list - Set element at index (returns new list)
    globals["set"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 3 || args[0].type != ValueType::LIST || args[1].type != ValueType::INT) {
            return args.empty() ? Value(std::vector<Value>{}) : args[0];
        }
        std::vector<Value> result = args[0].listVal;
        int64_t idx = args[1].intVal;
        if (idx < 0) idx = result.size() + idx;
        if (idx >= 0 && idx < (int64_t)result.size()) {
            result[idx] = args[2];
        }
        return Value(result);
    });

    // slice(list, start, end?) -> list - Get sublist
    globals["slice"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        const auto& list = args[0].listVal;
        int64_t start = args.size() > 1 && args[1].type == ValueType::INT ? args[1].intVal : 0;
        int64_t end = args.size() > 2 && args[2].type == ValueType::INT ? args[2].intVal : list.size();
        
        if (start < 0) start = std::max((int64_t)0, (int64_t)list.size() + start);
        if (end < 0) end = list.size() + end;
        start = std::max((int64_t)0, std::min(start, (int64_t)list.size()));
        end = std::max((int64_t)0, std::min(end, (int64_t)list.size()));
        
        if (start >= end) return Value(std::vector<Value>{});
        return Value(std::vector<Value>(list.begin() + start, list.begin() + end));
    });

    // concat(list1, list2) -> list - Concatenate lists
    globals["concat"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> result;
        for (const auto& arg : args) {
            if (arg.type == ValueType::LIST) {
                result.insert(result.end(), arg.listVal.begin(), arg.listVal.end());
            }
        }
        return Value(result);
    });

    // reverse(list) -> list - Reverse list
    globals["reverse"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result = args[0].listVal;
        std::reverse(result.begin(), result.end());
        return Value(result);
    });

    // sort(list) -> list - Sort list (numbers or strings)
    globals["sort"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result = args[0].listVal;
        std::sort(result.begin(), result.end(), [](const Value& a, const Value& b) {
            if (a.type == ValueType::INT && b.type == ValueType::INT) return a.intVal < b.intVal;
            if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
                double av = a.type == ValueType::FLOAT ? a.floatVal : (double)a.intVal;
                double bv = b.type == ValueType::FLOAT ? b.floatVal : (double)b.intVal;
                return av < bv;
            }
            return a.toString() < b.toString();
        });
        return Value(result);
    });

    // sort_desc(list) -> list - Sort list descending
    globals["sort_desc"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result = args[0].listVal;
        std::sort(result.begin(), result.end(), [](const Value& a, const Value& b) {
            if (a.type == ValueType::INT && b.type == ValueType::INT) return a.intVal > b.intVal;
            if (a.type == ValueType::FLOAT || b.type == ValueType::FLOAT) {
                double av = a.type == ValueType::FLOAT ? a.floatVal : (double)a.intVal;
                double bv = b.type == ValueType::FLOAT ? b.floatVal : (double)b.intVal;
                return av > bv;
            }
            return a.toString() > b.toString();
        });
        return Value(result);
    });

    // unique(list) -> list - Remove duplicates (preserves order)
    globals["unique"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result;
        std::unordered_set<std::string> seen;
        for (const auto& v : args[0].listVal) {
            std::string key = v.toString();
            if (seen.find(key) == seen.end()) {
                seen.insert(key);
                result.push_back(v);
            }
        }
        return Value(result);
    });

    // flatten(list) -> list - Flatten nested lists one level
    globals["flatten"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result;
        for (const auto& v : args[0].listVal) {
            if (v.type == ValueType::LIST) {
                result.insert(result.end(), v.listVal.begin(), v.listVal.end());
            } else {
                result.push_back(v);
            }
        }
        return Value(result);
    });

    // index(list, value) -> int - Find index of value (-1 if not found)
    globals["index"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::LIST) {
            return Value((int64_t)-1);
        }
        const auto& list = args[0].listVal;
        std::string target = args[1].toString();
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].toString() == target) return Value((int64_t)i);
        }
        return Value((int64_t)-1);
    });

    // count(list, value) -> int - Count occurrences of value
    globals["count"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::LIST) {
            return Value((int64_t)0);
        }
        std::string target = args[1].toString();
        int64_t count = 0;
        for (const auto& v : args[0].listVal) {
            if (v.toString() == target) count++;
        }
        return Value(count);
    });

    // includes(list, value) -> bool - Check if list contains value
    globals["includes"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::LIST) {
            return Value::makeBool(false);
        }
        std::string target = args[1].toString();
        for (const auto& v : args[0].listVal) {
            if (v.toString() == target) return Value::makeBool(true);
        }
        return Value::makeBool(false);
    });

    // zip(list1, list2) -> list - Zip two lists into list of pairs
    globals["zip"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::LIST || args[1].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result;
        size_t len = std::min(args[0].listVal.size(), args[1].listVal.size());
        for (size_t i = 0; i < len; i++) {
            result.push_back(Value(std::vector<Value>{args[0].listVal[i], args[1].listVal[i]}));
        }
        return Value(result);
    });

    // enumerate(list) -> list - Add indices: [[0, a], [1, b], ...]
    globals["enumerate"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result;
        for (size_t i = 0; i < args[0].listVal.size(); i++) {
            result.push_back(Value(std::vector<Value>{Value((int64_t)i), args[0].listVal[i]}));
        }
        return Value(result);
    });

    // shuffle(list) -> list - Randomly shuffle list
    globals["shuffle"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        std::vector<Value> result = args[0].listVal;
        static thread_local std::mt19937 rng(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        std::shuffle(result.begin(), result.end(), rng);
        return Value(result);
    });

    // take(list, n) -> list - Take first n elements
    globals["take"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        int64_t n = args.size() > 1 && args[1].type == ValueType::INT ? args[1].intVal : 0;
        if (n <= 0) return Value(std::vector<Value>{});
        const auto& list = args[0].listVal;
        n = std::min(n, (int64_t)list.size());
        return Value(std::vector<Value>(list.begin(), list.begin() + n));
    });

    // drop(list, n) -> list - Drop first n elements
    globals["drop"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) {
            return Value(std::vector<Value>{});
        }
        int64_t n = args.size() > 1 && args[1].type == ValueType::INT ? args[1].intVal : 0;
        const auto& list = args[0].listVal;
        if (n >= (int64_t)list.size()) return Value(std::vector<Value>{});
        if (n <= 0) return args[0];
        return Value(std::vector<Value>(list.begin() + n, list.end()));
    });

    // repeat_list(value, n) -> list - Create list with value repeated n times
    globals["repeat_list"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[1].type != ValueType::INT) {
            return Value(std::vector<Value>{});
        }
        int64_t n = args[1].intVal;
        if (n <= 0) return Value(std::vector<Value>{});
        return Value(std::vector<Value>(n, args[0]));
    });

    // range_list(start, end, step?) -> list - Create list from range
    globals["range_list"] = Value([](const std::vector<Value>& args) -> Value {
        int64_t start = 0, end = 0, step = 1;
        if (args.size() >= 1 && args[0].type == ValueType::INT) end = args[0].intVal;
        if (args.size() >= 2 && args[1].type == ValueType::INT) { start = end; end = args[1].intVal; }
        if (args.size() >= 3 && args[2].type == ValueType::INT) step = args[2].intVal;
        
        if (step == 0) return Value(std::vector<Value>{});
        
        std::vector<Value> result;
        if (step > 0) {
            for (int64_t i = start; i < end; i += step) result.push_back(Value(i));
        } else {
            for (int64_t i = start; i > end; i += step) result.push_back(Value(i));
        }
        return Value(result);
    });

    // min_of(list) -> value - Find minimum value
    globals["min_of"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST || args[0].listVal.empty()) {
            return Value();
        }
        Value result = args[0].listVal[0];
        for (const auto& v : args[0].listVal) {
            if (v.type == ValueType::INT && result.type == ValueType::INT) {
                if (v.intVal < result.intVal) result = v;
            } else if (v.type == ValueType::FLOAT || result.type == ValueType::FLOAT) {
                double a = result.type == ValueType::FLOAT ? result.floatVal : (double)result.intVal;
                double b = v.type == ValueType::FLOAT ? v.floatVal : (double)v.intVal;
                if (b < a) result = v;
            }
        }
        return result;
    });

    // max_of(list) -> value - Find maximum value
    globals["max_of"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST || args[0].listVal.empty()) {
            return Value();
        }
        Value result = args[0].listVal[0];
        for (const auto& v : args[0].listVal) {
            if (v.type == ValueType::INT && result.type == ValueType::INT) {
                if (v.intVal > result.intVal) result = v;
            } else if (v.type == ValueType::FLOAT || result.type == ValueType::FLOAT) {
                double a = result.type == ValueType::FLOAT ? result.floatVal : (double)result.intVal;
                double b = v.type == ValueType::FLOAT ? v.floatVal : (double)v.intVal;
                if (b > a) result = v;
            }
        }
        return result;
    });
}

} // namespace stdlib
} // namespace flex
