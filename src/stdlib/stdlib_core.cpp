// Flex Standard Library - Core Registration
#include "flex_stdlib.h"

namespace flex {
namespace stdlib {

void registerAll(std::unordered_map<std::string, Value>& globals) {
    // IO is now in native codegen (open, read, write, close, file_size)
    registerString(globals);
    registerMath(globals);
    registerList(globals);
    registerMap(globals);
    registerTime(globals);
    registerSystem(globals);
    registerJson(globals);
}

} // namespace stdlib
} // namespace flex
