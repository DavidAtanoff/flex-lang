// Flex Compiler - Type Checker Base Header
#ifndef FLEX_CHECKER_BASE_H
#define FLEX_CHECKER_BASE_H

#include "semantic/checker/type_checker.h"
#include "semantic/symbols/symbol_table.h"
#include "semantic/types/types.h"

namespace flex {

// Type checker implementation is split across:
// - checker_core.cpp: Core methods, type utilities, diagnostics
// - checker_expr.cpp: Expression type checking visitors
// - checker_stmt.cpp: Statement type checking visitors
// - checker_decl.cpp: Declaration type checking visitors

} // namespace flex

#endif // FLEX_CHECKER_BASE_H
