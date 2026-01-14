#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cmath>

// Unsigned 128-bit
struct ui128 {
    uint64_t low;
    uint64_t high;

    ui128(uint64_t l = 0, uint64_t h = 0) : low(l), high(h) {}
    ui128(const char* hexStr);

    // Arithmetic
    ui128 operator+(const ui128& o) const;
    ui128 operator-(const ui128& o) const;
    ui128 operator*(const ui128& o) const;
    ui128 operator/(const ui128& o) const;
    ui128 operator%(const ui128& o) const;

    // Comparison
    bool operator<(const ui128& o) const;
    bool operator>(const ui128& o) const;
    bool operator<=(const ui128& o) const;
    bool operator>=(const ui128& o) const;
    bool operator==(const ui128& o) const;
    bool operator!=(const ui128& o) const;

    // Bitwise
    ui128 operator&(const ui128& o) const;
    ui128 operator|(const ui128& o) const;
    ui128 operator^(const ui128& o) const;
    ui128 operator~() const;
};

// Signed 128-bit
struct i128 {
    uint64_t low;
    int64_t high;

    i128(uint64_t l = 0, int64_t h = 0) : low(l), high(h) {}
    i128(const char* hexStr);

    // Arithmetic
    i128 operator+(const i128& o) const;
    i128 operator-(const i128& o) const;
    i128 operator*(const i128& o) const;
    i128 operator/(const i128& o) const;
    i128 operator%(const i128& o) const;

    // Comparison
    bool operator<(const i128& o) const;
    bool operator>(const i128& o) const;
    bool operator<=(const i128& o) const;
    bool operator>=(const i128& o) const;
    bool operator==(const i128& o) const;
    bool operator!=(const i128& o) const;

    // Bitwise
    i128 operator&(const i128& o) const;
    i128 operator|(const i128& o) const;
    i128 operator^(const i128& o) const;
    i128 operator~() const;
};

// Converters
std::string i128_to_string(i128 v);
std::string ui128_to_string(ui128 v);
i128 string_to_i128(const std::string& s);
ui128 string_to_ui128(const std::string& s);

// Power Overloads for Glow's ** operator
ui128 glowPow(ui128 base, ui128 exp);
i128  glowPow(i128 base, i128 exp);
double glowPow(double base, double exp);

// Unified I/O Support
template<typename T, T (*Parser)(const std::string&)>
std::istream& read_128_bit(std::istream& is, T& v) {
    std::string s;
    if (is >> s) {
        try { v = Parser(s); }
        catch (...) { is.setstate(std::ios::failbit); }
    }
    return is;
}

inline std::istream& operator>>(std::istream& is, i128& v)  { return read_128_bit<i128, string_to_i128>(is, v); }
inline std::istream& operator>>(std::istream& is, ui128& v) { return read_128_bit<ui128, string_to_ui128>(is, v); }

inline std::ostream& operator<<(std::ostream& os, const i128& v)  { return os << i128_to_string(v); }
inline std::ostream& operator<<(std::ostream& os, const ui128& v) { return os << ui128_to_string(v); }
