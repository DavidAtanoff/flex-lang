// Flex Standard Library - System Module
// Environment, process, command execution

#include "stdlib/flex_stdlib.h"
#include <array>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdlib.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <cstdlib>
#endif

namespace flex {
namespace stdlib {

void registerSystem(std::unordered_map<std::string, Value>& globals) {
    // env(name) -> string - Get environment variable
    globals["env"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value("");
        const char* val = std::getenv(args[0].stringVal.c_str());
        return Value(val ? std::string(val) : "");
    });

    // set_env(name, value) -> bool - Set environment variable
    globals["set_env"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::STRING) return Value::makeBool(false);
#ifdef _WIN32
        std::string envStr = args[0].stringVal + "=" + args[1].toString();
        return Value(_putenv(envStr.c_str()) == 0);
#else
        return Value(setenv(args[0].stringVal.c_str(), args[1].toString().c_str(), 1) == 0);
#endif
    });

    // args() -> list - Get command line arguments
    // Note: This would need to be set up during VM initialization
    globals["args"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        // Return empty list - actual args would be set during initialization
        return Value(std::vector<Value>{});
    });

    // exit(code?) -> nil - Exit program with code
    globals["exit"] = Value([](const std::vector<Value>& args) -> Value {
        int code = 0;
        if (!args.empty() && args[0].type == ValueType::INT) {
            code = (int)args[0].intVal;
        }
        std::exit(code);
        return Value(); // Never reached
    });

    // exec(command) -> record - Execute shell command, return {output, code}
    globals["exec"] = Value([](const std::vector<Value>& args) -> Value {
        Value result = Value::makeRecord();
        result.recordVal["output"] = Value("");
        result.recordVal["code"] = Value((int64_t)-1);
        
        if (args.empty() || args[0].type != ValueType::STRING) return result;
        
        std::string output;
        std::array<char, 128> buffer;
        
        FILE* pipe = popen(args[0].stringVal.c_str(), "r");
        if (!pipe) return result;
        
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            output += buffer.data();
        }
        
        int code = pclose(pipe);
#ifdef _WIN32
        // Windows returns the exit code directly
#else
        if (WIFEXITED(code)) code = WEXITSTATUS(code);
#endif
        
        result.recordVal["output"] = Value(output);
        result.recordVal["code"] = Value((int64_t)code);
        return result;
    });

    // system(command) -> int - Execute command, return exit code
    globals["system"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value((int64_t)-1);
        return Value((int64_t)std::system(args[0].stringVal.c_str()));
    });

    // platform() -> string - Get platform name
    globals["platform"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
#ifdef _WIN32
        return Value("windows");
#elif __APPLE__
        return Value("macos");
#elif __linux__
        return Value("linux");
#else
        return Value("unknown");
#endif
    });

    // arch() -> string - Get architecture
    globals["arch"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
#if defined(__x86_64__) || defined(_M_X64)
        return Value("x64");
#elif defined(__i386__) || defined(_M_IX86)
        return Value("x86");
#elif defined(__aarch64__) || defined(_M_ARM64)
        return Value("arm64");
#elif defined(__arm__) || defined(_M_ARM)
        return Value("arm");
#else
        return Value("unknown");
#endif
    });

    // hostname() -> string - Get hostname
    globals["hostname"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        char buffer[256];
#ifdef _WIN32
        DWORD size = sizeof(buffer);
        if (GetComputerNameA(buffer, &size)) {
            return Value(std::string(buffer));
        }
#else
        if (gethostname(buffer, sizeof(buffer)) == 0) {
            return Value(std::string(buffer));
        }
#endif
        return Value("");
    });

    // username() -> string - Get current username
    globals["username"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
#ifdef _WIN32
        char buffer[256];
        DWORD size = sizeof(buffer);
        if (GetUserNameA(buffer, &size)) {
            return Value(std::string(buffer));
        }
#else
        const char* user = std::getenv("USER");
        if (user) return Value(std::string(user));
#endif
        return Value("");
    });

    // home_dir() -> string - Get user home directory
    globals["home_dir"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
#ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
#else
        const char* home = std::getenv("HOME");
#endif
        return Value(home ? std::string(home) : "");
    });

    // temp_dir() -> string - Get temp directory
    globals["temp_dir"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
#ifdef _WIN32
        char buffer[MAX_PATH];
        DWORD len = GetTempPathA(MAX_PATH, buffer);
        if (len > 0) return Value(std::string(buffer, len));
#else
        const char* tmp = std::getenv("TMPDIR");
        if (tmp) return Value(std::string(tmp));
        return Value("/tmp");
#endif
        return Value("");
    });

    // cpu_count() -> int - Get number of CPU cores
    globals["cpu_count"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
#ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return Value((int64_t)sysinfo.dwNumberOfProcessors);
#else
        return Value((int64_t)sysconf(_SC_NPROCESSORS_ONLN));
#endif
    });

    // assert(condition, message?) -> nil - Assert condition is true
    globals["assert"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value();
        if (!args[0].isTruthy()) {
            std::string msg = args.size() > 1 ? args[1].toString() : "Assertion failed";
            std::cerr << "Assertion error: " << msg << std::endl;
            std::exit(1);
        }
        return Value();
    });

    // panic(message) -> nil - Exit with error message
    globals["panic"] = Value([](const std::vector<Value>& args) -> Value {
        std::string msg = args.empty() ? "panic" : args[0].toString();
        std::cerr << "Panic: " << msg << std::endl;
        std::exit(1);
        return Value();
    });

    // debug(value) -> value - Print debug info and return value
    globals["debug"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            std::cerr << "[debug] nil" << std::endl;
            return Value();
        }
        std::cerr << "[debug] " << args[0].toString() << " (type: ";
        switch (args[0].type) {
            case ValueType::NIL: std::cerr << "nil"; break;
            case ValueType::BOOL: std::cerr << "bool"; break;
            case ValueType::INT: std::cerr << "int"; break;
            case ValueType::FLOAT: std::cerr << "float"; break;
            case ValueType::STRING: std::cerr << "string"; break;
            case ValueType::LIST: std::cerr << "list[" << args[0].listVal.size() << "]"; break;
            case ValueType::RECORD: std::cerr << "record{" << args[0].recordVal.size() << "}"; break;
            case ValueType::FUNCTION: std::cerr << "function"; break;
            case ValueType::NATIVE_FN: std::cerr << "native_fn"; break;
            case ValueType::RANGE: std::cerr << "range"; break;
        }
        std::cerr << ")" << std::endl;
        return args[0];
    });
}

} // namespace stdlib
} // namespace flex
