// Flex Standard Library - Core Registration
#include "flex_stdlib.h"

namespace flex {
namespace stdlib {

void registerAll(std::unordered_map<std::string, Value>& globals) {
    registerIO(globals);
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
