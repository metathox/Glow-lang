#pragma once
#include "compiler/ASTNodes.h"

namespace Glow {

    class ASTPrinter {
    public:
        void print(ASTNode* node, int indent = 0);

    private:
        void printIndent(int indent);
    };
}
