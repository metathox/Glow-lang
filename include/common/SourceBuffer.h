#pragma once
#include <string>

namespace Glow {

    class SourceBuffer {
    public:
        // Default constructor for general setup
        SourceBuffer() = default;

        // Loads a Glow (.glw) file from disk into memory
        bool loadFile(const std::string& path);
        
        // --- Movement and Inspection ---
        // Returns the current character without moving the cursor
        char peek() const;
        
        // Lookahead for complex sequences like '^^^' or '**='
        char peekNext() const;
        
        // Consumes and returns the current character, updating line/column info
        char advance();
        
        // Returns the character immediately before the current cursor
        char previous() const;
        
        // Returns true if the cursor has reached the end of the file
        bool isAtEnd() const;

        // --- Metadata Getters for Lexer/Diagnostics ---
        int getLine() const { return line; }
        int getColumn() const { return column; }
        std::string getPath() const { return filePath; }

        std::string getLineText(int lineNum) const;

    private:
        std::string content;
        std::string filePath;
        size_t cursor = 0;  // Character offset in the 'content' string
        int line = 1;      // Tracking for error reporting
        int column = 1;    // Tracking for error reporting
    };
}
