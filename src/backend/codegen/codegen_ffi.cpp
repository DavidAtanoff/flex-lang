// Flex Compiler - FFI/Extern Code Generation Helpers
// Handles: C type utilities, calling convention helpers

#include "backend/codegen/codegen_base.h"

namespace flex {

// FFI helper functions for C interop
// These are used by the main codegen to handle extern function calls

// Check if a type string represents a pointer type
bool isFFIPointerType(const std::string& type) {
    return !type.empty() && type[0] == '*';
}

// Check if a type string represents void
bool isFFIVoidType(const std::string& type) {
    return type == "void" || type == "*void";
}

// Get the size of a C-compatible type in bytes
size_t getFFICTypeSize(const std::string& type) {
    if (type.empty() || type == "void") return 0;
    if (isFFIPointerType(type)) return 8;  // All pointers are 64-bit
    if (type == "int" || type == "int32" || type == "i32") return 4;
    if (type == "int64" || type == "i64" || type == "long") return 8;
    if (type == "int16" || type == "i16" || type == "short") return 2;
    if (type == "int8" || type == "i8" || type == "char" || type == "byte") return 1;
    if (type == "uint" || type == "uint32" || type == "u32") return 4;
    if (type == "uint64" || type == "u64" || type == "ulong" || type == "usize") return 8;
    if (type == "uint16" || type == "u16" || type == "ushort") return 2;
    if (type == "uint8" || type == "u8" || type == "uchar") return 1;
    if (type == "float" || type == "f32" || type == "float32") return 4;
    if (type == "float64" || type == "f64" || type == "double") return 8;
    if (type == "bool") return 1;
    if (type == "str" || type == "string") return 8;  // String pointer
    return 8;  // Default to pointer size for unknown types
}

// Check if type should be passed in XMM register (floating point)
bool isFFIFloatType(const std::string& type) {
    return type == "float" || type == "f32" || type == "float32" ||
           type == "float64" || type == "f64" || type == "double";
}

} // namespace flex
