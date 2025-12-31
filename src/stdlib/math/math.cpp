// Flex Standard Library - Math Module
// Mathematical functions, constants, random numbers

#include "stdlib/flex_stdlib.h"
#include <cmath>
#include <random>
#include <chrono>

namespace flex {
namespace stdlib {

// Thread-local random engine
static thread_local std::mt19937_64 rng(
    std::chrono::steady_clock::now().time_since_epoch().count()
);

void registerMath(std::unordered_map<std::string, Value>& globals) {
    // Constants
    globals["PI"] = Value(3.14159265358979323846);
    globals["E"] = Value(2.71828182845904523536);
    globals["TAU"] = Value(6.28318530717958647692);
    globals["INF"] = Value(std::numeric_limits<double>::infinity());
    globals["NAN_VAL"] = Value(std::nan(""));

    // sin(x) -> float
    globals["sin"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::sin(x));
    });

    // cos(x) -> float
    globals["cos"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(1.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::cos(x));
    });

    // tan(x) -> float
    globals["tan"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::tan(x));
    });

    // asin(x) -> float
    globals["asin"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::asin(x));
    });

    // acos(x) -> float
    globals["acos"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::acos(x));
    });

    // atan(x) -> float
    globals["atan"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::atan(x));
    });

    // atan2(y, x) -> float
    globals["atan2"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value(0.0);
        double y = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        double x = args[1].type == ValueType::FLOAT ? args[1].floatVal : (double)args[1].intVal;
        return Value(std::atan2(y, x));
    });

    // sqrt(x) -> float
    globals["sqrt"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::sqrt(x));
    });

    // cbrt(x) -> float - Cube root
    globals["cbrt"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::cbrt(x));
    });

    // pow(base, exp) -> float
    globals["pow"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value(0.0);
        double base = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        double exp = args[1].type == ValueType::FLOAT ? args[1].floatVal : (double)args[1].intVal;
        return Value(std::pow(base, exp));
    });

    // exp(x) -> float - e^x
    globals["exp"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(1.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::exp(x));
    });

    // log(x) -> float - Natural logarithm
    globals["log"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::log(x));
    });

    // log10(x) -> float - Base 10 logarithm
    globals["log10"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::log10(x));
    });

    // log2(x) -> float - Base 2 logarithm
    globals["log2"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(std::log2(x));
    });

    // floor(x) -> int
    globals["floor"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value((int64_t)std::floor(x));
    });

    // ceil(x) -> int
    globals["ceil"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value((int64_t)std::ceil(x));
    });

    // round(x) -> int
    globals["round"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value((int64_t)std::round(x));
    });

    // trunc(x) -> int - Truncate towards zero
    globals["trunc"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value((int64_t)std::trunc(x));
    });

    // sign(x) -> int - Returns -1, 0, or 1
    globals["sign"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value((int64_t)0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value((int64_t)(x > 0 ? 1 : (x < 0 ? -1 : 0)));
    });

    // clamp(x, min, max) -> number
    globals["clamp"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 3) return args.empty() ? Value((int64_t)0) : args[0];
        
        bool useFloat = args[0].type == ValueType::FLOAT || 
                        args[1].type == ValueType::FLOAT || 
                        args[2].type == ValueType::FLOAT;
        
        if (useFloat) {
            double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
            double lo = args[1].type == ValueType::FLOAT ? args[1].floatVal : (double)args[1].intVal;
            double hi = args[2].type == ValueType::FLOAT ? args[2].floatVal : (double)args[2].intVal;
            return Value(std::max(lo, std::min(x, hi)));
        } else {
            int64_t x = args[0].intVal;
            int64_t lo = args[1].intVal;
            int64_t hi = args[2].intVal;
            return Value(std::max(lo, std::min(x, hi)));
        }
    });

    // lerp(a, b, t) -> float - Linear interpolation
    globals["lerp"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 3) return Value(0.0);
        double a = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        double b = args[1].type == ValueType::FLOAT ? args[1].floatVal : (double)args[1].intVal;
        double t = args[2].type == ValueType::FLOAT ? args[2].floatVal : (double)args[2].intVal;
        return Value(a + (b - a) * t);
    });

    // hypot(x, y) -> float - Hypotenuse
    globals["hypot"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2) return Value(0.0);
        double x = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        double y = args[1].type == ValueType::FLOAT ? args[1].floatVal : (double)args[1].intVal;
        return Value(std::hypot(x, y));
    });

    // deg_to_rad(degrees) -> float
    globals["deg_to_rad"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double deg = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(deg * 3.14159265358979323846 / 180.0);
    });

    // rad_to_deg(radians) -> float
    globals["rad_to_deg"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty()) return Value(0.0);
        double rad = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        return Value(rad * 180.0 / 3.14159265358979323846);
    });

    // random() -> float - Random float between 0 and 1
    globals["random"] = Value([](const std::vector<Value>& args) -> Value {
        (void)args;
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return Value(dist(rng));
    });

    // random_int(min, max) -> int - Random integer in range [min, max]
    globals["random_int"] = Value([](const std::vector<Value>& args) -> Value {
        int64_t lo = 0, hi = 100;
        if (args.size() >= 1 && args[0].type == ValueType::INT) hi = args[0].intVal;
        if (args.size() >= 2 && args[1].type == ValueType::INT) { lo = hi; hi = args[1].intVal; }
        std::uniform_int_distribution<int64_t> dist(lo, hi);
        return Value(dist(rng));
    });

    // random_float(min, max) -> float - Random float in range [min, max)
    globals["random_float"] = Value([](const std::vector<Value>& args) -> Value {
        double lo = 0.0, hi = 1.0;
        if (args.size() >= 1) {
            hi = args[0].type == ValueType::FLOAT ? args[0].floatVal : (double)args[0].intVal;
        }
        if (args.size() >= 2) {
            lo = hi;
            hi = args[1].type == ValueType::FLOAT ? args[1].floatVal : (double)args[1].intVal;
        }
        std::uniform_real_distribution<double> dist(lo, hi);
        return Value(dist(rng));
    });

    // seed_random(seed) -> nil - Seed the random generator
    globals["seed_random"] = Value([](const std::vector<Value>& args) -> Value {
        if (!args.empty() && args[0].type == ValueType::INT) {
            rng.seed(args[0].intVal);
        }
        return Value();
    });

    // is_nan(x) -> bool
    globals["is_nan"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::FLOAT) return Value::makeBool(false);
        return Value(std::isnan(args[0].floatVal));
    });

    // is_inf(x) -> bool
    globals["is_inf"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::FLOAT) return Value::makeBool(false);
        return Value(std::isinf(args[0].floatVal));
    });

    // gcd(a, b) -> int - Greatest common divisor
    globals["gcd"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::INT || args[1].type != ValueType::INT) {
            return Value((int64_t)0);
        }
        int64_t a = std::abs(args[0].intVal);
        int64_t b = std::abs(args[1].intVal);
        while (b != 0) {
            int64_t t = b;
            b = a % b;
            a = t;
        }
        return Value(a);
    });

    // lcm(a, b) -> int - Least common multiple
    globals["lcm"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.size() < 2 || args[0].type != ValueType::INT || args[1].type != ValueType::INT) {
            return Value((int64_t)0);
        }
        int64_t a = std::abs(args[0].intVal);
        int64_t b = std::abs(args[1].intVal);
        if (a == 0 || b == 0) return Value((int64_t)0);
        
        // Calculate GCD first
        int64_t ga = a, gb = b;
        while (gb != 0) {
            int64_t t = gb;
            gb = ga % gb;
            ga = t;
        }
        return Value((a / ga) * b);
    });

    // factorial(n) -> int
    globals["factorial"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::INT) return Value((int64_t)1);
        int64_t n = args[0].intVal;
        if (n < 0) return Value((int64_t)0);
        if (n > 20) return Value((int64_t)-1); // Overflow protection
        int64_t result = 1;
        for (int64_t i = 2; i <= n; i++) result *= i;
        return Value(result);
    });

    // fib(n) -> int - Fibonacci number
    globals["fib"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::INT) return Value((int64_t)0);
        int64_t n = args[0].intVal;
        if (n <= 0) return Value((int64_t)0);
        if (n == 1) return Value((int64_t)1);
        int64_t a = 0, b = 1;
        for (int64_t i = 2; i <= n; i++) {
            int64_t t = a + b;
            a = b;
            b = t;
        }
        return Value(b);
    });

    // sum(list) -> number - Sum of list elements
    globals["sum"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST) return Value((int64_t)0);
        bool hasFloat = false;
        double total = 0;
        for (const auto& v : args[0].listVal) {
            if (v.type == ValueType::FLOAT) {
                hasFloat = true;
                total += v.floatVal;
            } else if (v.type == ValueType::INT) {
                total += v.intVal;
            }
        }
        return hasFloat ? Value(total) : Value((int64_t)total);
    });

    // avg(list) -> float - Average of list elements
    globals["avg"] = Value([](const std::vector<Value>& args) -> Value {
        if (args.empty() || args[0].type != ValueType::LIST || args[0].listVal.empty()) {
            return Value(0.0);
        }
        double total = 0;
        for (const auto& v : args[0].listVal) {
            if (v.type == ValueType::FLOAT) total += v.floatVal;
            else if (v.type == ValueType::INT) total += v.intVal;
        }
        return Value(total / args[0].listVal.size());
    });
}

} // namespace stdlib
} // namespace flex
