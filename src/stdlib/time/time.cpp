// Flex Standard Library - Time Module
// Time operations, timestamps, formatting

#include "stdlib/flex_stdlib.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>

namespace flex {
namespace stdlib {

void registerTime(std::unordered_map<std::string, Value>& globals) {
    // now() -> int - Current Unix timestamp in seconds
    globals["now"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        return Value((int64_t)std::chrono::duration_cast<std::chrono::seconds>(epoch).count());
    });

    // now_ms() -> int - Current Unix timestamp in milliseconds
    globals["now_ms"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        return Value((int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count());
    });

    // now_us() -> int - Current Unix timestamp in microseconds
    globals["now_us"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        return Value((int64_t)std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
    });

    // clock() -> float - High-resolution clock for benchmarking (seconds)
    globals["clock"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        auto now = std::chrono::high_resolution_clock::now();
        auto epoch = now.time_since_epoch();
        return Value(std::chrono::duration<double>(epoch).count());
    });

    // sleep(ms) -> nil - Sleep for milliseconds
    globals["sleep"] = Value([](const std::vector<Value>& args) -> Value {
        if (!args.empty() && args[0].type == ValueType::INT) {
            std::this_thread::sleep_for(std::chrono::milliseconds(args[0].intVal));
        }
        return Value();
    });

    // date_str(timestamp?, format?) -> string - Format timestamp as date string
    globals["date_str"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp;
        if (!args.empty() && args[0].type == ValueType::INT) {
            timestamp = args[0].intVal;
        } else {
            timestamp = std::time(nullptr);
        }
        
        std::string format = "%Y-%m-%d %H:%M:%S";
        if (args.size() > 1 && args[1].type == ValueType::STRING) {
            format = args[1].stringVal;
        }
        
        std::tm* tm = std::localtime(&timestamp);
        std::ostringstream oss;
        oss << std::put_time(tm, format.c_str());
        return Value(oss.str());
    });

    // parse_date(str, format?) -> int - Parse date string to timestamp
    globals["parse_date"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::STRING) return Value((int64_t)0);
        
        std::string format = "%Y-%m-%d %H:%M:%S";
        if (args.size() > 1 && args[1].type == ValueType::STRING) {
            format = args[1].stringVal;
        }
        
        std::tm tm = {};
        std::istringstream iss(args[0].stringVal);
        iss >> std::get_time(&tm, format.c_str());
        if (iss.fail()) return Value((int64_t)0);
        
        return Value((int64_t)std::mktime(&tm));
    });

    // year(timestamp?) -> int - Get year from timestamp
    globals["year"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)(tm->tm_year + 1900));
    });

    // month(timestamp?) -> int - Get month (1-12) from timestamp
    globals["month"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)(tm->tm_mon + 1));
    });

    // day(timestamp?) -> int - Get day of month (1-31) from timestamp
    globals["day"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)tm->tm_mday);
    });

    // hour(timestamp?) -> int - Get hour (0-23) from timestamp
    globals["hour"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)tm->tm_hour);
    });

    // minute(timestamp?) -> int - Get minute (0-59) from timestamp
    globals["minute"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)tm->tm_min);
    });

    // second(timestamp?) -> int - Get second (0-59) from timestamp
    globals["second"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)tm->tm_sec);
    });

    // weekday(timestamp?) -> int - Get day of week (0=Sunday, 6=Saturday)
    globals["weekday"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)tm->tm_wday);
    });

    // day_of_year(timestamp?) -> int - Get day of year (1-366)
    globals["day_of_year"] = Value([](const std::vector<Value>& args) -> Value {
        time_t timestamp = !args.empty() && args[0].type == ValueType::INT 
                          ? args[0].intVal : std::time(nullptr);
        std::tm* tm = std::localtime(&timestamp);
        return Value((int64_t)(tm->tm_yday + 1));
    });

    // make_time(year, month, day, hour?, min?, sec?) -> int - Create timestamp
    globals["make_time"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 3) return Value((int64_t)0);
        
        std::tm tm = {};
        tm.tm_year = (args[0].type == ValueType::INT ? args[0].intVal : 2000) - 1900;
        tm.tm_mon = (args[1].type == ValueType::INT ? args[1].intVal : 1) - 1;
        tm.tm_mday = args[2].type == ValueType::INT ? args[2].intVal : 1;
        tm.tm_hour = args.size() > 3 && args[3].type == ValueType::INT ? args[3].intVal : 0;
        tm.tm_min = args.size() > 4 && args[4].type == ValueType::INT ? args[4].intVal : 0;
        tm.tm_sec = args.size() > 5 && args[5].type == ValueType::INT ? args[5].intVal : 0;
        tm.tm_isdst = -1;
        
        return Value((int64_t)std::mktime(&tm));
    });

    // add_days(timestamp, days) -> int - Add days to timestamp
    globals["add_days"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::INT || args[1].type != ValueType::INT) {
            return Value((int64_t)0);
        }
        return Value(args[0].intVal + args[1].intVal * 86400);
    });

    // add_hours(timestamp, hours) -> int - Add hours to timestamp
    globals["add_hours"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::INT || args[1].type != ValueType::INT) {
            return Value((int64_t)0);
        }
        return Value(args[0].intVal + args[1].intVal * 3600);
    });

    // diff_days(timestamp1, timestamp2) -> int - Difference in days
    globals["diff_days"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::INT || args[1].type != ValueType::INT) {
            return Value((int64_t)0);
        }
        return Value((args[1].intVal - args[0].intVal) / 86400);
    });

    // is_leap_year(year) -> bool - Check if year is a leap year
    globals["is_leap_year"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::INT) return Value::makeBool(false);
        int64_t year = args[0].intVal;
        return Value((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    });

    // timer() -> record - Create a timer object
    globals["timer"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        Value result = Value::makeRecord();
        auto now = std::chrono::high_resolution_clock::now();
        auto epoch = now.time_since_epoch();
        result.recordVal["start"] = Value((int64_t)std::chrono::duration_cast<std::chrono::microseconds>(epoch).count());
        return result;
    });

    // elapsed(timer) -> float - Get elapsed time in seconds from timer
    globals["elapsed"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::RECORD) return Value(0.0);
        auto it = args[0].recordVal.find("start");
        if (it == args[0].recordVal.end() || it->second.type != ValueType::INT) return Value(0.0);
        
        auto now = std::chrono::high_resolution_clock::now();
        auto epoch = now.time_since_epoch();
        int64_t nowUs = std::chrono::duration_cast<std::chrono::microseconds>(epoch).count();
        return Value((nowUs - it->second.intVal) / 1000000.0);
    });
}

} // namespace stdlib
} // namespace flex
