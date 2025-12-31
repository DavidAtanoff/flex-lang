cmake_minimum_required(VERSION 3.16)
project(flex VERSION 1.0.0 LANGUAGES CXX)

# Use C++17 for VS 2022 Insiders compatibility
# VS 18 Insiders has bugs with C++20 ranges/concepts in STL headers
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX- /Zc:__cplusplus /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Release optimizations for MinGW
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "")
        add_compile_options(-O2 -DNDEBUG)
        add_link_options(-s)  # Strip symbols
    endif()
endif()

# Source files - Modular structure
set(FRONTEND_SOURCES
    # AST
    src/frontend/ast/ast.cpp
    # Syntax macros
    src/frontend/macro/syntax_macro.cpp
    # Modular parser
    src/frontend/parser/parser_core.cpp
    src/frontend/parser/parser_declarations.cpp
    src/frontend/parser/parser_statements.cpp
    src/frontend/parser/parser_expressions.cpp
    src/frontend/parser/parser_types.cpp
    # Modular lexer
    src/frontend/lexer/lexer_core.cpp
    src/frontend/lexer/lexer_scan.cpp
)

set(SEMANTIC_SOURCES
    # Types
    src/semantic/types/types.cpp
    # Symbols
    src/semantic/symbols/symbol_table.cpp
    # Module system
    src/semantic/module/module_system.cpp
    # Modular type checker
    src/semantic/checker/checker_core.cpp
    src/semantic/checker/checker_expr.cpp
    src/semantic/checker/checker_stmt.cpp
    src/semantic/checker/checker_decl.cpp
    # Modular macro expander
    src/semantic/expander/expander_core.cpp
    src/semantic/expander/expander_expand.cpp
    src/semantic/expander/expander_clone.cpp
    # Optimizer passes (Tier 2-5)
    src/semantic/optimizer/optimizer.cpp
    src/semantic/optimizer/constant_folding.cpp
    src/semantic/optimizer/constant_propagation.cpp
    src/semantic/optimizer/dead_code.cpp
    src/semantic/optimizer/inlining.cpp
    src/semantic/optimizer/tail_call.cpp
    src/semantic/optimizer/ctfe.cpp
    src/semantic/optimizer/ssa.cpp
    src/semantic/optimizer/loop_optimizer.cpp
    src/semantic/optimizer/instruction_scheduler.cpp
    src/semantic/optimizer/cse.cpp
    src/semantic/optimizer/gvn.cpp
    src/semantic/optimizer/algebraic.cpp
    src/semantic/optimizer/pgo.cpp
)

set(CLI_SOURCES
    src/cli/ast_printer.cpp
)

set(STDLIB_SOURCES
    src/stdlib/stdlib_core.cpp
    src/stdlib/io/io.cpp
    src/stdlib/string/string.cpp
    src/stdlib/math/math.cpp
    src/stdlib/list/list.cpp
    src/stdlib/map/map.cpp
    src/stdlib/time/time.cpp
    src/stdlib/system/system.cpp
    src/stdlib/json/json.cpp
)

set(BACKEND_SOURCES
    # x64 native code generation
    src/backend/x64/x64_assembler.cpp
    src/backend/x64/pe_generator.cpp
    src/backend/x64/peephole.cpp
    # Object file format
    src/backend/object/object_file.cpp
    # Garbage collection
    src/backend/gc/gc.cpp
    # Modular native codegen
    src/backend/codegen/codegen_core.cpp
    src/backend/codegen/codegen_builtins.cpp
    src/backend/codegen/codegen_expr.cpp
    src/backend/codegen/codegen_stmt.cpp
    src/backend/codegen/codegen_decl.cpp
    src/backend/codegen/codegen_call.cpp
    src/backend/codegen/codegen_gc.cpp
    src/backend/codegen/register_allocator.cpp
    src/backend/codegen/global_register_allocator.cpp
    src/backend/codegen/vectorizer.cpp
    # Modular bytecode compiler
    src/backend/bytecode/compiler_core.cpp
    src/backend/bytecode/compiler_expr.cpp
    src/backend/bytecode/compiler_stmt.cpp
    src/backend/bytecode/compiler_decl.cpp
    # Modular VM
    src/backend/vm/vm_core.cpp
    src/backend/vm/vm_execute.cpp
    src/backend/vm/vm_builtins.cpp
    # Modular linker
    src/backend/linker/linker_core.cpp
    src/backend/linker/linker_symbols.cpp
    src/backend/linker/linker_layout.cpp
    src/backend/linker/linker_output.cpp
)

set(ALL_SOURCES
    ${FRONTEND_SOURCES}
    ${SEMANTIC_SOURCES}
    ${BACKEND_SOURCES}
    ${CLI_SOURCES}
    ${STDLIB_SOURCES}
    src/main.cpp
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/common
    # Frontend
    ${CMAKE_SOURCE_DIR}/src/frontend
    ${CMAKE_SOURCE_DIR}/src/frontend/ast
    ${CMAKE_SOURCE_DIR}/src/frontend/token
    ${CMAKE_SOURCE_DIR}/src/frontend/parser
    ${CMAKE_SOURCE_DIR}/src/frontend/lexer
    ${CMAKE_SOURCE_DIR}/src/frontend/macro
    # Semantic
    ${CMAKE_SOURCE_DIR}/src/semantic
    ${CMAKE_SOURCE_DIR}/src/semantic/types
    ${CMAKE_SOURCE_DIR}/src/semantic/symbols
    ${CMAKE_SOURCE_DIR}/src/semantic/checker
    ${CMAKE_SOURCE_DIR}/src/semantic/expander
    ${CMAKE_SOURCE_DIR}/src/semantic/module
    ${CMAKE_SOURCE_DIR}/src/semantic/optimizer
    # Backend
    ${CMAKE_SOURCE_DIR}/src/backend
    ${CMAKE_SOURCE_DIR}/src/backend/x64
    ${CMAKE_SOURCE_DIR}/src/backend/object
    ${CMAKE_SOURCE_DIR}/src/backend/runtime
    ${CMAKE_SOURCE_DIR}/src/backend/gc
    ${CMAKE_SOURCE_DIR}/src/backend/codegen
    ${CMAKE_SOURCE_DIR}/src/backend/bytecode
    ${CMAKE_SOURCE_DIR}/src/backend/vm
    ${CMAKE_SOURCE_DIR}/src/backend/linker
    # CLI
    ${CMAKE_SOURCE_DIR}/src/cli
    # Standard Library
    ${CMAKE_SOURCE_DIR}/src/stdlib
)

# Main executable
add_executable(flex ${ALL_SOURCES})

# Installation
install(TARGETS flex DESTINATION bin)

# Tests (optional)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    # Add test targets here
endif()
