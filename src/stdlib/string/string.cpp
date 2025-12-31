// Flex Standard Library - String Module
// String manipulation, search, formatting

#include "stdlib/flex_stdlib.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <cctype>

namespace flex {
namespace stdlib {

void registerString(std::unordered_map<std::string, Value>& globals) {
    // split(str, delimiter) -> list - Split string by delimiter
    globals["split"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> result;
        if (args.empty() || args[0].type != ValueType::STRING) return Value(result);
        
        std::string str = args[0].stringVal;
        std::string delim = args.size() > 1 && args[1].type == ValueType::STRING ? args[1].stringVal : " ";
        
        if (delim.empty()) {
            for (char c : str) result.push_back(Value(std::string(1, c)));
            return Value(result);
        }
        
        size_t pos = 0, prev = 0;
        while ((pos = str.find(delim, prev)) != std::string::npos) {
            result.push_back(Value(str.substr(prev, pos - prev)));
            prev = pos + delim.length();
        }
        result.push_back(Value(str.substr(prev)));
        return Value(result);
    });

    // join(list, delimiter) -> string - Join list elements with delimiter
    globals["join"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) return Value("");
        
        std::string delim = args.size() > 1 && args[1].type == ValueType::STRING ? args[1].stringVal : "";
        std::string result;
        
        for (size_t i = 0; i < args[0].listVal.size(); i++) {
            if (i > 0) result += delim;
            result += args[0].listVal[i].toString();
        }
        return Value(result);
    });

    // trim(str) -> string - Remove leading/trailing whitespace
    globals["trim"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
        s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
        return Value(s);
    });

    // ltrim(str) -> string - Remove leading whitespace
    globals["ltrim"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
        return Value(s);
    });

    // rtrim(str) -> string - Remove trailing whitespace
    globals["rtrim"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
        return Value(s);
    });

    // upper(str) -> string - Convert to uppercase
    globals["upper"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return Value(s);
    });

    // lower(str) -> string - Convert to lowercase
    globals["lower"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return Value(s);
    });

    // replace(str, old, new) -> string - Replace all occurrences
    globals["replace"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 3 || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        std::string oldStr = args[1].toString();
        std::string newStr = args[2].toString();
        
        if (oldStr.empty()) return Value(s);
        size_t pos = 0;
        while ((pos = s.find(oldStr, pos)) != std::string::npos) {
            s.replace(pos, oldStr.length(), newStr);
            pos += newStr.length();
        }
        return Value(s);
    });

    // contains(str, substr) -> bool - Check if string contains substring
    globals["contains"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) return Value::makeBool(false);
        return Value(args[0].stringVal.find(args[1].toString()) != std::string::npos);
    });

    // starts_with(str, prefix) -> bool
    globals["starts_with"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) return Value::makeBool(false);
        std::string prefix = args[1].toString();
        return Value(args[0].stringVal.substr(0, prefix.length()) == prefix);
    });

    // ends_with(str, suffix) -> bool
    globals["ends_with"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) return Value::makeBool(false);
        std::string s = args[0].stringVal;
        std::string suffix = args[1].toString();
        if (suffix.length() > s.length()) return Value::makeBool(false);
        return Value(s.substr(s.length() - suffix.length()) == suffix);
    });

    // index_of(str, substr) -> int - Find first occurrence (-1 if not found)
    globals["index_of"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) return Value((int64_t)-1);
        size_t pos = args[0].stringVal.find(args[1].toString());
        return Value(pos == std::string::npos ? (int64_t)-1 : (int64_t)pos);
    });

    // last_index_of(str, substr) -> int - Find last occurrence
    globals["last_index_of"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) return Value((int64_t)-1);
        size_t pos = args[0].stringVal.rfind(args[1].toString());
        return Value(pos == std::string::npos ? (int64_t)-1 : (int64_t)pos);
    });

    // substr(str, start, length?) -> string - Extract substring
    globals["substr"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        int64_t start = args.size() > 1 && args[1].type == ValueType::INT ? args[1].intVal : 0;
        
        if (start < 0) start = std::max((int64_t)0, (int64_t)s.length() + start);
        if (start >= (int64_t)s.length()) return Value("");
        
        if (args.size() > 2 && args[2].type == ValueType::INT) {
            return Value(s.substr(start, args[2].intVal));
        }
        return Value(s.substr(start));
    });

    // char_at(str, index) -> string - Get character at index
    globals["char_at"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::INT) {
            return Value("");
        }
        std::string s = args[0].stringVal;
        int64_t idx = args[1].intVal;
        if (idx < 0) idx = s.length() + idx;
        if (idx < 0 || idx >= (int64_t)s.length()) return Value("");
        return Value(std::string(1, s[idx]));
    });

    // repeat(str, count) -> string - Repeat string n times
    globals["repeat"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::INT) {
            return Value("");
        }
        std::string result;
        int64_t count = args[1].intVal;
        for (int64_t i = 0; i < count; i++) {
            result += args[0].stringVal;
        }
        return Value(result);
    });

    // reverse_str(str) -> string - Reverse string
    globals["reverse_str"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string s = args[0].stringVal;
        std::reverse(s.begin(), s.end());
        return Value(s);
    });

    // pad_left(str, length, char?) -> string - Pad string on left
    globals["pad_left"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::INT) {
            return args.empty() ? Value("") : Value(args[0].stringVal);
        }
        std::string s = args[0].stringVal;
        int64_t len = args[1].intVal;
        char pad = args.size() > 2 && args[2].type == ValueType::STRING && !args[2].stringVal.empty() 
                   ? args[2].stringVal[0] : ' ';
        if ((int64_t)s.length() >= len) return Value(s);
        return Value(std::string(len - s.length(), pad) + s);
    });

    // pad_right(str, length, char?) -> string - Pad string on right
    globals["pad_right"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::INT) {
            return args.empty() ? Value("") : Value(args[0].stringVal);
        }
        std::string s = args[0].stringVal;
        int64_t len = args[1].intVal;
        char pad = args.size() > 2 && args[2].type == ValueType::STRING && !args[2].stringVal.empty() 
                   ? args[2].stringVal[0] : ' ';
        if ((int64_t)s.length() >= len) return Value(s);
        return Value(s + std::string(len - s.length(), pad));
    });

    // is_digit(str) -> bool - Check if string is all digits
    globals["is_digit"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING || args[0].stringVal.empty()) {
            return Value::makeBool(false);
        }
        for (char c : args[0].stringVal) {
            if (!std::isdigit(c)) return Value::makeBool(false);
        }
        return Value::makeBool(true);
    });

    // is_alpha(str) -> bool - Check if string is all letters
    globals["is_alpha"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING || args[0].stringVal.empty()) {
            return Value::makeBool(false);
        }
        for (char c : args[0].stringVal) {
            if (!std::isalpha(c)) return Value::makeBool(false);
        }
        return Value::makeBool(true);
    });

    // is_alnum(str) -> bool - Check if string is alphanumeric
    globals["is_alnum"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING || args[0].stringVal.empty()) {
            return Value::makeBool(false);
        }
        for (char c : args[0].stringVal) {
            if (!std::isalnum(c)) return Value::makeBool(false);
        }
        return Value::makeBool(true);
    });

    // ord(char) -> int - Get ASCII code of character
    globals["ord"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING || args[0].stringVal.empty()) {
            return Value((int64_t)0);
        }
        return Value((int64_t)(unsigned char)args[0].stringVal[0]);
    });

    // chr(code) -> string - Get character from ASCII code
    globals["chr"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::INT) return Value("");
        int64_t code = args[0].intVal;
        if (code < 0 || code > 255) return Value("");
        return Value(std::string(1, (char)code));
    });

    // format(template, args...) -> string - Simple string formatting
    globals["format"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        std::string result = args[0].stringVal;
        
        for (size_t i = 1; i < args.size(); i++) {
            std::string placeholder = "{" + std::to_string(i - 1) + "}";
            size_t pos = result.find(placeholder);
            if (pos != std::string::npos) {
                result.replace(pos, placeholder.length(), args[i].toString());
            }
        }
        return Value(result);
    });
}

} // namespace stdlib
} // namespace flex
