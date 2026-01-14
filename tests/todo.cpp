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
    std::string tasks[10];
    int32_t task_count = 0;
    std::string cmd = "";
    bool running = true;
    std::cout << "-------------------------------" << '\n';
    std::cout << "   GLOW TASK SENTINEL v1.0" << '\n';
    std::cout << "-------------------------------" << '\n';
    std::cout << "Commands: [add] [list] [clear] [exit]" << '\n';
    while (running)  {
        std::cout << '\n' << "prompt: ";
        std::cin >> cmd;
        if (cmd == "add")
            if (task_count < 10) {
                std::cout << ">> Task Description: ";
                std::cin >> tasks[task_count];
                task_count++;
                std::cout << glowAdd({ "[OK] Task indexed at slot ", glow_to_string(task_count) }) << '\n';
            } else 
                std::cout << "[ERROR] Task buffer overflow (Max 10)." << '\n'; else if (cmd == "list")
            if (task_count == 0)
                std::cout << "[INFO] Task buffer is empty." << '\n'; else  {
                std::cout << "--- ACTIVE TASKS ---" << '\n';
                int32_t i = 0;
                while (i < task_count)  {
                    std::cout << glowAdd({ " [", glowFormat({ "", glow_to_string(i + 1), "" }), "] " }) + tasks[i] << '\n';
                    i++;
                }
                std::cout << "--------------------" << '\n';
            } else if (cmd == "clear") {
            task_count = 0;
            std::cout << "[SYSTEM] Task buffer cleared." << '\n';
        } else if (cmd == "exit") {
            std::cout << "[SHUTDOWN] Exiting Sentinel..." << '\n';
            running = false;
        } else 
            if (cmd != "")
                std::cout << glowAdd({ "[?] Unknown command: '", glow_to_string(cmd), "'" }) << '\n';
    }
    return 0;
}
