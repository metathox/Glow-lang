#pragma once
#include <iostream>
#include <string>
#include <cstdlib>
#include <atomic>
#include <vector>
#include <type_traits>

// --- Glow Runtime: System Logic ---
inline void glow_panic(const std::string& msg) {
    std::cerr << "\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    std::cerr << "  GLOW RUNTIME PANIC\n";
    std::cerr << "  MSG: " << msg << "\n";
    std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n" << std::endl;
    std::exit(1);
}

// --- Glow Runtime: Memory Management Hints ---
inline void glow_gc_sync() {
    // Native C++ uses RAII, but we keep this for consistency with the VM's GC logic
    std::atomic_thread_fence(std::memory_order_seq_cst);
}

// --- Glow Runtime: String Conversion Engine ---
template<typename T>
std::string glow_to_string(T val) {
    // Priority 1: Direct String types (Fastest)
    if constexpr (std::is_convertible_v<T, std::string>)
        return std::string(val);
    // Priority 2: Custom Systems Types
    else if constexpr (std::is_same_v<T, i128>)
        return i128_to_string(val);
    else if constexpr (std::is_same_v<T, ui128>)
        return ui128_to_string(val);
    // Priority 3: Boolean Formatting
    else if constexpr (std::is_same_v<T, bool>)
        return val ? "true" : "false";
    // Priority 4: All Standard Numbers (i32, f64, etc.)
    else if constexpr (std::is_arithmetic_v<T>)
        return std::to_string(val);
    // Priority 5: System Fallback
    else
        return "<obj>";
}

// --- Glow Runtime: Interpolation Engine ---
// Pre-calculates total size to prevent reallocation overhead.
inline std::string glowFormat(std::initializer_list<std::string> parts) {
    size_t total_size = 0;
    for (const auto& s : parts) total_size += s.length();
    
    std::string result;
    result.reserve(total_size);

    for (const auto& s : parts) result += s;
    return result;
}

// --- Glow Runtime: Unified Addition / Concatenation ---
template<typename T, typename U>
auto glowAdd(T a, U b) {
    // Optimized Path: If both are numbers, do raw machine addition
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>)
        return a + b; 
    // Mixed Path: If one is a string, perform concatenation
    else
        return glow_to_string(a) + glow_to_string(b);
}

inline std::string glowAdd(std::initializer_list<std::string> parts) {
    return glowFormat(parts);
}
