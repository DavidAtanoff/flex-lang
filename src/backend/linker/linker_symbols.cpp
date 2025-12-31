// Flex Compiler - Linker Symbol Collection and Resolution

#include "linker_base.h"
#include <iostream>

namespace flex {

bool Linker::collectSymbols() {
    if (config_.verbose) {
        std::cout << "Phase 1: Collecting symbols...\n";
    }
    
    for (size_t i = 0; i < objects_.size(); i++) {
        auto& obj = objects_[i];
        
        for (auto& imp : obj.imports) {
            collectedImports_[imp.dll].insert(imp.function);
        }
        
        for (auto& sym : obj.symbols) {
            if (sym.type == ObjSymbolType::UNDEFINED) continue;
            if (!sym.isExported) continue;
            
            auto it = globalSymbols_.find(sym.name);
            if (it != globalSymbols_.end()) {
                error("Duplicate symbol: " + sym.name + " (in " + obj.moduleName + 
                      " and " + it->second.sourceModule + ")");
                return false;
            }
            
            LinkedSymbol linked;
            linked.name = sym.name;
            linked.type = sym.type;
            linked.size = sym.size;
            linked.sourceModule = obj.moduleName;
            linked.rva = 0;
            globalSymbols_[sym.name] = linked;
            
            if (config_.verbose) {
                std::cout << "  Symbol: " << sym.name << " from " << obj.moduleName << "\n";
            }
        }
    }
    
    return true;
}

bool Linker::resolveSymbols() {
    if (config_.verbose) {
        std::cout << "Phase 2: Resolving symbols...\n";
    }
    
    for (auto& obj : objects_) {
        for (auto& rel : obj.codeRelocations) {
            if (globalSymbols_.find(rel.symbol) != globalSymbols_.end()) continue;
            
            bool isImport = false;
            for (auto& [dll, funcs] : collectedImports_) {
                if (funcs.count(rel.symbol)) {
                    isImport = true;
                    break;
                }
            }
            if (isImport) continue;
            
            if (obj.findSymbol(rel.symbol)) continue;
            
            error("Undefined symbol: " + rel.symbol + " (referenced in " + obj.moduleName + ")");
            return false;
        }
    }
    
    if (globalSymbols_.find(config_.entryPoint) == globalSymbols_.end()) {
        error("Entry point not found: " + config_.entryPoint);
        return false;
    }
    
    return true;
}

} // namespace flex
