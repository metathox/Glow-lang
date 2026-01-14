#include "vm/Value.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace Glow {
    void printValue(const Value& v) {
        switch (v.type) {
            case ValueType::NEWLINE: std::cout << "\n"; break;
            case ValueType::TAB: std::cout << "\t"; break;
            case ValueType::FLUSH:   std::fflush(stdout); break;
            case ValueType::BARE:    std::cout << "bare"; break;
            case ValueType::BOOL:    std::cout << (std::get<bool>(v.as) ? "true" : "false"); break;
            case ValueType::CHAR:    std::cout << "'" << std::get<char>(v.as) << "'"; break;
            
            // Standard Integers
            case ValueType::I16:     std::cout << std::get<int16_t>(v.as); break;
            case ValueType::I32:     std::cout << std::get<int32_t>(v.as); break;
            case ValueType::I64:     std::cout << std::get<int64_t>(v.as); break;
            case ValueType::UI16:    std::cout << std::get<uint16_t>(v.as); break;
            case ValueType::UI32:    std::cout << std::get<uint32_t>(v.as); break;
            case ValueType::UI64:    std::cout << std::get<uint64_t>(v.as); break;

            // 128-bit Systems Printing (Default to Decimal for standard output)
            case ValueType::I128:    std::cout << i128_to_string(std::get<i128>(v.as)); break;
            case ValueType::UI128:   std::cout << ui128_to_string(std::get<ui128>(v.as)); break;
            
            case ValueType::F32:     std::cout << std::get<float>(v.as); break;
            case ValueType::F64:     std::cout << std::get<double>(v.as); break;
            case ValueType::STR:     std::cout << std::get<std::string>(v.as); break;
            case ValueType::REF:     std::cout << "<ref:" << std::get<void*>(v.as) << ">"; break;
        }
    }

    std::string Value::toString() const {
        switch (this->type) {
            case ValueType::I16:  return std::to_string(std::get<int16_t>(as));
            case ValueType::I32:  return std::to_string(std::get<int32_t>(as));
            case ValueType::I64:  return std::to_string(std::get<int64_t>(as));
            case ValueType::UI16: return std::to_string(std::get<uint16_t>(as));
            case ValueType::UI32: return std::to_string(std::get<uint32_t>(as));
            case ValueType::UI64: return std::to_string(std::get<uint64_t>(as));
            case ValueType::I128:  return i128_to_string(std::get<i128>(as));
            case ValueType::UI128: return ui128_to_string(std::get<ui128>(as));

            case ValueType::F32: {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << std::get<float>(as);
                return ss.str();
            }
            case ValueType::F64: {
                std::stringstream ss;
                ss << std::setprecision(17) << std::get<double>(as);
                return ss.str();
            }
            
            case ValueType::NEWLINE: return "\n";
            case ValueType::TAB:     return "\t";
            case ValueType::FLUSH:   return "";
            case ValueType::BOOL:    return std::get<bool>(as) ? "true" : "false";
            case ValueType::CHAR:    return std::string(1, std::get<char>(as));
            case ValueType::STR:     return std::get<std::string>(as);
            case ValueType::REF: {
                std::stringstream ss;
                ss << "<ref:" << std::get<void*>(as) << ">";
                return ss.str();
            }
            default: return "bare";
        }
    }

    std::string Value::toRawString() const {
        switch (this->type) {
            case ValueType::REF: {
                // Print the hex address of the void* stored in 'as'
                char buffer[20];
                sprintf(buffer, "0x%p", std::get<void*>(this->as));
                return std::string(buffer);
            }
            case ValueType::I16:
            case ValueType::I32:
            case ValueType::I64:
                return std::to_string(asI64(*this));
            case ValueType::I128:
            case ValueType::UI128:
                // Promote systems-level transparency for raw interpolation
                return hex128(std::get<i128>(this->as));
            case ValueType::STR:
                return std::get<std::string>(this->as);
            default:
                // Fallback to existing toString() for everything else
                return this->toString(); 
        }
    }

    // Math helpers simply wrap overloaded operators from types.cpp
    // This ensures consistency between the VM and the Transpiler output.
    
    i128 add128(i128 a, i128 b) { return a + b; }
    i128 sub128(i128 a, i128 b) { return a - b; }
    
    // Systems hex helper for internal debugging/palettes
    std::string hex128(i128 val) {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setfill('0') 
           << std::setw(16) << (uint64_t)val.high 
           << std::setw(16) << val.low;
        return ss.str();
    }

    int64_t asI64(const Value& v) {
        switch(v.type){
            case ValueType::I16: return std::get<int16_t>(v.as);
            case ValueType::I32: return std::get<int32_t>(v.as);
            case ValueType::I64: return std::get<int64_t>(v.as);
            default: return 0;
        }
    }

    uint64_t asUI64(const Value& v) {
        switch(v.type) {
            case ValueType::UI16: return std::get<uint16_t>(v.as);
            case ValueType::UI32: return std::get<uint32_t>(v.as);
            case ValueType::UI64: return std::get<uint64_t>(v.as);
            default: return 0;
        }
    }

    double asF64(const Value& v) {
        switch(v.type) {
            case ValueType::F32: return static_cast<double>(std::get<float>(v.as));
            case ValueType::F64: return std::get<double>(v.as);
            default: return 0.0;
        }
    }

    // Constructor: Allocates a contiguous block of Value objects
    GlowArray::GlowArray(size_t len) : size(len) {
        if (len > 0)
            // Allocate raw memory on the heap
            data = new Value[len];
        else
            data = nullptr;
    }

    // Destructor: Cleans up the raw memory
    GlowArray::~GlowArray() {
        if (data != nullptr)
            // Because Value is a struct with a std::variant (which has a destructor),
            // we MUST use delete[] to ensure each element's destructor is called.
            delete[] data;
    }
}
