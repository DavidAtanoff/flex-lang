// Flex Compiler - Type System
#ifndef FLEX_TYPES_H
#define FLEX_TYPES_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <optional>

namespace flex {

struct Type;
struct TraitType;
using TypePtr = std::shared_ptr<Type>;
using TraitPtr = std::shared_ptr<TraitType>;

enum class TypeKind {
    VOID, BOOL, INT, INT8, INT16, INT32, INT64, UINT8, UINT16, UINT32, UINT64,
    FLOAT, FLOAT32, FLOAT64, STRING, LIST, MAP, RECORD, FUNCTION, PTR, REF,
    ANY, NEVER, UNKNOWN, ERROR,
    // New kinds for generics and traits
    TYPE_PARAM,     // Generic type parameter (e.g., T in fn swap[T])
    GENERIC,        // Generic type instantiation (e.g., List[int])
    TRAIT,          // Trait type
    TRAIT_OBJECT,   // Dynamic trait object (dyn Trait)
    FIXED_ARRAY,    // Fixed-size array (e.g., [int; 10])
    CHANNEL,        // Channel type for inter-thread communication (e.g., chan[int])
    MUTEX,          // Mutex type for mutual exclusion (e.g., Mutex[int])
    RWLOCK,         // Reader-writer lock type (e.g., RWLock[int])
    COND,           // Condition variable type
    SEMAPHORE       // Counting semaphore type
};

struct Type {
    TypeKind kind;
    bool isMutable = true;
    bool isNullable = false;
    Type(TypeKind k) : kind(k) {}
    virtual ~Type() = default;
    virtual std::string toString() const;
    virtual bool equals(const Type* other) const;
    virtual TypePtr clone() const;
    bool isNumeric() const;
    bool isInteger() const;
    bool isFloat() const;
    bool isPrimitive() const;
    bool isReference() const;
    bool isPointer() const;
    virtual size_t size() const;
    virtual size_t alignment() const;
};

struct PrimitiveType : Type {
    PrimitiveType(TypeKind k) : Type(k) {}
    std::string toString() const override;
    size_t size() const override;
};

struct PtrType : Type {
    TypePtr pointee;
    bool isRaw;
    PtrType(TypePtr p, bool raw = false) : Type(raw ? TypeKind::PTR : TypeKind::REF), pointee(std::move(p)), isRaw(raw) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

struct ListType : Type {
    TypePtr element;
    ListType(TypePtr elem) : Type(TypeKind::LIST), element(std::move(elem)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

struct MapType : Type {
    TypePtr key;
    TypePtr value;
    MapType(TypePtr k, TypePtr v) : Type(TypeKind::MAP), key(std::move(k)), value(std::move(v)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

struct RecordField { std::string name; TypePtr type; bool hasDefault = false; };

struct RecordType : Type {
    std::string name;
    std::vector<RecordField> fields;
    RecordType(std::string n = "") : Type(TypeKind::RECORD), name(std::move(n)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
    TypePtr getField(const std::string& name) const;
};

struct FunctionType : Type {
    std::vector<std::pair<std::string, TypePtr>> params;
    TypePtr returnType;
    bool isVariadic = false;
    std::vector<std::string> typeParams;  // Generic type parameters
    FunctionType() : Type(TypeKind::FUNCTION) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Type parameter (e.g., T in fn swap[T])
struct TypeParamType : Type {
    std::string name;
    std::vector<std::string> bounds;  // Trait bounds (e.g., T: Printable + Comparable)
    TypePtr defaultType;              // Optional default type
    TypeParamType(std::string n) : Type(TypeKind::TYPE_PARAM), name(std::move(n)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
    bool satisfiesBound(const std::string& traitName) const;
};

// Trait method signature
struct TraitMethod {
    std::string name;
    std::shared_ptr<FunctionType> signature;
    bool hasDefaultImpl = false;
};

// Trait type definition
struct TraitType : Type {
    std::string name;
    std::vector<std::string> typeParams;           // Generic params for the trait itself
    std::vector<TraitMethod> methods;              // Required methods
    std::vector<std::string> superTraits;          // Inherited traits
    TraitType(std::string n) : Type(TypeKind::TRAIT), name(std::move(n)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
    const TraitMethod* getMethod(const std::string& methodName) const;
};

// Trait object type (dyn Trait)
struct TraitObjectType : Type {
    std::string traitName;
    TraitPtr trait;
    TraitObjectType(std::string n, TraitPtr t = nullptr) 
        : Type(TypeKind::TRAIT_OBJECT), traitName(std::move(n)), trait(std::move(t)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Generic type instantiation (e.g., Pair[int, str])
struct GenericType : Type {
    std::string baseName;                          // Base type name (e.g., "Pair")
    std::vector<TypePtr> typeArgs;                 // Type arguments
    TypePtr resolvedType;                          // Resolved concrete type after substitution
    GenericType(std::string base) : Type(TypeKind::GENERIC), baseName(std::move(base)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Fixed-size array type (e.g., [int; 10], [[int; 3]; 4])
struct FixedArrayType : Type {
    TypePtr element;                               // Element type (can be another FixedArrayType for multi-dim)
    size_t size;                                   // Number of elements
    FixedArrayType(TypePtr elem, size_t sz) : Type(TypeKind::FIXED_ARRAY), element(std::move(elem)), size(sz) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
    size_t totalSize() const;                      // Total size in bytes (element size * count)
    size_t elementSize() const;                    // Size of one element in bytes
    size_t dimensions() const;                     // Number of dimensions (1 for [T;N], 2 for [[T;N];M], etc.)
    std::vector<size_t> shape() const;             // Shape of the array (e.g., {4, 3} for [[int;3];4])
};

// Channel type for inter-thread communication (e.g., chan[int], chan[int, 10])
struct ChannelType : Type {
    TypePtr element;                               // Element type being sent/received
    size_t bufferSize;                             // Buffer capacity (0 = unbuffered/synchronous)
    ChannelType(TypePtr elem, size_t buf = 0) : Type(TypeKind::CHANNEL), element(std::move(elem)), bufferSize(buf) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Mutex type for mutual exclusion (e.g., Mutex[int])
struct MutexType : Type {
    TypePtr element;                               // Protected data type
    MutexType(TypePtr elem) : Type(TypeKind::MUTEX), element(std::move(elem)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Reader-writer lock type (e.g., RWLock[int])
struct RWLockType : Type {
    TypePtr element;                               // Protected data type
    RWLockType(TypePtr elem) : Type(TypeKind::RWLOCK), element(std::move(elem)) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Condition variable type
struct CondType : Type {
    CondType() : Type(TypeKind::COND) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Counting semaphore type
struct SemaphoreType : Type {
    SemaphoreType() : Type(TypeKind::SEMAPHORE) {}
    std::string toString() const override;
    bool equals(const Type* other) const override;
    TypePtr clone() const override;
};

// Trait implementation record
struct TraitImpl {
    std::string traitName;
    std::string typeName;
    std::vector<TypePtr> typeArgs;                 // Type arguments for generic impls
    std::unordered_map<std::string, std::shared_ptr<FunctionType>> methods;
};

class TypeRegistry {
public:
    static TypeRegistry& instance();
    TypePtr voidType(); TypePtr boolType(); TypePtr intType();
    TypePtr int8Type(); TypePtr int16Type(); TypePtr int32Type(); TypePtr int64Type();
    TypePtr uint8Type(); TypePtr uint16Type(); TypePtr uint32Type(); TypePtr uint64Type();
    TypePtr floatType(); TypePtr float32Type(); TypePtr float64Type();
    TypePtr stringType(); TypePtr anyType(); TypePtr neverType(); TypePtr unknownType(); TypePtr errorType();
    TypePtr ptrType(TypePtr pointee, bool raw = false);
    TypePtr refType(TypePtr pointee);
    TypePtr listType(TypePtr element);
    TypePtr mapType(TypePtr key, TypePtr value);
    TypePtr recordType(const std::string& name);
    TypePtr functionType();
    TypePtr fromString(const std::string& str);
    void registerType(const std::string& name, TypePtr type);
    TypePtr lookupType(const std::string& name);
    
    // Generic and trait support
    TypePtr typeParamType(const std::string& name);
    TypePtr genericType(const std::string& baseName, const std::vector<TypePtr>& typeArgs);
    TraitPtr traitType(const std::string& name);
    TypePtr traitObjectType(const std::string& traitName);
    TypePtr fixedArrayType(TypePtr element, size_t size);  // Fixed-size array type
    TypePtr channelType(TypePtr element, size_t bufferSize = 0);  // Channel type for inter-thread communication
    TypePtr mutexType(TypePtr element);  // Mutex type for mutual exclusion
    TypePtr rwlockType(TypePtr element);  // Reader-writer lock type
    TypePtr condType();  // Condition variable type
    TypePtr semaphoreType();  // Counting semaphore type
    
    // Trait registration and lookup
    void registerTrait(const std::string& name, TraitPtr trait);
    TraitPtr lookupTrait(const std::string& name);
    
    // Trait implementation management
    void registerTraitImpl(const TraitImpl& impl);
    const TraitImpl* lookupTraitImpl(const std::string& traitName, const std::string& typeName);
    bool typeImplementsTrait(TypePtr type, const std::string& traitName);
    std::vector<const TraitImpl*> getTraitImpls(const std::string& typeName);
    
    // Generic type instantiation
    TypePtr instantiateGeneric(TypePtr genericType, const std::vector<TypePtr>& typeArgs);
    TypePtr substituteTypeParams(TypePtr type, const std::unordered_map<std::string, TypePtr>& substitutions);
    
    // Type constraint checking
    bool checkTraitBounds(TypePtr type, const std::vector<std::string>& bounds);
    
private:
    TypeRegistry();
    std::unordered_map<std::string, TypePtr> namedTypes_;
    std::unordered_map<std::string, TraitPtr> traits_;
    std::vector<TraitImpl> traitImpls_;
    TypePtr void_, bool_, int_, int8_, int16_, int32_, int64_;
    TypePtr uint8_, uint16_, uint32_, uint64_;
    TypePtr float_, float32_, float64_, string_;
    TypePtr any_, never_, unknown_, error_;
};

} // namespace flex

#endif // FLEX_TYPES_H
