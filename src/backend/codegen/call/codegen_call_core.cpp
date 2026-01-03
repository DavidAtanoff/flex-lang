// Flex Compiler - Native Code Generator Call Core
// Main CallExpr visitor - dispatches to modular builtin handlers

#include "backend/codegen/codegen_base.h"
#include "semantic/types/types.h"

namespace flex {

void NativeCodeGen::visit(CallExpr& node) {
    // Handle module function calls (e.g., math.add)
    if (auto* member = dynamic_cast<MemberExpr*>(node.callee.get())) {
        if (auto* moduleId = dynamic_cast<Identifier*>(member->object.get())) {
            std::string mangledName = moduleId->name + "." + member->member;
            
            if (asm_.labels.count(mangledName)) {
                for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                    node.args[i]->accept(*this);
                    asm_.push_rax();
                }
                
                if (node.args.size() >= 1) asm_.pop_rcx();
                if (node.args.size() >= 2) asm_.pop_rdx();
                if (node.args.size() >= 3) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x58);
                }
                if (node.args.size() >= 4) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x59);
                }
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                asm_.call_rel32(mangledName);
                if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                return;
            }
            
            // Check for trait method call (static dispatch)
            for (const auto& [implKey, info] : impls_) {
                if (info.typeName == moduleId->name) {
                    auto methodIt = info.methodLabels.find(member->member);
                    if (methodIt != info.methodLabels.end()) {
                        for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                            node.args[i]->accept(*this);
                            asm_.push_rax();
                        }
                        
                        if (node.args.size() >= 1) asm_.pop_rcx();
                        if (node.args.size() >= 2) asm_.pop_rdx();
                        if (node.args.size() >= 3) {
                            asm_.code.push_back(0x41); asm_.code.push_back(0x58);
                        }
                        if (node.args.size() >= 4) {
                            asm_.code.push_back(0x41); asm_.code.push_back(0x59);
                        }
                        
                        if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                        asm_.call_rel32(methodIt->second);
                        if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                        return;
                    }
                }
            }
        }
        
        // Check for instance method call (obj.method())
        for (const auto& [implKey, info] : impls_) {
            auto methodIt = info.methodLabels.find(member->member);
            if (methodIt != info.methodLabels.end()) {
                member->object->accept(*this);
                asm_.push_rax();
                
                for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                    node.args[i]->accept(*this);
                    asm_.push_rax();
                }
                
                size_t totalArgs = node.args.size() + 1;
                if (totalArgs >= 1) asm_.pop_rcx();
                if (totalArgs >= 2) asm_.pop_rdx();
                if (totalArgs >= 3) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x58);
                }
                if (totalArgs >= 4) {
                    asm_.code.push_back(0x41); asm_.code.push_back(0x59);
                }
                
                if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
                asm_.call_rel32(methodIt->second);
                if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
                return;
            }
        }
    }
    
    if (auto* id = dynamic_cast<Identifier*>(node.callee.get())) {
        // Check if this is an extern function
        auto externIt = externFunctions_.find(id->name);
        if (externIt != externFunctions_.end()) {
            for (int i = (int)node.args.size() - 1; i >= 0; i--) {
                node.args[i]->accept(*this);
                asm_.push_rax();
            }
            
            if (node.args.size() >= 1) asm_.pop_rcx();
            if (node.args.size() >= 2) asm_.pop_rdx();
            if (node.args.size() >= 3) {
                asm_.code.push_back(0x41); asm_.code.push_back(0x58);
            }
            if (node.args.size() >= 4) {
                asm_.code.push_back(0x41); asm_.code.push_back(0x59);
            }
            
            if (!stackAllocated_) asm_.sub_rsp_imm32(0x20);
            asm_.call_mem_rip(pe_.getImportRVA(id->name));
            if (!stackAllocated_) asm_.add_rsp_imm32(0x20);
            return;
        }
        
        // ===== String builtins =====
        if (id->name == "len" && node.args.size() == 1) {
            emitStringLen(node);
            return;
        }
        if (id->name == "upper" && node.args.size() == 1) {
            emitStringUpper(node);
            return;
        }
        if (id->name == "lower" && node.args.size() == 1) {
            emitStringLower(node);
            return;
        }
        if (id->name == "trim" && node.args.size() == 1) {
            emitStringTrim(node);
            return;
        }
        if (id->name == "starts_with" && node.args.size() == 2) {
            emitStringStartsWith(node);
            return;
        }
        if (id->name == "ends_with" && node.args.size() == 2) {
            emitStringEndsWith(node);
            return;
        }
        if (id->name == "substring" && (node.args.size() == 2 || node.args.size() == 3)) {
            emitStringSubstring(node);
            return;
        }
        if (id->name == "replace" && node.args.size() == 3) {
            emitStringReplace(node);
            return;
        }
        if (id->name == "split" && node.args.size() == 2) {
            emitStringSplit(node);
            return;
        }
        if (id->name == "join" && node.args.size() == 2) {
            emitStringJoin(node);
            return;
        }
        if (id->name == "index_of" && node.args.size() == 2) {
            emitStringIndexOf(node);
            return;
        }
        if (id->name == "contains" && node.args.size() == 2) {
            emitListContains(node);
            return;
        }
        
        // ===== List builtins =====
        if (id->name == "push" && node.args.size() == 2) {
            emitListPush(node);
            return;
        }
        if (id->name == "pop" && node.args.size() == 1) {
            emitListPop(node);
            return;
        }
        if (id->name == "range") {
            emitRange(node);
            return;
        }
        
        // ===== Basic Math builtins =====
        if (id->name == "abs" && node.args.size() == 1) {
            emitMathAbs(node);
            return;
        }
        if (id->name == "min" && node.args.size() == 2) {
            emitMathMin(node);
            return;
        }
        if (id->name == "max" && node.args.size() == 2) {
            emitMathMax(node);
            return;
        }
        if (id->name == "sqrt" && node.args.size() == 1) {
            emitMathSqrt(node);
            return;
        }
        if (id->name == "floor" && node.args.size() == 1) {
            emitMathFloor(node);
            return;
        }
        if (id->name == "ceil" && node.args.size() == 1) {
            emitMathCeil(node);
            return;
        }
        if (id->name == "round" && node.args.size() == 1) {
            emitMathRound(node);
            return;
        }
        if (id->name == "pow" && node.args.size() == 2) {
            emitMathPow(node);
            return;
        }
        
        // ===== Platform builtins =====
        if (id->name == "platform") {
            uint32_t rva = addString("windows");
            asm_.lea_rax_rip_fixup(rva);
            return;
        }
        if (id->name == "arch") {
            uint32_t rva = addString("x64");
            asm_.lea_rax_rip_fixup(rva);
            return;
        }
        
        // ===== Type conversion builtins =====
        if (id->name == "str" && node.args.size() == 1) {
            emitConvStr(node);
            return;
        }
        if (id->name == "int" && node.args.size() == 1) {
            emitConvInt(node);
            return;
        }
        if (id->name == "float" && node.args.size() == 1) {
            emitConvFloat(node);
            return;
        }
        if (id->name == "bool" && node.args.size() == 1) {
            emitConvBool(node);
            return;
        }
        
        // ===== Print builtins =====
        if (id->name == "print" || id->name == "println") {
            emitPrint(node, true);
            return;
        }
        
        // ===== Result type builtins =====
        if (id->name == "Ok" && node.args.size() == 1) {
            emitResultOk(node);
            return;
        }
        if (id->name == "Err" && node.args.size() == 1) {
            emitResultErr(node);
            return;
        }
        if (id->name == "is_ok" && node.args.size() == 1) {
            emitResultIsOk(node);
            return;
        }
        if (id->name == "is_err" && node.args.size() == 1) {
            emitResultIsErr(node);
            return;
        }
        if (id->name == "unwrap" && node.args.size() == 1) {
            emitResultUnwrap(node);
            return;
        }
        if (id->name == "unwrap_or" && node.args.size() == 2) {
            emitResultUnwrapOr(node);
            return;
        }
        
        // ===== File I/O builtins =====
        if (id->name == "open" && (node.args.size() == 1 || node.args.size() == 2)) {
            emitFileOpen(node);
            return;
        }
        if (id->name == "read" && node.args.size() == 2) {
            emitFileRead(node);
            return;
        }
        if (id->name == "write" && node.args.size() == 2) {
            emitFileWrite(node);
            return;
        }
        if (id->name == "close" && node.args.size() == 1) {
            emitFileClose(node);
            return;
        }
        if (id->name == "file_size" && node.args.size() == 1) {
            emitFileSize(node);
            return;
        }
        
        // ===== System builtins =====
        if (id->name == "hostname") {
            emitSystemHostname(node);
            return;
        }
        if (id->name == "username") {
            emitSystemUsername(node);
            return;
        }
        if (id->name == "cpu_count") {
            emitSystemCpuCount(node);
            return;
        }
        if (id->name == "sleep" && node.args.size() >= 1) {
            emitSystemSleep(node);
            return;
        }
        if (id->name == "now") {
            emitTimeNow(node);
            return;
        }
        if (id->name == "now_ms") {
            emitTimeNowMs(node);
            return;
        }
        if (id->name == "year") {
            emitTimeYear(node);
            return;
        }
        if (id->name == "month") {
            emitTimeMonth(node);
            return;
        }
        if (id->name == "day") {
            emitTimeDay(node);
            return;
        }
        if (id->name == "hour") {
            emitTimeHour(node);
            return;
        }
        if (id->name == "minute") {
            emitTimeMinute(node);
            return;
        }
        if (id->name == "second") {
            emitTimeSecond(node);
            return;
        }
        
        // ===== Extended String builtins =====
        if (id->name == "ltrim" && node.args.size() == 1) {
            emitStringLtrim(node);
            return;
        }
        if (id->name == "rtrim" && node.args.size() == 1) {
            emitStringRtrim(node);
            return;
        }
        if (id->name == "char_at" && node.args.size() == 2) {
            emitStringCharAt(node);
            return;
        }
        if (id->name == "repeat" && node.args.size() == 2) {
            emitStringRepeat(node);
            return;
        }
        if (id->name == "reverse_str" && node.args.size() == 1) {
            emitStringReverse(node);
            return;
        }
        if (id->name == "is_digit" && node.args.size() == 1) {
            emitStringIsDigit(node);
            return;
        }
        if (id->name == "is_alpha" && node.args.size() == 1) {
            emitStringIsAlpha(node);
            return;
        }
        if (id->name == "ord" && node.args.size() == 1) {
            emitStringOrd(node);
            return;
        }
        if (id->name == "chr" && node.args.size() == 1) {
            emitStringChr(node);
            return;
        }
        if (id->name == "last_index_of" && node.args.size() == 2) {
            emitStringLastIndexOf(node);
            return;
        }
        
        // ===== Extended Math builtins =====
        if (id->name == "sin" && node.args.size() == 1) {
            emitMathSin(node);
            return;
        }
        if (id->name == "cos" && node.args.size() == 1) {
            emitMathCos(node);
            return;
        }
        if (id->name == "tan" && node.args.size() == 1) {
            emitMathTan(node);
            return;
        }
        if (id->name == "exp" && node.args.size() == 1) {
            emitMathExp(node);
            return;
        }
        if (id->name == "log" && node.args.size() == 1) {
            emitMathLog(node);
            return;
        }
        if (id->name == "trunc" && node.args.size() == 1) {
            emitMathTrunc(node);
            return;
        }
        if (id->name == "sign" && node.args.size() == 1) {
            emitMathSign(node);
            return;
        }
        if (id->name == "clamp" && node.args.size() == 3) {
            emitMathClamp(node);
            return;
        }
        if (id->name == "lerp" && node.args.size() == 3) {
            emitMathLerp(node);
            return;
        }
        if (id->name == "gcd" && node.args.size() == 2) {
            emitMathGcd(node);
            return;
        }
        if (id->name == "lcm" && node.args.size() == 2) {
            emitMathLcm(node);
            return;
        }
        if (id->name == "factorial" && node.args.size() == 1) {
            emitMathFactorial(node);
            return;
        }
        if (id->name == "fib" && node.args.size() == 1) {
            emitMathFib(node);
            return;
        }
        if (id->name == "random" && node.args.empty()) {
            emitMathRandom(node);
            return;
        }
        if (id->name == "is_nan" && node.args.size() == 1) {
            emitMathIsNan(node);
            return;
        }
        if (id->name == "is_inf" && node.args.size() == 1) {
            emitMathIsInf(node);
            return;
        }
        
        // ===== Extended List builtins =====
        if (id->name == "first" && node.args.size() == 1) {
            emitListFirst(node);
            return;
        }
        if (id->name == "last" && node.args.size() == 1) {
            emitListLast(node);
            return;
        }
        if (id->name == "get" && node.args.size() == 2) {
            emitListGet(node);
            return;
        }
        if (id->name == "reverse" && node.args.size() == 1) {
            emitListReverse(node);
            return;
        }
        if (id->name == "index" && node.args.size() == 2) {
            emitListIndex(node);
            return;
        }
        if (id->name == "includes" && node.args.size() == 2) {
            emitListIncludes(node);
            return;
        }
        if (id->name == "take" && node.args.size() == 2) {
            emitListTake(node);
            return;
        }
        if (id->name == "drop" && node.args.size() == 2) {
            emitListDrop(node);
            return;
        }
        if (id->name == "min_of" && node.args.size() == 1) {
            emitListMinOf(node);
            return;
        }
        if (id->name == "max_of" && node.args.size() == 1) {
            emitListMaxOf(node);
            return;
        }
        
        // ===== Extended Time builtins =====
        if (id->name == "now_us" && node.args.empty()) {
            emitTimeNowUs(node);
            return;
        }
        if (id->name == "weekday" && node.args.empty()) {
            emitTimeWeekday(node);
            return;
        }
        if (id->name == "day_of_year" && node.args.empty()) {
            emitTimeDayOfYear(node);
            return;
        }
        if (id->name == "make_time" && node.args.size() == 6) {
            emitTimeMakeTime(node);
            return;
        }
        if (id->name == "add_days" && node.args.size() == 2) {
            emitTimeAddDays(node);
            return;
        }
        if (id->name == "add_hours" && node.args.size() == 2) {
            emitTimeAddHours(node);
            return;
        }
        if (id->name == "diff_days" && node.args.size() == 2) {
            emitTimeDiffDays(node);
            return;
        }
        if (id->name == "is_leap_year" && node.args.size() == 1) {
            emitTimeIsLeapYear(node);
            return;
        }
        
        // ===== Extended System builtins =====
        if (id->name == "env" && node.args.size() == 1) {
            emitSystemEnv(node);
            return;
        }
        if (id->name == "set_env" && node.args.size() == 2) {
            emitSystemSetEnv(node);
            return;
        }
        if (id->name == "home_dir" && node.args.empty()) {
            emitSystemHomeDir(node);
            return;
        }
        if (id->name == "temp_dir" && node.args.empty()) {
            emitSystemTempDir(node);
            return;
        }
        if (id->name == "assert" && (node.args.size() == 1 || node.args.size() == 2)) {
            emitSystemAssert(node);
            return;
        }
        if (id->name == "panic" && node.args.size() == 1) {
            emitSystemPanic(node);
            return;
        }
        if (id->name == "debug" && node.args.size() == 1) {
            emitSystemDebug(node);
            return;
        }
        if (id->name == "system" && node.args.size() == 1) {
            emitSystemCommand(node);
            return;
        }
        
        // ===== GC builtins =====
        if (id->name == "gc_collect" && node.args.empty()) {
            emitGCCollect(node);
            return;
        }
        if (id->name == "gc_stats" && node.args.empty()) {
            emitGCStats(node);
            return;
        }
        if (id->name == "gc_count" && node.args.empty()) {
            emitGCCount(node);
            return;
        }
        if (id->name == "gc_pin" && node.args.size() == 1) {
            emitGCPin(node);
            return;
        }
        if (id->name == "gc_unpin" && node.args.size() == 1) {
            emitGCUnpin(node);
            return;
        }
        if (id->name == "gc_add_root" && node.args.size() == 1) {
            emitGCAddRoot(node);
            return;
        }
        if (id->name == "gc_remove_root" && node.args.size() == 1) {
            emitGCRemoveRoot(node);
            return;
        }
        if (id->name == "set_allocator" && node.args.size() == 2) {
            emitSetAllocator(node);
            return;
        }
        if (id->name == "reset_allocator" && node.args.size() == 0) {
            emitResetAllocator(node);
            return;
        }
        if (id->name == "allocator_stats" && node.args.size() == 0) {
            emitAllocatorStats(node);
            return;
        }
        if (id->name == "allocator_peak" && node.args.size() == 0) {
            emitAllocatorPeak(node);
            return;
        }
        
        // ===== Memory builtins =====
        if (id->name == "alloc" && node.args.size() == 1) {
            emitMemAlloc(node);
            return;
        }
        if (id->name == "free" && node.args.size() == 1) {
            emitMemFree(node);
            return;
        }
        if (id->name == "stackalloc" && node.args.size() == 1) {
            emitMemStackAlloc(node);
            return;
        }
        if (id->name == "sizeof" && node.args.size() == 1) {
            emitMemSizeof(node);
            return;
        }
        if (id->name == "alignof" && node.args.size() == 1) {
            emitMemAlignof(node);
            return;
        }
        if (id->name == "offsetof" && node.args.size() == 2) {
            emitMemOffsetof(node);
            return;
        }
        if (id->name == "placement_new" && node.args.size() == 2) {
            emitMemPlacementNew(node);
            return;
        }
        if (id->name == "memcpy" && node.args.size() == 3) {
            emitMemcpy(node);
            return;
        }
        if (id->name == "memset" && node.args.size() == 3) {
            emitMemset(node);
            return;
        }
        if (id->name == "memmove" && node.args.size() == 3) {
            emitMemmove(node);
            return;
        }
        if (id->name == "memcmp" && node.args.size() == 3) {
            emitMemcmp(node);
            return;
        }
        
        // ===== Synchronization builtins =====
        if (id->name == "mutex_lock" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitMutexLock();
            return;
        }
        if (id->name == "mutex_unlock" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitMutexUnlock();
            return;
        }
        if (id->name == "rwlock_read" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitRWLockReadLock();
            return;
        }
        if (id->name == "rwlock_write" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitRWLockWriteLock();
            return;
        }
        if (id->name == "rwlock_unlock" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitRWLockUnlock();
            return;
        }
        if (id->name == "cond_wait" && node.args.size() == 2) {
            // cond_wait(cond, mutex)
            node.args[1]->accept(*this);  // mutex
            asm_.push_rax();
            node.args[0]->accept(*this);  // cond
            asm_.pop_rcx();  // mutex in RCX
            emitCondWait();
            return;
        }
        if (id->name == "cond_signal" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitCondSignal();
            return;
        }
        if (id->name == "cond_broadcast" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitCondBroadcast();
            return;
        }
        if (id->name == "sem_acquire" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitSemaphoreAcquire();
            return;
        }
        if (id->name == "sem_release" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitSemaphoreRelease();
            return;
        }
        if (id->name == "sem_try_acquire" && node.args.size() == 1) {
            node.args[0]->accept(*this);
            emitSemaphoreTryAcquire();
            return;
        }
        
        // ===== Generic function calls =====
        std::string callTarget = id->name;
        bool callReturnsFloat = false;
        bool callReturnsString = false;
        auto genericIt = genericFunctions_.find(id->name);
        if (genericIt != genericFunctions_.end()) {
            FnDecl* genericFn = genericIt->second;
            std::vector<TypePtr> typeArgs;
            auto& reg = TypeRegistry::instance();
            
            std::unordered_map<std::string, TypePtr> inferred;
            for (size_t i = 0; i < node.args.size() && i < genericFn->params.size(); i++) {
                const std::string& paramType = genericFn->params[i].second;
                
                for (const auto& tp : genericFn->typeParams) {
                    if (paramType == tp) {
                        TypePtr argType = reg.anyType();
                        
                        if (dynamic_cast<IntegerLiteral*>(node.args[i].get())) {
                            argType = reg.intType();
                        } else if (dynamic_cast<FloatLiteral*>(node.args[i].get())) {
                            argType = reg.floatType();
                        } else if (dynamic_cast<StringLiteral*>(node.args[i].get())) {
                            argType = reg.stringType();
                        } else if (dynamic_cast<BoolLiteral*>(node.args[i].get())) {
                            argType = reg.boolType();
                        } else if (auto* ident = dynamic_cast<Identifier*>(node.args[i].get())) {
                            if (floatVars.count(ident->name)) {
                                argType = reg.floatType();
                            } else if (constFloatVars.count(ident->name)) {
                                argType = reg.floatType();
                            } else if (constVars.count(ident->name)) {
                                argType = reg.intType();
                            } else if (constStrVars.count(ident->name)) {
                                argType = reg.stringType();
                            }
                        } else if (isFloatExpression(node.args[i].get())) {
                            argType = reg.floatType();
                        } else if (isStringReturningExpr(node.args[i].get())) {
                            argType = reg.stringType();
                        }
                        
                        if (inferred.find(tp) == inferred.end()) {
                            inferred[tp] = argType;
                        }
                        break;
                    }
                }
            }
            
            for (const auto& tp : genericFn->typeParams) {
                auto it = inferred.find(tp);
                if (it != inferred.end()) {
                    typeArgs.push_back(it->second);
                } else {
                    typeArgs.push_back(reg.anyType());
                }
            }
            
            callTarget = monomorphizer_.getMangledName(id->name, typeArgs);
            
            if (!monomorphizer_.hasInstantiation(id->name, typeArgs)) {
                monomorphizer_.recordFunctionInstantiation(id->name, typeArgs, genericFn);
                // Register the label for this new instantiation so the call can be resolved
                asm_.labels[callTarget] = 0;
            }
            
            // Ensure label is registered even if instantiation was already recorded
            // (may have been recorded by GenericCollector but label not yet registered)
            if (asm_.labels.find(callTarget) == asm_.labels.end()) {
                asm_.labels[callTarget] = 0;
            }
            
            callReturnsFloat = monomorphizer_.functionReturnsFloat(callTarget);
            callReturnsString = monomorphizer_.functionReturnsString(callTarget);
            
            if (!callReturnsFloat && !typeArgs.empty()) {
                for (const auto& arg : typeArgs) {
                    if (arg->toString() == "float") {
                        std::string returnType = genericFn->returnType;
                        for (size_t i = 0; i < genericFn->typeParams.size() && i < typeArgs.size(); i++) {
                            if (returnType == genericFn->typeParams[i] && typeArgs[i]->toString() == "float") {
                                callReturnsFloat = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        // Direct function call
        if (asm_.labels.count(callTarget) || asm_.labels.count(id->name)) {
            if (callReturnsFloat) {
                emitFloatFunctionCall(node, callTarget);
            } else {
                emitStandardFunctionCall(node, callTarget);
            }
            (void)callReturnsString;
            return;
        }
        
        // Function pointer call
        bool isFnPtrCall = fnPtrVars_.count(id->name) > 0;
        if (!isFnPtrCall && !asm_.labels.count(id->name)) {
            if (locals.count(id->name) || varRegisters_.count(id->name) || globalVarRegisters_.count(id->name)) {
                isFnPtrCall = true;
            }
        }
        
        if (isFnPtrCall) {
            emitFunctionPointerCall(node, id->name);
            return;
        }
    }
    
    // Indirect call through closure
    emitClosureCall(node);
}

} // namespace flex
