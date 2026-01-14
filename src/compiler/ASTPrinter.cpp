#include "compiler/ASTPrinter.h"
#include <iostream>

namespace Glow {

    void ASTPrinter::print(ASTNode* node, int indent) {
        if (!node) return;

        printIndent(indent);

        switch (node->nodeType) {
            case ASTNodeType::FUNCTION: {
                auto* n = static_cast<FunctionNode*>(node);
                std::cout << (n->isEntry ? "[Entry] " : "") << "Function: " << n->name.lexeme 
                          << " -> " << n->returnType.lexeme << std::endl;
                
                // Print Parameters
                if (!n->parameters.empty()) {
                    printIndent(indent + 1);
                    std::cout << "Params:" << std::endl;
                    for (const auto& param : n->parameters)
                        print(param.get(), indent + 2);
                }
                print(n->body.get(), indent + 1);
                break;
            }

            case ASTNodeType::VAR_DECL: {
                auto* n = static_cast<VarDeclNode*>(node);
                std::cout << "VarDecl: " << n->name.lexeme << " [Type: " << n->type.lexeme << "]" 
                          << (n->isArray ? " (Array)" : "") << std::endl;
                if (n->arraySize) {
                    printIndent(indent + 1);
                    std::cout << "Size:" << std::endl;
                    print(n->arraySize.get(), indent + 2);
                }
                if (n->initializer) print(n->initializer.get(), indent + 1);
                break;
            }

            case ASTNodeType::TRY_CATCH: {
                auto* n = static_cast<TryCatchNode*>(node);
                std::cout << "Try-Catch Statement" << std::endl;
                printIndent(indent + 1); std::cout << "Try:" << std::endl;
                print(n->tryBody.get(), indent + 2);
                printIndent(indent + 1); std::cout << "Catch:" << std::endl;
                print(n->catchBody.get(), indent + 2);
                break;
            }

            case ASTNodeType::QUALIFIED_ID: {
                auto* n = static_cast<QualifiedIDNode*>(node);
                std::cout << "QualifiedID: ";
                for (size_t i = 0; i < n->path.size(); ++i) {
                    std::cout << n->path[i].lexeme << (i < n->path.size() - 1 ? "::" : "");
                }
                std::cout << std::endl;
                break;
            }

            case ASTNodeType::COLLECTION: {
                auto* n = static_cast<CollectionLiteralNode*>(node);
                std::cout << "Collection Literal { }" << std::endl;
                for (const auto& elem : n->elements) {
                    print(elem.get(), indent + 1);
                }
                break;
            }

            case ASTNodeType::CALL: {
                auto* n = static_cast<CallExprNode*>(node);
                // Identify if this is the new function-style panic
                if (n->op.type == TokenType::PANIC)
                    std::cout << "Call: panic()" << std::endl;
                else {
                    std::cout << "Call:" << std::endl;
                    print(n->callee.get(), indent + 1);
                }

                // Recursively print all arguments passed to the call
                if (!n->arguments.empty()) {
                    printIndent(indent + 1);
                    std::cout << "Args:" << std::endl;
                    for (const auto& arg : n->arguments)
                        print(arg.get(), indent + 2);
                }
                break;
            }

            case ASTNodeType::INDEXING: {
                auto* n = static_cast<IndexExprNode*>(node);
                std::cout << "Indexing: []" << std::endl;
                print(n->target.get(), indent + 1);
                print(n->index.get(), indent + 1);
                break;
            }

            case ASTNodeType::BLOCK: {
                auto* n = static_cast<BlockNode*>(node);
                std::cout << "Block :" << std::endl;
                for (const auto& stmt : n->statements)
                    print(stmt.get(), indent + 1);
                printIndent(indent);
                std::cout << "end" << std::endl;
                break;
            }

            case ASTNodeType::IF: {
                auto* n = static_cast<IfNode*>(node);
                std::cout << "If Statement" << std::endl;
                
                // Initial If Condition and Body
                printIndent(indent + 1); std::cout << "Cond:" << std::endl;
                print(n->condition.get(), indent + 2);
                printIndent(indent + 1); std::cout << "Body:" << std::endl;
                print(n->ifBody.get(), indent + 2);

                // Elif Branches
                for (const auto& elif : n->elifBranches) {
                    printIndent(indent); std::cout << "Elif:" << std::endl;
                    printIndent(indent + 1); std::cout << "Cond:" << std::endl;
                    print(elif.condition.get(), indent + 2);
                    printIndent(indent + 1); std::cout << "Body:" << std::endl;
                    print(elif.body.get(), indent + 2);
                }

                // Optional Else Body
                if (n->elseBody) {
                    printIndent(indent); std::cout << "Else:" << std::endl;
                    print(n->elseBody.get(), indent + 1);
                }
                break;
            }

            case ASTNodeType::LOOP: {
                auto* n = static_cast<LoopNode*>(node);
                std::cout << "Loop Statement" << std::endl;
                if (n->init) print(n->init.get(), indent + 1);
                if (n->condition) print(n->condition.get(), indent + 1);
                if (n->increment) print(n->increment.get(), indent + 1);
                print(n->body.get(), indent + 1);
                break;
            }

            case ASTNodeType::SWITCH: {
                auto* n = static_cast<SwitchNode*>(node);
                std::cout << "Switch Statement" << std::endl;
                print(n->condition.get(), indent + 1);
                for (auto& c : n->cases) {
                    printIndent(indent + 1);
                    std::cout << (c->isDefault ? "def:" : "opt:") << std::endl;
                    if (c->matchValue) print(c->matchValue.get(), indent + 2);
                    print(c->body.get(), indent + 2);
                }
                break;
            }

            case ASTNodeType::BINARY: {
                auto* n = static_cast<BinaryExprNode*>(node);
                std::cout << "BinaryExpr: " << n->op.lexeme << std::endl;
                print(n->left.get(), indent + 1);
                print(n->right.get(), indent + 1);
                break;
            }

            case ASTNodeType::UNARY: {
                auto* n = static_cast<UnaryExprNode*>(node);
                std::cout << "UnaryExpr: " << n->op.lexeme << std::endl;
                print(n->right.get(), indent + 1);
                break;
            }

            case ASTNodeType::ASSIGNMENT: {
                auto* n = static_cast<AssignmentNode*>(node);
                
                // Instead of n->name.lexeme, we describe the target type
                std::cout << "Assignment (Op: " << n->op.lexeme << ")" << std::endl;
                
                // Recursively print the target (the left-hand side)
                printIndent(indent + 1);
                std::cout << "Target: ";
                print(n->target.get(), indent + 1);
                
                // Recursively print the value (the right-hand side)
                printIndent(indent + 1);
                std::cout << "Value: ";
                print(n->value.get(), indent + 1);
                break;
            }

            case ASTNodeType::VARIABLE: {
                auto* n = static_cast<VariableExprNode*>(node);
                std::cout << "Variable: " << n->name.lexeme << std::endl;
                break;
            }

            case ASTNodeType::LITERAL: {
                auto* n = static_cast<LiteralExprNode*>(node);
                // Formatted to show the raw lexeme (works for 128-bit hex strings)
                std::cout << "Literal: [" << n->value.lexeme << "]" << std::endl;
                break;
            }

            case ASTNodeType::RETURN: {
                auto* n = static_cast<ReturnNode*>(node);
                std::cout << "Return" << std::endl;
                if (n->value) print(n->value.get(), indent + 1);
                break;
            }

            case ASTNodeType::STREAM: {
                auto* n = static_cast<StreamNode*>(node);
                std::cout << "Stream Op (" << n->streamType.lexeme << "): " << n->op.lexeme << std::endl;
                
                // Safety check: ensures value isn't null or pointing to self
                if (n->value && n->value.get() != node)
                    print(n->value.get(), indent + 1);
                break;
            }

            case ASTNodeType::CHAIN: {
                auto* n = static_cast<ChainNode*>(node);
                std::cout << "Stream Chain:" << std::endl;
                for (const auto& link : n->links) {
                    if (link.get() == node) continue; // Safety recursion guard
                    print(link.get(), indent + 1);    // each link is a streamNode
                }
                break;
            }

            case ASTNodeType::STRING_INTERPOLATION: {
                auto* n = static_cast<StringInterpolationNode*>(node);
                
                // if it's just one literal, treat it as a plain string in the logs
                if (n->parts.size() == 1 && n->parts[0]->nodeType == ASTNodeType::LITERAL) {
                    auto* lit = static_cast<LiteralExprNode*>(n->parts[0].get());
                    std::cout << "String: [" << lit->value.lexeme << "]" << std::endl;
                }
                else {
                    // otherwise, show the full structural breakdown for actual interpolation
                    std::cout << "String Interpolation:" << std::endl;
                    size_t exprIdx = 0;
                    for (const auto& part : n->parts) {
                        if (part->nodeType == ASTNodeType::LITERAL) {
                            print(part.get(), indent + 1);
                        } else {
                            bool safe = n->isSafe[exprIdx++];
                            printIndent(indent + 1);
                            std::cout << "Interpolate (" << (safe ? "${}" : "$()") << "):" << std::endl;
                            print(part.get(), indent + 2);
                        }
                    }
                }
                break;
            }

            case ASTNodeType::UNIT: {
                auto* n = static_cast<UnitNode*>(node);
                std::cout << "Unit: " << n->name.lexeme;
                if (!n->parentName.lexeme.empty()) std::cout << " from " << n->parentName.lexeme;
                std::cout << std::endl;
                for (const auto& member : n->members) {
                    print(member.get(), indent + 1);
                }
                break;
            }

            case ASTNodeType::UNIT_MEMBER: {
                auto* n = static_cast<UnitMemberNode*>(node);
                
                // Map the visibility token type to a string
                std::string vis = "private";
                if (n->visibility.type == TokenType::PUB) vis = "public";
                else if (n->visibility.type == TokenType::PROT) vis = "protected";

                std::cout << "Member [" << vis << "]:" << std::endl;
                // Recursively print the actual variable or function inside
                print(n->member.get(), indent + 1);
                break;
            }

            case ASTNodeType::MEMBER_ACCESS: {
                auto* n = static_cast<MemberAccessNode*>(node);
                
                // Print the operator (. or ->) and the member name
                std::cout << "Member Access: " << n->op.lexeme << n->member.lexeme << std::endl;
                
                // Print the base structure being accessed (e.g., the variable 'cpu')
                printIndent(indent + 1);
                std::cout << "Structure:" << std::endl;
                print(n->structure.get(), indent + 2);
                break;
            }

            case ASTNodeType::ENUM: {
                auto* n = static_cast<EnumNode*>(node);
                std::cout << "Enum: " << n->name.lexeme << " -> " << n->underlyingType.lexeme << std::endl;
                for (const auto& entry : n->entries) {
                    printIndent(indent + 1);
                    std::cout << entry.first.lexeme << " = " << std::endl;
                    print(entry.second.get(), indent + 2);
                }
                break;
            }

            case ASTNodeType::INIT: {
                auto* n = static_cast<InitNode*>(node);
                std::cout << "Lifecycle: init()" << std::endl;
                if (!n->initializers.empty()) {
                    printIndent(indent + 1); std::cout << "Initializers:" << std::endl;
                    for (auto& init : n->initializers) {
                        printIndent(indent + 2);
                        std::cout << init.first.lexeme << " <- " << std::endl;
                        print(init.second.get(), indent + 3);
                    }
                }
                print(n->body.get(), indent + 1);
                break;
            }

            case ASTNodeType::KILL: {
                auto* n = static_cast<KillNode*>(node);
                std::cout << "Lifecycle: ~kill()" << std::endl;
                print(n->body.get(), indent + 1);
                break;
            }

            case ASTNodeType::CAST: {
                auto* n = static_cast<CastExprNode*>(node);
                std::cout << "Cast: <" << n->targetType.lexeme << ">" << std::endl;
                print(n->expression.get(), indent + 1);
                break;
            }

            case ASTNodeType::TERNARY: {
                auto* n = static_cast<TernaryExprNode*>(node);
                std::cout << "Ternary: ? :" << std::endl;
                print(n->condition.get(), indent + 1);
                print(n->trueExpr.get(), indent + 1);
                print(n->falseExpr.get(), indent + 1);
                break;
            }

            default:
                std::cout << "[Unimplemented Printer for Node: " << (int)node->nodeType << "]" << std::endl;
                break;
        }
    }

    void ASTPrinter::printIndent(int indent) {
        for (int i = 0; i < indent; ++i) std::cout << "  ";
    }
}
