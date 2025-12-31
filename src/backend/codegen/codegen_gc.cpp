// Flex Compiler - Native Code Generator GC Support
// Garbage collection integration for native code generation

#include "codegen_base.h"

namespace flex {

// Emit GC initialization at program start
// This should be called before any GC allocations
void NativeCodeGen::emitGCInit() {
    if (gcInitEmitted_ || !useGC_) return;
    
    // Call flex_gc_init()
    // The GC runtime is linked into the executable
    // For now, we'll use a simple approach: call our runtime init function
    
    // Save registers that might be in use
    asm_.push_rbx();
    
    // Allocate shadow space for Windows x64 calling convention
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
    
    // Call GetProcessHeap to initialize heap handle
    // This is used by our GC for allocations
    asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
    
    // Store heap handle in a known location (we'll use a global)
    // For now, just ensure heap is available
    
    if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
    
    // Restore registers
    asm_.pop_rbx();
    
    gcInitEmitted_ = true;
}

// Emit GC shutdown at program end
void NativeCodeGen::emitGCShutdown() {
    if (!useGC_) return;
    
    // For now, we rely on process exit to clean up
    // A full implementation would walk the allocation list and free
    // But since we're exiting, the OS will reclaim all memory anyway
}

// Emit GC allocation call
// size: number of bytes to allocate
// type: object type for tracing
// Result: pointer to allocated memory in RAX
void NativeCodeGen::emitGCAlloc(size_t size, GCObjectType type) {
    if (!useGC_) {
        // Fallback to HeapAlloc
        if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
        
        asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
        asm_.mov_rcx_rax();
        asm_.xor_rax_rax();
        asm_.mov_rdx_rax();  // HEAP_ZERO_MEMORY = 0x08, but 0 works for basic alloc
        asm_.mov_r8d_imm32(static_cast<int32_t>(size));
        asm_.call_mem_rip(pe_.getImportRVA("HeapAlloc"));
        
        if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
        return;
    }
    
    // GC allocation:
    // We allocate header + user data, return pointer to user data
    // Header layout: [size:4][type:2][marked:1][flags:1][next:8] = 16 bytes
    
    size_t totalSize = 16 + size;  // Header + user data
    totalSize = (totalSize + 7) & ~7;  // Align to 8 bytes
    
    if (!stackAllocated_) asm_.sub_rsp_imm32(0x28);
    
    // Get process heap
    asm_.call_mem_rip(pe_.getImportRVA("GetProcessHeap"));
    asm_.mov_rcx_rax();
    
    // HeapAlloc(heap, HEAP_ZERO_MEMORY, size)
    asm_.mov_rdx_imm64(0x08);  // HEAP_ZERO_MEMORY
    asm_.mov_r8d_imm32(static_cast<int32_t>(totalSize));
    asm_.call_mem_rip(pe_.getImportRVA("HeapAlloc"));
    
    if (!stackAllocated_) asm_.add_rsp_imm32(0x28);
    
    // RAX now points to header
    // Initialize header fields
    // [rax+0] = size (4 bytes)
    asm_.push_rax();  // Save header pointer
    
    // mov dword [rax], size
    asm_.code.push_back(0xC7);
    asm_.code.push_back(0x00);
    asm_.code.push_back(size & 0xFF);
    asm_.code.push_back((size >> 8) & 0xFF);
    asm_.code.push_back((size >> 16) & 0xFF);
    asm_.code.push_back((size >> 24) & 0xFF);
    
    // [rax+4] = type (2 bytes)
    // mov word [rax+4], type
    asm_.code.push_back(0x66);
    asm_.code.push_back(0xC7);
    asm_.code.push_back(0x40);
    asm_.code.push_back(0x04);
    asm_.code.push_back(static_cast<uint16_t>(type) & 0xFF);
    asm_.code.push_back((static_cast<uint16_t>(type) >> 8) & 0xFF);
    
    // [rax+6] = marked (1 byte) = 0 (already zeroed)
    // [rax+7] = flags (1 byte) = 0 (already zeroed)
    // [rax+8] = next (8 bytes) = 0 (already zeroed, will be set by GC)
    
    // Return pointer to user data (header + 16)
    asm_.pop_rax();
    asm_.add_rax_imm32(16);
}

// Emit list allocation via GC
// capacity: initial capacity (number of elements)
// Result: pointer to list data in RAX
// List layout: [count:8][capacity:8][elements:capacity*8]
void NativeCodeGen::emitGCAllocList(size_t capacity) {
    size_t size = 16 + capacity * 8;  // count + capacity + elements
    emitGCAlloc(size, GCObjectType::LIST);
    
    // Initialize list header
    // RAX points to user data
    asm_.push_rax();
    
    // [rax+0] = count = 0
    asm_.code.push_back(0x48);
    asm_.code.push_back(0xC7);
    asm_.code.push_back(0x00);
    asm_.code.push_back(0x00);
    asm_.code.push_back(0x00);
    asm_.code.push_back(0x00);
    asm_.code.push_back(0x00);
    
    // [rax+8] = capacity
    asm_.code.push_back(0x48);
    asm_.code.push_back(0xC7);
    asm_.code.push_back(0x40);
    asm_.code.push_back(0x08);
    asm_.code.push_back(capacity & 0xFF);
    asm_.code.push_back((capacity >> 8) & 0xFF);
    asm_.code.push_back((capacity >> 16) & 0xFF);
    asm_.code.push_back((capacity >> 24) & 0xFF);
    
    asm_.pop_rax();
}

// Emit record allocation via GC
// fieldCount: number of fields
// Result: pointer to record data in RAX
// Record layout: [fieldCount:8][fields:fieldCount*8]
void NativeCodeGen::emitGCAllocRecord(size_t fieldCount) {
    size_t size = 8 + fieldCount * 8;  // fieldCount + fields
    emitGCAlloc(size, GCObjectType::RECORD);
    
    // Initialize record header
    asm_.push_rax();
    
    // [rax+0] = fieldCount
    asm_.code.push_back(0x48);
    asm_.code.push_back(0xC7);
    asm_.code.push_back(0x00);
    asm_.code.push_back(fieldCount & 0xFF);
    asm_.code.push_back((fieldCount >> 8) & 0xFF);
    asm_.code.push_back((fieldCount >> 16) & 0xFF);
    asm_.code.push_back((fieldCount >> 24) & 0xFF);
    
    asm_.pop_rax();
}

// Emit closure allocation via GC
// captureCount: number of captured variables
// Result: pointer to closure data in RAX
// Closure layout: [fnPtr:8][captureCount:8][captures:captureCount*8]
void NativeCodeGen::emitGCAllocClosure(size_t captureCount) {
    size_t size = 16 + captureCount * 8;  // fnPtr + captureCount + captures
    emitGCAlloc(size, GCObjectType::CLOSURE);
    
    // Initialize closure header
    asm_.push_rax();
    
    // [rax+0] = fnPtr = 0 (will be set later)
    // [rax+8] = captureCount
    asm_.code.push_back(0x48);
    asm_.code.push_back(0xC7);
    asm_.code.push_back(0x40);
    asm_.code.push_back(0x08);
    asm_.code.push_back(captureCount & 0xFF);
    asm_.code.push_back((captureCount >> 8) & 0xFF);
    asm_.code.push_back((captureCount >> 16) & 0xFF);
    asm_.code.push_back((captureCount >> 24) & 0xFF);
    
    asm_.pop_rax();
}

// Emit stack frame push for GC (conservative stack scanning)
void NativeCodeGen::emitGCPushFrame() {
    if (!useGC_) return;
    
    // For conservative stack scanning, we'd push the current RBP
    // This allows the GC to scan the stack for potential pointers
    // For now, this is a placeholder - full implementation would
    // maintain a list of stack frames
}

// Emit stack frame pop for GC
void NativeCodeGen::emitGCPopFrame() {
    if (!useGC_) return;
    
    // Pop the stack frame from GC's frame list
    // Placeholder for now
}

} // namespace flex
