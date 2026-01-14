#include "core/Diagnostics.h"
#include <iostream>
#include <cstdlib>

namespace Glow {

    // Define the static member to avoid linker errors
    int Diagnostics::errorCount = 0;

    void Diagnostics::report(int line, int col, const std::string& msg,
            LogLevel level, const std::string& sourceLine) {
        std::string prefix;
        
        switch (level) {
            case LogLevel::INFO:    prefix = "[INFO]";    break;
            case LogLevel::WARNING: prefix = "[WARNING]"; break;
            case LogLevel::ERROR:   
                prefix = "[ERROR]";   
                errorCount++; 
                break;
            case LogLevel::FATAL:   
                prefix = "[FATAL]";   
                errorCount++; 
                break;
        }

        // Standardized format: [LEVEL] at line X, col Y: Message
        std::cerr << prefix << " at line " << line << ", col " << col << ": " << msg << std::endl;

        // GCC-Style Caret line *Polish*
        if (!sourceLine.empty()) {
            std::cerr << "    " << sourceLine << std::endl;
            std::cerr << "    ";
            for (int i = 1; i < col; ++i) {
                // Handle tabs so the caret stays aligned
                if (sourceLine[i-1] == '\t') std::cerr << "\t";
                else std::cerr << " ";
            }
            std::cerr << "^" << std::endl;
        }

        // Immediately halt the toolchain on unrecoverable logic failures
        if (level == LogLevel::FATAL)
            std::exit(1); 
    }
}
