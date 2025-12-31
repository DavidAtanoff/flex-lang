// Flex Compiler - Linker
#ifndef FLEX_LINKER_H
#define FLEX_LINKER_H

#include "backend/object/object_file.h"
#include "backend/x64/pe_generator.h"
#include <set>

namespace flex {

struct LinkedSymbol {
    std::string name;
    ObjSymbolType type;
    uint32_t rva;
    uint32_t size;
    std::string sourceModule;
    LinkedSymbol() : type(ObjSymbolType::UNDEFINED), rva(0), size(0) {}
};

struct LinkerConfig {
    uint64_t imageBase = 0x140000000ULL;
    uint32_t sectionAlignment = 0x1000;
    uint32_t fileAlignment = 0x200;
    std::string entryPoint = "_start";
    std::string outputFile = "a.exe";
    bool verbose = false;
    bool generateMap = false;
    std::string mapFile;
    std::vector<std::string> libraryPaths;
    std::vector<std::string> defaultLibs = {"kernel32.dll"};
};

class Linker {
public:
    Linker();
    void setConfig(const LinkerConfig& config) { config_ = config; }
    LinkerConfig& config() { return config_; }
    bool addObjectFile(const std::string& filename);
    bool addObjectFile(const ObjectFile& obj);
    bool addLibrary(const std::string& filename);
    bool link();
    const std::vector<std::string>& getErrors() const { return errors_; }
    
private:
    LinkerConfig config_;
    std::vector<ObjectFile> objects_;
    std::vector<std::string> errors_;
    std::map<std::string, LinkedSymbol> globalSymbols_;
    std::map<std::string, uint32_t> importSymbols_;
    std::vector<uint8_t> mergedCode_;
    std::vector<uint8_t> mergedData_;
    std::vector<uint8_t> mergedRodata_;
    uint32_t codeRVA_ = 0;
    uint32_t dataRVA_ = 0;
    uint32_t rodataRVA_ = 0;
    uint32_t idataRVA_ = 0;
    struct ObjectLayout { uint32_t codeOffset; uint32_t dataOffset; uint32_t rodataOffset; };
    std::vector<ObjectLayout> objectLayouts_;
    std::map<std::string, std::set<std::string>> collectedImports_;
    
    bool collectSymbols();
    bool resolveSymbols();
    bool layoutSections();
    bool applyRelocations();
    bool generateExecutable();
    void error(const std::string& msg);
    uint32_t alignUp(uint32_t value, uint32_t alignment);
    void buildImportSection(std::vector<uint8_t>& section, const std::map<std::string, std::set<std::string>>& imports, uint32_t baseRVA);
    void generateMapFile();
};

} // namespace flex

#endif // FLEX_LINKER_H
