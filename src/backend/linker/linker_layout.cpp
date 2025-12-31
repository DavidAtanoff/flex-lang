// Flex Compiler - Linker Section Layout

#include "linker_base.h"
#include <iostream>

namespace flex {

bool Linker::layoutSections() {
    if (config_.verbose) {
        std::cout << "Phase 3: Laying out sections...\n";
    }
    
    objectLayouts_.resize(objects_.size());
    
    for (size_t i = 0; i < objects_.size(); i++) {
        auto& obj = objects_[i];
        auto& layout = objectLayouts_[i];
        
        layout.codeOffset = (uint32_t)mergedCode_.size();
        layout.dataOffset = (uint32_t)mergedData_.size();
        layout.rodataOffset = (uint32_t)mergedRodata_.size();
        
        mergedCode_.insert(mergedCode_.end(), obj.codeSection.begin(), obj.codeSection.end());
        mergedData_.insert(mergedData_.end(), obj.dataSection.begin(), obj.dataSection.end());
        mergedRodata_.insert(mergedRodata_.end(), obj.rodataSection.begin(), obj.rodataSection.end());
        
        while (mergedCode_.size() % 16 != 0) mergedCode_.push_back(0xCC);
        while (mergedData_.size() % 16 != 0) mergedData_.push_back(0);
        while (mergedRodata_.size() % 16 != 0) mergedRodata_.push_back(0);
    }
    
    codeRVA_ = 0x1000;
    dataRVA_ = alignUp(codeRVA_ + (uint32_t)mergedCode_.size(), config_.sectionAlignment);
    rodataRVA_ = alignUp(dataRVA_ + (uint32_t)mergedData_.size(), config_.sectionAlignment);
    idataRVA_ = alignUp(rodataRVA_ + (uint32_t)mergedRodata_.size(), config_.sectionAlignment);
    
    for (size_t i = 0; i < objects_.size(); i++) {
        auto& obj = objects_[i];
        auto& layout = objectLayouts_[i];
        
        for (auto& sym : obj.symbols) {
            if (sym.type == ObjSymbolType::UNDEFINED) continue;
            
            uint32_t baseRVA = 0;
            uint32_t baseOffset = 0;
            
            switch (sym.section) {
                case 0:
                    baseRVA = codeRVA_;
                    baseOffset = layout.codeOffset;
                    break;
                case 1:
                    baseRVA = dataRVA_;
                    baseOffset = layout.dataOffset;
                    break;
                case 2:
                    baseRVA = rodataRVA_;
                    baseOffset = layout.rodataOffset;
                    break;
            }
            
            uint32_t finalRVA = baseRVA + baseOffset + sym.offset;
            
            auto it = globalSymbols_.find(sym.name);
            if (it != globalSymbols_.end()) {
                it->second.rva = finalRVA;
            }
        }
    }
    
    if (config_.verbose) {
        std::cout << "  .text:  RVA=0x" << std::hex << codeRVA_ << " size=" << std::dec << mergedCode_.size() << "\n";
        std::cout << "  .data:  RVA=0x" << std::hex << dataRVA_ << " size=" << std::dec << mergedData_.size() << "\n";
        std::cout << "  .rdata: RVA=0x" << std::hex << rodataRVA_ << " size=" << std::dec << mergedRodata_.size() << "\n";
        std::cout << "  .idata: RVA=0x" << std::hex << idataRVA_ << "\n";
    }
    
    return true;
}

bool Linker::applyRelocations() {
    if (config_.verbose) {
        std::cout << "Phase 4: Applying relocations...\n";
    }
    
    PEGenerator pe;
    for (auto& [dll, funcs] : collectedImports_) {
        for (auto& func : funcs) {
            pe.addImport(dll, func);
        }
    }
    pe.finalizeImports();
    
    for (auto& [dll, funcs] : collectedImports_) {
        for (auto& func : funcs) {
            uint32_t peRVA = pe.getImportRVA(func);
            uint32_t adjustedRVA = peRVA - PEGenerator::IDATA_RVA + idataRVA_;
            importSymbols_[func] = adjustedRVA;
        }
    }
    
    for (size_t i = 0; i < objects_.size(); i++) {
        auto& obj = objects_[i];
        auto& layout = objectLayouts_[i];
        
        for (auto& rel : obj.codeRelocations) {
            uint32_t patchOffset = layout.codeOffset + rel.offset;
            uint32_t targetRVA = 0;
            
            auto globalIt = globalSymbols_.find(rel.symbol);
            if (globalIt != globalSymbols_.end()) {
                targetRVA = globalIt->second.rva;
            } else {
                auto importIt = importSymbols_.find(rel.symbol);
                if (importIt != importSymbols_.end()) {
                    targetRVA = importIt->second;
                } else {
                    auto* localSym = obj.findSymbol(rel.symbol);
                    if (localSym) {
                        uint32_t baseRVA = (localSym->section == 0) ? codeRVA_ : 
                                          (localSym->section == 1) ? dataRVA_ : rodataRVA_;
                        uint32_t baseOff = (localSym->section == 0) ? layout.codeOffset :
                                          (localSym->section == 1) ? layout.dataOffset : layout.rodataOffset;
                        targetRVA = baseRVA + baseOff + localSym->offset;
                    } else {
                        error("Cannot resolve symbol: " + rel.symbol);
                        return false;
                    }
                }
            }
            
            switch (rel.type) {
                case RelocType::REL32: {
                    uint32_t instrRVA = codeRVA_ + patchOffset + 4;
                    int32_t relValue = (int32_t)(targetRVA - instrRVA) + rel.addend;
                    memcpy(&mergedCode_[patchOffset], &relValue, 4);
                    break;
                }
                case RelocType::RIP32: {
                    uint32_t ripRVA = codeRVA_ + patchOffset + 4;
                    int32_t relValue = (int32_t)(targetRVA - ripRVA) + rel.addend;
                    memcpy(&mergedCode_[patchOffset], &relValue, 4);
                    break;
                }
                case RelocType::ABS64: {
                    uint64_t absValue = config_.imageBase + targetRVA + rel.addend;
                    memcpy(&mergedCode_[patchOffset], &absValue, 8);
                    break;
                }
                case RelocType::ABS32: {
                    uint32_t absValue = targetRVA + rel.addend;
                    memcpy(&mergedCode_[patchOffset], &absValue, 4);
                    break;
                }
            }
        }
    }
    
    return true;
}

} // namespace flex
