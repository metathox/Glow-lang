#pragma once
#include <string>
#include <variant>
#include <cstdint>
#include <vector>
#include "types.hpp" // Use the authoritative definitions for i128 and ui128

namespace Glow {

    // --- Forward Declarations ---
    struct Value;

    // Every type & collection defined in the Token palette is represented here
    enum class ValueType { 
        BARE, BOOL, I16, I32, I64, I128, UI16, UI32, UI64, UI128, F32, F64, CHAR, STR, REF,
        ARRAY, NEWLINE, TAB, FLUSH
    };

    struct GlowArray {
        Value* data;
        size_t size;
        GlowArray(size_t len);
        ~GlowArray();
    };

    struct Value {
        ValueType type;
        
        // Tagged union for safe storage. 
        // Using types from types.hpp (i128, ui128)
        std::variant<
            std::monostate, // BARE
            bool,           // BOOL
            int16_t,        // I16
            int32_t,        // I32
            int64_t,        // I64
            i128,           // I128
            uint16_t,       // UI16
            uint32_t,       // UI32
            uint64_t,       // UI64
            ui128,          // UI128
            float,          // F32
            double,         // F64
            char,           // CHAR
            std::string,    // STR
            void*           // REF
        > as;

        // --- Constructors ---
        Value() : type(ValueType::BARE), as(std::monostate{}) {}
        Value(ValueType t)   : type(t), as(std::monostate{})  {}
        Value(bool v)        : type(ValueType::BOOL),  as(v)  {}
        Value(int16_t v)     : type(ValueType::I16),   as(v)  {}
        Value(int32_t v)     : type(ValueType::I32),   as(v)  {}
        Value(int64_t v)     : type(ValueType::I64),   as(v)  {}
        Value(i128 v)        : type(ValueType::I128),  as(v)  {}
        Value(uint16_t v)    : type(ValueType::UI16),  as(v)  {}
        Value(uint32_t v)    : type(ValueType::UI32),  as(v)  {}
        Value(uint64_t v)    : type(ValueType::UI64),  as(v)  {}
        Value(ui128 v)       : type(ValueType::UI128), as(v)  {}
        Value(float v)       : type(ValueType::F32),   as(v)  {}
        Value(double v)      : type(ValueType::F64),   as(v)  {}
        Value(char v)        : type(ValueType::CHAR),  as(v)  {}
        Value(void* ptr)     : type(ValueType::REF),   as(ptr) {}
        Value(std::string v) : type(ValueType::STR),   as(std::move(v)) {}
        Value(GlowArray* arr) : type(ValueType::ARRAY), as((void*)arr) {}

        std::string toString() const;
        std::string toRawString() const;
    };

    // --- Helpers for VM and Debugging ---

    void printValue(const Value& v);
    
    // declarations to provide a clean API for the VM, 
    // they internally call the operators defined in types.hpp.
    i128 add128(i128 a, i128 b);
    i128 sub128(i128 a, i128 b);
    
    // Systems-style hex output for 128-bit types
    std::string hex128(i128 val);

    // --- STANDALONE PROMOTION HELPERS ---
    
    inline bool isSignedInt(ValueType t) {
        return t == ValueType::I16 || t == ValueType::I32 || t == ValueType::I64;
    }

    inline bool isUnsignedInt(ValueType t) {
        return t == ValueType::UI16 || t == ValueType::UI32 || t == ValueType::UI64;
    }

    // Maps to the "left.asI64()" style calls in VM
    int64_t asI64(const Value& v);
    uint64_t asUI64(const Value& v);
    double asF64(const Value& v);
}
