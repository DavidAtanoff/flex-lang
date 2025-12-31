// Flex Compiler - Symbol Table
#ifndef FLEX_SYMBOL_TABLE_H
#define FLEX_SYMBOL_TABLE_H

#include "semantic/types/types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace flex {

enum class SymbolKind { VARIABLE, FUNCTION, PARAMETER, TYPE, RECORD_FIELD, MODULE, MACRO, LAYER };
enum class StorageClass { LOCAL, GLOBAL, HEAP, REGISTER };

struct Symbol {
    std::string name;
    SymbolKind kind;
    TypePtr type;
    StorageClass storage = StorageClass::LOCAL;
    bool isMutable = true;
    bool isExported = false;
    bool isInitialized = false;
    int32_t offset = 0;
    int paramCount = 0;
    bool isVariadic = false;
    std::string file;
    int line = 0;
    int column = 0;
    Symbol() = default;
    Symbol(std::string n, SymbolKind k, TypePtr t) : name(std::move(n)), kind(k), type(std::move(t)) {}
};

class Scope {
public:
    enum class Kind { GLOBAL, MODULE, FUNCTION, BLOCK, LOOP, UNSAFE };
    Scope(Kind k, Scope* parent = nullptr) : kind_(k), parent_(parent) {}
    bool define(const Symbol& sym);
    Symbol* lookup(const std::string& name);
    Symbol* lookupLocal(const std::string& name);
    Kind kind() const { return kind_; }
    Scope* parent() const { return parent_; }
    bool isGlobal() const { return kind_ == Kind::GLOBAL; }
    bool isFunction() const { return kind_ == Kind::FUNCTION; }
    bool isUnsafe() const;
    const std::unordered_map<std::string, Symbol>& symbols() const { return symbols_; }
    int32_t allocateLocal(size_t size);
    int32_t currentStackOffset() const { return stackOffset_; }
private:
    Kind kind_;
    Scope* parent_;
    std::unordered_map<std::string, Symbol> symbols_;
    int32_t stackOffset_ = 0;
};

class SymbolTable {
public:
    SymbolTable();
    void pushScope(Scope::Kind kind);
    void popScope();
    Scope* currentScope() { return current_; }
    Scope* globalScope() { return &global_; }
    bool define(const Symbol& sym);
    Symbol* lookup(const std::string& name);
    Symbol* lookupLocal(const std::string& name);
    void registerType(const std::string& name, TypePtr type);
    TypePtr lookupType(const std::string& name);
    bool inFunction() const;
    bool inLoop() const;
    bool inUnsafe() const;
    Scope* enclosingFunction();
private:
    Scope global_;
    Scope* current_;
    std::vector<std::unique_ptr<Scope>> scopes_;
};

} // namespace flex

#endif // FLEX_SYMBOL_TABLE_H
