#include "compiler/Transpiler.h"
#include <iostream>
#include <algorithm>

namespace Glow {
    bool Transpiler::compile(std::unique_ptr<BlockNode>& root, const std::string& outPath) {
        std::ofstream out(outPath);
        if (!out.is_open()) return false;

        // Glow Runtime Header for native support e.g. 128bit
        out << "#include \"types.hpp\"\n";
        out << "#include \"runtime.hpp\"\n";
        out << "#include <atomic>\n";
        // Standard C++ Headers
        out << "#include <iostream>\n#include <cstdint>\n#include <string>\n#include <cmath>\n#include <cstdio>\n";
        out << "using namespace std;\n\n";

        // Generate Code
        for (auto& stmt : root->statements) {
            write(stmt.get(), out, 0);
            out << "\n";
        }

        out.close();
        return true;
    }

    void Transpiler::write(ASTNode* node, std::ofstream& out, int indent) {
        if (!node) return;

        switch (node->nodeType) {
            case ASTNodeType::IF: {
                auto* n = static_cast<IfNode*>(node);

                // Initial if
                out << "if (";
                write(n->condition.get(), out, indent);
                out << ")";
                write(n->ifBody.get(), out, indent);

                // Iterative elifs
                for (const auto& branch : n->elifBranches) {
                    out << " else if (";
                    write(branch.condition.get(), out, indent);
                    out << ")";
                    write(branch.body.get(), out, indent);
                }

                // Final else
                if (n->elseBody) {
                    out << " else ";
                    write(n->elseBody.get(), out, indent);
                }
                break;
            }

            case ASTNodeType::LOOP: {
                auto* n = static_cast<LoopNode*>(node);

                switch (n->variant) {
                    case LoopVariant::RANGE: {
                        // Glow loop(i32 i : arr) -> C++ (int32_t i : arr)
                        out << "for (";
                        
                        if (n->init->nodeType == ASTNodeType::VAR_DECL) {
                            auto* var = static_cast<VarDeclNode*>(n->init.get());
                            // Map the Glow type to C++ (e.g., i32 -> int32_t)
                            out << mapType(var->type.type) << " " << var->name.lexeme;
                        } 
                        else {
                            out << "auto& ";
                            write(n->init.get(), out, indent);
                        }

                        out << " : ";
                        write(n->condition.get(), out, indent);
                        out << ") ";
                        break; 
                    }

                    case LoopVariant::WHILE:
                        // Glow: loop(condition) -> C++: while(condition)
                        out << "while (";
                        if (n->condition) write(n->condition.get(), out, indent);
                        else out << "true"; 
                        out << ") ";
                        break;

                    case LoopVariant::FOR:
                    default:
                        // Glow: loop(init, cond, upd) -> C++: for(init; cond; upd)
                        out << "for (";
                        if (n->init) write(n->init.get(), out, indent);
                        out << "; ";
                        if (n->condition) write(n->condition.get(), out, indent);
                        out << "; ";
                        if (n->increment) write(n->increment.get(), out, indent);
                        out << ") ";
                        break;
                }

                write(n->body.get(), out, indent);
                break;
            }

            case ASTNodeType::CALL: {
                auto* n = static_cast<CallExprNode*>(node);
                
                // Check if this is a 'panic' call (which we treat as a function)
                if (n->op.type == TokenType::PANIC) {
                    out << "glow_panic("; 
                    write(n->arguments[0].get(), out, indent);
                    out << ")";
                }
                else {
                    write(n->callee.get(), out, indent);
                    out << "(";
                    for (size_t i = 0; i < n->arguments.size(); ++i) {
                        write(n->arguments[i].get(), out, indent);
                        if (i < n->arguments.size() - 1) out << ", ";
                    }
                    out << ")";
                }
                break;
            }

            case ASTNodeType::FUNCTION: {
                auto* n = static_cast<FunctionNode*>(node);
                
                // map the Glow return type to C++. 
                // if it's the entry point, C++ requires 'int' for main.
                std::string returnType = n->isEntry ? "int" : mapType(n->returnType.type);
                std::string funcName = n->isEntry ? "main" : n->name.lexeme;

                out << returnType << " " << funcName << "(";
                
                // open parameter scope for type tracking during transpilation
                symbols.enterScope(); 

                for (size_t i = 0; i < n->parameters.size(); ++i) {
                    auto& param = n->parameters[i];
                    
                    // Track types (especially 128-bit) so assignments inside the body work correctly
                    Value typeMarker;
                    typeMarker.type = (param->type.type == TokenType::I128) ? ValueType::I128 :
                                    (param->type.type == TokenType::UI128) ? ValueType::UI128 : ValueType::I32;
                    symbols.define(param->name.lexeme, typeMarker);

                    out << mapType(param->type.type) << " " << param->name.lexeme;
                    if (i < n->parameters.size() - 1) out << ", ";
                }
                out << ")";

                // the Terminator Switch
                // logic: 0 = Header/Declaration (;), 1 = Implementation/Stub (: / ::)
                int bodyState = (n->body == nullptr) ? 0 : 1; 

                switch (bodyState) {
                    case 0: // Header Declaration (from .glh)
                        out << ";";
                        break;

                    case 1: // Implementation (:) or Wrapper Stub (::)
                        write(n->body.get(), out, indent);
                        break;
                        
                    default:
                        // Safety fallback
                        out << ";"; 
                        break;
                }

                symbols.exitScope(); // Cleanup parameter scope
                break;
            }

            case ASTNodeType::BLOCK: {
                auto* n = static_cast<BlockNode*>(node);
                symbols.enterScope();

                // Determine if we can omit braces for a one-liner
                bool isOneLiner = (n->statements.size() == 1);

                if (isOneLiner) {
                    // One-liner: No braces, just a newline and the statement
                    out << "\n";
                    auto& stmt = n->statements[0];
                    writeIndent(out, indent + 1);
                    write(stmt.get(), out, indent + 1);
                    
                    // Handle semicolon for the one-liner
                    if (stmt->nodeType != ASTNodeType::IF && stmt->nodeType != ASTNodeType::LOOP)
                        out << ";";
                }
                else {
                    // Multi-line block: Use standard braces
                    out << " {\n";
                    for (auto& stmt : n->statements) {
                        writeIndent(out, indent + 1);
                        write(stmt.get(), out, indent + 1);

                        // Semicolon Dispatcher
                        switch (stmt->nodeType) {
                            case ASTNodeType::IF:
                            case ASTNodeType::LOOP:
                            case ASTNodeType::SWITCH:
                            case ASTNodeType::TRY_CATCH:
                            case ASTNodeType::BLOCK:
                                out << "\n";
                                break;
                            default:
                                out << ";\n";
                                break;
                        }
                    }
                    writeIndent(out, indent);
                    out << "}";
                }

                symbols.exitScope();
                break;
            }

            case ASTNodeType::VAR_DECL: {
                auto* n = static_cast<VarDeclNode*>(node);
                
                // Register type for 128-bit tracking (Internal Symbol Table)
                Value typeMarker;
                typeMarker.type = (n->type.type == TokenType::I128) ? ValueType::I128 :
                                (n->type.type == TokenType::UI128) ? ValueType::UI128 : ValueType::I32;
                symbols.define(n->name.lexeme, typeMarker);

                // Output C++ type and name
                out << mapType(n->type.type) << " " << n->name.lexeme;
                
                // Handle Array Brackets and Sizes
                // If it's declared as an array OR assigned a collection like [1, 2, 3]
                if (n->isArray || (n->initializer && n->initializer->nodeType == ASTNodeType::COLLECTION)) {
                    out << "[";
                    if (n->arraySize) {
                        // We call write() here because it returns void.
                        // It will write the expression (like '5') directly into 'out'.
                        write(n->arraySize.get(), out, indent);
                    }
                    out << "]";
                }

                // Handle Initialization
                if (n->initializer) {
                    out << " = ";
                    handlePotential128(n->type.type, n->initializer.get(), out, indent);
                }
                break;
            }

            case ASTNodeType::BINARY: {
                auto* n = static_cast<BinaryExprNode*>(node);

                switch (n->op.type) {
                    // --- Power Operator (**) ---
                    case TokenType::POW:
                        out << "glowPow(";
                        write(n->left.get(), out, indent); 
                        out << ", "; 
                        write(n->right.get(), out, indent); 
                        out << ")";
                        break;

                    // --- Advanced Bitwise (NAND, NOR, XOR) ---
                    case TokenType::NAND: 
                    case TokenType::NOR: 
                    case TokenType::AMPERSAND: 
                    case TokenType::PIPE: 
                    case TokenType::CARET:
                    case TokenType::TILDE: 
                        writeBitwise(n, out, indent);
                        break;

                    // --- All Other Arithmetic, Comparison, and Logic ---
                    default: {
                        // Only use glowAdd for String concatenation
                        // Simple integers and floats use native C++ operators for 10/10 performance
                        if (n->op.type == TokenType::PLUS) {
                        // Check for STRING_INTERPOLATION as well as LITERAL
                        auto isStringNode = [](ASTNode* node) {
                            switch (node->nodeType) {
                                case ASTNodeType::STRING_INTERPOLATION:
                                    return true;
                                case ASTNodeType::LITERAL:
                                    return static_cast<LiteralExprNode*>(node)->value.type == TokenType::STRING_LIT;
                                default:
                                    return false;
                            }
                        };

                        bool isString = isStringNode(n->left.get()) || isStringNode(n->right.get());

                        if (isString) {
                            out << "glowAdd({ ";
                            std::function<void(ASTNode*)> collect = [&](ASTNode* operand) {
                                if (operand->nodeType == ASTNodeType::BINARY && 
                                    static_cast<BinaryExprNode*>(operand)->op.type == TokenType::PLUS) {
                                    auto* b = static_cast<BinaryExprNode*>(operand);
                                    collect(b->left.get());
                                    out << ", ";
                                    collect(b->right.get());
                                } else {
                                    // FIX: Update the switch to handle the node types correctly
                                    switch (operand->nodeType) {
                                        case ASTNodeType::STRING_INTERPOLATION:
                                        case ASTNodeType::LITERAL: {
                                            bool isRawString = (operand->nodeType == ASTNodeType::STRING_INTERPOLATION) || 
                                                            (static_cast<LiteralExprNode*>(operand)->value.type == TokenType::STRING_LIT);
                                            if (isRawString) {
                                                write(operand, out, indent);
                                            } else {
                                                out << "glow_to_string("; write(operand, out, indent); out << ")";
                                            }
                                            break;
                                        }
                                        default:
                                            out << "glow_to_string("; write(operand, out, indent); out << ")";
                                            break;
                                    }
                                }
                            };
                            collect(n);
                            out << " })";
                        } else {
                            // Native performance path for x + y
                            write(n->left.get(), out, indent);
                            out << " + ";
                            write(n->right.get(), out, indent);
                        }
                    }
                        else {
                            // --- Standard Arithmetic/Logic Path ---
                            // We removed the outer '(' for cleaner generated code
                            write(n->left.get(), out, indent); 
                            
                            switch(n->op.type) {
                                case TokenType::MINUS:         out << " - "; break;
                                case TokenType::STAR:          out << " * "; break;
                                case TokenType::SLASH:         out << " / "; break;
                                case TokenType::PERCENT:       out << " % "; break;
                                case TokenType::AND:           out << " && "; break;
                                case TokenType::OR:            out << " || "; break;
                                case TokenType::EQUAL_EQUAL:   out << " == "; break;
                                case TokenType::BANG_EQUAL:    out << " != "; break;
                                case TokenType::LESS:          out << " < ";  break;
                                case TokenType::GREATER:       out << " > ";  break;
                                case TokenType::LESS_EQUAL:    out << " <= "; break;
                                case TokenType::GREATER_EQUAL: out << " >= "; break;
                                case TokenType::INSERT:        out << " << "; break;
                                case TokenType::EXTRACT:       out << " >> "; break;
                                default:                       out << " " << n->op.lexeme << " "; break; 
                            }
                            
                            write(n->right.get(), out, indent);
                        }
                        break;
                    }
                }
                break;
            }

            case ASTNodeType::UNARY: {
                auto* n = static_cast<UnaryExprNode*>(node);
                
                if (n->isPostfix) {
                    // Output: i++
                    write(n->right.get(), out, indent);
                    out << (n->op.type == TokenType::PLUS_PLUS ? "++" : "--");
                }
                else {
                    // Output: ++i, !, -, ~, etc.
                    switch (n->op.type) {
                        case TokenType::AMPERSAND:  out << "&"; break;
                        case TokenType::STAR:       out << "*"; break;
                        case TokenType::BANG:       out << "!"; break;
                        case TokenType::TILDE:      out << "~"; break;
                        case TokenType::MINUS:      out << "-"; break;
                        case TokenType::PLUS_PLUS:   out << "++"; break;
                        case TokenType::MINUS_MINUS: out << "--"; break;
                        default:                    out << n->op.lexeme; break;
                    }
                    write(n->right.get(), out, indent);
                }
                break;
            }

            case ASTNodeType::TERNARY: {
                auto* n = static_cast<TernaryExprNode*>(node);
                out << "(";
                write(n->condition.get(), out, indent);
                out << " ? ";
                write(n->trueExpr.get(), out, indent);
                out << " : ";
                write(n->falseExpr.get(), out, indent);
                out << ")";
                break;
            }

            case ASTNodeType::STREAM: {
                auto* n = static_cast<StreamNode*>(node);
                if (n->streamType.lexeme == "ostream") {
                    out << "std::cout << ";
                    write(n->value.get(), out, indent);
                } else if (n->streamType.lexeme == "istream") {
                    out << "std::cin >> ";
                    write(n->value.get(), out, indent);
                }
                break;
            }

            case ASTNodeType::CHAIN: {
                auto* n = static_cast<ChainNode*>(node);
                for (size_t i = 0; i < n->links.size(); ++i) {
                    auto* link = static_cast<StreamNode*>(n->links[i].get());
                    
                    // Only the very first link in the chain writes "std::cout" or "std::cin"
                    if (i == 0)
                        out << (link->streamType.lexeme == "ostream" ? "std::cout" : "std::cin");
                    
                    // Write the operator (<< or >>) and the value
                    out << (link->op.type == TokenType::INSERT ? " << " : " >> ");
                    write(link->value.get(), out, indent);
                }
                break;
            }

            case ASTNodeType::LITERAL: {
                auto* n = static_cast<LiteralExprNode*>(node);

                if (n->value.lexeme.size() > 18 && n->value.lexeme.substr(0, 2) == "0x") {
                    // If it starts with '-', it's a signed i128, otherwise ui128
                    if (n->value.lexeme[0] == '-')
                        out << "i128(\"" << n->value.lexeme << "\")";
                    else
                        out << "ui128(\"" << n->value.lexeme << "\")"; 
                    break;
                }
                
                switch (n->value.type) {
                    case TokenType::PANIC:
                        // This maps the 'panic;' keyword directly to a C++ exit
                        out << "std::cerr << \"SYSTEM PANIC!\" << std::endl; std::exit(1)";
                        break;

                    case TokenType::BARE_LIT:
                        out << "nullptr";
                        break;

                    case TokenType::STRING_LIT:
                        // FIX: Re-escape "cooked" characters so C++ is valid
                        out << "\"";
                        for (char c : n->value.lexeme) {
                            switch (c) {
                                case '\n': out << "\\n"; break;
                                case '\t': out << "\\t"; break;
                                case '\\': out << "\\\\"; break;
                                case '\"': out << "\\\""; break;
                                default:   out << c;      break;
                            }
                        }
                        out << "\"";
                        break;

                    case TokenType::CHAR_LIT:
                        out << "'" << n->value.lexeme << "'";
                        break;

                    case TokenType::NUMBER_LIT:
                        out << n->value.lexeme;
                        break;

                    case TokenType::NEWLINE:
                        out << "'\\n'"; // Native char: fast, buffered
                        break;

                    case TokenType::TAB:
                        out << "'\\t'";
                        break;

                    case TokenType::FLUSH:
                        out << "std::endl"; // Standard manipulator
                        break;

                    default:
                        out << n->value.lexeme;
                        break;
                }
                break;
            }

            case ASTNodeType::VARIABLE: {
                auto* n = static_cast<VariableExprNode*>(node);
                if (n->name.lexeme == "my")
                    out << "this"; // Pass the pointer to the current instance
                else out << n->name.lexeme;
                break;
            }

            case ASTNodeType::ASSIGNMENT: {
                auto* n = static_cast<AssignmentNode*>(node);
                
                // Write the target
                write(n->target.get(), out, indent); 
                out << " " << n->op.lexeme << " ";

                // Identify target type for 128-bit wrapping
                TokenType targetType = TokenType::I32; 
                
                // Handle the value
                handlePotential128(targetType, n->value.get(), out, indent);
                break;
            }

            case ASTNodeType::COLLECTION: {
                auto* n = static_cast<CollectionLiteralNode*>(node);
                out << "{ ";
                for (size_t i = 0; i < n->elements.size(); ++i) {
                    write(n->elements[i].get(), out, indent);
                    if (i < n->elements.size() - 1) out << ", ";
                }
                out << " }";
                break;
            }

            case ASTNodeType::INDEXING: {
                auto* n = static_cast<IndexExprNode*>(node);
                write(n->target.get(), out, indent);
                out << "[";
                write(n->index.get(), out, indent);
                out << "]";
                break;
            }

            case ASTNodeType::RETURN: {
                auto* n = static_cast<ReturnNode*>(node);
                out << "return ";
                if (n->value) write(n->value.get(), out, indent);
                break;
            }

            case ASTNodeType::QUALIFIED_ID: {
                auto* n = static_cast<QualifiedIDNode*>(node);
                for (size_t i = 0; i < n->path.size(); ++i) {
                    out << n->path[i].lexeme;
                    if (i < n->path.size() - 1) {
                        out << "::"; 
                    }
                }
                break;
            }

            case ASTNodeType::ENUM: {
                auto* n = static_cast<EnumNode*>(node);
                out << "enum class " << n->name.lexeme << " : " << mapType(n->underlyingType.type) << " {\n";
                // Iterate entries...
                out << "};\n";
                break;
            }

            case ASTNodeType::UNIT: {
                auto* n = static_cast<UnitNode*>(node);
                
                // Class Header
                out << "class " << n->name.lexeme;
                if (!n->parentName.lexeme.empty())
                    out << " : public " << n->parentName.lexeme;
                out << " {\n";

                // Context Tracking
                std::string oldUnitName = currentUnitName;
                std::string oldParentName = currentParentName;
                currentUnitName = n->name.lexeme;
                currentParentName = n->parentName.lexeme;

                // --- AUTOMATIC GENERATION START ---
                // Rule: We generate a public default constructor so inheritance works 
                // without forcing the user to write an empty init() in Glow.
                out << "public:\n";
                writeIndent(out, indent + 1);
                out << currentUnitName << "() {}\n\n";
                // Since we just printed 'public:', start our lazy tracker there.
                TokenType lastVisibility = TokenType::PUB; 
                // ----------------------------------

                // Lazy Visibility Analysis
                bool hasAnyVisibilityKeyword = false;
                for (auto& memberNode : n->members) {
                    auto* mWrap = static_cast<UnitMemberNode*>(memberNode.get());
                    if (mWrap->visibility.type != TokenType::PRIV) {
                        hasAnyVisibilityKeyword = true;
                        break;
                    }
                }

                // Member Iteration
                for (auto& memberNode : n->members) {
                    auto* mWrap = static_cast<UnitMemberNode*>(memberNode.get());

                    if (mWrap->visibility.type != lastVisibility) {
                        bool shouldPrint = true;
                        // Respect the rule: Only print if Glow has it itself
                        if (lastVisibility == TokenType::EOF_TOKEN && 
                            mWrap->visibility.type == TokenType::PRIV && 
                            !hasAnyVisibilityKeyword) {
                            shouldPrint = false;
                        }

                        if (shouldPrint) {
                            lastVisibility = mWrap->visibility.type;
                            switch (lastVisibility) {
                                case TokenType::PUB:  out << "public:\n";    break;
                                case TokenType::PRIV: out << "private:\n";   break;
                                case TokenType::PROT: out << "protected:\n"; break;
                                default: break; 
                            }
                        } else lastVisibility = mWrap->visibility.type;
                    }

                    writeIndent(out, indent + 1);
                    write(mWrap->member.get(), out, indent + 1);

                    if (mWrap->member->nodeType == ASTNodeType::VAR_DECL) out << ";\n";
                    else out << "\n";
                }

                // Cleanup
                out << "};\n";
                currentUnitName = oldUnitName;
                currentParentName = oldParentName;
                break;
            }

            case ASTNodeType::INIT: {
                auto* n = static_cast<InitNode*>(node);

                out << currentUnitName << "("; 
                for (size_t i = 0; i < n->parameters.size(); ++i) {
                    out << mapType(n->parameters[i]->type.type) << " " << n->parameters[i]->name.lexeme;
                    if (i < n->parameters.size() - 1) out << ", ";
                }
                out << ")";

                // --- THE INITIALIZER LIST LOGIC ---
                bool hasColon = false;

                // Handle Inheritance first
                if (!currentParentName.empty()) {
                    out << " : " << currentParentName << "()";
                    hasColon = true;
                }

                // Handle Glow initializers (init() : member(val))
                if (!n->initializers.empty()) {
                    // If we already have a colon from the parent, use a comma. Otherwise, use a colon.
                    out << (hasColon ? ", " : " : ");
                    for (size_t i = 0; i < n->initializers.size(); ++i) {
                        out << n->initializers[i].first.lexeme << "(";
                        write(n->initializers[i].second.get(), out, indent);
                        out << ")";
                        if (i < n->initializers.size() - 1) out << ", ";
                    }
                }

                // The Body
                write(n->body.get(), out, indent); 
                break;
            }

            case ASTNodeType::MEMBER_ACCESS: {
                auto* n = static_cast<MemberAccessNode*>(node);
                
                // Determine if the base is 'my'
                bool isMy = false;
                if (n->structure->nodeType == ASTNodeType::VARIABLE)
                    if (static_cast<VariableExprNode*>(n->structure.get())->name.lexeme == "my")
                        isMy = true;
                if (isMy)
                    out << "this->"; // Glow 'my.' or 'my->' always becomes C++ 'this->'
                else {
                    write(n->structure.get(), out, indent);
                    out << (n->op.type == TokenType::ARROW ? "->" : ".");
                }
                out << n->member.lexeme;
                break;
            }

            case ASTNodeType::DELETE: {
                auto* n = static_cast<DeleteNode*>(node);
                
                // Emit the Glow-origin comment for debugging
                out << "/* Glow del */ ";

                // Identify the target
                if (n->target->nodeType == ASTNodeType::VARIABLE) {
                    auto* var = static_cast<VariableExprNode*>(n->target.get());
                    std::string name = var->name.lexeme;

                    out << "{ ";
                    write(n->target.get(), out, indent); 
                    out << " = {}; "; // Universal zero-initializer
                    out << "}";
                }
                else {
                    write(n->target.get(), out, indent);
                    out << " = {}"; 
                }
                break;
            }

            case ASTNodeType::GC_TOGGLE: {
                auto* n = static_cast<GCToggleNode*>(node);
                if (n->enable) {
                    out << "// GLOW_GC_SYNC_POINT\n";
                    writeIndent(out, indent);
                    out << "std::atomic_thread_fence(std::memory_order_seq_cst)";
                }
                else
                    out << "// GLOW_GC_SUSPENDED (Manual Memory Management Active)";
                break;
            }

            case ASTNodeType::CAST: {
                auto* n = static_cast<CastExprNode*>(node);
                out << "static_cast<" << mapType(n->targetType.type) << ">(";
                write(n->expression.get(), out, indent);
                out << ")";
                break;
            }

            case ASTNodeType::STRING_INTERPOLATION: {
                auto* n = static_cast<StringInterpolationNode*>(node);
                
                // Single literal chunk is just a standard string
                if (n->parts.size() == 1 && n->parts[0]->nodeType == ASTNodeType::LITERAL)
                    write(n->parts[0].get(), out, indent);
                else {
                    // Uses the runtime's variadic formatter
                    out << "glowFormat({ ";
                    size_t exprIdx = 0;
                    for (size_t i = 0; i < n->parts.size(); ++i) {
                        if (n->parts[i]->nodeType == ASTNodeType::LITERAL)
                            // Literals don't need conversion, but write() handles their quotes
                            write(n->parts[i].get(), out, indent);
                        else {
                            // Expressions need conversion to string
                            // We check if it was ${} (safe) or $() (unsafe)
                            bool safe = n->isSafe[exprIdx++];
                            out << (safe ? "glow_to_string(" : "glow_to_string(");
                            write(n->parts[i].get(), out, indent);
                            out << ")";
                        }
                        if (i < n->parts.size() - 1) out << ", ";
                    }
                    out << " })";
                }
                break;
            }

            default: break;
        }
    }

    void Transpiler::handlePotential128(TokenType targetType, ASTNode* valueNode, std::ofstream& out, int indent) {
        if ((targetType == TokenType::I128 || targetType == TokenType::UI128) && 
            valueNode->nodeType == ASTNodeType::LITERAL) {
            
            auto* lit = static_cast<LiteralExprNode*>(valueNode);
            if (lit->value.type == TokenType::NUMBER_LIT) {
                // Mapping Logic:
                // i128 uses string_to_i128
                // ui128 uses string_to_ui128
                std::string wrapper = (targetType == TokenType::I128) ? "string_to_i128" : "string_to_ui128";
                
                out << wrapper << "(\"" << lit->value.lexeme << "\")";
                return;
            }
        }
        
        // Fallback: write normally if not a 128-bit assignment
        write(valueNode, out, indent);
    }

    void Transpiler::writeBitwise(BinaryExprNode* node, std::ofstream& out, int indent) {
        out << "(";
        switch (node->op.type) {
            case TokenType::NAND:
                out << "~("; write(node->left.get(), out, indent); out << " & "; write(node->right.get(), out, indent); out << ")";
                break;
            case TokenType::NOR:
                out << "~("; write(node->left.get(), out, indent); out << " | "; write(node->right.get(), out, indent); out << ")";
                break;
            case TokenType::AMPERSAND:
                write(node->left.get(), out, indent); out << " & "; write(node->right.get(), out, indent);
                break;
            case TokenType::PIPE:
                write(node->left.get(), out, indent); out << " | "; write(node->right.get(), out, indent);
                break;
            case TokenType::CARET:
                write(node->left.get(), out, indent); out << " ^ "; write(node->right.get(), out, indent);
                break;
            default: break;
        }
        out << ")";
    }

    std::string Transpiler::mapType(TokenType type) {
        switch (type) {
            // --- Fixed-width Integers ---
            case TokenType::I16:    return "int16_t";
            case TokenType::I32:    return "int32_t";
            case TokenType::I64:    return "int64_t";
            case TokenType::I128:   return "i128";
            case TokenType::UI16:   return "uint16_t";
            case TokenType::UI32:   return "uint32_t";
            case TokenType::UI64:   return "uint64_t";
            case TokenType::UI128:  return "ui128";

            // --- Floats & Standard Types ---
            case TokenType::F32:    return "float";
            case TokenType::F64:    return "double";
            case TokenType::CHAR:   return "char";
            case TokenType::STR:    return "std::string";
            case TokenType::BOOL:   return "bool";
            case TokenType::HOLLOW: return "void";

            // --- Smart Pointer Bases (Internal use) ---
            case TokenType::WEAK:   return "std::weak_ptr";
            case TokenType::UNIQUE: return "std::unique_ptr";
            case TokenType::SHARED: return "std::shared_ptr";

            // --- Fallback ---
            default:                return "auto";
        }
    }

    void Transpiler::writeIndent(std::ofstream& out, int indent) {
        for (int i = 0; i < indent; ++i) out << "    ";
    }
}
