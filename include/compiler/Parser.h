#pragma once
#include <vector>
#include <memory>
#include <string>
#include <set>
#include <filesystem>
#include <unordered_map>
#include "common/Token.h"
#include "common/SourceBuffer.h"
#include "compiler/ASTNodes.h"
#include "core/Diagnostics.h"

namespace Glow {
    // Forward declarations
    class Lexer;

    struct ParseError : public std::runtime_error {
        ParseError() : std::runtime_error("") {}
    };

    class Parser {
    public:
        explicit Parser(Lexer& lexer, std::vector<Token>& tokens, const SourceBuffer& buffer);
        std::unique_ptr<BlockNode> parseProgram();

    private:
        // Buffer & Lexer reference
        const SourceBuffer& sourceBuffer;
        Lexer& lexer;

        // --- Navigation Helpers ---
        Token advance();
        Token peek() const;
        Token previous() const;
        Token consume(TokenType type, const std::string& message);
        Token consumeIdentifier(const std::string& msg);
        void consumeTerminator(const std::string& message);
        bool check(TokenType type) const;
        bool checkNext(TokenType type) const;
        bool match(const std::vector<TokenType>& types);
        bool lookaheadCheck(int distance, TokenType type) const;
        bool isAtEnd() const;
        ParseError error(const Token& token, const std::string& message);

        // --- Private Utility Helpers ---
        LoopVariant getLoopVariant(); 
        bool isType(TokenType type) const;
        bool isAssignable(ASTNode* node);
        void synchronize();
        void resolveAlias(Token& token);
        void registerAlias(const std::string& alias, const std::vector<Token>& originalPath);

        // --- Grammar Rules ---
        std::unique_ptr<ASTNode> handleUse();
        std::unique_ptr<ASTNode> declaration();
        std::unique_ptr<ASTNode> unitDeclaration();
        std::unique_ptr<ASTNode> functionDeclaration();
        std::unique_ptr<ASTNode> varDeclaration(bool expectSemicolon = true);
        std::unique_ptr<ASTNode> enumDeclaration();
        std::unique_ptr<ASTNode> ternary();
        std::unique_ptr<ASTNode> cast();
        std::unique_ptr<ASTNode> lifecycleMethod();
        
        // --- Statements ---
        std::unique_ptr<ASTNode> statement();
        std::unique_ptr<BlockNode> block();
        std::unique_ptr<ASTNode> loopStatement();
        std::unique_ptr<ASTNode> ifStatement();
        std::unique_ptr<ASTNode> switchStatement();
        std::unique_ptr<ASTNode> tryCatchStatement();
        std::unique_ptr<ASTNode> streamStatement();
        std::unique_ptr<ASTNode> returnStatement();
        std::unique_ptr<ASTNode> panicStatement();
        std::unique_ptr<ASTNode> parseDeleteStatement();
        std::unique_ptr<ASTNode> GCToggleStatement(bool enable);

        // --- Expressions ---
        std::unique_ptr<ASTNode> expression();
        std::unique_ptr<ASTNode> assignment();
        std::unique_ptr<ASTNode> logicalOr();
        std::unique_ptr<ASTNode> logicalAnd();
        std::unique_ptr<ASTNode> bitwise();
        std::unique_ptr<ASTNode> equality();
        std::unique_ptr<ASTNode> comparison();
        std::unique_ptr<ASTNode> term();
        std::unique_ptr<ASTNode> factor();
        std::unique_ptr<ASTNode> power();
        std::unique_ptr<ASTNode> unary();
        std::unique_ptr<ASTNode> postfix();
        std::unique_ptr<ASTNode> call();
        std::unique_ptr<ASTNode> finishCall(std::unique_ptr<ASTNode> callee);
        std::unique_ptr<ASTNode> primary();
        std::unique_ptr<ASTNode> collectionLiteral(TokenType closer);

        // --- Systems Data Members ---
        std::vector<Token> tokens;
        int current = 0;
        std::unordered_map<std::string, std::string> aliases;
        std::vector<std::unique_ptr<SourceBuffer>> moduleBuffers;
        
        // Thread-safe Module Tracking
        std::set<std::string> loadingStack;
        std::set<std::string> loadedModules;
    };
}
