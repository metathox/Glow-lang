#pragma once
#include <vector>
#include <string>
#include <memory>
#include "common/Token.h"

namespace Glow {
    enum class LoopVariant { FOR, WHILE, RANGE };

    // Forward Declarations
    struct BlockNode;
    struct IfNode;
    struct ElifBranch;
    struct ChainNode;
    class StreamNode;
    class StringInterpolationNode;
    class LiteralExprNode;
    class BinaryExprNode;
    class UnaryExprNode;
    class VariableExprNode;
    class TryCatchNode;
    class CallExprNode;
    class IndexExprNode;
    class CollectionLiteralNode;
    class VarDeclNode;
    class ReturnNode;
    class LoopNode;
    class SwitchCaseNode;
    class SwitchNode;
    class FunctionNode;
    class QualifiedIDNode;
    class AssignmentNode;
    class EnumNode;
    class UnitNode;
    class UnitMemberNode;
    class MemberAccessNode;
    class InitNode;
    class KillNode;
    class DeleteNode;
    class GCToggleNode;
    class TernaryExprNode;
    class CastExprNode;

    // The Visitor Interface
    class Visitor {
    public:
        virtual ~Visitor() = default;
        virtual void visitBlock(BlockNode* node) = 0;
        virtual void visitStream(StreamNode* node) = 0;
        virtual void visitChain(ChainNode* node) = 0;
        virtual void visitStringInterpolation(StringInterpolationNode* node) = 0;
        virtual void visitLiteralExpr(LiteralExprNode* node) = 0;
        virtual void visitBinaryExpr(BinaryExprNode* node) = 0;
        virtual void visitUnaryExpr(UnaryExprNode* node) = 0;
        virtual void visitVariableExpr(VariableExprNode* node) = 0;
        virtual void visitTryCatch(TryCatchNode* node) = 0;
        virtual void visitCallExpr(CallExprNode* node) = 0;
        virtual void visitIndexExpr(IndexExprNode* node) = 0;
        virtual void visitCollectionLiteral(CollectionLiteralNode* node) = 0;
        virtual void visitVarDecl(VarDeclNode* node) = 0;
        virtual void visitReturn(ReturnNode* node) = 0;
        virtual void visitLoop(LoopNode* node) = 0;
        virtual void visitIf(IfNode* node) = 0;
        virtual void visitSwitchCase(SwitchCaseNode* node) = 0;
        virtual void visitSwitch(SwitchNode* node) = 0;
        virtual void visitFunction(FunctionNode* node) = 0;
        virtual void visitQualifiedID(QualifiedIDNode* node) = 0;
        virtual void visitAssignment(AssignmentNode* node) = 0;
        virtual void visitEnum(EnumNode* node) = 0;
        virtual void visitUnit(UnitNode* node) = 0;
        virtual void visitUnitMember(UnitMemberNode* node) = 0;
        virtual void visitMemberAccess(MemberAccessNode* node) = 0;
        virtual void visitInit(InitNode* node) = 0;
        virtual void visitKill(KillNode* node) = 0;
        virtual void visitDelete(DeleteNode* node) = 0;
        virtual void visitGCToggle(GCToggleNode* node) = 0;
        virtual void visitTernaryExpr(TernaryExprNode* node) = 0;
        virtual void visitCastExpr(CastExprNode* node) = 0;
    };

    // Node Type Enum
    enum class ASTNodeType {
        BLOCK, VAR_DECL, IF, LOOP, SWITCH, SWITCH_CASE, TRY_CATCH,
        FUNCTION, QUALIFIED_ID, ASSIGNMENT, COLLECTION,
        BINARY, UNARY, TERNARY, VARIABLE, INDEXING, STREAM, CHAIN,
        LITERAL, STRING_INTERPOLATION, CAST, RETURN, CALL, ENUM,
        UNIT, UNIT_MEMBER, MEMBER_ACCESS, INIT, KILL, DELETE,
        GC_TOGGLE,
    };

    // Base Node Class
    class ASTNode {
    public:
        ASTNodeType nodeType; 
        virtual ~ASTNode() = default;
        virtual void accept(Visitor* v) = 0; // Pure virtual for Visitor Pattern

    protected:
        ASTNode(ASTNodeType type) : nodeType(type) {}
    };

    // --- Node Implementations ---

    struct BlockNode : public ASTNode {
        std::vector<std::unique_ptr<ASTNode>> statements;
        BlockNode() : ASTNode(ASTNodeType::BLOCK) {}
        void accept(Visitor* v) override { v->visitBlock(this); }
    };

    struct ChainNode : public ASTNode {
        std::vector<std::unique_ptr<ASTNode>> links;
        ChainNode() : ASTNode(ASTNodeType::CHAIN) {}
        void accept(Visitor* v) override { v->visitChain(this); }
    };

    class LiteralExprNode : public ASTNode {
    public:
        LiteralExprNode() : ASTNode(ASTNodeType::LITERAL) {}
        Token value; 
        void accept(Visitor* v) override { v->visitLiteralExpr(this); }
        bool isLargeInteger() const {
            return value.type == TokenType::I128_LIT || value.type == TokenType::UI128_LIT;
        }
    };

    class BinaryExprNode : public ASTNode {
    public:
        BinaryExprNode() : ASTNode(ASTNodeType::BINARY) {}
        std::unique_ptr<ASTNode> left;
        std::unique_ptr<ASTNode> right;
        Token op;
        void accept(Visitor* v) override { v->visitBinaryExpr(this); }
    };

    class UnaryExprNode : public ASTNode {
    public:
        UnaryExprNode() : ASTNode(ASTNodeType::UNARY) {}
        Token op;
        std::unique_ptr<ASTNode> right;
        bool isPostfix;
        void accept(Visitor* v) override { v->visitUnaryExpr(this); }
    };

    class VariableExprNode : public ASTNode {
    public:
        VariableExprNode() : ASTNode(ASTNodeType::VARIABLE) {}
        Token name;
        void accept(Visitor* v) override { v->visitVariableExpr(this); }
    };

    class TryCatchNode : public ASTNode {
    public:
        TryCatchNode() : ASTNode(ASTNodeType::TRY_CATCH) {}
        std::unique_ptr<BlockNode> tryBody;
        std::unique_ptr<BlockNode> catchBody;
        void accept(Visitor* v) override { v->visitTryCatch(this); }
    };

    class CallExprNode : public ASTNode {
    public:
        CallExprNode() : ASTNode(ASTNodeType::CALL) {}
        Token op;
        std::unique_ptr<ASTNode> callee;
        std::vector<std::unique_ptr<ASTNode>> arguments;
        void accept(Visitor* v) override { v->visitCallExpr(this); }
    };

    class IndexExprNode : public ASTNode {
    public:
        IndexExprNode() : ASTNode(ASTNodeType::INDEXING) {}
        std::unique_ptr<ASTNode> target;
        std::unique_ptr<ASTNode> index; 
        void accept(Visitor* v) override { v->visitIndexExpr(this); }
    };

    class CollectionLiteralNode : public ASTNode {
    public:
        CollectionLiteralNode() : ASTNode(ASTNodeType::COLLECTION) {}
        std::vector<std::unique_ptr<ASTNode>> elements;
        void accept(Visitor* v) override { v->visitCollectionLiteral(this); }
    };

    class VarDeclNode : public ASTNode {
    public:
        VarDeclNode() : ASTNode(ASTNodeType::VAR_DECL) {}
        Token type;
        Token name;
        std::unique_ptr<ASTNode> initializer;
        bool isArray = false;
        std::unique_ptr<ASTNode> arraySize;
        void accept(Visitor* v) override { v->visitVarDecl(this); }
    };

    class ReturnNode : public ASTNode {
    public:
        ReturnNode() : ASTNode(ASTNodeType::RETURN) {}
        std::unique_ptr<ASTNode> value;
        void accept(Visitor* v) override { v->visitReturn(this); }
    };

    struct ElifBranch {
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<BlockNode> body;
    };

    struct IfNode : public ASTNode {
        IfNode() : ASTNode(ASTNodeType::IF) {}

        // initial 'if'
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<BlockNode> ifBody;

        // Any number of 'elif' blocks
        std::vector<ElifBranch> elifBranches;

        // optional 'else'
        std::unique_ptr<BlockNode> elseBody;

        void accept(Visitor* v) override { v->visitIf(this); }
    };

    class LoopNode : public ASTNode {
    public:
        LoopNode() : ASTNode(ASTNodeType::LOOP) {}
        LoopVariant variant = LoopVariant::FOR; // Default
        std::unique_ptr<ASTNode> init;
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<ASTNode> increment;
        std::unique_ptr<BlockNode> body;
        void accept(Visitor* v) override { v->visitLoop(this); }
    };

    class SwitchCaseNode : public ASTNode {
    public:
        SwitchCaseNode() : ASTNode(ASTNodeType::SWITCH_CASE) {}
        bool isDefault = false;
        std::unique_ptr<ASTNode> matchValue; 
        std::unique_ptr<BlockNode> body;
        void accept(Visitor* v) override { v->visitSwitchCase(this); }
    };

    class SwitchNode : public ASTNode {
    public:
        SwitchNode() : ASTNode(ASTNodeType::SWITCH) {}
        std::unique_ptr<ASTNode> condition;
        std::vector<std::unique_ptr<SwitchCaseNode>> cases;
        void accept(Visitor* v) override { v->visitSwitch(this); }
    };

    class FunctionNode : public ASTNode {
    public:
        FunctionNode() : ASTNode(ASTNodeType::FUNCTION) {}
        Token name;
        bool isEntry = false;
        Token returnType;
        std::vector<std::unique_ptr<VarDeclNode>> parameters;
        std::unique_ptr<BlockNode> body;
        void accept(Visitor* v) override { v->visitFunction(this); }
    };

    // --- Structural Nodes ---

    class EnumNode : public ASTNode {
    public:
        EnumNode() : ASTNode(ASTNodeType::ENUM) {}
        Token name;
        Token underlyingType; // supports str, f32, etc. via enum Name -> type:
        std::vector<std::pair<Token, std::unique_ptr<ASTNode>>> entries;
        void accept(Visitor* v) override { v->visitEnum(this); }
    };

    class UnitNode : public ASTNode {
    public:
        UnitNode() : ASTNode(ASTNodeType::UNIT) {}
        std::unique_ptr<BlockNode> body;
        Token name;
        Token parentName;   // inheritance
        std::vector<std::unique_ptr<UnitMemberNode>> members;
        void accept(Visitor* v) override { v->visitUnit(this); }
    };

    class UnitMemberNode : public ASTNode {
    public:
        UnitMemberNode() : ASTNode(ASTNodeType::UNIT_MEMBER) {}
        Token visibility; 
        std::unique_ptr<ASTNode> member; 
        void accept(Visitor* v) override { v->visitUnitMember(this); }
    };


    class MemberAccessNode : public ASTNode {
    public:
        MemberAccessNode() : ASTNode(ASTNodeType::MEMBER_ACCESS) {}
        std::unique_ptr<ASTNode> structure; // The object or pointer (e.g., 'player')
        Token member;                       // The field or method name (e.g., 'health')
        Token op;                           // The operator uses (DOT '.' or ARROW '->')
        void accept(Visitor* v) override { v->visitMemberAccess(this); }
    };

    class InitNode : public ASTNode {
    public:
        InitNode() : ASTNode(ASTNodeType::INIT) {}
        // Maps member name to initializer expression: init() : _val(x)
        std::vector<std::pair<Token, std::unique_ptr<ASTNode>>> initializers;
        std::vector<std::unique_ptr<VarDeclNode>> parameters;
        std::unique_ptr<BlockNode> body;
        void accept(Visitor* v) override { v->visitInit(this); }
    };

    class KillNode : public ASTNode {
    public:
        KillNode() : ASTNode(ASTNodeType::KILL) {}
        std::unique_ptr<BlockNode> body;
        void accept(Visitor* v) override { v->visitKill(this); }
    };

    class DeleteNode : public ASTNode {
    public:
        DeleteNode() : ASTNode(ASTNodeType::DELETE) {}
        // The target to be removed (usually a VariableExprNode)
        std::unique_ptr<ASTNode> target;
        void accept(Visitor* v) override { v->visitDelete(this); }
    };

    class GCToggleNode : public ASTNode {
    public:
        GCToggleNode() : ASTNode(ASTNodeType::GC_TOGGLE) {}
        bool enable; // true for ON, false for OFF
        void accept(Visitor* v) override { v->visitGCToggle(this); }
    };

    class QualifiedIDNode : public ASTNode {
    public:
        QualifiedIDNode() : ASTNode(ASTNodeType::QUALIFIED_ID) {}
        std::vector<Token> path;
        void accept(Visitor* v) override { v->visitQualifiedID(this); }
    };

    class AssignmentNode : public ASTNode {
    public:
        AssignmentNode() : ASTNode(ASTNodeType::ASSIGNMENT) {}
        Token op;
        std::unique_ptr<ASTNode> target;
        std::unique_ptr<ASTNode> value;
        void accept(Visitor* v) override { v->visitAssignment(this); }
    };

    class StreamNode : public ASTNode {
    public:
        StreamNode() : ASTNode(ASTNodeType::STREAM) {}
        std::unique_ptr<ASTNode> value;
        Token streamType; 
        Token op; 
        void accept(Visitor* v) override { v->visitStream(this); }
    };

    class StringInterpolationNode : public ASTNode {
    public:
        // parts[0] is always a LiteralExprNode (the starting text or empty string)
        // parts[1] is an expression, and its safety is stored in isSafe[0]
        StringInterpolationNode() : ASTNode(ASTNodeType::STRING_INTERPOLATION) {}
        std::vector<bool> isSafe;
        std::vector<std::unique_ptr<ASTNode>> parts;
        void accept(Visitor* v) override { v->visitStringInterpolation(this); }
    };

    // --- Logic & Casting ---

    class TernaryExprNode : public ASTNode {
    public:
        TernaryExprNode() : ASTNode(ASTNodeType::TERNARY) {}
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<ASTNode> trueExpr;
        std::unique_ptr<ASTNode> falseExpr;
        void accept(Visitor* v) override { v->visitTernaryExpr(this); }
    };

    class CastExprNode : public ASTNode {
    public:
        CastExprNode() : ASTNode(ASTNodeType::CAST) {}
        Token targetType; // The type inside < >
        std::unique_ptr<ASTNode> expression;
        void accept(Visitor* v) override { v->visitCastExpr(this); }
    };
}
