// Flex Compiler - Type Checker Core
// Core methods, type utilities, diagnostics

#include "checker_base.h"
#include <regex>
#include <functional>

namespace flex {

TypeChecker::TypeChecker() : currentType_(nullptr), expectedReturn_(nullptr) {
    // Register built-in functions
    registerBuiltins();
}

void TypeChecker::registerBuiltins() {
    auto& reg = TypeRegistry::instance();
    
    // print/println - variadic, returns void
    auto printFn = std::make_shared<FunctionType>();
    printFn->isVariadic = true;
    printFn->returnType = reg.voidType();
    symbols_.define(Symbol("print", SymbolKind::FUNCTION, printFn));
    symbols_.define(Symbol("println", SymbolKind::FUNCTION, printFn));
    
    // len(x) -> int
    auto lenFn = std::make_shared<FunctionType>();
    lenFn->params.push_back({"x", reg.anyType()});
    lenFn->returnType = reg.intType();
    symbols_.define(Symbol("len", SymbolKind::FUNCTION, lenFn));
    
    // str(x) -> string
    auto strFn = std::make_shared<FunctionType>();
    strFn->params.push_back({"x", reg.anyType()});
    strFn->returnType = reg.stringType();
    symbols_.define(Symbol("str", SymbolKind::FUNCTION, strFn));
    
    // int(x) -> int - Convert string/float/bool to int
    auto intFn = std::make_shared<FunctionType>();
    intFn->params.push_back({"x", reg.anyType()});
    intFn->returnType = reg.intType();
    symbols_.define(Symbol("int", SymbolKind::FUNCTION, intFn));
    
    // float(x) -> float - Convert string/int/bool to float
    auto floatFn = std::make_shared<FunctionType>();
    floatFn->params.push_back({"x", reg.anyType()});
    floatFn->returnType = reg.floatType();
    symbols_.define(Symbol("float", SymbolKind::FUNCTION, floatFn));
    
    // bool(x) -> bool - Convert int/string to bool
    auto boolFn = std::make_shared<FunctionType>();
    boolFn->params.push_back({"x", reg.anyType()});
    boolFn->returnType = reg.boolType();
    symbols_.define(Symbol("bool", SymbolKind::FUNCTION, boolFn));
    
    // upper(s) -> string
    auto upperFn = std::make_shared<FunctionType>();
    upperFn->params.push_back({"s", reg.stringType()});
    upperFn->returnType = reg.stringType();
    symbols_.define(Symbol("upper", SymbolKind::FUNCTION, upperFn));
    
    // lower(s) -> string
    auto lowerFn = std::make_shared<FunctionType>();
    lowerFn->params.push_back({"s", reg.stringType()});
    lowerFn->returnType = reg.stringType();
    symbols_.define(Symbol("lower", SymbolKind::FUNCTION, lowerFn));
    
    // trim(s) -> string
    auto trimFn = std::make_shared<FunctionType>();
    trimFn->params.push_back({"s", reg.stringType()});
    trimFn->returnType = reg.stringType();
    symbols_.define(Symbol("trim", SymbolKind::FUNCTION, trimFn));
    
    // starts_with(s, prefix) -> bool
    auto startsWithFn = std::make_shared<FunctionType>();
    startsWithFn->params.push_back({"s", reg.stringType()});
    startsWithFn->params.push_back({"prefix", reg.stringType()});
    startsWithFn->returnType = reg.boolType();
    symbols_.define(Symbol("starts_with", SymbolKind::FUNCTION, startsWithFn));
    
    // ends_with(s, suffix) -> bool
    auto endsWithFn = std::make_shared<FunctionType>();
    endsWithFn->params.push_back({"s", reg.stringType()});
    endsWithFn->params.push_back({"suffix", reg.stringType()});
    endsWithFn->returnType = reg.boolType();
    symbols_.define(Symbol("ends_with", SymbolKind::FUNCTION, endsWithFn));
    
    // substring(s, start, len?) -> string
    auto substringFn = std::make_shared<FunctionType>();
    substringFn->params.push_back({"s", reg.stringType()});
    substringFn->params.push_back({"start", reg.intType()});
    substringFn->params.push_back({"len", reg.intType()});
    substringFn->isVariadic = true;  // len is optional
    substringFn->returnType = reg.stringType();
    symbols_.define(Symbol("substring", SymbolKind::FUNCTION, substringFn));
    
    // replace(s, old, new) -> string
    auto replaceFn = std::make_shared<FunctionType>();
    replaceFn->params.push_back({"s", reg.stringType()});
    replaceFn->params.push_back({"old", reg.stringType()});
    replaceFn->params.push_back({"new_str", reg.stringType()});
    replaceFn->returnType = reg.stringType();
    symbols_.define(Symbol("replace", SymbolKind::FUNCTION, replaceFn));
    
    // index_of(s, substr) -> int (-1 if not found)
    auto indexOfFn = std::make_shared<FunctionType>();
    indexOfFn->params.push_back({"s", reg.stringType()});
    indexOfFn->params.push_back({"substr", reg.stringType()});
    indexOfFn->returnType = reg.intType();
    symbols_.define(Symbol("index_of", SymbolKind::FUNCTION, indexOfFn));
    
    // split(s, delimiter) -> list[string]
    auto splitFn = std::make_shared<FunctionType>();
    splitFn->params.push_back({"s", reg.stringType()});
    splitFn->params.push_back({"delimiter", reg.stringType()});
    splitFn->returnType = reg.listType(reg.stringType());
    symbols_.define(Symbol("split", SymbolKind::FUNCTION, splitFn));
    
    // join(list, delimiter) -> string
    auto joinFn = std::make_shared<FunctionType>();
    joinFn->params.push_back({"list", reg.anyType()});
    joinFn->params.push_back({"delimiter", reg.stringType()});
    joinFn->returnType = reg.stringType();
    symbols_.define(Symbol("join", SymbolKind::FUNCTION, joinFn));
    
    // contains(s, sub) -> bool
    auto containsFn = std::make_shared<FunctionType>();
    containsFn->params.push_back({"s", reg.stringType()});
    containsFn->params.push_back({"sub", reg.stringType()});
    containsFn->returnType = reg.boolType();
    symbols_.define(Symbol("contains", SymbolKind::FUNCTION, containsFn));
    
    // range(n) or range(start, end) -> list[int]
    auto rangeFn = std::make_shared<FunctionType>();
    rangeFn->params.push_back({"n", reg.intType()});
    rangeFn->isVariadic = true;  // Can take 1-3 args
    rangeFn->returnType = reg.listType(reg.intType());
    symbols_.define(Symbol("range", SymbolKind::FUNCTION, rangeFn));
    
    // push(list, elem) -> list
    auto pushFn = std::make_shared<FunctionType>();
    pushFn->params.push_back({"list", reg.anyType()});
    pushFn->params.push_back({"elem", reg.anyType()});
    pushFn->returnType = reg.anyType();
    symbols_.define(Symbol("push", SymbolKind::FUNCTION, pushFn));
    
    // platform() -> string
    auto platformFn = std::make_shared<FunctionType>();
    platformFn->returnType = reg.stringType();
    symbols_.define(Symbol("platform", SymbolKind::FUNCTION, platformFn));
    
    // arch() -> string
    auto archFn = std::make_shared<FunctionType>();
    archFn->returnType = reg.stringType();
    symbols_.define(Symbol("arch", SymbolKind::FUNCTION, archFn));
    
    // hostname() -> string
    auto hostnameFn = std::make_shared<FunctionType>();
    hostnameFn->returnType = reg.stringType();
    symbols_.define(Symbol("hostname", SymbolKind::FUNCTION, hostnameFn));
    
    // username() -> string
    auto usernameFn = std::make_shared<FunctionType>();
    usernameFn->returnType = reg.stringType();
    symbols_.define(Symbol("username", SymbolKind::FUNCTION, usernameFn));
    
    // cpu_count() -> int
    auto cpuCountFn = std::make_shared<FunctionType>();
    cpuCountFn->returnType = reg.intType();
    symbols_.define(Symbol("cpu_count", SymbolKind::FUNCTION, cpuCountFn));
    
    // sleep(ms) -> void
    auto sleepFn = std::make_shared<FunctionType>();
    sleepFn->params.push_back({"ms", reg.intType()});
    sleepFn->returnType = reg.voidType();
    symbols_.define(Symbol("sleep", SymbolKind::FUNCTION, sleepFn));
    
    // now() -> int (seconds)
    auto nowFn = std::make_shared<FunctionType>();
    nowFn->returnType = reg.intType();
    symbols_.define(Symbol("now", SymbolKind::FUNCTION, nowFn));
    
    // now_ms() -> int (milliseconds)
    auto nowMsFn = std::make_shared<FunctionType>();
    nowMsFn->returnType = reg.intType();
    symbols_.define(Symbol("now_ms", SymbolKind::FUNCTION, nowMsFn));
    
    // year(), month(), day(), hour(), minute(), second() -> int
    auto timeFn = std::make_shared<FunctionType>();
    timeFn->returnType = reg.intType();
    symbols_.define(Symbol("year", SymbolKind::FUNCTION, timeFn));
    symbols_.define(Symbol("month", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("day", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("hour", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("minute", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    symbols_.define(Symbol("second", SymbolKind::FUNCTION, std::make_shared<FunctionType>(*timeFn)));
    
    // Result type functions
    // Ok(value) -> Result (encoded as int with LSB=1)
    auto okFn = std::make_shared<FunctionType>();
    okFn->params.push_back({"value", reg.anyType()});
    okFn->returnType = reg.intType();  // Result is encoded as int
    symbols_.define(Symbol("Ok", SymbolKind::FUNCTION, okFn));
    
    // Err(value) -> Result (encoded as int with LSB=0)
    auto errFn = std::make_shared<FunctionType>();
    errFn->params.push_back({"value", reg.anyType()});
    errFn->returnType = reg.intType();
    symbols_.define(Symbol("Err", SymbolKind::FUNCTION, errFn));
    
    // is_ok(result) -> bool
    auto isOkFn = std::make_shared<FunctionType>();
    isOkFn->params.push_back({"result", reg.anyType()});
    isOkFn->returnType = reg.boolType();
    symbols_.define(Symbol("is_ok", SymbolKind::FUNCTION, isOkFn));
    
    // is_err(result) -> bool
    auto isErrFn = std::make_shared<FunctionType>();
    isErrFn->params.push_back({"result", reg.anyType()});
    isErrFn->returnType = reg.boolType();
    symbols_.define(Symbol("is_err", SymbolKind::FUNCTION, isErrFn));
    
    // unwrap(result) -> any (extracts value from Ok)
    auto unwrapFn = std::make_shared<FunctionType>();
    unwrapFn->params.push_back({"result", reg.anyType()});
    unwrapFn->returnType = reg.anyType();
    symbols_.define(Symbol("unwrap", SymbolKind::FUNCTION, unwrapFn));
    
    // unwrap_or(result, default) -> any
    auto unwrapOrFn = std::make_shared<FunctionType>();
    unwrapOrFn->params.push_back({"result", reg.anyType()});
    unwrapOrFn->params.push_back({"default", reg.anyType()});
    unwrapOrFn->returnType = reg.anyType();
    symbols_.define(Symbol("unwrap_or", SymbolKind::FUNCTION, unwrapOrFn));
    
    // File I/O functions
    // open(filename, mode?) -> int (file handle, -1 on error)
    auto openFn = std::make_shared<FunctionType>();
    openFn->params.push_back({"filename", reg.stringType()});
    openFn->params.push_back({"mode", reg.stringType()});  // Optional: "r", "w", "a", "rw"
    openFn->isVariadic = true;  // mode is optional
    openFn->returnType = reg.intType();
    symbols_.define(Symbol("open", SymbolKind::FUNCTION, openFn));
    
    // read(handle, size) -> string
    auto readFn = std::make_shared<FunctionType>();
    readFn->params.push_back({"handle", reg.intType()});
    readFn->params.push_back({"size", reg.intType()});
    readFn->returnType = reg.stringType();
    symbols_.define(Symbol("read", SymbolKind::FUNCTION, readFn));
    
    // write(handle, data) -> int (bytes written)
    auto writeFn = std::make_shared<FunctionType>();
    writeFn->params.push_back({"handle", reg.intType()});
    writeFn->params.push_back({"data", reg.stringType()});
    writeFn->returnType = reg.intType();
    symbols_.define(Symbol("write", SymbolKind::FUNCTION, writeFn));
    
    // close(handle) -> int (0 on success)
    auto closeFn = std::make_shared<FunctionType>();
    closeFn->params.push_back({"handle", reg.intType()});
    closeFn->returnType = reg.intType();
    symbols_.define(Symbol("close", SymbolKind::FUNCTION, closeFn));
    
    // file_size(handle) -> int
    auto fileSizeFn = std::make_shared<FunctionType>();
    fileSizeFn->params.push_back({"handle", reg.intType()});
    fileSizeFn->returnType = reg.intType();
    symbols_.define(Symbol("file_size", SymbolKind::FUNCTION, fileSizeFn));
    
    // Garbage Collection functions
    // gc_collect() -> void - Force garbage collection
    auto gcCollectFn = std::make_shared<FunctionType>();
    gcCollectFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_collect", SymbolKind::FUNCTION, gcCollectFn));
    
    // gc_disable() -> void - Disable automatic GC
    auto gcDisableFn = std::make_shared<FunctionType>();
    gcDisableFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_disable", SymbolKind::FUNCTION, gcDisableFn));
    
    // gc_enable() -> void - Enable automatic GC
    auto gcEnableFn = std::make_shared<FunctionType>();
    gcEnableFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_enable", SymbolKind::FUNCTION, gcEnableFn));
    
    // gc_stats() -> int - Get GC statistics (allocated bytes)
    auto gcStatsFn = std::make_shared<FunctionType>();
    gcStatsFn->returnType = reg.intType();
    symbols_.define(Symbol("gc_stats", SymbolKind::FUNCTION, gcStatsFn));
    
    // gc_threshold() -> int or gc_threshold(bytes) -> void - Get or set collection threshold
    auto gcThresholdGetFn = std::make_shared<FunctionType>();
    gcThresholdGetFn->returnType = reg.intType();
    symbols_.define(Symbol("gc_threshold", SymbolKind::FUNCTION, gcThresholdGetFn));
    
    // gc_count() -> int - Get number of collections performed
    auto gcCountFn = std::make_shared<FunctionType>();
    gcCountFn->returnType = reg.intType();
    symbols_.define(Symbol("gc_count", SymbolKind::FUNCTION, gcCountFn));
    
    // Manual memory management (for unsafe blocks)
    // alloc(size) -> ptr - Allocate raw memory (not GC managed)
    auto allocFn = std::make_shared<FunctionType>();
    allocFn->params.push_back({"size", reg.intType()});
    allocFn->returnType = reg.intType();  // Returns pointer as int
    symbols_.define(Symbol("alloc", SymbolKind::FUNCTION, allocFn));
    
    // free(ptr) -> void - Free raw memory
    auto freeFn = std::make_shared<FunctionType>();
    freeFn->params.push_back({"ptr", reg.intType()});
    freeFn->returnType = reg.voidType();
    symbols_.define(Symbol("free", SymbolKind::FUNCTION, freeFn));
    
    // stackalloc(size) -> ptr - Allocate memory on the stack (requires unsafe)
    auto stackallocFn = std::make_shared<FunctionType>();
    stackallocFn->params.push_back({"size", reg.intType()});
    stackallocFn->returnType = reg.intType();  // Returns pointer as int
    symbols_.define(Symbol("stackalloc", SymbolKind::FUNCTION, stackallocFn));
    
    // placement_new(ptr, value) -> ptr - Construct value at specific address (requires unsafe)
    auto placementNewFn = std::make_shared<FunctionType>();
    placementNewFn->params.push_back({"ptr", reg.intType()});
    placementNewFn->params.push_back({"value", reg.anyType()});
    placementNewFn->returnType = reg.intType();  // Returns the same pointer
    symbols_.define(Symbol("placement_new", SymbolKind::FUNCTION, placementNewFn));
    
    // gc_pin(ptr) -> void - Pin GC object to prevent collection/movement (requires unsafe)
    auto gcPinFn = std::make_shared<FunctionType>();
    gcPinFn->params.push_back({"ptr", reg.intType()});
    gcPinFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_pin", SymbolKind::FUNCTION, gcPinFn));
    
    // gc_unpin(ptr) -> void - Unpin GC object (requires unsafe)
    auto gcUnpinFn = std::make_shared<FunctionType>();
    gcUnpinFn->params.push_back({"ptr", reg.intType()});
    gcUnpinFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_unpin", SymbolKind::FUNCTION, gcUnpinFn));
    
    // gc_add_root(ptr) -> void - Register external pointer as GC root (requires unsafe)
    auto gcAddRootFn = std::make_shared<FunctionType>();
    gcAddRootFn->params.push_back({"ptr", reg.intType()});
    gcAddRootFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_add_root", SymbolKind::FUNCTION, gcAddRootFn));
    
    // gc_remove_root(ptr) -> void - Unregister external pointer as GC root (requires unsafe)
    auto gcRemoveRootFn = std::make_shared<FunctionType>();
    gcRemoveRootFn->params.push_back({"ptr", reg.intType()});
    gcRemoveRootFn->returnType = reg.voidType();
    symbols_.define(Symbol("gc_remove_root", SymbolKind::FUNCTION, gcRemoveRootFn));
    
    // Custom allocator functions
    // set_allocator(alloc_fn, free_fn) -> void - Set custom allocator (requires unsafe)
    auto setAllocatorFn = std::make_shared<FunctionType>();
    setAllocatorFn->params.push_back({"alloc_fn", reg.intType()});  // Function pointer
    setAllocatorFn->params.push_back({"free_fn", reg.intType()});   // Function pointer
    setAllocatorFn->returnType = reg.voidType();
    symbols_.define(Symbol("set_allocator", SymbolKind::FUNCTION, setAllocatorFn));
    
    // reset_allocator() -> void - Reset to default system allocator
    auto resetAllocatorFn = std::make_shared<FunctionType>();
    resetAllocatorFn->returnType = reg.voidType();
    symbols_.define(Symbol("reset_allocator", SymbolKind::FUNCTION, resetAllocatorFn));
    
    // allocator_stats() -> int - Get total bytes allocated by current allocator
    auto allocatorStatsFn = std::make_shared<FunctionType>();
    allocatorStatsFn->returnType = reg.intType();
    symbols_.define(Symbol("allocator_stats", SymbolKind::FUNCTION, allocatorStatsFn));
    
    // allocator_peak() -> int - Get peak memory usage
    auto allocatorPeakFn = std::make_shared<FunctionType>();
    allocatorPeakFn->returnType = reg.intType();
    symbols_.define(Symbol("allocator_peak", SymbolKind::FUNCTION, allocatorPeakFn));
    
    // Type introspection functions
    // sizeof(T) -> int - Get byte size of type
    auto sizeofFn = std::make_shared<FunctionType>();
    sizeofFn->params.push_back({"type", reg.anyType()});  // Type name passed as identifier
    sizeofFn->returnType = reg.intType();
    symbols_.define(Symbol("sizeof", SymbolKind::FUNCTION, sizeofFn));
    
    // alignof(T) -> int - Get alignment requirement of type
    auto alignofFn = std::make_shared<FunctionType>();
    alignofFn->params.push_back({"type", reg.anyType()});  // Type name passed as identifier
    alignofFn->returnType = reg.intType();
    symbols_.define(Symbol("alignof", SymbolKind::FUNCTION, alignofFn));
    
    // offsetof(Record, field) -> int - Get byte offset of field in record
    auto offsetofFn = std::make_shared<FunctionType>();
    offsetofFn->params.push_back({"record", reg.anyType()});  // Record type name
    offsetofFn->params.push_back({"field", reg.anyType()});   // Field name
    offsetofFn->returnType = reg.intType();
    symbols_.define(Symbol("offsetof", SymbolKind::FUNCTION, offsetofFn));
}

bool TypeChecker::check(Program& program) {
    diagnostics_.clear();
    exprTypes_.clear();
    currentTypeParams_.clear();
    currentTypeParamNames_.clear();
    program.accept(*this);
    return !hasErrors();
}

bool TypeChecker::hasErrors() const {
    for (auto& d : diagnostics_) {
        if (d.level == TypeDiagnostic::Level::ERROR) return true;
    }
    return false;
}

TypePtr TypeChecker::getType(Expression* expr) {
    auto it = exprTypes_.find(expr);
    return it != exprTypes_.end() ? it->second : TypeRegistry::instance().unknownType();
}

TypePtr TypeChecker::inferType(Expression* expr) {
    expr->accept(*this);
    exprTypes_[expr] = currentType_;
    return currentType_;
}

void TypeChecker::error(const std::string& msg, const SourceLocation& loc) {
    diagnostics_.emplace_back(TypeDiagnostic::Level::ERROR, msg, loc);
}

void TypeChecker::warning(const std::string& msg, const SourceLocation& loc) {
    diagnostics_.emplace_back(TypeDiagnostic::Level::WARNING, msg, loc);
}

void TypeChecker::note(const std::string& msg, const SourceLocation& loc) {
    diagnostics_.emplace_back(TypeDiagnostic::Level::NOTE, msg, loc);
}

TypePtr TypeChecker::parseTypeAnnotation(const std::string& str) {
    if (str.empty()) return TypeRegistry::instance().unknownType();
    
    // Check for generic type syntax: Name[T, U, ...]
    auto genericType = parseGenericType(str);
    if (genericType) return genericType;
    
    // Check if it's a type parameter in scope
    auto typeParam = resolveTypeParam(str);
    if (typeParam) return typeParam;
    
    return TypeRegistry::instance().fromString(str);
}

TypePtr TypeChecker::parseGenericType(const std::string& str) {
    auto& reg = TypeRegistry::instance();
    
    // Match pattern: Name[Type1, Type2, ...]
    size_t bracketPos = str.find('[');
    if (bracketPos == std::string::npos) return nullptr;
    
    std::string baseName = str.substr(0, bracketPos);
    size_t endBracket = str.rfind(']');
    if (endBracket == std::string::npos || endBracket <= bracketPos) return nullptr;
    
    std::string argsStr = str.substr(bracketPos + 1, endBracket - bracketPos - 1);
    
    // Parse type arguments (simple comma-separated for now)
    std::vector<TypePtr> typeArgs;
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i <= argsStr.size(); i++) {
        if (i == argsStr.size() || (argsStr[i] == ',' && depth == 0)) {
            std::string arg = argsStr.substr(start, i - start);
            // Trim whitespace
            size_t first = arg.find_first_not_of(" \t");
            size_t last = arg.find_last_not_of(" \t");
            if (first != std::string::npos) {
                arg = arg.substr(first, last - first + 1);
            }
            if (!arg.empty()) {
                typeArgs.push_back(parseTypeAnnotation(arg));
            }
            start = i + 1;
        } else if (argsStr[i] == '[') {
            depth++;
        } else if (argsStr[i] == ']') {
            depth--;
        }
    }
    
    // Check for built-in generic types
    if (baseName == "List" || baseName == "list") {
        if (typeArgs.size() == 1) {
            return reg.listType(typeArgs[0]);
        }
    } else if (baseName == "Map" || baseName == "map") {
        if (typeArgs.size() == 2) {
            return reg.mapType(typeArgs[0], typeArgs[1]);
        }
    } else if (baseName == "Result") {
        // Result[T, E] - for now treat as generic
        return reg.genericType(baseName, typeArgs);
    }
    
    // Look up user-defined generic type
    TypePtr baseType = reg.lookupType(baseName);
    if (baseType) {
        return reg.instantiateGeneric(baseType, typeArgs);
    }
    
    // Return as unresolved generic
    return reg.genericType(baseName, typeArgs);
}

TypePtr TypeChecker::resolveTypeParam(const std::string& name) {
    // Check if name is a type parameter in current scope
    auto it = currentTypeParams_.find(name);
    if (it != currentTypeParams_.end()) {
        return it->second;
    }
    
    // Check if it's in the list of type param names (unbound)
    for (const auto& paramName : currentTypeParamNames_) {
        if (paramName == name) {
            return TypeRegistry::instance().typeParamType(name);
        }
    }
    
    return nullptr;
}

bool TypeChecker::checkTraitBounds(TypePtr type, const std::vector<std::string>& bounds, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    
    for (const auto& bound : bounds) {
        if (!reg.typeImplementsTrait(type, bound)) {
            error("Type '" + type->toString() + "' does not implement trait '" + bound + "'", loc);
            return false;
        }
    }
    return true;
}

TypePtr TypeChecker::instantiateGenericFunction(FunctionType* fnType, const std::vector<TypePtr>& typeArgs, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    
    if (fnType->typeParams.size() != typeArgs.size()) {
        error("Wrong number of type arguments: expected " + std::to_string(fnType->typeParams.size()) +
              ", got " + std::to_string(typeArgs.size()), loc);
        return reg.errorType();
    }
    
    // Build substitution map
    std::unordered_map<std::string, TypePtr> substitutions;
    for (size_t i = 0; i < fnType->typeParams.size(); i++) {
        substitutions[fnType->typeParams[i]] = typeArgs[i];
    }
    
    // Create instantiated function type
    auto newFn = std::make_shared<FunctionType>();
    for (const auto& param : fnType->params) {
        newFn->params.push_back({param.first, reg.substituteTypeParams(param.second, substitutions)});
    }
    newFn->returnType = reg.substituteTypeParams(fnType->returnType, substitutions);
    newFn->isVariadic = fnType->isVariadic;
    
    return newFn;
}

void TypeChecker::checkTraitImpl(const std::string& traitName, const std::string& typeName,
                                  const std::vector<std::unique_ptr<FnDecl>>& methods, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    
    TraitPtr trait = reg.lookupTrait(traitName);
    if (!trait) {
        error("Unknown trait '" + traitName + "'", loc);
        return;
    }
    
    // Collect all required methods (including from super traits)
    std::vector<std::pair<std::string, const TraitMethod*>> requiredMethods;
    
    // Add methods from this trait
    for (const auto& traitMethod : trait->methods) {
        requiredMethods.push_back({traitName, &traitMethod});
    }
    
    // Add methods from super traits (recursively)
    std::function<void(const std::string&)> collectSuperMethods = [&](const std::string& superName) {
        TraitPtr superTrait = reg.lookupTrait(superName);
        if (!superTrait) return;
        
        for (const auto& method : superTrait->methods) {
            requiredMethods.push_back({superName, &method});
        }
        
        // Recurse into super traits of super traits
        for (const auto& superSuper : superTrait->superTraits) {
            collectSuperMethods(superSuper);
        }
    };
    
    for (const auto& superTrait : trait->superTraits) {
        collectSuperMethods(superTrait);
    }
    
    // Check that all required methods are implemented
    for (const auto& [fromTrait, traitMethod] : requiredMethods) {
        if (traitMethod->hasDefaultImpl) continue;  // Has default, not required
        
        bool found = false;
        for (const auto& implMethod : methods) {
            if (implMethod->name == traitMethod->name) {
                found = true;
                
                // Check method signature matches
                auto implFnType = std::make_shared<FunctionType>();
                for (const auto& p : implMethod->params) {
                    implFnType->params.push_back({p.first, parseTypeAnnotation(p.second)});
                }
                implFnType->returnType = parseTypeAnnotation(implMethod->returnType);
                
                // Compare signatures (simplified - just check param count and return type)
                if (implFnType->params.size() != traitMethod->signature->params.size()) {
                    error("Method '" + traitMethod->name + "' has wrong number of parameters", implMethod->location);
                }
                
                break;
            }
        }
        
        if (!found) {
            std::string errorMsg = "Missing implementation of method '" + traitMethod->name + "'";
            if (fromTrait != traitName) {
                errorMsg += " (required by super trait '" + fromTrait + "')";
            }
            errorMsg += " for trait '" + traitName + "'";
            error(errorMsg, loc);
        }
    }
    
    // Register the implementation
    TraitImpl impl;
    impl.traitName = traitName;
    impl.typeName = typeName;
    for (const auto& method : methods) {
        auto fnType = std::make_shared<FunctionType>();
        for (const auto& p : method->params) {
            fnType->params.push_back({p.first, parseTypeAnnotation(p.second)});
        }
        fnType->returnType = parseTypeAnnotation(method->returnType);
        impl.methods[method->name] = fnType;
    }
    reg.registerTraitImpl(impl);
}

TypePtr TypeChecker::unify(TypePtr a, TypePtr b, const SourceLocation& loc) {
    auto& reg = TypeRegistry::instance();
    if (a->kind == TypeKind::UNKNOWN) return b;
    if (b->kind == TypeKind::UNKNOWN) return a;
    if (a->kind == TypeKind::ANY || b->kind == TypeKind::ANY) return reg.anyType();
    if (a->equals(b.get())) return a;
    
    // Handle type parameter unification
    if (a->kind == TypeKind::TYPE_PARAM) {
        auto* tp = static_cast<TypeParamType*>(a.get());
        // Check bounds
        if (!tp->bounds.empty() && !reg.checkTraitBounds(b, tp->bounds)) {
            error("Type '" + b->toString() + "' does not satisfy bounds of '" + tp->name + "'", loc);
            return reg.errorType();
        }
        return b;
    }
    if (b->kind == TypeKind::TYPE_PARAM) {
        auto* tp = static_cast<TypeParamType*>(b.get());
        if (!tp->bounds.empty() && !reg.checkTraitBounds(a, tp->bounds)) {
            error("Type '" + a->toString() + "' does not satisfy bounds of '" + tp->name + "'", loc);
            return reg.errorType();
        }
        return a;
    }
    
    if (a->isNumeric() && b->isNumeric()) {
        if (a->isFloat() || b->isFloat()) return reg.floatType();
        if (a->size() >= b->size()) return a;
        return b;
    }
    error("Cannot unify types '" + a->toString() + "' and '" + b->toString() + "'", loc);
    return reg.errorType();
}

void TypeChecker::checkUnusedVariables(Scope* scope) {
    if (!scope) return;
    
    for (auto& [name, sym] : scope->symbolsMut()) {
        // Skip functions, types, modules, etc.
        if (sym.kind != SymbolKind::VARIABLE && sym.kind != SymbolKind::PARAMETER) {
            continue;
        }
        
        // Skip if already used
        if (sym.isUsed) continue;
        
        // Skip special variables (loop variables, destructuring temps, etc.)
        if (name.empty() || name[0] == '$' || name == "_") continue;
        
        // Skip parameters that start with underscore (intentionally unused)
        if (name.size() > 1 && name[0] == '_') continue;
        
        // Generate warning
        if (sym.kind == SymbolKind::PARAMETER) {
            warning("Unused parameter '" + name + "'", sym.location);
        } else {
            warning("Unused variable '" + name + "'", sym.location);
        }
    }
}

} // namespace flex
