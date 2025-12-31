// Flex Standard Library - IO Module
// File operations, stdin/stdout, path utilities

#include "stdlib/flex_stdlib.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace flex {
namespace stdlib {

void registerIO(std::unordered_map<std::string, Value>& globals) {
    // read_file(path) -> string - Read entire file contents
    globals["read_file"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value(""); // Return empty on error
        }
        std::ifstream file(args[0].stringVal);
        if (!file.is_open()) return Value("");
        std::stringstream buffer;
        buffer << file.rdbuf();
        return Value(buffer.str());
    });

    // write_file(path, content) -> bool - Write content to file
    globals["write_file"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        std::ofstream file(args[0].stringVal);
        if (!file.is_open()) return Value::makeBool(false);
        file << args[1].toString();
        return Value::makeBool(true);
    });

    // append_file(path, content) -> bool - Append content to file
    globals["append_file"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        std::ofstream file(args[0].stringVal, std::ios::app);
        if (!file.is_open()) return Value::makeBool(false);
        file << args[1].toString();
        return Value::makeBool(true);
    });

    // file_exists(path) -> bool - Check if file exists
    globals["file_exists"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        return Value(fs::exists(args[0].stringVal));
    });

    // is_file(path) -> bool - Check if path is a file
    globals["is_file"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        return Value(fs::is_regular_file(args[0].stringVal));
    });

    // is_dir(path) -> bool - Check if path is a directory
    globals["is_dir"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        return Value(fs::is_directory(args[0].stringVal));
    });

    // mkdir(path) -> bool - Create directory
    globals["mkdir"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        try {
            return Value(fs::create_directories(args[0].stringVal));
        } catch (...) {
            return Value::makeBool(false);
        }
    });

    // remove_file(path) -> bool - Delete file or empty directory
    globals["remove_file"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value::makeBool(false);
        }
        try {
            return Value(fs::remove(args[0].stringVal));
        } catch (...) {
            return Value::makeBool(false);
        }
    });

    // list_dir(path) -> list - List directory contents
    globals["list_dir"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> result;
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value(result);
        }
        try {
            for (const auto& entry : fs::directory_iterator(args[0].stringVal)) {
                result.push_back(Value(entry.path().filename().string()));
            }
        } catch (...) {}
        return Value(result);
    });

    // file_size(path) -> int - Get file size in bytes
    globals["file_size"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value((int64_t)-1);
        }
        try {
            return Value((int64_t)fs::file_size(args[0].stringVal));
        } catch (...) {
            return Value((int64_t)-1);
        }
    });

    // cwd() -> string - Get current working directory
    globals["cwd"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        try {
            return Value(fs::current_path().string());
        } catch (...) {
            return Value("");
        }
    });

    // path_join(parts...) -> string - Join path components
    globals["path_join"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value("");
        fs::path result;
        for (const auto& arg : args) {
            if (arg.type == ValueType::STRING) {
                if (result.empty()) result = arg.stringVal;
                else result /= arg.stringVal;
            }
        }
        return Value(result.string());
    });

    // path_basename(path) -> string - Get filename from path
    globals["path_basename"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        return Value(fs::path(args[0].stringVal).filename().string());
    });

    // path_dirname(path) -> string - Get directory from path
    globals["path_dirname"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        return Value(fs::path(args[0].stringVal).parent_path().string());
    });

    // path_ext(path) -> string - Get file extension
    globals["path_ext"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        return Value(fs::path(args[0].stringVal).extension().string());
    });

    // read_lines(path) -> list - Read file as list of lines
    globals["read_lines"] = Value([](const std::vector<Value>& args) -> Value {
        std::vector<Value> lines;
        if (args.empty() || args[0].type != ValueType::STRING) {
            return Value(lines);
        }
        std::ifstream file(args[0].stringVal);
        if (!file.is_open()) return Value(lines);
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(Value(line));
        }
        return Value(lines);
    });

    // write_lines(path, lines) -> bool - Write list of lines to file
    globals["write_lines"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING || args[1].type != ValueType::LIST) {
            return Value::makeBool(false);
        }
        std::ofstream file(args[0].stringVal);
        if (!file.is_open()) return Value::makeBool(false);
        for (const auto& line : args[1].listVal) {
            file << line.toString() << "\n";
        }
        return Value::makeBool(true);
    });
}

} // namespace stdlib
} // namespace flex
