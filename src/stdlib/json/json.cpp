// Flex Standard Library - JSON Module
// JSON parsing and serialization

#include "stdlib/flex_stdlib.h"
#include <sstream>
#include <cctype>

namespace flex {
namespace stdlib {

// Simple JSON parser
class JsonParser {
public:
    JsonParser(const std::string& input) : src(input), pos(0) {}
    
    Value parse() {
        skipWhitespace();
        return parseValue();
    }
    
private:
    std::string src;
    size_t pos;
    
    char peek() { return pos < src.length() ? src[pos] : '\0'; }
    char advance() { return pos < src.length() ? src[pos++] : '\0'; }
    
    void skipWhitespace() {
        while (pos < src.length() && std::isspace(src[pos])) pos++;
    }
    
    Value parseValue() {
        skipWhitespace();
        char c = peek();
        
        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || std::isdigit(c)) return parseNumber();
        
        return Value(); // Error case
    }
    
    Value parseString() {
        advance(); // Skip opening quote
        std::string result;
        while (pos < src.length() && peek() != '"') {
            char c = advance();
            if (c == '\\' && pos < src.length()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '\\': result += '\\'; break;
                    case '"': result += '"'; break;
                    case '/': result += '/'; break;
                    default: result += escaped; break;
                }
            } else {
                result += c;
            }
        }
        advance(); // Skip closing quote
        return Value(result);
    }
    
    Value parseNumber() {
        size_t start = pos;
        bool isFloat = false;
        
        if (peek() == '-') advance();
        while (std::isdigit(peek())) advance();
        
        if (peek() == '.') {
            isFloat = true;
            advance();
            while (std::isdigit(peek())) advance();
        }
        
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (std::isdigit(peek())) advance();
        }
        
        std::string numStr = src.substr(start, pos - start);
        if (isFloat) {
            return Value(std::stod(numStr));
        } else {
            return Value((int64_t)std::stoll(numStr));
        }
    }
    
    Value parseObject() {
        advance(); // Skip {
        Value result = Value::makeRecord();
        skipWhitespace();
        
        if (peek() == '}') {
            advance();
            return result;
        }
        
        while (true) {
            skipWhitespace();
            if (peek() != '"') break;
            
            Value key = parseString();
            skipWhitespace();
            
            if (peek() != ':') break;
            advance(); // Skip :
            
            Value value = parseValue();
            result.recordVal[key.stringVal] = value;
            
            skipWhitespace();
            if (peek() == ',') {
                advance();
            } else {
                break;
            }
        }
        
        skipWhitespace();
        if (peek() == '}') advance();
        return result;
    }
    
    Value parseArray() {
        advance(); // Skip [
        std::vector<Value> result;
        skipWhitespace();
        
        if (peek() == ']') {
            advance();
            return Value(result);
        }
        
        while (true) {
            result.push_back(parseValue());
            skipWhitespace();
            
            if (peek() == ',') {
                advance();
            } else {
                break;
            }
        }
        
        skipWhitespace();
        if (peek() == ']') advance();
        return Value(result);
    }
    
    Value parseBool() {
        if (src.substr(pos, 4) == "true") {
            pos += 4;
            return Value::makeBool(true);
        } else if (src.substr(pos, 5) == "false") {
            pos += 5;
            return Value::makeBool(false);
        }
        return Value::makeBool(false);
    }
    
    Value parseNull() {
        if (src.substr(pos, 4) == "null") {
            pos += 4;
        }
        return Value();
    }
};

// JSON serializer
std::string toJson(const Value& val, int indent = 0, bool pretty = false) {
    std::string indentStr = pretty ? std::string(indent * 2, ' ') : "";
    std::string newline = pretty ? "\n" : "";
    std::string space = pretty ? " " : "";
    
    switch (val.type) {
        case ValueType::NIL:
            return "null";
        case ValueType::BOOL:
            return val.boolVal ? "true" : "false";
        case ValueType::INT:
            return std::to_string(val.intVal);
        case ValueType::FLOAT: {
            std::ostringstream oss;
            oss << val.floatVal;
            return oss.str();
        }
        case ValueType::STRING: {
            std::string result = "\"";
            for (char c : val.stringVal) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += c; break;
                }
            }
            return result + "\"";
        }
        case ValueType::LIST: {
            if (val.listVal.empty()) return "[]";
            std::string result = "[" + newline;
            for (size_t i = 0; i < val.listVal.size(); i++) {
                if (i > 0) result += "," + newline;
                if (pretty) result += std::string((indent + 1) * 2, ' ');
                result += toJson(val.listVal[i], indent + 1, pretty);
            }
            result += newline + indentStr + "]";
            return result;
        }
        case ValueType::RECORD: {
            if (val.recordVal.empty()) return "{}";
            std::string result = "{" + newline;
            bool first = true;
            for (const auto& [k, v] : val.recordVal) {
                if (!first) result += "," + newline;
                first = false;
                if (pretty) result += std::string((indent + 1) * 2, ' ');
                result += "\"" + k + "\":" + space + toJson(v, indent + 1, pretty);
            }
            result += newline + indentStr + "}";
            return result;
        }
        default:
            return "null";
    }
}

void registerJson(std::unordered_map<std::string, Value>& globals) {
    // json_parse(str) -> value - Parse JSON string to value
    globals["json_parse"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value();
        try {
            JsonParser parser(args[0].stringVal);
            return parser.parse();
        } catch (...) {
            return Value();
        }
    });

    // json_str(value, pretty?) -> string - Convert value to JSON string
    globals["json_str"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value("null");
        bool pretty = args.size() > 1 && args[1].isTruthy();
        return Value(toJson(args[0], 0, pretty));
    });

    // json_pretty(value) -> string - Convert value to pretty-printed JSON
    globals["json_pretty"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value("null");
        return Value(toJson(args[0], 0, true));
    });

    // is_valid_json(str) -> bool - Check if string is valid JSON
    globals["is_valid_json"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value::makeBool(false);
        try {
            JsonParser parser(args[0].stringVal);
            parser.parse();
            return Value::makeBool(true);
        } catch (...) {
            return Value::makeBool(false);
        }
    });
}

} // namespace stdlib
} // namespace flex
