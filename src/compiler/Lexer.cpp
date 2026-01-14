#include "compiler/Lexer.h"
#include "core/Diagnostics.h"
#include <cctype>

namespace Glow {

    Lexer::Lexer(SourceBuffer& buffer) : buffer(buffer) {
        initKeywords();
    }

    void Lexer::initKeywords() {
        // --- Structure & Modules ---
        keywords["fn"]         = TokenType::FN;
        keywords["entry"]      = TokenType::ENTRY;
        keywords["ret"]        = TokenType::RET;
        keywords["use"]        = TokenType::USE;
        keywords["once"]       = TokenType::ONCE;
        keywords["as"]         = TokenType::AS;
        keywords["unit"]       = TokenType::UNIT;
        keywords["init"]       = TokenType::INIT;
        keywords["face"]       = TokenType::FACE;
        keywords["from"]       = TokenType::FROM;
        keywords["kill"]       = TokenType::KILL;
        keywords["priv"]       = TokenType::PRIV;
        keywords["prot"]       = TokenType::PROT;
        keywords["pub"]        = TokenType::PUB;

        // --- Control Flow ---
        keywords["if"]         = TokenType::IF;
        keywords["else"]       = TokenType::ELSE;
        keywords["elif"]       = TokenType::ELIF;
        keywords["loop"]       = TokenType::LOOP;
        keywords["switch"]     = TokenType::SWITCH;
        keywords["opt"]        = TokenType::OPT;
        keywords["def"]        = TokenType::DEF;
        keywords["try"]        = TokenType::TRY;
        keywords["catch"]      = TokenType::CATCH;
        keywords["panic"]      = TokenType::PANIC;
        keywords["del"]        = TokenType::DEL;
        keywords["GC_ON"]      = TokenType::GC_ON;
        keywords["GC_OFF"]     = TokenType::GC_OFF;
        keywords["end"]        = TokenType::END;
        keywords["newline"]    = TokenType::NEWLINE;
        keywords["flush"]      = TokenType::FLUSH;

        // --- Type Keywords & Pointers Palette ---
        keywords["i16"]        = TokenType::I16;
        keywords["i32"]        = TokenType::I32;
        keywords["i64"]        = TokenType::I64;
        keywords["i128"]       = TokenType::I128;
        keywords["f32"]        = TokenType::F32;
        keywords["f64"]        = TokenType::F64;
        keywords["ui16"]       = TokenType::UI16;
        keywords["ui32"]       = TokenType::UI32;
        keywords["ui64"]       = TokenType::UI64;
        keywords["ui128"]      = TokenType::UI128;
        keywords["bool"]       = TokenType::BOOL;
        keywords["char"]       = TokenType::CHAR;
        keywords["str"]        = TokenType::STR;
        keywords["ref"]        = TokenType::REF;
        keywords["hollow"]     = TokenType::HOLLOW;  // void
        keywords["unique"]     = TokenType::UNIQUE;  // std::unique_ptr
        keywords["shared"]     = TokenType::SHARED;  // std::shared_ptr
        keywords["weak"]       = TokenType::WEAK;    // std::weak_ptr
        keywords["vol"]        = TokenType::VOL;     // volatile (data, pointer, or both)
        
        // --- Literals ---
        keywords["true"]       = TokenType::TRUE_LIT;
        keywords["false"]      = TokenType::FALSE_LIT;
        keywords["bare"]       = TokenType::BARE_LIT;
    }

    bool Lexer::match(char expected) {
        if (buffer.isAtEnd()) return false;
        if (buffer.peek() != expected) return false;
        buffer.advance();
        return true;
    }

    void Lexer::skipWhitespace() {
        for (;;) {
            char c = buffer.peek();
            switch (c) {
                case ' ':
                case '\r':
                case '\t':
                case '\n':
                    buffer.advance();
                    break;
                case '#': // Glow single-line comments
                    while (buffer.peek() != '\n' && !buffer.isAtEnd()) buffer.advance();
                    break;
                default:
                    return;
            }
        }
    }

    std::vector<Token> Lexer::tokenize() {
        std::vector<Token> tokens;
        while (!buffer.isAtEnd()) {
            // Only skip whitespace if we aren't scanning a string fragment
            if (!inStringMode) skipWhitespace(); 
            if (buffer.isAtEnd()) break;
            tokens.push_back(scanToken());
        }
        tokens.push_back(makeToken(TokenType::EOF_TOKEN, ""));
        return tokens;
    }

    Token Lexer::tokenize_single() {
        skipWhitespace();
        if (buffer.isAtEnd()) return makeToken(TokenType::EOF_TOKEN, "");
        return scanToken();
    }

    Token Lexer::scanToken() {
        // FORCE handleString if we are in the middle of an interpolation sequence
        if (inStringMode && buffer.peek() != '$') return handleString();

        char c = buffer.advance();
        switch (c) {
            case '"': { inStringMode = true; return handleString(); }

            // --- Static Single Characters ---
            case '{': return makeToken(TokenType::L_BRACE, "{");
            case '}':
                // BOUNDARY CHECK: if this brace closes a ${ interpolation, 
                // the NEXT token must be a string fragment.
                if (!braceStack.empty() && braceStack.back() == TokenType::INTERPOLATION_SAFE) {
                    braceStack.pop_back();
                    inStringMode = true; 
                }
                return makeToken(TokenType::R_BRACE, "}");
            case '(': return makeToken(TokenType::L_PAREN, "(");
            case ')': {
                // BOUNDARY CHECK: if this paren closes a $( interpolation
                if (!braceStack.empty() && braceStack.back() == TokenType::INTERPOLATION_UNSAFE) {
                    braceStack.pop_back();
                    inStringMode = true;
                }
                return makeToken(TokenType::R_PAREN, ")");
            }

            case '[': return makeToken(TokenType::L_BRACK, "[");
            case ']': return makeToken(TokenType::R_BRACK, "]");
            case ',': return makeToken(TokenType::COMMA, ",");
            case '.': return makeToken(TokenType::DOT, ".");
            case ';': return makeToken(TokenType::SEMICOLON, ";");
            case '?': return makeToken(TokenType::QUESTION, "?");

            case '$': {
                if (match('{')) {
                    braceStack.push_back(TokenType::INTERPOLATION_SAFE);
                    inStringMode = false; // Turn off string mode to scan code (model, speed, etc.)
                    return makeToken(TokenType::INTERPOLATION_SAFE, "${");
                }
                if (match('(')) {
                    braceStack.push_back(TokenType::INTERPOLATION_UNSAFE);
                    inStringMode = false;
                    return makeToken(TokenType::INTERPOLATION_UNSAFE, "$(");
                }
                return makeToken(TokenType::ERROR, "$"); 
            }

            // --- Multi-Char Dispatchers ---
            case ':': {
                bool isDouble = match(':');
                return makeToken(isDouble ? TokenType::COLON_COLON : TokenType::COLON, isDouble ? "::" : ":");
            }
            case '=': {
                bool isDouble = match('=');
                return makeToken(isDouble ? TokenType::EQUAL_EQUAL : TokenType::EQUAL, isDouble ? "==" : "=");
            }
            case '!': {
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::BANG_EQUAL : TokenType::BANG, isEqual ? "!=" : "!");
            }
            case '+': {
                if (match('+')) return makeToken(TokenType::PLUS_PLUS, "++");
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::PLUS_EQUAL : TokenType::PLUS, isEqual ? "+=" : "+");
            }
            case '-': {
                if (match('-')) return makeToken(TokenType::MINUS_MINUS, "--");
                if (match('>')) return makeToken(TokenType::ARROW, "->");
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::MINUS_EQUAL : TokenType::MINUS, isEqual ? "-=" : "-");
            }
            case '*': {
                if (match('*')) {
                    bool isEqual = match('=');
                    return makeToken(isEqual ? TokenType::POW_EQUAL : TokenType::POW, isEqual ? "**=" : "**");
                }
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::STAR_EQUAL : TokenType::STAR, isEqual ? "*=" : "*");
            }
            case '<': {
                if (match('<')) return makeToken(TokenType::INSERT, "<<");
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::LESS_EQUAL : TokenType::LESS, isEqual ? "<=" : "<");
            }
            case '>': {
                if (match('>')) return makeToken(TokenType::EXTRACT, ">>");
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::GREATER_EQUAL : TokenType::GREATER, isEqual ? ">=" : ">");
            }
            case '&': {
                if (match('&')) return makeToken(TokenType::AND, "&&");
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::AMP_EQUAL : TokenType::AMPERSAND, isEqual ? "&=" : "&");
            }
            case '|': {
                if (match('|')) return makeToken(TokenType::OR, "||");
                bool isEqual = match('=');
                return makeToken(isEqual ? TokenType::PIPE_EQUAL : TokenType::PIPE, isEqual ? "|=" : "|");
            }
            
            case '\\': {
                if (match('n'))
                    return makeToken(TokenType::NEWLINE, "\\n");
                if (match('t'))
                    return makeToken(TokenType::TAB, "\\t");
                return makeToken(TokenType::ERROR, "Unexpected backslash");
            }

            // --- The Bitwise NOT / NAND / NOR Group ---
            case '~': {
                char next = buffer.peek();
                switch (next) {
                    case '=': buffer.advance(); return makeToken(TokenType::TILDE_EQUAL, "~=");
                    case '&': 
                        buffer.advance(); 
                        return makeToken(match('=') ? TokenType::NAND_EQUAL : TokenType::NAND, match('=') ? "~&=" : "~&");
                    case '|': 
                        buffer.advance(); 
                        return makeToken(match('=') ? TokenType::NOR_EQUAL : TokenType::NOR, match('=') ? "~|=" : "~|");
                    default: return makeToken(TokenType::TILDE, "~");
                }
            }

            case '^': {
                // Check for Glow Multi-line Comment: ^^^
                if (buffer.peek() == '^' && buffer.peekNext() == '^') {
                    buffer.advance(); buffer.advance();
                    skipMultiLineComment();
                    return tokenize_single();
                }
                return makeToken(match('=') ? TokenType::CARET_EQUAL : TokenType::CARET, match('=') ? "^=" : "^");
            }
            case '\'': return handleChar();

            default:
                if (isdigit(c)) return handleNumber();
                if (isalpha(c) || c == '_') return handleIdentifier();
                return makeToken(TokenType::ERROR, std::string(1, c));
        }
    }

    char Lexer::translateEscape(char anchor) {
        switch (anchor) {
            case 'n':  return '\n';
            case 't':  return '\t';
            case 'r':  return '\r';
            case '\\': return '\\';
            case '"':  return '"';
            case '\'': return '\'';
            case '0':  return '\0';
            default:
                // If it's an unrecognized escape, we just return the char itself
                return anchor; 
        }
    }

    Token Lexer::handleIdentifier() {
        std::string text;
        text += buffer.previous();
        while (isalnum(buffer.peek()) || buffer.peek() == '_') text += buffer.advance();

        auto it = keywords.find(text);
        if (it != keywords.end()) return makeToken(it->second, text);
        return makeToken(TokenType::IDENTIFIER, text);
    }

    Token Lexer::handleNumber() {
        std::string text;
        text += buffer.previous();

        auto consumeDigits = [&](auto predicate) {
            while (predicate(buffer.peek()) || buffer.peek() == '_') {
                if (buffer.peek() == '_') { buffer.advance(); continue; }
                text += buffer.advance();
            }
        };

        if (text == "0" && tolower(buffer.peek()) == 'x') {
            text += buffer.advance(); // 'x'
            consumeDigits(isxdigit);
        }
        
        else {
            consumeDigits(isdigit);
            if (buffer.peek() == '.' && isdigit(buffer.peekNext())) {
                text += buffer.advance();
                while (isdigit(buffer.peek())) text += buffer.advance();
            }
        }
        return makeToken(TokenType::NUMBER_LIT, text);
    }

    Token Lexer::handleChar() {
        if (buffer.isAtEnd()) return makeToken(TokenType::ERROR, "Unterminated char");
        char c = buffer.advance();
        if (!match('\'')) return makeToken(TokenType::ERROR, "Missing closing ' for char literal");
        return makeToken(TokenType::CHAR_LIT, std::string(1, c));
    }

    Token Lexer::handleString() {
        std::string cooked;

        while (!buffer.isAtEnd()) {
            char c = buffer.peek();

            // 1. TERMINATION: Hit the closing quote
            if (c == '"') break;

            // 2. BOUNDARY CHECK: Inside interpolation, stop if we hit the closing '}' or ')'
            if (!braceStack.empty()) {
                char target = (braceStack.back() == TokenType::INTERPOLATION_SAFE) ? '}' : ')';
                if (c == target) {
                    inStringMode = false; // Exit string mode to let scanToken handle the brace
                    return makeToken(TokenType::STRING_LIT, cooked);
                }
            }

            // 3. TRIGGER CHECK: New nested interpolation entry
            if (c == '$' && (buffer.peekNext() == '{' || buffer.peekNext() == '(')) {
                // If we have text before the $, return that fragment first
                if (!cooked.empty()) return makeToken(TokenType::STRING_LIT, cooked);

                buffer.advance(); // consume '$'
                if (match('{')) {
                    braceStack.push_back(TokenType::INTERPOLATION_SAFE);
                    inStringMode = false;
                    return makeToken(TokenType::INTERPOLATION_SAFE, "${");
                }
                if (match('(')) {
                    braceStack.push_back(TokenType::INTERPOLATION_UNSAFE);
                    inStringMode = false;
                    return makeToken(TokenType::INTERPOLATION_UNSAFE, "$(");
                }
            }

            // CHARACTER GATHERING
            buffer.advance(); // Move past current char
            if (c == '\\') {
                if (!buffer.isAtEnd())
                    cooked += translateEscape(buffer.advance());
            }
            else cooked += c;
        }

        // EOF SAFETY
        if (buffer.isAtEnd()) {
            inStringMode = false;
            return makeToken(TokenType::ERROR, "Unterminated string literal.");
        }

        // CONSUME FINAL QUOTE
        buffer.advance(); // consume '"'
        inStringMode = false; 

        // This handles "" perfectly: cooked is empty, loop breaks at (1), returns empty STRING_LIT
        return makeToken(TokenType::STRING_LIT, cooked);
    }

    Token Lexer::makeToken(TokenType type, std::string lexeme) {
        // Ensuring lexeme is populated for all tokens facilitates better debugging
        if (lexeme.empty())
            lexeme = ""; // This is a simplified fallback for standard punctuation
        return { type, lexeme, buffer.getLine(), buffer.getColumn() };
    }

    void Lexer::skipMultiLineComment() {
        while (!buffer.isAtEnd()) {
            // Look for the sequence ^^^
            // We use peek() for the 1st, peekNext() for the 2nd
            if (buffer.peek() == '^' && buffer.peekNext() == '^') {
                buffer.advance(); // consume 1st ^
                buffer.advance(); // consume 2nd ^
                
                // Now check if there is a 3rd one
                if (buffer.peek() == '^') {
                    buffer.advance(); // consume 3rd ^
                    return; // Successfully closed, exit the loop
                }
                // If it was just ^^, the loop continues
                continue; 
            }
            buffer.advance();
        }
        Diagnostics::report(buffer.getLine(), buffer.getColumn(), "Unterminated multi-line comment");
    }
}
