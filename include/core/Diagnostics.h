#pragma once
#include <string>
#include <iostream>

namespace Glow {

    // Log levels for different stages of the toolchain
    enum class LogLevel { INFO, WARNING, ERROR, FATAL };

    class Diagnostics {
    public:
        // Main reporting entry point used by Lexer and Parser
        static void report(int line, int col, const std::string& msg,
            LogLevel level = LogLevel::ERROR, const std::string& sourceLine = "");
        
        // Returns true if any ERROR or FATAL has occurred
        static bool hasErrors() { return errorCount > 0; }
        
        // Clears error state for fresh compilation passes
        static void reset() { errorCount = 0; }

        static int errorCount;
    };
}
