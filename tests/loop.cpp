#include "types.hpp"
#include "runtime.hpp"
#include <atomic>
#include <iostream>
#include <cstdint>
#include <string>
#include <cmath>
#include <cstdio>
using namespace std;

int main() {
    int32_t x = 10;
    int32_t y = 20;
    int32_t result = x + y;
    std::cout << "\nx = " << x << '\n';
    std::cout << "y = " << y << '\n';
    std::cout << "\nx + y result: " << x + y << '\n';
    int32_t i = 10;
    std::cout << '\n' << "While loop:" << '\n';
    while (i >= 0) 
        std::cout << i-- << '\n';
    i = 0;
    int32_t arr[] = { 2, 7, 3 };
    std::cout << '\n' << "Range loop:" << '\n';
    for (int32_t j : arr)  {
        std::cout << glowAdd({ "arr[", glow_to_string(i), "]: " }) << j << '\n';
        ++i;
    }
    std::cout << '\n' << "For loop:" << '\n';
    for (int32_t k = 0; k <= 10; k++) 
        std::cout << k << '\n';
    std::cout << '\n';
    return 0;
}
