// Flex Standard Library - Main Header
#ifndef FLEX_STDLIB_H
#define FLEX_STDLIB_H

#include "backend/runtime/value.h"
#include <unordered_map>
#include <functional>

namespace flex {
namespace stdlib {

// Module registration function type
using ModuleRegisterFn = std::function<void(std::unordered_map<std::string, Value>&)>;

// Register all standard library modules
void registerAll(std::unordered_map<std::string, Value>& globals);

// Individual module registration (IO is now in native codegen)
void registerString(std::unordered_map<std::string, Value>& globals);
void registerMath(std::unordered_map<std::string, Value>& globals);
void registerList(std::unordered_map<std::string, Value>& globals);
void registerMap(std::unordered_map<std::string, Value>& globals);
void registerTime(std::unordered_map<std::string, Value>& globals);
void registerSystem(std::unordered_map<std::string, Value>& globals);
void registerJson(std::unordered_map<std::string, Value>& globals);

} // namespace stdlib
} // namespace flex

#endif // FLEX_STDLIB_H
