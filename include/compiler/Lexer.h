#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include "common/Token.h"
#include "common/SourceBuffer.h"

namespace Glow {

    class Lexer {
    public:
        // Explicit constructor requiring a SourceBuffer reference
        explicit Lexer(SourceBuffer& buffer);
        void setInStringMode(bool val) { inStringMode = val; }

        // --- Core Scanning Logic ---
        Token scanToken();       // The main switch for operators and dispatching handlers

        // Main entry point: converts raw source into a stream of Glow tokens
        std::vector<Token> tokenize();

    private:
        bool inStringMode = false;
        SourceBuffer& buffer;
        std::vector<TokenType> braceStack;
        std::unordered_map<std::string, TokenType> keywords;

        // --- Setup and Utilities ---
        void initKeywords();     // Populates the map with entry, sli, end, etc.
        void skipWhitespace();   // Handles spaces, tabs, and single-line '#' comments
        void skipMultiLineComment(); // Handles the unique '^^^' Glow block comments
        
        // --- Literal & Identifier Handlers ---
        char translateEscape(char anchor); // handles escape sequences
        Token handleIdentifier(); // Processes words and cross-references initKeywords
        Token handleNumber();     // Processes i32/f32 numeric literals, including underscores
        Token handleString();     // Processes "double-quoted" string literals
        Token handleChar();       // Processes 'single-quoted' character literals
        
        // --- Helper Methods ---
        // Looks ahead at the next character without consuming if it doesn't match
        bool match(char expected);
        Token tokenize_single();
        
        // Packages the TokenType with current line/column metadata
        Token makeToken(TokenType type, std::string lexeme = "");
    };
}
