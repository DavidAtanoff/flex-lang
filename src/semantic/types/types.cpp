// Flex Compiler - Type System Implementation
#include "semantic/types/types.h"
#include <algorithm>

namespace flex {

std::string Type::toString() const {
    switch (kind) {
        case TypeKind::VOID: return "void"; case TypeKind::BOOL: return "bool";
        case TypeKind::INT: return "int"; case TypeKind::INT8: return "i8"; case TypeKind::INT16: return "i16";
        case TypeKind::INT32: return "i32"; case TypeKind::INT64: return "i64";
        case TypeKind::UINT8: return "u8"; case TypeKind::UINT16: return "u16";
        case TypeKind::UINT32: return "u32"; case TypeKind::UINT64: return "u64";
        case TypeKind::FLOAT: return "float"; case TypeKind::FLOAT32: return "f32"; case TypeKind::FLOAT64: return "f64";
        case TypeKind::STRING: return "str"; case TypeKind::ANY: return "any";
        case TypeKind::NEVER: return "never"; case TypeKind::UNKNOWN: return "?"; case TypeKind::ERROR: return "<error>";
        case TypeKind::TYPE_PARAM: return "<type_param>";
        case TypeKind::GENERIC: return "<generic>";
        case TypeKind::TRAIT: return "<trait>";
        case TypeKind::TRAIT_OBJECT: return "<dyn>";
        case TypeKind::FIXED_ARRAY: return "<fixed_array>";
        default: return "<type>";
    }
}

bool Type::equals(const Type* other) const { return other && kind == other->kind; }
TypePtr Type::clone() const { return std::make_shared<Type>(kind); }
bool Type::isNumeric() const { return isInteger() || isFloat(); }
bool Type::isInteger() const {
    switch (kind) {
        case TypeKind::INT: case TypeKind::INT8: case TypeKind::INT16: case TypeKind::INT32: case TypeKind::INT64:
        case TypeKind::UINT8: case TypeKind::UINT16: case TypeKind::UINT32: case TypeKind::UINT64: return true;
        default: return false;
    }
}
bool Type::isFloat() const { return kind == TypeKind::FLOAT || kind == TypeKind::FLOAT32 || kind == TypeKind::FLOAT64; }
bool Type::isPrimitive() const {
    switch (kind) {
        case TypeKind::VOID: case TypeKind::BOOL: case TypeKind::INT: case TypeKind::INT8: case TypeKind::INT16:
        case TypeKind::INT32: case TypeKind::INT64: case TypeKind::UINT8: case TypeKind::UINT16: case TypeKind::UINT32:
        case TypeKind::UINT64: case TypeKind::FLOAT: case TypeKind::FLOAT32: case TypeKind::FLOAT64: return true;
        default: return false;
    }
}
bool Type::isReference() const {
    switch (kind) {
        case TypeKind::STRING: case TypeKind::LIST: case TypeKind::MAP: case TypeKind::RECORD:
        case TypeKind::FUNCTION: case TypeKind::REF: return true;
        default: return false;
    }
}
bool Type::isPointer() const { return kind == TypeKind::PTR || kind == TypeKind::REF; }
size_t Type::size() const {
    switch (kind) {
        case TypeKind::VOID: return 0; case TypeKind::BOOL: return 1;
        case TypeKind::INT8: case TypeKind::UINT8: return 1;
        case TypeKind::INT16: case TypeKind::UINT16: return 2;
        case TypeKind::INT32: case TypeKind::UINT32: case TypeKind::FLOAT32: return 4;
        case TypeKind::INT: case TypeKind::INT64: case TypeKind::UINT64: case TypeKind::FLOAT: case TypeKind::FLOAT64: return 8;
        case TypeKind::PTR: case TypeKind::REF: return 8;
        default: return 8;
    }
}
size_t Type::alignment() const { return size(); }

std::string PrimitiveType::toString() const { return Type::toString(); }
size_t PrimitiveType::size() const { return Type::size(); }

std::string PtrType::toString() const { return (isRaw ? "ptr<" : "ref<") + pointee->toString() + ">"; }
bool PtrType::equals(const Type* other) const {
    if (auto* p = dynamic_cast<const PtrType*>(other)) return isRaw == p->isRaw && pointee->equals(p->pointee.get());
    return false;
}
TypePtr PtrType::clone() const { return std::make_shared<PtrType>(pointee->clone(), isRaw); }

std::string ListType::toString() const { return "[" + element->toString() + "]"; }
bool ListType::equals(const Type* other) const {
    if (auto* l = dynamic_cast<const ListType*>(other)) return element->equals(l->element.get());
    return false;
}
TypePtr ListType::clone() const { return std::make_shared<ListType>(element->clone()); }

std::string MapType::toString() const { return "{" + key->toString() + ": " + value->toString() + "}"; }
bool MapType::equals(const Type* other) const {
    if (auto* m = dynamic_cast<const MapType*>(other)) return key->equals(m->key.get()) && value->equals(m->value.get());
    return false;
}
TypePtr MapType::clone() const { return std::make_shared<MapType>(key->clone(), value->clone()); }

std::string RecordType::toString() const {
    if (!name.empty()) return name;
    std::string s = "{";
    for (size_t i = 0; i < fields.size(); i++) { if (i > 0) s += ", "; s += fields[i].name + ": " + fields[i].type->toString(); }
    return s + "}";
}
bool RecordType::equals(const Type* other) const {
    if (auto* r = dynamic_cast<const RecordType*>(other)) {
        if (!name.empty() && !r->name.empty()) return name == r->name;
        if (fields.size() != r->fields.size()) return false;
        for (size_t i = 0; i < fields.size(); i++) {
            if (fields[i].name != r->fields[i].name || !fields[i].type->equals(r->fields[i].type.get())) return false;
        }
        return true;
    }
    return false;
}
TypePtr RecordType::clone() const {
    auto r = std::make_shared<RecordType>(name);
    for (auto& f : fields) r->fields.push_back({f.name, f.type->clone(), f.hasDefault});
    return r;
}
TypePtr RecordType::getField(const std::string& fieldName) const {
    for (auto& f : fields) if (f.name == fieldName) return f.type;
    return nullptr;
}

std::string FunctionType::toString() const {
    std::string s = "fn";
    if (!typeParams.empty()) {
        s += "[";
        for (size_t i = 0; i < typeParams.size(); i++) {
            if (i > 0) s += ", ";
            s += typeParams[i];
        }
        s += "]";
    }
    s += "(";
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) s += ", ";
        if (!params[i].first.empty()) s += params[i].first + ": ";
        s += params[i].second->toString();
    }
    if (isVariadic) s += "...";
    s += ")";
    if (returnType && returnType->kind != TypeKind::VOID) s += " -> " + returnType->toString();
    return s;
}
bool FunctionType::equals(const Type* other) const {
    if (auto* f = dynamic_cast<const FunctionType*>(other)) {
        if (params.size() != f->params.size()) return false;
        if (typeParams.size() != f->typeParams.size()) return false;
        for (size_t i = 0; i < params.size(); i++) if (!params[i].second->equals(f->params[i].second.get())) return false;
        if (returnType && f->returnType) return returnType->equals(f->returnType.get());
        return !returnType && !f->returnType;
    }
    return false;
}
TypePtr FunctionType::clone() const {
    auto f = std::make_shared<FunctionType>();
    for (auto& p : params) f->params.push_back({p.first, p.second->clone()});
    if (returnType) f->returnType = returnType->clone();
    f->isVariadic = isVariadic;
    f->typeParams = typeParams;
    return f;
}

// TypeParamType implementation
std::string TypeParamType::toString() const {
    std::string s = name;
    if (!bounds.empty()) {
        s += ": ";
        for (size_t i = 0; i < bounds.size(); i++) {
            if (i > 0) s += " + ";
            s += bounds[i];
        }
    }
    return s;
}
bool TypeParamType::equals(const Type* other) const {
    if (auto* tp = dynamic_cast<const TypeParamType*>(other)) {
        return name == tp->name;
    }
    return false;
}
TypePtr TypeParamType::clone() const {
    auto tp = std::make_shared<TypeParamType>(name);
    tp->bounds = bounds;
    if (defaultType) tp->defaultType = defaultType->clone();
    return tp;
}
bool TypeParamType::satisfiesBound(const std::string& traitName) const {
    return std::find(bounds.begin(), bounds.end(), traitName) != bounds.end();
}

// TraitType implementation
std::string TraitType::toString() const {
    std::string s = "trait " + name;
    if (!typeParams.empty()) {
        s += "[";
        for (size_t i = 0; i < typeParams.size(); i++) {
            if (i > 0) s += ", ";
            s += typeParams[i];
        }
        s += "]";
    }
    return s;
}
bool TraitType::equals(const Type* other) const {
    if (auto* t = dynamic_cast<const TraitType*>(other)) {
        return name == t->name;
    }
    return false;
}
TypePtr TraitType::clone() const {
    auto t = std::make_shared<TraitType>(name);
    t->typeParams = typeParams;
    t->methods = methods;
    t->superTraits = superTraits;
    return t;
}
const TraitMethod* TraitType::getMethod(const std::string& methodName) const {
    for (const auto& m : methods) {
        if (m.name == methodName) return &m;
    }
    return nullptr;
}

// TraitObjectType implementation
std::string TraitObjectType::toString() const {
    return "dyn " + traitName;
}
bool TraitObjectType::equals(const Type* other) const {
    if (auto* to = dynamic_cast<const TraitObjectType*>(other)) {
        return traitName == to->traitName;
    }
    return false;
}
TypePtr TraitObjectType::clone() const {
    return std::make_shared<TraitObjectType>(traitName, trait);
}

// GenericType implementation
std::string GenericType::toString() const {
    std::string s = baseName + "[";
    for (size_t i = 0; i < typeArgs.size(); i++) {
        if (i > 0) s += ", ";
        s += typeArgs[i]->toString();
    }
    return s + "]";
}
bool GenericType::equals(const Type* other) const {
    if (auto* g = dynamic_cast<const GenericType*>(other)) {
        if (baseName != g->baseName || typeArgs.size() != g->typeArgs.size()) return false;
        for (size_t i = 0; i < typeArgs.size(); i++) {
            if (!typeArgs[i]->equals(g->typeArgs[i].get())) return false;
        }
        return true;
    }
    return false;
}
TypePtr GenericType::clone() const {
    auto g = std::make_shared<GenericType>(baseName);
    for (const auto& arg : typeArgs) {
        g->typeArgs.push_back(arg->clone());
    }
    if (resolvedType) g->resolvedType = resolvedType->clone();
    return g;
}

// FixedArrayType implementation
std::string FixedArrayType::toString() const {
    return "[" + element->toString() + "; " + std::to_string(size) + "]";
}
bool FixedArrayType::equals(const Type* other) const {
    if (auto* fa = dynamic_cast<const FixedArrayType*>(other)) {
        return size == fa->size && element->equals(fa->element.get());
    }
    return false;
}
TypePtr FixedArrayType::clone() const {
    return std::make_shared<FixedArrayType>(element->clone(), size);
}
size_t FixedArrayType::elementSize() const {
    if (auto* nested = dynamic_cast<FixedArrayType*>(element.get())) {
        return nested->totalSize();
    }
    return element->size();
}
size_t FixedArrayType::totalSize() const {
    return elementSize() * size;
}
size_t FixedArrayType::dimensions() const {
    if (auto* nested = dynamic_cast<FixedArrayType*>(element.get())) {
        return 1 + nested->dimensions();
    }
    return 1;
}
std::vector<size_t> FixedArrayType::shape() const {
    std::vector<size_t> result;
    result.push_back(size);
    if (auto* nested = dynamic_cast<FixedArrayType*>(element.get())) {
        auto nestedShape = nested->shape();
        result.insert(result.end(), nestedShape.begin(), nestedShape.end());
    }
    return result;
}

// ChannelType implementation
std::string ChannelType::toString() const {
    if (bufferSize > 0) {
        return "chan[" + element->toString() + ", " + std::to_string(bufferSize) + "]";
    }
    return "chan[" + element->toString() + "]";
}
bool ChannelType::equals(const Type* other) const {
    if (auto* ch = dynamic_cast<const ChannelType*>(other)) {
        return bufferSize == ch->bufferSize && element->equals(ch->element.get());
    }
    return false;
}
TypePtr ChannelType::clone() const {
    return std::make_shared<ChannelType>(element->clone(), bufferSize);
}

// MutexType implementation
std::string MutexType::toString() const {
    return "Mutex[" + element->toString() + "]";
}
bool MutexType::equals(const Type* other) const {
    if (auto* m = dynamic_cast<const MutexType*>(other)) {
        return element->equals(m->element.get());
    }
    return false;
}
TypePtr MutexType::clone() const {
    return std::make_shared<MutexType>(element->clone());
}

// RWLockType implementation
std::string RWLockType::toString() const {
    return "RWLock[" + element->toString() + "]";
}
bool RWLockType::equals(const Type* other) const {
    if (auto* r = dynamic_cast<const RWLockType*>(other)) {
        return element->equals(r->element.get());
    }
    return false;
}
TypePtr RWLockType::clone() const {
    return std::make_shared<RWLockType>(element->clone());
}

// CondType implementation
std::string CondType::toString() const {
    return "Cond";
}
bool CondType::equals(const Type* other) const {
    return dynamic_cast<const CondType*>(other) != nullptr;
}
TypePtr CondType::clone() const {
    return std::make_shared<CondType>();
}

// SemaphoreType implementation
std::string SemaphoreType::toString() const {
    return "Semaphore";
}
bool SemaphoreType::equals(const Type* other) const {
    return dynamic_cast<const SemaphoreType*>(other) != nullptr;
}
TypePtr SemaphoreType::clone() const {
    return std::make_shared<SemaphoreType>();
}


TypeRegistry& TypeRegistry::instance() { static TypeRegistry reg; return reg; }

TypeRegistry::TypeRegistry() {
    void_ = std::make_shared<PrimitiveType>(TypeKind::VOID);
    bool_ = std::make_shared<PrimitiveType>(TypeKind::BOOL);
    int_ = std::make_shared<PrimitiveType>(TypeKind::INT);
    int8_ = std::make_shared<PrimitiveType>(TypeKind::INT8);
    int16_ = std::make_shared<PrimitiveType>(TypeKind::INT16);
    int32_ = std::make_shared<PrimitiveType>(TypeKind::INT32);
    int64_ = std::make_shared<PrimitiveType>(TypeKind::INT64);
    uint8_ = std::make_shared<PrimitiveType>(TypeKind::UINT8);
    uint16_ = std::make_shared<PrimitiveType>(TypeKind::UINT16);
    uint32_ = std::make_shared<PrimitiveType>(TypeKind::UINT32);
    uint64_ = std::make_shared<PrimitiveType>(TypeKind::UINT64);
    float_ = std::make_shared<PrimitiveType>(TypeKind::FLOAT);
    float32_ = std::make_shared<PrimitiveType>(TypeKind::FLOAT32);
    float64_ = std::make_shared<PrimitiveType>(TypeKind::FLOAT64);
    string_ = std::make_shared<PrimitiveType>(TypeKind::STRING);
    any_ = std::make_shared<PrimitiveType>(TypeKind::ANY);
    never_ = std::make_shared<PrimitiveType>(TypeKind::NEVER);
    unknown_ = std::make_shared<PrimitiveType>(TypeKind::UNKNOWN);
    error_ = std::make_shared<PrimitiveType>(TypeKind::ERROR);
    
    namedTypes_["void"] = void_; namedTypes_["bool"] = bool_; namedTypes_["int"] = int_;
    namedTypes_["i8"] = int8_; namedTypes_["i16"] = int16_; namedTypes_["i32"] = int32_; namedTypes_["i64"] = int64_;
    namedTypes_["u8"] = uint8_; namedTypes_["u16"] = uint16_; namedTypes_["u32"] = uint32_; namedTypes_["u64"] = uint64_;
    namedTypes_["float"] = float_; namedTypes_["f32"] = float32_; namedTypes_["f64"] = float64_;
    namedTypes_["str"] = string_; namedTypes_["string"] = string_; namedTypes_["any"] = any_;
}

TypePtr TypeRegistry::voidType() { return void_; }
TypePtr TypeRegistry::boolType() { return bool_; }
TypePtr TypeRegistry::intType() { return int_; }
TypePtr TypeRegistry::int8Type() { return int8_; }
TypePtr TypeRegistry::int16Type() { return int16_; }
TypePtr TypeRegistry::int32Type() { return int32_; }
TypePtr TypeRegistry::int64Type() { return int64_; }
TypePtr TypeRegistry::uint8Type() { return uint8_; }
TypePtr TypeRegistry::uint16Type() { return uint16_; }
TypePtr TypeRegistry::uint32Type() { return uint32_; }
TypePtr TypeRegistry::uint64Type() { return uint64_; }
TypePtr TypeRegistry::floatType() { return float_; }
TypePtr TypeRegistry::float32Type() { return float32_; }
TypePtr TypeRegistry::float64Type() { return float64_; }
TypePtr TypeRegistry::stringType() { return string_; }
TypePtr TypeRegistry::anyType() { return any_; }
TypePtr TypeRegistry::neverType() { return never_; }
TypePtr TypeRegistry::unknownType() { return unknown_; }
TypePtr TypeRegistry::errorType() { return error_; }
TypePtr TypeRegistry::ptrType(TypePtr pointee, bool raw) { return std::make_shared<PtrType>(std::move(pointee), raw); }
TypePtr TypeRegistry::refType(TypePtr pointee) { return std::make_shared<PtrType>(std::move(pointee), false); }
TypePtr TypeRegistry::listType(TypePtr element) { return std::make_shared<ListType>(std::move(element)); }
TypePtr TypeRegistry::mapType(TypePtr key, TypePtr value) { return std::make_shared<MapType>(std::move(key), std::move(value)); }
TypePtr TypeRegistry::recordType(const std::string& name) { return std::make_shared<RecordType>(name); }
TypePtr TypeRegistry::functionType() { return std::make_shared<FunctionType>(); }
TypePtr TypeRegistry::fromString(const std::string& str) {
    // Handle empty string
    if (str.empty()) return unknown_;
    
    // Handle pointer types: *T, **T, etc.
    if (str[0] == '*') {
        std::string pointeeStr = str.substr(1);
        TypePtr pointee = fromString(pointeeStr);
        return ptrType(pointee, true);  // raw pointer
    }
    
    // Handle reference types: &T, &mut T
    if (str[0] == '&') {
        std::string rest = str.substr(1);
        bool isMut = false;
        if (rest.size() > 4 && rest.substr(0, 4) == "mut ") {
            isMut = true;
            rest = rest.substr(4);
        }
        TypePtr pointee = fromString(rest);
        auto ref = refType(pointee);
        ref->isMutable = isMut;
        return ref;
    }
    
    // Handle ptr<T> syntax (legacy)
    if (str.size() > 4 && str.substr(0, 4) == "ptr<" && str.back() == '>') {
        std::string pointeeStr = str.substr(4, str.size() - 5);
        TypePtr pointee = fromString(pointeeStr);
        return ptrType(pointee, true);
    }
    
    // Handle ref<T> syntax
    if (str.size() > 4 && str.substr(0, 4) == "ref<" && str.back() == '>') {
        std::string pointeeStr = str.substr(4, str.size() - 5);
        TypePtr pointee = fromString(pointeeStr);
        return refType(pointee);
    }
    
    // Handle channel types: chan[T] or chan[T, N]
    if (str.size() > 5 && str.substr(0, 5) == "chan[" && str.back() == ']') {
        std::string inner = str.substr(5, str.size() - 6);
        
        // Check for buffered channel syntax: chan[T, N]
        int bracketDepth = 0;
        size_t commaPos = std::string::npos;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '[') bracketDepth++;
            else if (inner[i] == ']') bracketDepth--;
            else if (inner[i] == ',' && bracketDepth == 0) {
                commaPos = i;
                break;
            }
        }
        
        if (commaPos != std::string::npos) {
            // Buffered channel: chan[T, N]
            std::string elemStr = inner.substr(0, commaPos);
            std::string sizeStr = inner.substr(commaPos + 1);
            // Trim whitespace
            while (!sizeStr.empty() && (sizeStr[0] == ' ' || sizeStr[0] == '\t')) sizeStr = sizeStr.substr(1);
            while (!sizeStr.empty() && (sizeStr.back() == ' ' || sizeStr.back() == '\t')) sizeStr.pop_back();
            
            TypePtr elem = fromString(elemStr);
            size_t bufSize = std::stoull(sizeStr);
            return channelType(elem, bufSize);
        }
        
        // Unbuffered channel: chan[T]
        TypePtr elem = fromString(inner);
        return channelType(elem, 0);
    }
    
    // Handle Mutex types: Mutex[T]
    if (str.size() > 6 && str.substr(0, 6) == "Mutex[" && str.back() == ']') {
        std::string inner = str.substr(6, str.size() - 7);
        TypePtr elem = fromString(inner);
        return mutexType(elem);
    }
    
    // Handle RWLock types: RWLock[T]
    if (str.size() > 7 && str.substr(0, 7) == "RWLock[" && str.back() == ']') {
        std::string inner = str.substr(7, str.size() - 8);
        TypePtr elem = fromString(inner);
        return rwlockType(elem);
    }
    
    // Handle Cond type
    if (str == "Cond") {
        return condType();
    }
    
    // Handle Semaphore type
    if (str == "Semaphore") {
        return semaphoreType();
    }
    
    // Handle list types: [T] or fixed-size arrays: [T; N]
    if (str.size() > 2 && str[0] == '[' && str.back() == ']') {
        std::string inner = str.substr(1, str.size() - 2);
        
        // Check for fixed-size array syntax: [T; N]
        // Need to find the semicolon that's not inside nested brackets
        int bracketDepth = 0;
        size_t semicolonPos = std::string::npos;
        for (size_t i = 0; i < inner.size(); i++) {
            if (inner[i] == '[') bracketDepth++;
            else if (inner[i] == ']') bracketDepth--;
            else if (inner[i] == ';' && bracketDepth == 0) {
                semicolonPos = i;
                break;
            }
        }
        
        if (semicolonPos != std::string::npos) {
            // Fixed-size array: [T; N]
            std::string elemStr = inner.substr(0, semicolonPos);
            std::string sizeStr = inner.substr(semicolonPos + 1);
            // Trim whitespace from sizeStr
            while (!sizeStr.empty() && (sizeStr[0] == ' ' || sizeStr[0] == '\t')) sizeStr = sizeStr.substr(1);
            while (!sizeStr.empty() && (sizeStr.back() == ' ' || sizeStr.back() == '\t')) sizeStr.pop_back();
            
            TypePtr elem = fromString(elemStr);
            size_t arraySize = std::stoull(sizeStr);
            return fixedArrayType(elem, arraySize);
        }
        
        // Regular list type: [T]
        TypePtr elem = fromString(inner);
        return listType(elem);
    }
    
    // Handle function pointer types: fn(...) -> T or fn(int, int) -> int
    if (str.size() > 2 && str.substr(0, 2) == "fn") {
        auto fnType = std::make_shared<FunctionType>();
        
        // Find the opening parenthesis
        size_t parenStart = str.find('(');
        if (parenStart == std::string::npos) {
            return fnType;  // Just "fn" without params
        }
        
        // Find matching closing parenthesis
        int depth = 1;
        size_t parenEnd = parenStart + 1;
        while (parenEnd < str.size() && depth > 0) {
            if (str[parenEnd] == '(') depth++;
            else if (str[parenEnd] == ')') depth--;
            parenEnd++;
        }
        parenEnd--;  // Point to the closing paren
        
        // Parse parameter types
        std::string paramsStr = str.substr(parenStart + 1, parenEnd - parenStart - 1);
        if (!paramsStr.empty() && paramsStr != "...") {
            // Split by comma, respecting nested brackets
            size_t start = 0;
            int bracketDepth = 0;
            int parenDepth = 0;
            for (size_t i = 0; i <= paramsStr.size(); i++) {
                if (i == paramsStr.size() || (paramsStr[i] == ',' && bracketDepth == 0 && parenDepth == 0)) {
                    std::string paramStr = paramsStr.substr(start, i - start);
                    // Trim whitespace
                    while (!paramStr.empty() && (paramStr[0] == ' ' || paramStr[0] == '\t')) paramStr = paramStr.substr(1);
                    while (!paramStr.empty() && (paramStr.back() == ' ' || paramStr.back() == '\t')) paramStr.pop_back();
                    
                    if (!paramStr.empty() && paramStr != "...") {
                        TypePtr paramType = fromString(paramStr);
                        fnType->params.push_back({"", paramType});
                    }
                    if (paramStr == "...") {
                        fnType->isVariadic = true;
                    }
                    start = i + 1;
                } else if (paramsStr[i] == '[') {
                    bracketDepth++;
                } else if (paramsStr[i] == ']') {
                    bracketDepth--;
                } else if (paramsStr[i] == '(') {
                    parenDepth++;
                } else if (paramsStr[i] == ')') {
                    parenDepth--;
                }
            }
        }
        
        // Check for return type: -> T
        size_t arrowPos = str.find("->", parenEnd);
        if (arrowPos != std::string::npos) {
            std::string retStr = str.substr(arrowPos + 2);
            // Trim whitespace
            while (!retStr.empty() && (retStr[0] == ' ' || retStr[0] == '\t')) retStr = retStr.substr(1);
            while (!retStr.empty() && (retStr.back() == ' ' || retStr.back() == '\t')) retStr.pop_back();
            fnType->returnType = fromString(retStr);
        } else {
            fnType->returnType = void_;
        }
        
        return fnType;
    }
    
    // Handle nullable types: T?
    if (str.size() > 1 && str.back() == '?') {
        std::string baseStr = str.substr(0, str.size() - 1);
        TypePtr base = fromString(baseStr);
        auto result = base->clone();
        result->isNullable = true;
        return result;
    }
    
    // Look up named type
    auto it = namedTypes_.find(str);
    return it != namedTypes_.end() ? it->second : unknown_;
}
void TypeRegistry::registerType(const std::string& name, TypePtr type) { namedTypes_[name] = std::move(type); }
TypePtr TypeRegistry::lookupType(const std::string& name) { auto it = namedTypes_.find(name); return it != namedTypes_.end() ? it->second : nullptr; }

// Generic and trait support
TypePtr TypeRegistry::typeParamType(const std::string& name) {
    return std::make_shared<TypeParamType>(name);
}

TypePtr TypeRegistry::genericType(const std::string& baseName, const std::vector<TypePtr>& typeArgs) {
    auto g = std::make_shared<GenericType>(baseName);
    g->typeArgs = typeArgs;
    return g;
}

TraitPtr TypeRegistry::traitType(const std::string& name) {
    return std::make_shared<TraitType>(name);
}

TypePtr TypeRegistry::traitObjectType(const std::string& traitName) {
    TraitPtr trait = lookupTrait(traitName);
    return std::make_shared<TraitObjectType>(traitName, trait);
}

TypePtr TypeRegistry::fixedArrayType(TypePtr element, size_t size) {
    return std::make_shared<FixedArrayType>(std::move(element), size);
}

TypePtr TypeRegistry::channelType(TypePtr element, size_t bufferSize) {
    return std::make_shared<ChannelType>(std::move(element), bufferSize);
}

TypePtr TypeRegistry::mutexType(TypePtr element) {
    return std::make_shared<MutexType>(std::move(element));
}

TypePtr TypeRegistry::rwlockType(TypePtr element) {
    return std::make_shared<RWLockType>(std::move(element));
}

TypePtr TypeRegistry::condType() {
    return std::make_shared<CondType>();
}

TypePtr TypeRegistry::semaphoreType() {
    return std::make_shared<SemaphoreType>();
}

void TypeRegistry::registerTrait(const std::string& name, TraitPtr trait) {
    traits_[name] = std::move(trait);
}

TraitPtr TypeRegistry::lookupTrait(const std::string& name) {
    auto it = traits_.find(name);
    return it != traits_.end() ? it->second : nullptr;
}

void TypeRegistry::registerTraitImpl(const TraitImpl& impl) {
    traitImpls_.push_back(impl);
}

const TraitImpl* TypeRegistry::lookupTraitImpl(const std::string& traitName, const std::string& typeName) {
    for (const auto& impl : traitImpls_) {
        if (impl.traitName == traitName && impl.typeName == typeName) {
            return &impl;
        }
    }
    return nullptr;
}

bool TypeRegistry::typeImplementsTrait(TypePtr type, const std::string& traitName) {
    if (!type) return false;
    
    // Type parameters satisfy their bounds
    if (auto* tp = dynamic_cast<TypeParamType*>(type.get())) {
        return tp->satisfiesBound(traitName);
    }
    
    // Check for explicit implementation
    std::string typeName = type->toString();
    return lookupTraitImpl(traitName, typeName) != nullptr;
}

std::vector<const TraitImpl*> TypeRegistry::getTraitImpls(const std::string& typeName) {
    std::vector<const TraitImpl*> result;
    for (const auto& impl : traitImpls_) {
        if (impl.typeName == typeName) {
            result.push_back(&impl);
        }
    }
    return result;
}

TypePtr TypeRegistry::instantiateGeneric(TypePtr genericType, const std::vector<TypePtr>& typeArgs) {
    if (!genericType) return nullptr;
    
    // Handle generic record types
    if (auto* rec = dynamic_cast<RecordType*>(genericType.get())) {
        auto newRec = std::make_shared<RecordType>(rec->name);
        // Substitute type parameters in fields
        for (const auto& field : rec->fields) {
            TypePtr fieldType = field.type;
            // If field type is a type parameter, substitute it
            if (auto* tp = dynamic_cast<TypeParamType*>(fieldType.get())) {
                // Find the index of this type parameter
                // For now, simple name-based lookup
                for (size_t i = 0; i < typeArgs.size(); i++) {
                    // Assume type params are named T, U, V, etc. or indexed
                    if (i < typeArgs.size()) {
                        fieldType = typeArgs[i];
                        break;
                    }
                }
            }
            newRec->fields.push_back({field.name, fieldType, field.hasDefault});
        }
        return newRec;
    }
    
    // Handle generic function types
    if (auto* fn = dynamic_cast<FunctionType*>(genericType.get())) {
        if (fn->typeParams.empty() || typeArgs.size() != fn->typeParams.size()) {
            return genericType;
        }
        
        std::unordered_map<std::string, TypePtr> substitutions;
        for (size_t i = 0; i < fn->typeParams.size(); i++) {
            substitutions[fn->typeParams[i]] = typeArgs[i];
        }
        
        auto newFn = std::make_shared<FunctionType>();
        for (const auto& param : fn->params) {
            newFn->params.push_back({param.first, substituteTypeParams(param.second, substitutions)});
        }
        newFn->returnType = substituteTypeParams(fn->returnType, substitutions);
        newFn->isVariadic = fn->isVariadic;
        // Don't copy typeParams - this is now a concrete instantiation
        return newFn;
    }
    
    return genericType;
}

TypePtr TypeRegistry::substituteTypeParams(TypePtr type, const std::unordered_map<std::string, TypePtr>& substitutions) {
    if (!type) return nullptr;
    
    // Substitute type parameters
    if (auto* tp = dynamic_cast<TypeParamType*>(type.get())) {
        auto it = substitutions.find(tp->name);
        if (it != substitutions.end()) {
            return it->second;
        }
        return type;
    }
    
    // Recursively substitute in compound types
    if (auto* list = dynamic_cast<ListType*>(type.get())) {
        return listType(substituteTypeParams(list->element, substitutions));
    }
    
    if (auto* map = dynamic_cast<MapType*>(type.get())) {
        return mapType(
            substituteTypeParams(map->key, substitutions),
            substituteTypeParams(map->value, substitutions)
        );
    }
    
    if (auto* ptr = dynamic_cast<PtrType*>(type.get())) {
        return ptrType(substituteTypeParams(ptr->pointee, substitutions), ptr->isRaw);
    }
    
    if (auto* fn = dynamic_cast<FunctionType*>(type.get())) {
        auto newFn = std::make_shared<FunctionType>();
        for (const auto& param : fn->params) {
            newFn->params.push_back({param.first, substituteTypeParams(param.second, substitutions)});
        }
        newFn->returnType = substituteTypeParams(fn->returnType, substitutions);
        newFn->isVariadic = fn->isVariadic;
        return newFn;
    }
    
    if (auto* gen = dynamic_cast<GenericType*>(type.get())) {
        auto newGen = std::make_shared<GenericType>(gen->baseName);
        for (const auto& arg : gen->typeArgs) {
            newGen->typeArgs.push_back(substituteTypeParams(arg, substitutions));
        }
        return newGen;
    }
    
    return type;
}

bool TypeRegistry::checkTraitBounds(TypePtr type, const std::vector<std::string>& bounds) {
    for (const auto& bound : bounds) {
        if (!typeImplementsTrait(type, bound)) {
            return false;
        }
    }
    return true;
}

} // namespace flex
