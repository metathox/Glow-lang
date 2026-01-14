#pragma once
#include <vector>
#include <unordered_map>
#include "compiler/ASTNodes.h"
#include "compiler/SymbolTable.h"
#include "vm/Value.h"

namespace Glow {

    class VirtualMachine {
    public:
        VirtualMachine();
        void interpret(ASTNode* root);
        void setVerbose(bool enabled) { verbose = enabled; }

        // --- Core Engine Helpers (Public) ---
        void collectGarbage();
        void setGCState(bool state) { GCEnabled = state; }

    private:
        bool verbose = false;
        SymbolTable globals;

        // --- Statement Execution ---
        void execute(ASTNode* node);
        void executeBlock(BlockNode* node);
        void executeVarDecl(VarDeclNode* node);
        void executeLoop(LoopNode* node);
        void executeIf(IfNode* node);
        void executeStream(StreamNode* node);
        void executeChain(ChainNode* node);
        void executeSwitch(SwitchNode* node);
        void executeAssignment(AssignmentNode* node);
        void executeTryCatch(TryCatchNode* node);
        void executePanic(const std::string& message);
        void executeFunction(FunctionNode* node);
        void executeUnit(UnitNode* node);
        void executeEnum(EnumNode* node);
        void executeDelete(DeleteNode* node);
        void executeKill(KillNode* node);
        void executeGCToggle(GCToggleNode* node);

        // --- Expression Evaluation ---
        Value evaluate(ASTNode* node);
        Value evaluateBinary(BinaryExprNode* node);
        Value evaluateUnary(UnaryExprNode* node);
        Value evaluateTernary(TernaryExprNode* node);
        Value evaluateLiteral(LiteralExprNode* node);
        Value evaluateIndexing(IndexExprNode* node);
        Value evaluateCollection(CollectionLiteralNode* node);
        Value evaluateStringInterpolation(StringInterpolationNode* node);
        Value evaluateCall(CallExprNode* node);
        Value evaluateQualifiedID(QualifiedIDNode* node);
        Value evaluateMemberAccess(MemberAccessNode* node);
        Value evaluateCast(CastExprNode* node);
        
        // --- Core Engine Helpers (Private) ---
        Value applyOperator(Value left, Token op, Value right);
        Value lookUpVariable(const Token& name);

        bool typeMatch(TokenType expected, ValueType actual);
        bool isTruthy(const Value& v);
        bool GCEnabled = true;

        void processGlowFile(const std::string& path);
        void loadModule(const std::string& moduleName);
        std::string moduleSearchPath = "src/include/GlowModules/";
        std::vector<std::unique_ptr<GlowArray>> arrayPool;
        std::vector<std::unique_ptr<std::string>> addressPool;
        std::unordered_map<void*, std::string> instanceRegistry;
    };
}
