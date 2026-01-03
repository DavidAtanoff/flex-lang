// Flex Compiler - Native Code Generator Synchronization Expressions
// Handles: Mutex, RWLock, Cond, Semaphore

#include "backend/codegen/codegen_base.h"

namespace flex {

// Mutex structure layout (allocated on heap):
// Offset 0:  mutex handle (8 bytes) - Windows mutex handle
// Offset 8:  data pointer (8 bytes) - pointer to protected data
// Offset 16: element size (8 bytes) - size of protected data
// Total: 24 bytes for mutex header + element data

void NativeCodeGen::emitMutexCreate(int32_t elementSize) {
    // Allocate mutex structure (24 bytes) + data
    size_t totalSize = 24 + elementSize;
    
    // Allocate memory for mutex
    asm_.mov_rcx_imm64(totalSize);
    emitGCAllocRaw(totalSize);
    // RAX now contains pointer to mutex structure
    
    asm_.push_rax();  // Save mutex pointer
    
    // Create Windows mutex
    asm_.xor_rcx_rcx();  // lpMutexAttributes = NULL
    asm_.xor_rdx_rdx();  // bInitialOwner = FALSE
    asm_.xor_r8_r8();    // lpName = NULL
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("CreateMutexA"));
    asm_.add_rsp_imm32(0x28);
    
    // Store mutex handle at offset 0
    asm_.mov_rcx_rax();  // mutex handle
    asm_.mov_rax_mem_rsp(0);  // mutex pointer
    asm_.mov_mem_rax_rcx(0);  // store handle at offset 0
    
    // Set data pointer (offset 8) - points to offset 24
    asm_.mov_rax_mem_rsp(0);
    asm_.lea_rcx_rax_offset(24);
    asm_.mov_mem_rax_rcx(8);
    
    // Set element size (offset 16)
    asm_.mov_rax_mem_rsp(0);
    asm_.mov_rcx_imm64(elementSize);
    asm_.mov_mem_rax_rcx(16);
    
    // Return mutex pointer
    asm_.pop_rax();
}

void NativeCodeGen::emitMutexLock() {
    // Mutex pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Wait for mutex
    asm_.mov_rax_mem_rsp(8);  // mutex pointer
    asm_.mov_rcx_mem_rax(0);  // mutex handle
    asm_.mov_rdx_imm64(0xFFFFFFFF);  // INFINITE
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("WaitForSingleObject"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitMutexUnlock() {
    // Mutex pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Release mutex
    asm_.mov_rax_mem_rsp(8);  // mutex pointer
    asm_.mov_rcx_mem_rax(0);  // mutex handle
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("ReleaseMutex"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

// RWLock structure layout (allocated on heap):
// Offset 0:  SRWLOCK (8 bytes) - Windows SRW lock
// Offset 8:  data pointer (8 bytes) - pointer to protected data
// Offset 16: element size (8 bytes) - size of protected data
// Total: 24 bytes for rwlock header + element data

void NativeCodeGen::emitRWLockCreate(int32_t elementSize) {
    // Allocate rwlock structure (24 bytes) + data
    size_t totalSize = 24 + elementSize;
    
    // Allocate memory for rwlock
    asm_.mov_rcx_imm64(totalSize);
    emitGCAllocRaw(totalSize);
    // RAX now contains pointer to rwlock structure
    
    asm_.push_rax();  // Save rwlock pointer
    
    // Initialize SRW lock (SRWLOCK is initialized to 0, which is SRWLOCK_INIT)
    // The memory is already zeroed by GC allocation, but we call InitializeSRWLock for safety
    asm_.mov_rcx_rax();  // pointer to SRWLOCK
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("InitializeSRWLock"));
    asm_.add_rsp_imm32(0x28);
    
    // Set data pointer (offset 8) - points to offset 24
    asm_.mov_rax_mem_rsp(0);
    asm_.lea_rcx_rax_offset(24);
    asm_.mov_mem_rax_rcx(8);
    
    // Set element size (offset 16)
    asm_.mov_rax_mem_rsp(0);
    asm_.mov_rcx_imm64(elementSize);
    asm_.mov_mem_rax_rcx(16);
    
    // Return rwlock pointer
    asm_.pop_rax();
}

void NativeCodeGen::emitRWLockReadLock() {
    // RWLock pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Acquire shared lock
    asm_.mov_rax_mem_rsp(8);  // rwlock pointer (SRWLOCK is at offset 0)
    asm_.mov_rcx_rax();
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("AcquireSRWLockShared"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitRWLockWriteLock() {
    // RWLock pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Acquire exclusive lock
    asm_.mov_rax_mem_rsp(8);  // rwlock pointer (SRWLOCK is at offset 0)
    asm_.mov_rcx_rax();
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("AcquireSRWLockExclusive"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitRWLockUnlock() {
    // RWLock pointer in RAX
    // Note: We need to track whether we have a read or write lock
    // For simplicity, we'll release exclusive lock (caller must track lock type)
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Release exclusive lock
    asm_.mov_rax_mem_rsp(8);  // rwlock pointer
    asm_.mov_rcx_rax();
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("ReleaseSRWLockExclusive"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

// Condition variable structure layout (allocated on heap):
// Offset 0:  CONDITION_VARIABLE (8 bytes) - Windows condition variable
// Total: 8 bytes

void NativeCodeGen::emitCondCreate() {
    // Allocate condition variable structure (8 bytes)
    asm_.mov_rcx_imm64(8);
    emitGCAllocRaw(8);
    // RAX now contains pointer to condition variable
    
    asm_.push_rax();  // Save cond pointer
    
    // Initialize condition variable
    asm_.mov_rcx_rax();  // pointer to CONDITION_VARIABLE
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("InitializeConditionVariable"));
    asm_.add_rsp_imm32(0x28);
    
    // Return cond pointer
    asm_.pop_rax();
}

void NativeCodeGen::emitCondWait() {
    // Cond pointer in RAX, Mutex pointer in RCX
    asm_.push_rax();  // cond
    asm_.push_rcx();  // mutex
    
    // SleepConditionVariableSRW(ConditionVariable, SRWLock, dwMilliseconds, Flags)
    // We use the mutex's underlying handle - but Windows CV works with SRWLock
    // For compatibility, we'll use SleepConditionVariableSRW with the mutex treated as SRWLock
    asm_.mov_rax_mem_rsp(8);  // cond pointer
    asm_.mov_rcx_rax();
    asm_.mov_rax_mem_rsp(0);  // mutex pointer (use as SRWLock)
    asm_.mov_rdx_rax();
    asm_.mov_r8_imm64(0xFFFFFFFF);  // INFINITE
    asm_.xor_r9_r9();  // Flags = 0 (exclusive mode)
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("SleepConditionVariableSRW"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitCondSignal() {
    // Cond pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Wake one waiter
    asm_.mov_rax_mem_rsp(8);  // cond pointer
    asm_.mov_rcx_rax();
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("WakeConditionVariable"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitCondBroadcast() {
    // Cond pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Wake all waiters
    asm_.mov_rax_mem_rsp(8);  // cond pointer
    asm_.mov_rcx_rax();
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("WakeAllConditionVariable"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

// Semaphore structure layout (allocated on heap):
// Offset 0:  semaphore handle (8 bytes) - Windows semaphore handle
// Total: 8 bytes

void NativeCodeGen::emitSemaphoreCreate(int64_t initialCount, int64_t maxCount) {
    // Allocate semaphore structure (8 bytes)
    asm_.mov_rcx_imm64(8);
    emitGCAllocRaw(8);
    // RAX now contains pointer to semaphore structure
    
    asm_.push_rax();  // Save semaphore pointer
    
    // Create Windows semaphore
    asm_.xor_rcx_rcx();  // lpSemaphoreAttributes = NULL
    asm_.mov_rdx_imm64(initialCount);  // lInitialCount
    asm_.mov_r8_imm64(maxCount);  // lMaximumCount
    asm_.xor_r9_r9();  // lpName = NULL
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("CreateSemaphoreA"));
    asm_.add_rsp_imm32(0x28);
    
    // Store semaphore handle at offset 0
    asm_.mov_rcx_rax();  // semaphore handle
    asm_.mov_rax_mem_rsp(0);  // semaphore pointer
    asm_.mov_mem_rax_rcx(0);  // store handle at offset 0
    
    // Return semaphore pointer
    asm_.pop_rax();
}

void NativeCodeGen::emitSemaphoreAcquire() {
    // Semaphore pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Wait for semaphore
    asm_.mov_rax_mem_rsp(8);  // semaphore pointer
    asm_.mov_rcx_mem_rax(0);  // semaphore handle
    asm_.mov_rdx_imm64(0xFFFFFFFF);  // INFINITE
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("WaitForSingleObject"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitSemaphoreRelease() {
    // Semaphore pointer in RAX
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Release semaphore
    asm_.mov_rax_mem_rsp(8);  // semaphore pointer
    asm_.mov_rcx_mem_rax(0);  // semaphore handle
    asm_.mov_rdx_imm64(1);  // lReleaseCount = 1
    asm_.xor_r8_r8();  // lpPreviousCount = NULL
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("ReleaseSemaphore"));
    asm_.add_rsp_imm32(0x28);
    
    asm_.add_rsp_imm32(16);  // Clean up
}

void NativeCodeGen::emitSemaphoreTryAcquire() {
    // Semaphore pointer in RAX
    // Returns 1 if acquired, 0 if not
    asm_.push_rax();
    asm_.sub_rsp_imm32(8);  // Align stack
    
    // Try to acquire semaphore with 0 timeout
    asm_.mov_rax_mem_rsp(8);  // semaphore pointer
    asm_.mov_rcx_mem_rax(0);  // semaphore handle
    asm_.xor_rdx_rdx();  // dwMilliseconds = 0 (no wait)
    asm_.sub_rsp_imm32(0x28);
    asm_.call_mem_rip(pe_.getImportRVA("WaitForSingleObject"));
    asm_.add_rsp_imm32(0x28);
    
    // Check result: WAIT_OBJECT_0 (0) = success, WAIT_TIMEOUT (258) = failed
    asm_.test_rax_rax();  // Check if RAX == 0
    std::string successLabel = newLabel("sem_try_success");
    std::string doneLabel = newLabel("sem_try_done");
    asm_.jz_rel32(successLabel);
    
    // Failed - return 0
    asm_.xor_rax_rax();
    asm_.jmp_rel32(doneLabel);
    
    asm_.label(successLabel);
    // Success - return 1
    asm_.mov_rax_imm64(1);
    
    asm_.label(doneLabel);
    asm_.add_rsp_imm32(16);  // Clean up
}

// AST visitor implementations

void NativeCodeGen::visit(MakeMutexExpr& node) {
    int32_t elemSize = getTypeSize(node.elementType);
    if (elemSize == 0) elemSize = 8;  // Default to 8 bytes
    emitMutexCreate(elemSize);
}

void NativeCodeGen::visit(MakeRWLockExpr& node) {
    int32_t elemSize = getTypeSize(node.elementType);
    if (elemSize == 0) elemSize = 8;  // Default to 8 bytes
    emitRWLockCreate(elemSize);
}

void NativeCodeGen::visit(MakeCondExpr& node) {
    emitCondCreate();
}

void NativeCodeGen::visit(MakeSemaphoreExpr& node) {
    emitSemaphoreCreate(node.initialCount, node.maxCount);
}

void NativeCodeGen::visit(MutexLockExpr& node) {
    node.mutex->accept(*this);
    emitMutexLock();
}

void NativeCodeGen::visit(MutexUnlockExpr& node) {
    node.mutex->accept(*this);
    emitMutexUnlock();
}

void NativeCodeGen::visit(RWLockReadExpr& node) {
    node.rwlock->accept(*this);
    emitRWLockReadLock();
}

void NativeCodeGen::visit(RWLockWriteExpr& node) {
    node.rwlock->accept(*this);
    emitRWLockWriteLock();
}

void NativeCodeGen::visit(RWLockUnlockExpr& node) {
    node.rwlock->accept(*this);
    emitRWLockUnlock();
}

void NativeCodeGen::visit(CondWaitExpr& node) {
    // Evaluate mutex first
    node.mutex->accept(*this);
    asm_.push_rax();  // Save mutex
    
    // Evaluate cond
    node.cond->accept(*this);
    asm_.pop_rcx();  // Mutex in RCX
    
    emitCondWait();
}

void NativeCodeGen::visit(CondSignalExpr& node) {
    node.cond->accept(*this);
    emitCondSignal();
}

void NativeCodeGen::visit(CondBroadcastExpr& node) {
    node.cond->accept(*this);
    emitCondBroadcast();
}

void NativeCodeGen::visit(SemAcquireExpr& node) {
    node.sem->accept(*this);
    emitSemaphoreAcquire();
}

void NativeCodeGen::visit(SemReleaseExpr& node) {
    node.sem->accept(*this);
    emitSemaphoreRelease();
}

void NativeCodeGen::visit(SemTryAcquireExpr& node) {
    node.sem->accept(*this);
    emitSemaphoreTryAcquire();
}

void NativeCodeGen::visit(LockStmt& node) {
    // Evaluate mutex
    node.mutex->accept(*this);
    asm_.push_rax();  // Save mutex pointer
    
    // Lock the mutex
    emitMutexLock();
    
    // Execute the body
    node.body->accept(*this);
    
    // Unlock the mutex
    asm_.pop_rax();  // Restore mutex pointer
    emitMutexUnlock();
}

} // namespace flex
