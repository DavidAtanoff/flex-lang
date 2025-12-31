// Flex Compiler - PE Executable Generator with Import Support
#ifndef FLEX_PE_GENERATOR_H
#define FLEX_PE_GENERATOR_H

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>
#include <map>

namespace flex {

// Fixup types for RIP-relative addressing
enum class FixupType {
    DATA,   // Reference to .data section
    IDATA   // Reference to .idata section (imports)
};

struct CodeFixup {
    size_t offset;      // Offset in code section where the 32-bit value is
    uint32_t targetRVA; // Original placeholder RVA
    FixupType type;     // What section this references
};

class PEGenerator {
public:
    void addCode(const std::vector<uint8_t>& code);
    void addCodeWithFixups(const std::vector<uint8_t>& code, 
                           const std::vector<std::pair<size_t, uint32_t>>& ripFixups);
    uint32_t addData(const void* data, size_t size);
    uint32_t addString(const std::string& str);
    uint32_t addQword(uint64_t value);
    void addImport(const std::string& dll, const std::string& function);
    void finalizeImports();
    uint32_t getImportRVA(const std::string& function);
    bool write(const std::string& filename);
    
    // Get the actual RVAs after code generation (for fixup resolution)
    uint32_t getActualDataRVA() const;
    uint32_t getActualIdataRVA() const;
    
    static const uint64_t IMAGE_BASE = 0x140000000ULL;
    static const uint32_t CODE_RVA = 0x1000;
    // These are now placeholder values - actual RVAs computed at write time
    static const uint32_t DATA_RVA_PLACEHOLDER = 0x100000;   // Large placeholder
    static const uint32_t IDATA_RVA_PLACEHOLDER = 0x200000;  // Large placeholder
    // Aliases for backward compatibility
    static const uint32_t DATA_RVA = DATA_RVA_PLACEHOLDER;
    static const uint32_t IDATA_RVA = IDATA_RVA_PLACEHOLDER;
    
private:
    std::vector<uint8_t> codeSection;
    std::vector<uint8_t> dataSection;
    std::vector<uint8_t> idataSection;
    std::vector<CodeFixup> codeFixups;  // Tracked fixups for precise relocation
    std::map<std::string, std::vector<std::string>> imports;
    std::map<std::string, uint32_t> importRVAs;  // Relative to IDATA_RVA_PLACEHOLDER
    bool importsFinalized = false;
    
    uint32_t actualDataRVA_ = 0;
    uint32_t actualIdataRVA_ = 0;
    
    void buildImportSection();
    void calculateActualRVAs();
    void applyFixups();
};

} // namespace flex

#endif // FLEX_PE_GENERATOR_H
