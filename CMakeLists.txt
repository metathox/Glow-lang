cmake_minimum_required(VERSION 3.10)
project(GlowCompiler VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Source Definitions --- 

# Compiler: Frontend & Backend Logic
set(COMPILER_SOURCES
    src/common/SourceBuffer.cpp
    src/compiler/Lexer.cpp
    src/compiler/SymbolTable.cpp
    src/compiler/Parser.cpp
    src/compiler/ASTPrinter.cpp
    src/compiler/Transpiler.cpp
)

# Virtual Machine (Interpreter): Bytecode execution and Value management
set(VM_SOURCES
    src/vm/Value.cpp
    src/vm/VirtualMachine.cpp
)

# Core: Diagnostics and Error Handling
set(CORE_SOURCES
    src/core/Diagnostics.cpp
)

# Runtime built-in types such as 128-bit
set(RUNTIME_SOURCES
    src/include/GlowRuntime/Built-in/types.cpp 
)

# --- Target Configuration ---

# Main Executable Entry Point
add_executable(glow
    src/main.cpp
    ${CORE_SOURCES}
    ${COMPILER_SOURCES}
    ${VM_SOURCES}
    ${RUNTIME_SOURCES}
)

# "PRIVATE" ensures these paths are only used to build the 'glow' tool binary itself
target_include_directories(glow PRIVATE 
    "include"
    "src/include/GlowRuntime/Built-in"
    "src/include/GlowModules"
)

# Ensure we use math functions for UNIX
if(UNIX)
    target_link_libraries(glow m)
endif()

# Note for Windows: CMake handles MSVC/Ninja linkage automatically.
