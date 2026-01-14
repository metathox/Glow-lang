#include "compiler/Parser.h"
#include "compiler/Lexer.h"
#include "core/Diagnostics.h"
#include <stdexcept>
#include <set>

namespace Glow {

    Parser::Parser(Lexer& lexer, std::vector<Token>& tokens, const SourceBuffer& buff)
     : lexer(lexer), tokens(tokens), sourceBuffer(buff), current(0) {}

    std::unique_ptr<BlockNode> Parser::parseProgram() {
        auto program = std::make_unique<BlockNode>();
        while (!isAtEnd()) {
            auto decl = declaration();
            if (decl) program->statements.push_back(std::move(decl));
        }
        return program;
    }

    // --- Declarations & Initialization ---
    std::unique_ptr<ASTNode> Parser::declaration() {
        if (check(TokenType::END) || check(TokenType::ELSE) || check(TokenType::CATCH)) return nullptr; 

        try {
            if (match({TokenType::USE})) return handleUse();

            // Check for specific keywords first
            switch (peek().type) {
                case TokenType::ENTRY:
                case TokenType::FN:
                    advance(); 
                    return functionDeclaration();
                case TokenType::UNIT:
                    advance();
                    return unitDeclaration();
            }

            // We check if the current token is a type (i32, ui128, etc.)
            if (isType(peek().type))
                // We pass 'true' because top-level/function vars usually need ';'
                // unitDeclaration will manually call varDeclaration(false) for members
                return varDeclaration(true); 

            // if it's not a declaration, it must be a statement
            return statement();
        }
        catch (const ParseError& e) {
            synchronize(); 
            return nullptr;
        }
    }

    std::unique_ptr<ASTNode> Parser::varDeclaration(bool expectSemicolon) {
        Token type = advance(); 
        Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");
        
        // Flags & Arrays
        bool isPointer  = match({TokenType::STAR});
        bool isUnique   = match({TokenType::UNIQUE});
        bool isVolatile = match({TokenType::VOL});

        bool isArray = false;
        std::unique_ptr<ASTNode> arraySize = nullptr;
        if (match({TokenType::L_BRACK})) {
            isArray = true;
            if (!check(TokenType::R_BRACK)) arraySize = expression();
            consume(TokenType::R_BRACK, "Expect ']' after array size.");
        }

        std::unique_ptr<ASTNode> initializer = nullptr;
        if (match({TokenType::EQUAL})) initializer = expression();

        // Don't force a semicolon if we hit 'end' or a newline
        if (expectSemicolon) {
            bool hasSemicolon = match({TokenType::SEMICOLON});
            bool isNewline    = (peek().line > name.line);
            bool isEndOfBlock = check(TokenType::END) || isAtEnd();

            // If it's NOT a semicolon AND NOT a newline AND NOT the end of the file...
            if (!hasSemicolon && !isNewline && !isEndOfBlock) {
                Diagnostics::report(peek().line, peek().column, 
                    "Missing semicolon after declaration of '" + name.lexeme + "'.");
                // We don't 'throw' here, so the parser can try to keep going!
            }
        }

        auto node = std::make_unique<VarDeclNode>();
        node->type = type; node->name = name; 
        node->isArray = isArray; node->arraySize = std::move(arraySize); 
        node->initializer = std::move(initializer);
        return node;
    }

    // --- Statements (Colon-to-End Style) ---
    std::unique_ptr<ASTNode> Parser::statement() {
        switch (peek().type) {
            case TokenType::IF:     advance(); return ifStatement();
            case TokenType::RET:    advance(); return returnStatement();
            case TokenType::LOOP:   advance(); return loopStatement();
            case TokenType::SWITCH: advance(); return switchStatement();
            case TokenType::TRY:    advance(); return tryCatchStatement();
            case TokenType::END:               return nullptr;
            
            // calls panicStatement() for panic("msg") syntax
            case TokenType::PANIC:  advance(); return panicStatement();

            // --- Memory Management Statements ---
            case TokenType::DEL:    
                advance(); return parseDeleteStatement();
            case TokenType::GC_ON:  
                advance(); return GCToggleStatement(true);
            case TokenType::GC_OFF: 
                advance(); return GCToggleStatement(false);

            case TokenType::IDENTIFIER:
                // Check for built-in streams without losing the identifier for assignments
                if (peek().lexeme == "ostream" || peek().lexeme == "istream") {
                    advance();
                    return streamStatement();
                }
                break;
                
            default:
                break;
        }

        // Fallback: This handles assignments, function calls, etc.
        auto expr = expression();
        consume(TokenType::SEMICOLON, "Expect ';' after expression.");
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::ifStatement() {
        auto node = std::make_unique<IfNode>();

        // Initial 'if'
        consume(TokenType::L_PAREN, "Expect '(' after 'if'");
        node->condition = expression();
        consume(TokenType::R_PAREN, "Expect ')' after condition");
        node->ifBody = block(); // block() handles the ':'

        // Multiple 'elif' blocks
        while (match({TokenType::ELIF})) {
            ElifBranch branch;
            consume(TokenType::L_PAREN, "Expect '(' after 'elif'");
            branch.condition = expression();
            consume(TokenType::R_PAREN, "Expect ')' after elif condition");
            branch.body = block();
            node->elifBranches.push_back(std::move(branch));
        }

        // Optional 'else'
        if (match({TokenType::ELSE}))
            node->elseBody = block();

        consume(TokenType::END, "Expect 'end' after if statement");
        return node;
    }

    std::unique_ptr<ASTNode> Parser::returnStatement() {
        auto node = std::make_unique<ReturnNode>();
        if (!check(TokenType::SEMICOLON)) {
            node->value = expression();
        }
        consume(TokenType::SEMICOLON, "Expect ';' after return.");
        return node;
    }

    std::unique_ptr<ASTNode> Parser::panicStatement() {
        Token op = previous(); // The 'panic' keyword

        consume(TokenType::L_PAREN, "Expect '(' after 'panic'.");
        auto message = expression();
        consume(TokenType::R_PAREN, "Expect ')' after panic message.");
        consume(TokenType::SEMICOLON, "Expect ';' after panic statement.");

        // We reuse CallExprNode. The 'op' is the 'panic' token.
        auto node = std::make_unique<CallExprNode>();
        node->op = op;
        node->arguments.push_back(std::move(message));
        return node;
    }

    std::unique_ptr<ASTNode> Parser::loopStatement() {
        consume(TokenType::L_PAREN, "Expect '(' after 'loop'.");
        
        auto node = std::make_unique<LoopNode>();
        node->variant = getLoopVariant();

        switch (node->variant) {
            case LoopVariant::RANGE: {
                // Syntax: loop(type name : collection)
                if (isType(peek().type))
                    node->init = varDeclaration(false);
                else
                    node->init = expression();
                
                consume(TokenType::COLON, "Expect ':' in range loop.");
                node->condition = expression();
                break;
            }

            case LoopVariant::WHILE: {
                if (!check(TokenType::R_PAREN))
                    node->condition = expression();
                break;
            }

            case LoopVariant::FOR: {
                // Init (Optional: i32 i = 0 OR i = 0)
                if (!check(TokenType::COMMA)) {
                    if (isType(peek().type))
                        node->init = varDeclaration(false);
                    else
                        node->init = expression();
                }
                consume(TokenType::COMMA, "Expect ',' after loop init.");

                // Condition (Optional)
                if (!check(TokenType::COMMA))
                    node->condition = expression();
                consume(TokenType::COMMA, "Expect ',' after loop condition.");

                // Updation (Optional)
                if (!check(TokenType::R_PAREN))
                    node->increment = expression();
                break;
            }
        }

        consume(TokenType::R_PAREN, "Expect ')' after loop clauses.");
        node->body = block();
        consume(TokenType::END, "Expect 'end' after loop.");

        return node;
    }

    std::unique_ptr<ASTNode> Parser::switchStatement() {
        consume(TokenType::L_PAREN, "Expect '(' after 'switch'.");
        auto condition = expression();
        consume(TokenType::R_PAREN, "Expect ')' after switch condition.");
        consume(TokenType::COLON, "Expect ':' to start switch block.");

        auto node = std::make_unique<SwitchNode>();
        node->condition = std::move(condition);

        while (!check(TokenType::END) && !isAtEnd()) {
            if (match({TokenType::OPT})) {
                auto swCase = std::make_unique<SwitchCaseNode>();
                swCase->matchValue = expression();
                swCase->body = block();
                node->cases.push_back(std::move(swCase));
            } else if (match({TokenType::DEF})) {
                auto swCase = std::make_unique<SwitchCaseNode>();
                swCase->isDefault = true;
                swCase->body = block();
                node->cases.push_back(std::move(swCase));
            } else {
                advance(); 
            }
        }
        consume(TokenType::END, "Expect 'end' after switch.");
        return node;
    }

    std::unique_ptr<ASTNode> Parser::tryCatchStatement() {
        auto tryBody = block();
        consume(TokenType::CATCH, "Expect 'catch' after try block.");
        auto catchBody = block();
        consume(TokenType::END, "Expect 'end' after try-catch.");

        auto node = std::make_unique<TryCatchNode>();
        node->tryBody = std::move(tryBody);
        node->catchBody = std::move(catchBody);
        return node;
    }

    std::unique_ptr<ASTNode> Parser::streamStatement() {
        Token streamType = previous(); 
        auto chain = std::make_unique<ChainNode>();

        do {
            if (!check(TokenType::INSERT) && !check(TokenType::EXTRACT)) break;
            Token op = advance(); 
            auto val = expression();

            auto node = std::make_unique<StreamNode>();
            node->streamType = streamType;
            node->op = op;
            node->value = std::move(val);
            chain->links.push_back(std::move(node));
        } while (check(TokenType::INSERT) || check(TokenType::EXTRACT));

        consumeTerminator("Expect ';' or newline after stream chain.");
        return chain;
    }

    std::unique_ptr<BlockNode> Parser::block() {
        consume(TokenType::COLON, "Expect ':' to start block.");
        auto node = std::make_unique<BlockNode>();
        
        while (!check(TokenType::END) && !check(TokenType::ELIF) && !check(TokenType::ELSE) && 
            !check(TokenType::OPT) && !check(TokenType::DEF) && 
            !check(TokenType::CATCH) && !isAtEnd()) {
            auto decl = declaration();
            if (decl) node->statements.push_back(std::move(decl));
        }
        return node;
    }

    // --- Expression Precedence Chain ---

    std::unique_ptr<ASTNode> Parser::expression() {
        return assignment();
    }

    std::unique_ptr<ASTNode> Parser::assignment() {
        auto expr = ternary();

        if (match({TokenType::EQUAL, TokenType::PLUS_EQUAL, TokenType::MINUS_EQUAL, 
                TokenType::STAR_EQUAL, TokenType::SLASH_EQUAL, TokenType::NAND_EQUAL, 
                TokenType::NOR_EQUAL, TokenType::PIPE_EQUAL, TokenType::AMP_EQUAL, 
                TokenType::TILDE_EQUAL, TokenType::CARET_EQUAL, TokenType::POW_EQUAL})) {
            
            Token op = previous();
            auto value = assignment();

            if (isAssignable(expr.get())) {
                auto node = std::make_unique<AssignmentNode>();
                node->target = std::move(expr); 
                node->op = op; 
                node->value = std::move(value);
                return node;
            }
            throw std::runtime_error("Systems Error: Invalid assignment l-value.");
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::logicalOr() {
        auto expr = logicalAnd();
        while (match({TokenType::OR})) {
            Token op = previous();
            auto right = logicalAnd();
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); node->op = op; node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::logicalAnd() {
        auto expr = bitwise();
        while (match({TokenType::AND})) {
            Token op = previous();
            auto right = bitwise(); // Parse the next bitwise block
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); 
            node->op = op; 
            node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::bitwise() {
        auto expr = equality();
        while (match({TokenType::PIPE, TokenType::CARET, TokenType::AMPERSAND, 
                      TokenType::NAND, TokenType::NOR})) { 
            Token op = previous();
            auto right = equality();
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); node->op = op; node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::equality() {
        auto expr = comparison();
        while (match({TokenType::EQUAL_EQUAL, TokenType::BANG_EQUAL})) {
            Token op = previous();
            auto right = comparison();
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); 
            node->op = op; 
            node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::comparison() {
        auto expr = term();
        while (match({TokenType::GREATER, TokenType::GREATER_EQUAL, TokenType::LESS, TokenType::LESS_EQUAL})) {
            Token op = previous();
            auto right = term();
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); node->op = op; node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::term() {
        auto expr = factor();
        while (match({TokenType::PLUS, TokenType::MINUS})) {
            Token op = previous();
            auto right = factor();
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); node->op = op; node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::factor() {
        auto expr = power();
        while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
            Token op = previous();
            auto right = power();
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr); node->op = op; node->right = std::move(right);
            expr = std::move(node);
        }
        return expr;
    }

    std::unique_ptr<ASTNode> Parser::power() {
        auto expr = unary();

        if (match({TokenType::POW})) {
            Token op = previous();
            // Recursion here makes it Right-Associative
            auto right = power(); 
            
            auto node = std::make_unique<BinaryExprNode>();
            node->left = std::move(expr);
            node->op = op;
            node->right = std::move(right);
            return node;
        }

        return expr;
    }

    std::unique_ptr<ASTNode> Parser::cast() {
        // Current token is already TokenType::LESS ('<')
        advance(); // Consume '<'
        Token targetType = advance(); // Consume the type (i32, Player*, etc.)
        consume(TokenType::GREATER, "Expect '>' after type cast.");
        
        auto node = std::make_unique<CastExprNode>();
        node->targetType = targetType;
        node->expression = unary(); // Apply cast to the next unary expression
        return node;
    }

    std::unique_ptr<ASTNode> Parser::unary() {
        // Check for Cast: <type>expression
        if (check(TokenType::LESS) && isType(tokens[current + 1].type)) {
            return cast();
        }

        // Standard prefix operators: ! - ~ ++ --
        if (match({TokenType::BANG, TokenType::MINUS, TokenType::PLUS_PLUS, 
                TokenType::MINUS_MINUS, TokenType::TILDE})) {
            Token op = previous();
            auto right = unary();
            auto node = std::make_unique<UnaryExprNode>();
            node->op = op; 
            node->right = std::move(right);
            node->isPostfix = false;
            return node;
        }

        return postfix();
    }

    std::unique_ptr<ASTNode> Parser::ternary() {
        auto expr = logicalOr(); // Start with standard logic precedence

        if (match({TokenType::QUESTION})) {
            auto node = std::make_unique<TernaryExprNode>();
            node->condition = std::move(expr);
            node->trueExpr = expression(); // Middle part
            consume(TokenType::COLON, "Expect ':' after ternary condition.");
            node->falseExpr = ternary(); // Recursive for nested ternaries
            return node;
        }
        return expr;
    }

    // --- Leaf Nodes ---

    std::unique_ptr<ASTNode> Parser::primary() {
        switch (peek().type) {
            case TokenType::NUMBER_LIT:
            case TokenType::TRUE_LIT:
            case TokenType::FALSE_LIT:
            case TokenType::NEWLINE:
            case TokenType::TAB:
            case TokenType::FLUSH:
            case TokenType::BARE_LIT: {
                auto node = std::make_unique<LiteralExprNode>();
                node->value = advance(); 
                return node;
            }

            case TokenType::L_BRACE:
            case TokenType::L_BRACK: {
                TokenType opener = advance().type;
                TokenType closer = (opener == TokenType::L_BRACK) ? TokenType::R_BRACK : TokenType::R_BRACE;
                return collectionLiteral(closer);
            }

            case TokenType::IDENTIFIER: {
                resolveAlias(tokens[current]); 
                auto node = std::make_unique<VariableExprNode>();
                node->name = advance(); 
                return node;
            }

            case TokenType::L_PAREN: {
                advance();
                auto expr = expression();
                consume(TokenType::R_PAREN, "Expect ')' after expression.");
                return expr;
            }

            case TokenType::MY: {
                advance(); 
                auto node = std::make_unique<VariableExprNode>();
                node->name = previous(); // Lexeme "my"
                return node;
            }

            // Interpolation can start an entry point
            case TokenType::STRING_LIT:
            case TokenType::INTERPOLATION_SAFE:
            case TokenType::INTERPOLATION_UNSAFE: {
                auto node = std::make_unique<StringInterpolationNode>();
                
                while (true) {
                    auto fragment = std::make_unique<LiteralExprNode>();
                    // if the Lexer returned an error token, stop immediately.
                    if (check(TokenType::ERROR)) {
                        advance(); 
                        return node; 
                    }
                    // Always start/continue with the string fragment
                    // If we didn't start with a STRING_LIT, we advance manually with an empty one
                    if (check(TokenType::STRING_LIT))
                        fragment->value = advance();
                    else
                        fragment->value = {TokenType::STRING_LIT, "", peek().line, peek().column};
                    node->parts.push_back(std::move(fragment));

                    // Look for triggers
                    if (match({TokenType::INTERPOLATION_SAFE})) {
                        node->parts.push_back(expression());
                        node->isSafe.push_back(true);
                        consume(TokenType::R_BRACE, "Expect '}' after interpolation.");
                        
                        // Tell lexer to resume greedy string scanning for the next fragment
                        lexer.setInStringMode(true);
                    } 
                    else if (match({TokenType::INTERPOLATION_UNSAFE})) {
                        node->parts.push_back(expression());
                        node->isSafe.push_back(false);
                        consume(TokenType::R_PAREN, "Expect ')' after interpolation.");
                        lexer.setInStringMode(true); 
                    }
                    else break; // No more interpolation, end the loop

                    // IMPORTANT: Check if the string actually continues
                    // If the Lexer hasn't queued another STRING_LIT, the string is over.
                    if (!check(TokenType::STRING_LIT)) break;
                }
                return node;
            }

            default:
                throw error(peek(), "Expect expression. Found: " + peek().lexeme);
        }
    }

    std::unique_ptr<ASTNode> Parser::postfix() {
        auto expr = primary();

        while (true) {
            switch (peek().type) {
                case TokenType::L_PAREN: {
                    advance(); // consume '('
                    expr = finishCall(std::move(expr));
                    break;
                }
                case TokenType::L_BRACK: {
                    advance(); // consume '['
                    auto index = expression();
                    consume(TokenType::R_BRACK, "Expect ']' after index.");
                    auto node = std::make_unique<IndexExprNode>();
                    node->target = std::move(expr);
                    node->index = std::move(index);
                    expr = std::move(node);
                    break;
                }
                case TokenType::DOT:
                case TokenType::ARROW: {
                    Token op = advance(); // consume '.' or '->'
                    Token name = consume(TokenType::IDENTIFIER, "Expect member name.");
                    
                    auto node = std::make_unique<MemberAccessNode>();
                    node->structure = std::move(expr);
                    node->member = name;
                    node->op = op;
                    expr = std::move(node);
                    break;
                }
                case TokenType::PLUS_PLUS:
                case TokenType::MINUS_MINUS: {
                    Token op = advance(); // consume '++' or '--'
                    auto node = std::make_unique<UnaryExprNode>();
                    node->right = std::move(expr);
                    node->op = op;
                    node->isPostfix = true;
                    expr = std::move(node);
                    break;
                }
                default:
                    return expr; // returns the AST
            }
        }
    }

    std::unique_ptr<ASTNode> Parser::collectionLiteral(TokenType closer) {
        auto node = std::make_unique<CollectionLiteralNode>();
        
        if (!check(closer)) {
            do {
                node->elements.push_back(expression());
            } while (match({TokenType::COMMA}));
        }
        
        // Use the closer we determined in primary()
        consume(closer, closer == TokenType::R_BRACK ? "Expect ']' after collection." : "Expect '}' after collection.");
        return node;
    }

    // --- Complex Structure Handlers ---

    std::unique_ptr<ASTNode> Parser::functionDeclaration() {
        // 1. If the keyword was 'entry', this is our program start
        bool isEntryKeyword = (previous().type == TokenType::ENTRY);
        
        Token name = consume(TokenType::IDENTIFIER, "Expect function name.");
        consume(TokenType::L_PAREN, "Expect '(' after name.");
        
        // ... Parameter Parsing Logic ...
        std::vector<std::unique_ptr<VarDeclNode>> params;
        if (!check(TokenType::R_PAREN)) {
            do {
                Token pType = advance();
                Token pName = consume(TokenType::IDENTIFIER, "Expect param name.");
                auto paramNode = std::make_unique<VarDeclNode>();
                paramNode->type = pType; paramNode->name = pName;
                params.push_back(std::move(paramNode));
            } while (match({TokenType::COMMA}));
        }
        consume(TokenType::R_PAREN, "Expect ')'.");
        
        consume(TokenType::ARROW, "Expect '->'.");
        Token returnType = advance();

        // 2. Initialize the node
        auto node = std::make_unique<FunctionNode>();
        node->name = name; 
        node->returnType = returnType; 
        node->parameters = std::move(params);

        // 3. Body/Terminator Logic
        switch (peek().type) {
            case TokenType::SEMICOLON:
                advance(); // Consume ';'
                node->body = nullptr; 
                node->isEntry = false; 
                break;

            case TokenType::COLON_COLON:
                advance(); // Consume '::'
                node->body = std::make_unique<BlockNode>(); 
                node->isEntry = false; 
                break;

            case TokenType::COLON:
                node->body = block(); // block() consumes ':'
                consume(TokenType::END, "Expect 'end' after body.");
                // Only 'entry' definitions with bodies can be the true entry point
                node->isEntry = isEntryKeyword; 
                break;

            default:
                throw std::runtime_error("Expect ':', '::', or ';' after function signature.");
        }

        return node;
    }

    std::unique_ptr<ASTNode> Parser::unitDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expect unit name.");
        Token parent;
        if (match({TokenType::FROM}))
            parent = consume(TokenType::IDENTIFIER, "Expect parent unit name.");

        consume(TokenType::COLON, "Expect ':' to start unit block.");
        auto node = std::make_unique<UnitNode>();
        node->name = name; node->parentName = parent;

        TokenType currentVisibility = TokenType::PRIV; 

        while (!check(TokenType::END) && !isAtEnd()) {
            // Handle Visibility Modifiers (public etc.)
            if (match({TokenType::PUB, TokenType::PRIV, TokenType::PROT})) {
                currentVisibility = previous().type;
                consume(TokenType::COLON, "Expect ':' after access modifier.");
                continue; 
            }

            auto memberNode = std::make_unique<UnitMemberNode>();
            memberNode->visibility = Token{currentVisibility, "", 0, 0};

            // The Core Member Dispatcher
            switch (peek().type) {
                case TokenType::INIT:
                case TokenType::TILDE:
                    memberNode->member = lifecycleMethod();
                    break;

                case TokenType::FN:
                    advance(); // Consume 'fn'
                    memberNode->member = functionDeclaration();
                    break;

                default:
                // --- THE CLEANUP LOGIC ---
                if (isType(peek().type)) {
                    // if it starts with a type, it's a variable member.
                    // we pass 'false' to varDeclaration so it doesn't throw if ';' is missing.
                    memberNode->member = varDeclaration(false);

                    if (check(TokenType::SEMICOLON) || peek().line > previous().line)
                        if (check(TokenType::SEMICOLON)) advance();
                    
                    // CLEANUP: If there IS a semicolon (optional), we must consume it now
                    // so the next iteration of the loop starts at a fresh token.
                    if (match({TokenType::SEMICOLON})) { /* Successfully cleaned */ }
                }
                else {
                    // if it wasn't a lifecycle method, fn, or a variable.
                    // We try general declaration() for nested units or enums.
                    memberNode->member = declaration();
                    
                    // SAFETY VALVE: If declaration() failed (returned nullptr), we are 
                    // stuck on a "junk" token. We advance once to break the infinite loop.
                    if (!memberNode->member) advance();
                    
                }
                break;
            }

            if (memberNode->member) node->members.push_back(std::move(memberNode));
        }

        consume(TokenType::END, "Expect 'end' after unit body.");
        return node;
    }

    std::unique_ptr<ASTNode> Parser::lifecycleMethod() {
        switch (peek().type) {
        case TokenType::INIT: {
            advance(); 
            auto node = std::make_unique<InitNode>();
            consume(TokenType::L_PAREN, "Expect '(' after 'init'.");

            if (!check(TokenType::R_PAREN)) {
                while (true) {
                    // Safety check: if we see the end of the list, stop.
                    if (check(TokenType::R_PAREN) || isAtEnd()) break;

                    // Strictly enforce Type Name sequence
                    if (!isType(peek().type)) throw error(peek(), "Expect parameter type.");
                    Token pType = advance();
                    Token pName = consume(TokenType::IDENTIFIER, "Expect parameter name.");
                    
                    auto paramNode = std::make_unique<VarDeclNode>();
                    paramNode->type = pType; 
                    paramNode->name = pName;
                    node->parameters.push_back(std::move(paramNode));

                    if (!match({TokenType::COMMA})) break;
                }
            }
            consume(TokenType::R_PAREN, "Expect ')' after parameters.");

                // The Initializer List Logic: : member(val)
                // CRITICAL: We only consume the colon if it is followed by an Identifier AND a Parenthesis.
                // This prevents us from "stealing" the colon meant for the block() start.
                if (check(TokenType::COLON) && 
                    lookaheadCheck(1, TokenType::IDENTIFIER) && 
                    lookaheadCheck(2, TokenType::L_PAREN)) {

                    advance(); // Confirmed initializer list; consume the ':'
                    do {
                        Token mName = consume(TokenType::IDENTIFIER, "Expect member name.");
                        consume(TokenType::L_PAREN, "Expect '(' after member name.");
                        auto val = expression();
                        consume(TokenType::R_PAREN, "Expect ')' after value.");
                        node->initializers.push_back({mName, std::move(val)});
                    } while (match({TokenType::COMMA}));
                }

                // Body Parsing init(...) : [body-code] end
                // block() starts by consuming its own colon, so we leave it here for block() to find.
                if (check(TokenType::COLON) || check(TokenType::COLON_COLON)) {
                    node->body = block();
                    consume(TokenType::END, "Expect 'end' after init body.");
                }
                
                return node;
            }

            case TokenType::TILDE: {
                advance(); // Consume '~'
                consume(TokenType::KILL, "Expect 'kill' after '~' for destructor.");
                consume(TokenType::L_PAREN, "Expect '(' after 'kill'.");
                consume(TokenType::R_PAREN, "Expect ')' for destructor.");
                
                auto node = std::make_unique<KillNode>();
                // Destructors in Glow usually just have a body, no initializer list
                if (check(TokenType::COLON) || check(TokenType::COLON_COLON)) {
                    node->body = block();
                    consume(TokenType::END, "Expect 'end' after ~kill body.");
                }
                return node;
            }

            default:
                throw error(peek(), "Unexpected lifecycle token.");
        }
    }

    std::unique_ptr<ASTNode> Parser::enumDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expect enum name.");
        consume(TokenType::ARROW, "Expect '->' for enum type.");
        Token type = advance(); // Consume the underlying type (str, i32, etc.)
        consume(TokenType::COLON, "Expect ':' to start enum.");

        auto node = std::make_unique<EnumNode>();
        node->name = name;
        node->underlyingType = type;

        while (!check(TokenType::END) && !isAtEnd()) {
            Token entryName = consume(TokenType::IDENTIFIER, "Expect enum entry name.");
            consume(TokenType::EQUAL, "Expect '=' after enum entry.");
            auto value = expression();
            node->entries.push_back({entryName, std::move(value)});
        }

        consume(TokenType::END, "Expect 'end' after enum.");
        return node;
    }

    // --- Navigation Helpers ---
    Token Parser::advance() { if (!isAtEnd()) current++; return previous(); }
    Token Parser::peek() const { return tokens[current]; }
    Token Parser::previous() const { return tokens[current - 1]; }
    bool Parser::isAtEnd() const { return peek().type == TokenType::EOF_TOKEN; }
    bool Parser::check(TokenType type) const { return !isAtEnd() && peek().type == type; }

    bool Parser::lookaheadCheck(int distance, TokenType type) const {
        if (current + distance >= tokens.size()) return false;
        return tokens[current + distance].type == type;
    }
    
    bool Parser::match(const std::vector<TokenType>& types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }
        return false;
    }

    Token Parser::consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        Diagnostics::report(peek().line, peek().column, message);
        throw error(peek(), message);
    }

    void Parser::consumeTerminator(const std::string& message) {
        switch (peek().type) {
            case TokenType::SEMICOLON:
            case TokenType::NEWLINE:
                advance(); // Successfully consumed
                return;

            case TokenType::END:
            case TokenType::EOF_TOKEN:
            case TokenType::R_BRACE:
                return; // Valid implicit terminators at block ends

            default:
                // Allow line jumps ONLY if the current expression isn't "hanging" 
                // (for instance if the previous token was a binary operator, we shouldn't terminate)
                if (peek().line > previous().line) {
                    TokenType prev = previous().type;
                    // List of tokens that should NEVER be followed by an implicit terminator
                    if (prev != TokenType::INSERT && prev != TokenType::EXTRACT && 
                        prev != TokenType::PLUS   && prev != TokenType::MINUS &&
                        prev != TokenType::STAR   && prev != TokenType::SLASH) {
                        return; 
                    }
                }
                break;
        }
        throw error(peek(), message);
    }

    Token Parser::consumeIdentifier(const std::string& msg) {
        if (check(TokenType::IDENTIFIER)) return advance();
        // ALLOW 'entry' to be used as a variable name
        if (check(TokenType::ENTRY)) return advance(); 
        throw error(peek(), msg);
    }

    bool Parser::isType(TokenType type) const {
        switch (type) {
            case TokenType::I16:   case TokenType::I32:  case TokenType::I64:  case TokenType::I128:
            case TokenType::UI16:  case TokenType::UI32: case TokenType::UI64: case TokenType::UI128:
            case TokenType::F32:   case TokenType::F64:  case TokenType::BOOL: case TokenType::CHAR: 
            case TokenType::STR:   case TokenType::REF:  case TokenType::HOLLOW:        return true;
            case TokenType::IDENTIFIER:
                return (lookaheadCheck(1, TokenType::IDENTIFIER)
                || lookaheadCheck(1, TokenType::STAR) || lookaheadCheck(1, TokenType::REF))
                && !lookaheadCheck(2, TokenType::DOT) && !lookaheadCheck(2, TokenType::L_PAREN);
            default: 
                return false; 
        }
    }

    // The Error Reporter
    ParseError Parser::error(const Token& token, const std::string& message) {
        std::string lineText = sourceBuffer.getLineText(token.line);
        Diagnostics::report(token.line, token.column, message, LogLevel::ERROR, lineText);
        return ParseError(); // Return the signal to be thrown
    }

    // The Panic Mode Recovery (Synchronization)
    void Parser::synchronize() {
        advance();

        while (!isAtEnd()) {
            // Stop skipping if we hit a semicolon (end of a statement)
            if (previous().type == TokenType::SEMICOLON) return;

            // Stop skipping if we hit a major keyword (start of a new statement)
            switch (peek().type) {
                case TokenType::IF:
                case TokenType::LOOP:
                case TokenType::FN:
                case TokenType::UNIT:
                case TokenType::RET:
                case TokenType::PANIC:
                case TokenType::ENTRY:
                    return;
                default:
                    if (isType(peek().type)) return;
                    break;
            }

            advance();
        }
    }

    bool Parser::isAssignable(ASTNode* node) {
        if (!node) return false;
        switch (node->nodeType) {
            case ASTNodeType::VARIABLE:      // x = 5
            case ASTNodeType::MEMBER_ACCESS: // my->x = 5
            case ASTNodeType::INDEXING:      // arr[0] = 5
                return true;
            default:
                return false;
        }
    }

    bool Parser::checkNext(TokenType type) const {
        // Safety check for end of stream
        if (isAtEnd()) return false;
        
        // Look exactly one token ahead
        size_t nextIndex = current + 1;
        if (nextIndex >= tokens.size()) return false;
        
        // Compare with the expected type
        return tokens[nextIndex].type == type;
    }

    LoopVariant Parser::getLoopVariant() {
        int lookahead = current;
        int commas = 0;

        // Look ahead until we hit the closing parenthesis of the loop header
        while (tokens[lookahead].type != TokenType::R_PAREN && 
            tokens[lookahead].type != TokenType::EOF_TOKEN) {
            
            switch (tokens[lookahead].type) {
                case TokenType::COLON:
                    // Immediate exit: ':' only exists in RANGE syntax loop(i : list)
                    return LoopVariant::RANGE;

                case TokenType::COMMA:
                    // Count commas to distinguish FOR loop(i, cond, upd) from WHILE loop(cond)
                    commas++;
                    break;

                default:
                    // Skip all other tokens (identifiers, literals, math)
                    break;
            }
            
            lookahead++;
        }

        return (commas > 0) ? LoopVariant::FOR : LoopVariant::WHILE;
    }

    std::unique_ptr<ASTNode> Parser::call() {
        return postfix();
    }

    std::unique_ptr<ASTNode> Parser::finishCall(std::unique_ptr<ASTNode> callee) {
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (!check(TokenType::R_PAREN)) {
            do {
                arguments.push_back(expression());
            } while (match({TokenType::COMMA}));
        }

        consume(TokenType::R_PAREN, "Expect ')' after arguments.");
        
        auto node = std::make_unique<CallExprNode>();
        node->callee = std::move(callee);
        node->arguments = std::move(arguments); 
        return node;
    }

    std::unique_ptr<ASTNode> Parser::handleUse() {
        bool isOnce = match({TokenType::ONCE});
        std::vector<Token> pathParts;

        do {
            pathParts.push_back(consume(TokenType::IDENTIFIER, "Expect module name."));
            if (!match({TokenType::COLON_COLON})) break;
        } while (true);

        Token aliasToken;
        if (match({TokenType::AS}))
            aliasToken = consume(TokenType::IDENTIFIER, "Expect alias name after 'as'.");

        consume(TokenType::SEMICOLON, "Expect ';' after use statement.");

        // Resolve path...
        std::string base = "src/include/GlowModules/";
        for (size_t i = 0; i < pathParts.size(); ++i)
            base += pathParts[i].lexeme + (i == pathParts.size() - 1 ? "" : "/");

        std::string attemptPath = std::filesystem::exists(base + ".glh") ? base + ".glh" : base + ".glw";

        if (this->loadingStack.count(attemptPath))
            throw std::runtime_error("Circular dependency detected: " + attemptPath);
        
        if (isOnce && this->loadedModules.count(attemptPath)) return nullptr;

        this->loadingStack.insert(attemptPath);
        this->loadedModules.insert(attemptPath);

        auto subBuffer = std::make_unique<SourceBuffer>();
        if (!subBuffer->loadFile(attemptPath)) {
            this->loadingStack.erase(attemptPath);
            throw std::runtime_error("Could not find module: " + attemptPath);
        }
        
        Lexer subLexer(*subBuffer);
        std::vector<Token> subTokens = subLexer.tokenize();
        if (!subTokens.empty() && subTokens.back().type == TokenType::EOF_TOKEN) subTokens.pop_back();

        tokens.insert(tokens.begin() + current, subTokens.begin(), subTokens.end());
        moduleBuffers.push_back(std::move(subBuffer));
        
        this->loadingStack.erase(attemptPath); 
        return nullptr; 
    }

    std::unique_ptr<ASTNode> Parser::parseDeleteStatement() {
        auto node = std::make_unique<DeleteNode>();
        // Parse the target (for example variable name)
        node->target = expression(); 
        consume(TokenType::SEMICOLON, "Expect ';' after 'del' statement.");
        return node;
    }

    std::unique_ptr<ASTNode> Parser::GCToggleStatement(bool enable) {
        auto node = std::make_unique<GCToggleNode>();
        node->enable = enable; //
        
        consume(TokenType::SEMICOLON, "Expect ';' after GC toggle command.");
        return node;
    }

    void Parser::registerAlias(const std::string& alias, const std::vector<Token>& originalPath) {
        std::string fullOriginalName = "";
        for (size_t i = 0; i < originalPath.size(); ++i) {
            fullOriginalName += originalPath[i].lexeme;
            if (i < originalPath.size() - 1) fullOriginalName += "::";
        }
        this->aliases[alias] = fullOriginalName;
    }

    void Parser::resolveAlias(Token& token) {
        auto it = aliases.find(token.lexeme);
        if (it != aliases.end())
            // Swaps the 'nickname' for the full module path
            token.lexeme = it->second;
    }
}
