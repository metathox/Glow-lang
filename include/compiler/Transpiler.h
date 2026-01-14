#pragma once
#include "compiler/ASTNodes.h"
#include "compiler/SymbolTable.h"
#include <functional>
#include <fstream>
#include <string>

namespace Glow {
    class Transpiler {
    public:
        // Converts a .glw AST into a standalone .cpp file
        bool compile(std::unique_ptr<BlockNode>& root, const std::string& outPath);

    private:
        SymbolTable symbols;
        void write(ASTNode* node, std::ofstream& out, int indent = 0);
        void writeIndent(std::ofstream& out, int indent);

        // Helper to wrap 128-bit literals
        void handlePotential128(TokenType targetType, ASTNode* valueNode, std::ofstream& out, int indent);
        
        // Maps Glow types (i32, i128, etc) to C++ types
        std::string mapType(TokenType type);

        // store the class name so that init(...) will work for any class (unit)
        std::string currentUnitName;
        std::string currentParentName;
        
        // Maps Glow bitwise operators to C++ expressions
        void writeBitwise(BinaryExprNode* node, std::ofstream& out, int indent);
    };
}
