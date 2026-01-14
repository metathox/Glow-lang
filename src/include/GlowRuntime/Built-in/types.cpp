#include "types.hpp"

// --- Internal Engine: Unsigned 128-bit Div/Mod ---
static ui128 div_mod128(ui128 n, ui128 d, ui128* r) {
    if (d.low == 0 && d.high == 0) return {0, 0}; 
    ui128 quot = {0, 0}, rem = {0, 0};
    for (int i = 127; i >= 0; i--) {
        uint64_t carry = (rem.low >> 63);
        rem.low <<= 1; rem.high = (rem.high << 1) | carry;
        uint64_t bit = (i >= 64) ? (n.high >> (i - 64)) : (n.low >> i);
        rem.low |= (bit & 1);

        if (rem.high > d.high || (rem.high == d.high && rem.low >= d.low)) {
            uint64_t borrow = (rem.low < d.low) ? 1 : 0;
            rem.low -= d.low; rem.high -= (d.high + borrow);
            if (i >= 64) quot.high |= (1ULL << (i - 64));
            else quot.low |= (1ULL << i);
        }
    }
    if (r) *r = rem;
    return quot;
}

// --- UI128 Implementation ---
ui128 ui128::operator+(const ui128& o) const {
    ui128 res = {low + o.low, high + o.high};
    if (res.low < low) res.high++;
    return res;
}

ui128 ui128::operator-(const ui128& o) const {
    return {low - o.low, high - o.high - (low < o.low ? 1 : 0)};
}

ui128 ui128::operator*(const ui128& o) const {
    uint64_t al = low & 0xFFFFFFFF, ah = low >> 32;
    uint64_t bl = o.low & 0xFFFFFFFF, bh = o.low >> 32;
    uint64_t rl = al * bl;
    uint64_t m1 = ah * bl + (rl >> 32), m2 = al * bh;
    rl = (rl & 0xFFFFFFFF) | ((m1 + m2) << 32);
    uint64_t rh = high * o.low + low * o.high + ((m1 + m2) >> 32) + ah * bh;
    return {rl, rh};
}

ui128 ui128::operator/(const ui128& o) const { return div_mod128(*this, o, nullptr); }
ui128 ui128::operator%(const ui128& o) const { ui128 r; div_mod128(*this, o, &r); return r; }

bool ui128::operator<(const ui128& o) const { return (high != o.high) ? high < o.high : low < o.low; }
bool ui128::operator>(const ui128& o) const { return o < *this; }
bool ui128::operator==(const ui128& o) const { return low == o.low && high == o.high; }
bool ui128::operator!=(const ui128& o) const { return !(*this == o); }

// --- I128 Implementation ---
i128 i128::operator+(const i128& o) const {
    ui128 a = {low, (uint64_t)high}, b = {o.low, (uint64_t)o.high};
    ui128 r = a + b; return {r.low, (int64_t)r.high};
}

i128 i128::operator-(const i128& o) const {
    ui128 a = {low, (uint64_t)high}, b = {o.low, (uint64_t)o.high};
    ui128 r = a - b; return {r.low, (int64_t)r.high};
}

i128 i128::operator*(const i128& o) const {
    ui128 r = ui128{low, (uint64_t)high} * ui128{o.low, (uint64_t)o.high};
    return {r.low, (int64_t)r.high};
}

i128 i128::operator/(const i128& o) const {
    if (o.low == 0 && o.high == 0) throw std::runtime_error("Divide by zero");
    
    bool negative = (high < 0) ^ (o.high < 0);
    i128 a = (high < 0) ? i128{0, 0} - *this : *this;
    i128 b = (o.high < 0) ? i128{0, 0} - o : o;
    
    ui128 uA = {a.low, (uint64_t)a.high};
    ui128 uB = {b.low, (uint64_t)b.high};
    ui128 r = uA / uB;
    
    i128 res = {r.low, (int64_t)r.high};
    return negative ? i128{0, 0} - res : res;
}

i128 i128::operator%(const i128& o) const {
    bool neg = (high < 0);
    i128 a = (high < 0) ? i128{0, 0} - *this : *this;
    i128 b = (o.high < 0) ? i128{0, 0} - o : o;
    ui128 r = ui128{a.low, (uint64_t)a.high} % ui128{b.low, (uint64_t)b.high};
    i128 res = {r.low, (int64_t)r.high};
    return neg ? i128{0, 0} - res : res;
}

bool i128::operator<(const i128& o) const { return (high != o.high) ? high < o.high : low < o.low; }
bool i128::operator==(const i128& o) const { return low == o.low && high == o.high; }

// --- Conversions ---
ui128 string_to_ui128(const std::string& s) {
    // If it's hex, use our specialized constructor logic
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return ui128(s.c_str());

    // Fallback: Standard base-10 parsing for "123456"
    ui128 res = {0, 0};
    for (char c : s) {
        if (c < '0' || c > '9') break;
        res = (res * ui128{10, 0}) + ui128{(uint64_t)(c - '0'), 0};
    }
    return res;
}

i128 string_to_i128(const std::string& s) {
    if (s.empty()) return {0, 0};
    
    // Check if the string (ignoring potential negative sign) is hex
    size_t start = (s[0] == '-') ? 1 : 0;
    if (s.size() >= start + 2 && s[start] == '0' && (s[start+1] == 'x' || s[start+1] == 'X'))
        return i128(s.c_str());
    
    // Fallback: Standard base-10 parsing
    bool negative = (s[0] == '-');
    ui128 r = string_to_ui128(negative ? s.substr(1) : s);
    i128 res = {r.low, (int64_t)r.high};
    return negative ? i128{0, 0} - res : res;
}

std::string ui128_to_string(ui128 v) {
    if (v == ui128{0, 0}) return "0";
    std::string s = "";
    while (v != ui128{0, 0}) {
        ui128 r; v = div_mod128(v, {10, 0}, &r);
        s += (char)('0' + r.low);
    }
    std::reverse(s.begin(), s.end());
    return s;
}

std::string i128_to_string(i128 v) {
    if (v == i128{0, 0}) return "0";
    bool neg = (v.high < 0);
    if (neg) v = i128{0, 0} - v;
    std::string s = ui128_to_string({v.low, (uint64_t)v.high});
    return neg ? "-" + s : s;
}

// --- Comparisons for UI128 ---
bool ui128::operator<=(const ui128& o) const { return !(*this > o); }
bool ui128::operator>=(const ui128& o) const { return !(*this < o); }

// --- Additional Comparisons for I128 ---
bool i128::operator>(const i128& o) const  { return o < *this; }
bool i128::operator<=(const i128& o) const { return !(*this > o); }
bool i128::operator>=(const i128& o) const { return !(*this < o); }
bool i128::operator!=(const i128& o) const { return !(*this == o); }

// --- Bitwise Implementation for UI128 ---
ui128 ui128::operator&(const ui128& o) const { return {low & o.low, high & o.high}; }
ui128 ui128::operator|(const ui128& o) const { return {low | o.low, high | o.high}; }
ui128 ui128::operator^(const ui128& o) const { return {low ^ o.low, high ^ o.high}; }
ui128 ui128::operator~() const { return {~low, ~high}; }

// --- Bitwise Implementation for I128 ---
i128 i128::operator&(const i128& o) const { return {low & o.low, high & (int64_t)o.high}; }
i128 i128::operator|(const i128& o) const { return {low | o.low, high | (int64_t)o.high}; }
i128 i128::operator^(const i128& o) const { return {low ^ o.low, high ^ (int64_t)o.high}; }
i128 i128::operator~() const { return {~low, ~high}; }


// Fast Power for 128-bit types (Square and Multiply)
ui128 glowPow(ui128 base, ui128 exp) {
    ui128 res = {1, 0};
    while (exp > ui128{0, 0}) {
        if (exp.low & 1) res = res * base;
        base = base * base;
        exp = exp / ui128{2, 0};
    }
    return res;
}

i128 glowPow(i128 base, i128 exp) {
    ui128 a = {base.low, (uint64_t)base.high};
    ui128 b = {exp.low, (uint64_t)exp.high};
    ui128 r = glowPow(a, b);
    return {r.low, (int64_t)r.high};
}

// Fallback for standard types (f32 / f64)
double glowPow(double base, double exp) {
    return std::pow(base, exp);
}

// --- Hex String Literal Implementations ---

ui128::ui128(const char* hexStr) {
    std::string s(hexStr);
    // Handle 0x prefix
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s = s.substr(2);

    // Pad to 32 chars (128 bits) with leading zeros
    if (s.length() < 32)
        s.insert(0, 32 - s.length(), '0');
    
    // Slice into two 64-bit hex chunks
    std::string highPart = s.substr(0, 16);
    std::string lowPart = s.substr(16, 16);

    // Convert hex string to uint64_t
    this->high = std::stoull(highPart, nullptr, 16);
    this->low = std::stoull(lowPart, nullptr, 16);
}

i128::i128(const char* hexStr) {
    std::string s(hexStr);
    bool negative = false;
    
    // Handle negative sign
    if (!s.empty() && s[0] == '-') {
        negative = true;
        s = s.substr(1);
    }

    // Handle 0x prefix
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s = s.substr(2);
    if (s.length() < 32)
        s.insert(0, 32 - s.length(), '0');

    std::string highPart = s.substr(0, 16);
    std::string lowPart = s.substr(16, 16);

    uint64_t uHigh = std::stoull(highPart, nullptr, 16);
    uint64_t uLow = std::stoull(lowPart, nullptr, 16);

    this->high = (int64_t)uHigh;
    this->low = uLow;

    if (negative) {
        // Perform 128-bit two's complement negation
        low = ~low + 1;
        high = ~high + (low == 0 ? 1 : 0);
    }
}
