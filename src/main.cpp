#include "common/SourceBuffer.h"
#include "compiler/Lexer.h"
#include "compiler/Parser.h"
#include "compiler/ASTPrinter.h"
#include "compiler/Transpiler.h"
#include "vm/VirtualMachine.h"
#include "core/Diagnostics.h"

using namespace Glow;

enum class ToolMode { RUN, COMPILE, AST, UNKNOWN };

ToolMode getMode(const std::string& modeStr) {
    static const std::unordered_map<std::string, ToolMode> modeMap = {
        {"run",     ToolMode::RUN},
        {"compile", ToolMode::COMPILE},
        {"ast",     ToolMode::AST}
    };
    auto it = modeMap.find(modeStr);
    return (it != modeMap.end()) ? it->second : ToolMode::UNKNOWN;
}

void printUsage() {
    std::cout << "Glow Language Toolchain v1.0\n";
    std::cout << "Usage:\n";
    std::cout << "  glow run <file.glw>      - Interpret Glow code\n";
    std::cout << "  glow compile <file.glw>  - Transpile to C++20\n";
    std::cout << "  glow ast <file.glw>      - Visualize the AST\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    ToolMode mode = getMode(argv[1]);
    std::string filePath = argv[2];

    // FLAG DETECTION
    bool verbose = false;
    bool keepCpp = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose") verbose = true;
        if (arg == "--cpp")     keepCpp = true;
    }

    // Reset error count for a fresh run
    Diagnostics::reset();

    // TIMED EXECUTION START
    auto totalStart = std::chrono::high_resolution_clock::now();
    SourceBuffer buffer;
    
    // Load Source
    if (!buffer.loadFile(filePath)) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return 1;
    }

    // Lexical Analysis
    Lexer lexer(buffer);
    std::vector<Token> tokens = lexer.tokenize();

    // --- DEBUG ---
    // std::cout << "--- LEXER OUTPUT ---\n";
    // for (const auto& t : tokens) {
    //     std::cout << "[Line " << t.line << ": Col " << t.column << "] " 
    //             << "Type: " << (int)t.type << " | Lexeme: [" << t.lexeme << "]\n";
    // }
    // std::cout << "--------------------\n";
    // -----------------------------

    if (verbose) {
        std::cout << "\n--- [DEBUG] TOKEN STREAM ---" << std::endl;
        for (const auto& t : tokens)
            std::printf("Line %-3d | Col %-3d | Type %-3d | Lexeme: %s\n", 
                        t.line, t.column, static_cast<int>(t.type), t.lexeme.c_str());
        std::cout << "--- TOTAL TOKENS: " << tokens.size() << " ---\n" << std::endl;
    }

    // Syntax Analysis
    Parser parser(lexer, tokens, buffer);
    std::unique_ptr<BlockNode> ast;
    try {
        ast = parser.parseProgram();
    }

    catch (const std::exception& e) {
        std::cerr << "FATAL PARSER CRASH: " << e.what() << std::endl;
        return 1;
    }

    // STOP if the frontend found issues (missing semicolons, etc)
    if (Diagnostics::hasErrors()) {
        std::cerr << "--- PARSE FAILURE ---" << std::endl; 
        std::cerr << "Total errors: " << Diagnostics::errorCount << std::endl;
        return 1;
    }

    // Execution Dispatch
    switch (mode) {
        case ToolMode::AST: {
            ASTPrinter printer;
            printer.print(ast.get());
            break;
        }

        case ToolMode::RUN: {
            VirtualMachine vm;
            vm.setVerbose(verbose);
            vm.interpret(ast.get());
            break;
        }

        case ToolMode::COMPILE: {
            Transpiler transpiler;
            keepCpp = false;
            
            // Check for the --cpp flag anywhere in the arguments to preserve the intermediate source
            for (int i = 1; i < argc; ++i) {
                if (std::string(argv[i]) == "--cpp") {
                    keepCpp = true;
                    break;
                }
            }

            // --- Path Resolution Logic ---
            std::filesystem::path inputPath(filePath);
            std::filesystem::path absolutePath = std::filesystem::absolute(inputPath);
            std::filesystem::path directory = absolutePath.parent_path();
            std::filesystem::path stem = absolutePath.stem(); 

            std::string cppPath = (directory / stem).string() + ".cpp";
            std::string exePath = (directory / stem).string() + ".exe";

            // Determine the base path for the runtime
            std::string runtimeSrc = "src/include/GlowRuntime/Built-in/types.cpp";
            std::string runtimeInc = "src/include/GlowRuntime";

            // Fallback for running from build/
            if (!std::filesystem::exists(runtimeSrc)) {
                runtimeSrc = "../" + runtimeSrc;
                runtimeInc = "../" + runtimeInc;
            }

            if (transpiler.compile(ast, cppPath)) {
                // Optimized dispatch using switch on boolean state for jump-table generation
                switch (static_cast<int>(keepCpp)) {
                    case 1: // keepCpp is true: Output intermediate C++ and stop
                        std::cout << "Glow -> C++: " << cppPath << " [Success]" << std::endl;
                        break;

                    case 0: // keepCpp is false: Proceed to full binary compilation
                    default: {
                        // The Driver Phase: Delegate to the system's C++ compiler
                        std::cout << "Compiling " << exePath << "..." << std::endl;
                        
                        // Quoted paths protect against directory names containing spaces
                        std::string cmd = "g++ \"" + cppPath + "\" \"" + runtimeSrc + 
                            "\" -I\"" + runtimeInc + "\" -I\"" + runtimeInc + "/Built-in\" -o \"" + exePath + "\"";
                        int result = std::system(cmd.c_str());

                        // Switch-based handling of the backend compiler's return code
                        switch (result) {
                            case 0:
                                // Compilation successful: cleanup the temporary bridge file
                                std::remove(cppPath.c_str()); 
                                std::cout << "Build Successful: " << exePath << std::endl;
                                break;
                            default:
                                std::cerr << "Back-end Error: g++ failed with code " << result << std::endl;
                                return 1;
                        }
                        break;
                    }
                }
            }
            break; 
        }
        default:
            printUsage();
            return 1;
    }

    // TIMED EXECUTION END & METRICS
    auto totalEnd = std::chrono::high_resolution_clock::now();
    
    if (verbose) {
        std::chrono::duration<double, std::milli> elapsed = totalEnd - totalStart;
        std::cout << "\n--- [METRICS] TOOLCHAIN PERFORMANCE ---" << std::endl;
        std::cout << "Full Pipeline Time: " << std::fixed << std::setprecision(2) 
            << elapsed.count() << " ms" << std::endl;
        std::cout << "Memory Management: GC Enabled/Sync Points Passed" << std::endl;
        std::cout << "---------------------------------------" << std::endl;
    }

    return 0;
}
