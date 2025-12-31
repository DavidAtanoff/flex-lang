// Flex Compiler - Linker Core
// Main link entry point and helpers

#include "linker_base.h"
#include <iostream>
#include <fstream>

namespace flex {

Linker::Linker() {}

void Linker::error(const std::string& msg) {
    errors_.push_back(msg);
    if (config_.verbose) {
        std::cerr << "Linker error: " << msg << "\n";
    }
}

uint32_t Linker::alignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

bool Linker::addObjectFile(const std::string& filename) {
    ObjectFile obj;
    if (!obj.read(filename)) {
        error("Failed to read object file: " + filename);
        return false;
    }
    objects_.push_back(std::move(obj));
    return true;
}

bool Linker::addObjectFile(const ObjectFile& obj) {
    objects_.push_back(obj);
    return true;
}

bool Linker::addLibrary(const std::string& filename) {
    (void)filename;
    return true;
}

bool Linker::link() {
    if (objects_.empty()) {
        error("No input files");
        return false;
    }
    
    errors_.clear();
    globalSymbols_.clear();
    importSymbols_.clear();
    mergedCode_.clear();
    mergedData_.clear();
    mergedRodata_.clear();
    objectLayouts_.clear();
    collectedImports_.clear();
    
    if (config_.verbose) {
        std::cout << "Linking " << objects_.size() << " object file(s)...\n";
    }
    
    if (!collectSymbols()) return false;
    if (!resolveSymbols()) return false;
    if (!layoutSections()) return false;
    if (!applyRelocations()) return false;
    if (!generateExecutable()) return false;
    
    if (config_.verbose) {
        std::cout << "Successfully linked: " << config_.outputFile << "\n";
    }
    
    return true;
}

} // namespace flex
