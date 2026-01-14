#pragma once
#include <string>
#include <string_view>

namespace Glow {

    enum class TokenType {
        // --- Structure & Modules ---
        USE, AS, ONCE, FN, ENTRY, RET,      // use (inclusion), as (alias), once -> use once (pragma once)
        UNIT, FROM, ENUM,       // unit (class), from (inheritance), and universal enums (all types supported)
        PUB, PRIV, PROT,        // access blocks: pub:, priv:, prot:
        INIT, KILL,             // lifecycle: init (initializer / constructor), ~kill (destructor)
        MY, MEMBER_ACCESS,      // instance pointer, my->field etc.
        FACE, MODIFY,           // polymorphism: face (v-table definition), modify (override)
        STATIC, CONST,          // shared state and explicit read-only marker

        // --- Control Flow ---
        IF, ELIF, ELSE, LOOP, SWITCH, OPT,
        DEF, DEL, TRY, CATCH, PANIC,  // panic is the runtime error
        INTERPOLATION_UNSAFE,         
        INTERPOLATION_SAFE,
        GC_ON, GC_OFF,                // We can use GC or manually manage memory with DEL

        // --- Type Keywords Palette ---
        I16, I32, I64, I128, UI16, UI32, UI64, UI128, F32, F64,
        BOOL, CHAR, STR, HOLLOW, // hollow (void)

        // --- Collections ---
        ARRAY, LIST, LINKED_LIST, MAP, HASH, QUEUE, SET, STACK, HEAP,
        
        // --- Pointer & Reference Palette ---
        PTR, REF,               // Raw pointer (T*) and Reference (T&)
        UNIQUE, SHARED, WEAK,   // Smart ownership: unique*, shared*, weak*
        VOL,    // Modifier Type. Volatile hardware data(1) / pointer(2), can be both(3) -> vol i32, i32 vol*, vol* i32

        // --- Specialized Literals (Integer types) ---
        I16_LIT, UI16_LIT,
        I32_LIT, UI32_LIT,
        I64_LIT, UI64_LIT,
        I128_LIT, UI128_LIT,

        // --- Literals ---
        IDENTIFIER,     // variable / function / unit.. names
        NUMBER_LIT,     // General Number Literal, acts as a 'Fallback'
        BARE_LIT,       // bare (no value->empty)
        CHAR_LIT,
        STRING_LIT,
        TRUE_LIT,
        FALSE_LIT,

        // --- Operators & Punctuation ---
        EQUAL,          // = 
        COLON,          // :
        COLON_COLON,    // :: scope resolution OR wrapper stub
        SEMICOLON,      // ;
        COMMA,          // ,
        DOT,            // .
        PIPE,           // |
        INSERT,         // << (ostream)
        EXTRACT,        // >> (istream)
        ARROW,          // ->
        QUESTION,       // ? (ternary part 1)
        END,            // block end

        // --- Logic & Comparisons ---
        BANG,           // !
        BANG_EQUAL,     // !=
        EQUAL_EQUAL,    // ==
        GREATER,        // >
        LESS,           // <
        GREATER_EQUAL,  // >=
        LESS_EQUAL,     // <=
        AND,            // &&
        OR,             // ||

        // --- Compound Assignments ---
        PLUS_EQUAL,     // +=
        MINUS_EQUAL,    // -=
        STAR_EQUAL,     // *=
        SLASH_EQUAL,    // /=
        PERCENT_EQUAL,  // %=
        POW_EQUAL,      // **=
        PIPE_EQUAL,     // |=
        TILDE_EQUAL,    // ~=
        CARET_EQUAL,    // ^=
        AMP_EQUAL,      // &=
        NAND_EQUAL,     // ~&=
        NOR_EQUAL,      // ~|=
        
        // --- Math ---
        PLUS, MINUS, STAR, SLASH, POW,
        PERCENT, PLUS_PLUS, MINUS_MINUS, 

        // --- Advanced Bitwise ---
        CARET,          // ^ (Bitwise XOR)
        TILDE,          // ~ (Bitwise NOT / Destructor prefix)
        AMPERSAND,      // & (Bitwise AND / Address-of)
        NAND,           // ~&  (NOT-AND)
        NOR,            // ~|  (NOT-OR)

        // --- Brackets ---
        L_PAREN, R_PAREN,   // ( )
        L_BRACE, R_BRACE,   // { }
        L_BRACK, R_BRACK,   // [ ]

        // We also use < > for type-casting, example: <i32>variable

        // --- Stream keywords ---
        NEWLINE,            // \n
        TAB,                // \t
        FLUSH,              // flush (cxx endl)

        // --- System ---
        EOF_TOKEN,      // End of file
        ERROR           // For lexer errors
    };

    struct Token {
        TokenType type;
        std::string lexeme;
        int line;
        int column;

        // Helper to check if it's a specific type
        bool is(TokenType t) const { return type == t; }
    };
}
