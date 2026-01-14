#include "vm/VirtualMachine.h"
#include "compiler/Lexer.h"
#include "compiler/Parser.h"
#include "common/SourceBuffer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>
#include <iomanip>
#include <algorithm>
#include <unordered_set>

namespace Glow {

    // Internal exception to handle the 'ret' keyword across nested blocks
    struct ReturnException {
        Value value;
    };

    VirtualMachine::VirtualMachine() {
        globals.enterScope(); // Initialize global scope
    }

    void VirtualMachine::interpret(ASTNode* root) {
        try {
            // Pass 1: Global Registration
            // This executes top-level declarations (fn, unit, enum)
            execute(root); 

            // Pass 2: Entry Point Search
            const auto& symbols = globals.getValues();
            FunctionNode* entryPoint = nullptr;

            for (auto const& [name, sym] : symbols) {
                if (sym.value.type == ValueType::REF) {
                    void* rawPtr = std::get<void*>(sym.value.as);
                    if (!rawPtr) continue;

                    // SAFETY GUARD: Variable Pointer validation
                    // Ensure this isn't a pointer to a string in the address pool.
                    // AST Nodes are allocated on the heap separately from the addressPool.
                    bool isVarAddr = std::any_of(addressPool.begin(), addressPool.end(),
                        [rawPtr](const auto& ptr) { return ptr.get() == rawPtr; });
                    
                    if (isVarAddr) continue;

                    // Safe to inspect nodeType now
                    auto* node = static_cast<ASTNode*>(rawPtr);
                    if (node->nodeType == ASTNodeType::FUNCTION) {
                        auto* func = static_cast<FunctionNode*>(node);
                        if (func->isEntry) {
                            entryPoint = func;
                            break; 
                        }
                    }
                }
            }

            // Pass 3: Program Entry
            if (!entryPoint)
                executePanic("Configuration Error: No 'entry' function found in source.");
            
            if (!entryPoint->body)
                executePanic("Runtime Error: 'entry' function declared without implementation.");

            // Run the code
            executeBlock(entryPoint->body.get()); 

        }
        catch (const ReturnException& e) {
            // Entry point exited via 'ret'
        }
        catch (const std::exception& e) {
            executePanic("Internal Engine Fault: " + std::string(e.what()));
        }
        catch (...) {
            executePanic("Critical Hardware Fault: Unhandled exception during interpretation.");
        }
    }

    // --- High-Level Execution Logic ---

    void VirtualMachine::execute(ASTNode* node) {
        if (!node) [[unlikely]] return;

        switch (node->nodeType) {
            case ASTNodeType::BLOCK:      executeBlock(static_cast<BlockNode*>(node)); break;
            case ASTNodeType::DELETE:     executeDelete(static_cast<DeleteNode*>(node)); break;
            case ASTNodeType::VAR_DECL:   executeVarDecl(static_cast<VarDeclNode*>(node)); break;
            case ASTNodeType::IF:         executeIf(static_cast<IfNode*>(node)); break;
            case ASTNodeType::LOOP:       executeLoop(static_cast<LoopNode*>(node)); break;
            case ASTNodeType::STREAM:     executeStream(static_cast<StreamNode*>(node)); break;
            case ASTNodeType::CHAIN:      executeChain(static_cast<ChainNode*>(node)); break;
            case ASTNodeType::SWITCH:     executeSwitch(static_cast<SwitchNode*>(node)); break;
            case ASTNodeType::ASSIGNMENT: executeAssignment(static_cast<AssignmentNode*>(node)); break;
            case ASTNodeType::TRY_CATCH:  executeTryCatch(static_cast<TryCatchNode*>(node)); break;
            case ASTNodeType::FUNCTION:   executeFunction(static_cast<FunctionNode*>(node)); break;
            case ASTNodeType::UNIT:       executeUnit(static_cast<UnitNode*>(node)); break;
            case ASTNodeType::ENUM:       executeEnum(static_cast<EnumNode*>(node)); break;
            case ASTNodeType::INIT:       executeBlock(static_cast<InitNode*>(node)->body.get()); break; 
            case ASTNodeType::KILL:       executeKill(static_cast<KillNode*>(node)); break;
            
            case ASTNodeType::RETURN: {
                auto* ret = static_cast<ReturnNode*>(node);
                Value val = ret->value ? evaluate(ret->value.get()) : Value(); 
                throw ReturnException{val};
            }
            default:
                evaluate(node); // Handle expression statements
                break;
        }
    }

    void VirtualMachine::executeBlock(BlockNode* node) {
        for (auto& stmt : node->statements) execute(stmt.get());
    }

    void VirtualMachine::executeVarDecl(VarDeclNode* node) {
        Value val;
        
        if (node->initializer) {
            val = evaluate(node->initializer.get());
            
            if (!typeMatch(node->type.type, val.type))
                // Removed the non-existent helper functions
                throw std::runtime_error("Runtime Type Error: Variable '" + node->name.lexeme + "' has a type mismatch.");
        }
        else
            val = Value(); 
        globals.define(node->name.lexeme, val);
    }

    void VirtualMachine::executeAssignment(AssignmentNode* node) {
        Value rhs = evaluate(node->value.get());
        std::string targetSymbol;
        Value current;

        switch (node->target->nodeType) {
            case ASTNodeType::VARIABLE: {
                auto* var = static_cast<VariableExprNode*>(node->target.get());
                targetSymbol = var->name.lexeme;
                current = globals.get(targetSymbol);

                if (current.type == ValueType::REF) {
                    if (void* ptr = std::get<void*>(current.as)) {
                        std::string* addr = static_cast<std::string*>(ptr);
                        if (globals.exists(*addr)) {
                            targetSymbol = *addr;
                            current = globals.get(targetSymbol);
                        }
                    }
                }
                break;
            }

            case ASTNodeType::MEMBER_ACCESS: {
                auto* access = static_cast<MemberAccessNode*>(node->target.get());
                Value base = evaluate(access->structure.get());
                
                if (base.type != ValueType::REF)
                    throw std::runtime_error("Error: Cannot assign to member of non-pointer type.");
                    
                void* ptr = std::get<void*>(base.as);
                if (!ptr) throw std::runtime_error("Error: Null reference in member assignment.");

                // Resolve the mangled name (like "Player1::health")
                std::string* instanceName = static_cast<std::string*>(ptr);
                targetSymbol = *instanceName + "::" + access->member.lexeme;
                
                // Safety check: ensure the member actually exists before overwrite
                if (!globals.exists(targetSymbol))
                    throw std::runtime_error("Error: Member '" + access->member.lexeme + "' does not exist.");
                    
                current = globals.get(targetSymbol);
                break;
            }

            case ASTNodeType::INDEXING: {
                auto* indexNode = static_cast<IndexExprNode*>(node->target.get());
                Value indexVal = evaluate(indexNode->index.get());
                
                // Use your standalone helper to get a safe integer for the key
                int64_t idx = asI64(indexVal); 

                if (indexNode->target->nodeType == ASTNodeType::VARIABLE) {
                    auto* var = static_cast<VariableExprNode*>(indexNode->target.get());
                    // Mangle the name: "arr[68]" etc.
                    targetSymbol = var->name.lexeme + "[" + std::to_string(idx) + "]";
                    current = globals.get(targetSymbol);
                }
                break;
            }

            default:
                throw std::runtime_error("Error: Invalid assignment l-value.");
        }

        if (node->op.type == TokenType::EQUAL) {
            globals.assign(targetSymbol, rhs);
            return;
        }

        // --- Operator Switch ---
        TokenType baseOp;
        switch (node->op.type) {
            case TokenType::PLUS_EQUAL:    baseOp = TokenType::PLUS; break;
            case TokenType::MINUS_EQUAL:   baseOp = TokenType::MINUS; break;
            case TokenType::STAR_EQUAL:    baseOp = TokenType::STAR; break;
            case TokenType::SLASH_EQUAL:   baseOp = TokenType::SLASH; break;
            case TokenType::PIPE_EQUAL:    baseOp = TokenType::PIPE; break;
            case TokenType::AMP_EQUAL:     baseOp = TokenType::AMPERSAND; break;
            case TokenType::CARET_EQUAL:   baseOp = TokenType::CARET; break;
            case TokenType::TILDE_EQUAL:   baseOp = TokenType::TILDE; break; 
            case TokenType::NAND_EQUAL:    baseOp = TokenType::NAND; break;
            case TokenType::NOR_EQUAL:     baseOp = TokenType::NOR; break;
            default: return;
        }

        Value result = applyOperator(current, Token{baseOp, "", 0, 0}, rhs);
        globals.assign(targetSymbol, result);
    }

    void VirtualMachine::executeIf(IfNode* node) {
        // Evaluate the initial 'if'
        Value cond = evaluate(node->condition.get());

        if (isTruthy(cond)) {
            executeBlock(node->ifBody.get());
            return;
        }

        // Check each 'elif' branch in order
        for (const auto& branch : node->elifBranches) {
            Value elifCond = evaluate(branch.condition.get());
            
            if (isTruthy(elifCond)) {
                executeBlock(branch.body.get());
                return; // Exit as soon as a condition is met
            }
        }

        // Fall back to 'else' if everything else failed
        if (node->elseBody)
            executeBlock(node->elseBody.get());
    }

    void VirtualMachine::executeLoop(LoopNode* node) {
        globals.enterScope(); // Loops have their own scope

        switch (node->variant) {
            case LoopVariant::WHILE: {
                while (true) {
                    if (node->condition && !isTruthy(evaluate(node->condition.get()))) break;
                    if (!node->condition) {} // infinite loop: loop()
                    
                    executeBlock(node->body.get());
                }
                break;
            }

            case LoopVariant::FOR: {
                if (node->init) execute(node->init.get());
                
                while (true) {
                    if (node->condition && !isTruthy(evaluate(node->condition.get()))) break;
                    
                    executeBlock(node->body.get());
                    if (node->increment) evaluate(node->increment.get());
                }
                break;
            }

            case LoopVariant::RANGE: {
                Value collection = evaluate(node->condition.get());
                if (collection.type != ValueType::ARRAY) 
                    throw std::runtime_error("Error: Expected array in range loop.");

                // Cast back to our raw structure
                GlowArray* arr = static_cast<GlowArray*>(std::get<void*>(collection.as));
                
                std::string iteratorName = (node->init->nodeType == ASTNodeType::VAR_DECL) 
                    ? static_cast<VarDeclNode*>(node->init.get())->name.lexeme 
                    : static_cast<VariableExprNode*>(node->init.get())->name.lexeme;

                // Iterate the raw memory
                for (size_t i = 0; i < arr->size; ++i) {
                    globals.enterScope();
                    globals.define(iteratorName, arr->data[i]);
                    
                    try {
                        executeBlock(node->body.get());
                    }
                    catch (const ReturnException& e) {
                        globals.exitScope();
                        throw; 
                    }
                    globals.exitScope();
                }
                break;
            }
        }

        globals.exitScope();
    }

    void VirtualMachine::executeSwitch(SwitchNode* node) {
        Value conditionVal = evaluate(node->condition.get());
        for (auto& swCase : node->cases) {
            if (swCase->isDefault) {
                executeBlock(swCase->body.get());
                return;
            }
            Value matchVal = evaluate(swCase->matchValue.get());
            Value comparison = applyOperator(conditionVal, Token{TokenType::EQUAL_EQUAL, "==", 0, 0}, matchVal);
            if (isTruthy(comparison)) {
                executeBlock(swCase->body.get());
                return;
            }
        }
    }

    void VirtualMachine::executeStream(StreamNode* node) {
        switch (node->op.type) {
            case TokenType::INSERT: { // Glow: ostream << value;
                Value val = evaluate(node->value.get());
                
                if (node->streamType.lexeme == "ostream") {
                    // Delegation: Let the Value class handle formatting and output
                    printValue(val); 
                }
                break;
            }

            case TokenType::EXTRACT: { // Glow: istream >> variable;
                if (node->value->nodeType == ASTNodeType::VARIABLE) {
                    auto* var = static_cast<VariableExprNode*>(node->value.get());
                    Value current = globals.get(var->name.lexeme);

                    std::string writeTarget = var->name.lexeme;
                    if (current.type == ValueType::REF) {
                        // Tunnel through the pointer to the address it holds
                        std::string* addr = static_cast<std::string*>(std::get<void*>(current.as));
                        if (addr && globals.exists(*addr)) {
                            writeTarget = *addr;
                            current = globals.get(writeTarget); 
                        }
                    }
                    
                    std::string input;
                    if (!(std::cin >> input)) return;

                    Value finalInput;
                    // Dispatch based on the variable's established type
                    switch (current.type) {
                        case ValueType::I32:   finalInput = Value(std::stoi(input)); break;
                        case ValueType::I128:  finalInput = Value(string_to_i128(input)); break;
                        case ValueType::UI128: finalInput = Value(string_to_ui128(input)); break;
                        case ValueType::F64:   finalInput = Value(std::stod(input)); break;
                        default:               finalInput = Value(input); break;
                    }

                    // Assign to the resolved target (variable or address)
                    globals.assign(writeTarget, finalInput);
                }
                break;
            }

            default: break;
        }
    }

    void VirtualMachine::executeChain(ChainNode* node) {
        for (auto& link : node->links)
        // Each link is a StreamNode, so we call executeStream
        executeStream(static_cast<StreamNode*>(link.get()));
    }

    void VirtualMachine::executeTryCatch(TryCatchNode* node) {
        try {
            executeBlock(node->tryBody.get());
        } catch (...) {
            if (node->catchBody) executeBlock(node->catchBody.get());
        }
    }

    void VirtualMachine::executePanic(const std::string& message) {
        std::cerr << "\n" << std::string(40, '!') << std::endl;
        std::cerr << "         GLOW SYSTEM PANIC          " << std::endl;
        std::cerr << "  MSG: " << message << std::endl;
        std::cerr << std::string(40, '!') << "\n" << std::endl;

        // --- Post-Mortem Memory Analysis ---
        std::cerr << "--- Memory Management State ---" << std::endl;
        std::cerr << "  GC Status    : " << (GCEnabled ? "ENABLED" : "DISABLED") << std::endl;
        std::cerr << "  Address Pool : " << addressPool.size() << " strings" << std::endl;

        // --- Post-Mortem Global State ---
        std::cerr << "\n--- Post-Mortem Global State ---" << std::endl;
        const auto& symbols = globals.getValues();
        if (symbols.empty())
            std::cerr << "  (No symbols defined)" << std::endl;
        else {
            for (auto const& [name, sym] : symbols) {
                // Standard formatting for easier reading
                std::string valStr = (sym.value.type == ValueType::REF) ? "<pointer>" : sym.value.toString();
                std::cerr << "  " << std::left << std::setw(15) << name << " : " << valStr << std::endl;
            }
        }

        std::cerr << "\nExecution Halted." << std::endl;
        std::exit(1); 
    }

    void VirtualMachine::executeFunction(FunctionNode* node) {
        // In a real VM, this stores the function AST in a map
        globals.define(node->name.lexeme, Value((void*)node));
    }

    void VirtualMachine::executeUnit(UnitNode* node) {
        globals.define(node->name.lexeme, Value((void*)node));
    }

    void VirtualMachine::executeEnum(EnumNode* node) {
        // We create a global symbol that points to a "static" scope for this enum
        for (auto& entry : node->entries) {
            std::string fullName = node->name.lexeme + "::" + entry.first.lexeme;
            globals.defineGlobal(fullName, evaluate(entry.second.get()), true); // Enums are always const
        }
    }

    void VirtualMachine::executeDelete(DeleteNode* node) {
        // reusable check: ensuring we are deleting a resolvable identifier
        if (node->target->nodeType != ASTNodeType::VARIABLE)
            throw std::runtime_error("Error: 'del' must target a resolvable variable identifier.");
        auto* var = static_cast<VariableExprNode*>(node->target.get());
        std::string name = var->name.lexeme;

        // Remove from symbol table to drop the reference.
        // If this was the last reference to a REF (pointer), 
        // the next collectGarbage() call will reclaim the memory.
        if (globals.exists(name))
            globals.remove(name);
    }

    void VirtualMachine::executeKill(KillNode* node) {
        // Destructors are executed during object cleanup (RAII).
        if (node->body) executeBlock(node->body.get());
    }

    void VirtualMachine::executeGCToggle(GCToggleNode* node) {
        this->GCEnabled = node->enable;
        // If GC turned back ON, perform an immediate sweep 
        // to catch anything leaked while it was OFF.
        if (this->GCEnabled) collectGarbage();
    }

    void VirtualMachine::collectGarbage() {
        if (!GCEnabled) return;

        size_t stringsBefore = addressPool.size();
        size_t arraysBefore = arrayPool.size();

        std::unordered_set<void*> reachable;
        
        // Helper to recursively mark values (important for nested arrays/strings)
        auto markValue = [&](const Value& v, auto& self) -> void {
            if (v.type == ValueType::REF)
                reachable.insert(std::get<void*>(v.as));
            else if (v.type == ValueType::ARRAY) {
                GlowArray* arr = static_cast<GlowArray*>(std::get<void*>(v.as));
                if (arr && reachable.find(arr) == reachable.end()) {
                    reachable.insert(arr);
                    // Scan the array's contents for nested references
                    for (size_t i = 0; i < arr->size; ++i)
                        self(arr->data[i], self);
                }
            }
        };

        // --- Mark Phase ---
        for (const auto& scopeMap : globals.getAllScopes())
            for (auto const& [name, symbol] : scopeMap)
                markValue(symbol.value, markValue);
        
        // --- Sweep Phase: Strings ---
        addressPool.erase(
            std::remove_if(addressPool.begin(), addressPool.end(),
                [&reachable](const std::unique_ptr<std::string>& ptr) {
                    return reachable.find(ptr.get()) == reachable.end();
                }),
            addressPool.end()
        );

        // --- Sweep Phase: Arrays ---
        arrayPool.erase(
            std::remove_if(arrayPool.begin(), arrayPool.end(),
                [&reachable](const std::unique_ptr<GlowArray>& ptr) {
                    // If the GlowArray object address isn't reachable, delete it
                    return reachable.find(ptr.get()) == reachable.end();
                }),
            arrayPool.end()
        );

        if (verbose) {
            size_t stringsFreed = stringsBefore - addressPool.size();
            size_t arraysFreed = arraysBefore - arrayPool.size();
            if (stringsFreed > 0 || arraysFreed > 0)
                std::cout << "[VM] GC Sweep: Freed " << stringsFreed << " strings and " 
                        << arraysFreed << " arrays." << std::endl;
        }
    }

    // --- Expression Evaluation Logic ---

    Value VirtualMachine::evaluate(ASTNode* node) {
        if (!node) return Value();

        switch (node->nodeType) {
            case ASTNodeType::LITERAL:       return evaluateLiteral(static_cast<LiteralExprNode*>(node));
            case ASTNodeType::BINARY:        return evaluateBinary(static_cast<BinaryExprNode*>(node));
            case ASTNodeType::UNARY:         return evaluateUnary(static_cast<UnaryExprNode*>(node));
            case ASTNodeType::TERNARY:       return evaluateTernary(static_cast<TernaryExprNode*>(node));
            case ASTNodeType::VARIABLE:      return lookUpVariable(static_cast<VariableExprNode*>(node)->name);
            case ASTNodeType::INDEXING:      return evaluateIndexing(static_cast<IndexExprNode*>(node));
            case ASTNodeType::COLLECTION:    return evaluateCollection(static_cast<CollectionLiteralNode*>(node));
            case ASTNodeType::CALL:          return evaluateCall(static_cast<CallExprNode*>(node));
            case ASTNodeType::CAST:          return evaluateCast(static_cast<CastExprNode*>(node));
            case ASTNodeType::MEMBER_ACCESS: return evaluateMemberAccess(static_cast<MemberAccessNode*>(node));
            case ASTNodeType::QUALIFIED_ID:  return evaluateQualifiedID(static_cast<QualifiedIDNode*>(node));
            case ASTNodeType::STRING_INTERPOLATION: 
                return evaluateStringInterpolation(static_cast<StringInterpolationNode*>(node));
            default: return Value();
        }
    }

    Value VirtualMachine::evaluateLiteral(LiteralExprNode* node) {
        const auto& t = node->value;
        switch (t.type) {
            case TokenType::NUMBER_LIT:
                // Handle 128-bit strings or Hex
                if (t.lexeme.find("0x") == 0 || t.lexeme.length() > 10) 
                    return Value(string_to_i128(t.lexeme));
                return Value(static_cast<int32_t>(std::stoll(t.lexeme)));
            case TokenType::NEWLINE:    return Value(ValueType::NEWLINE);
            case TokenType::TAB:        return Value(ValueType::TAB);
            case TokenType::FLUSH:      return Value(ValueType::FLUSH);
            case TokenType::STRING_LIT: return Value(t.lexeme);
            case TokenType::TRUE_LIT:   return Value(true);
            case TokenType::FALSE_LIT:  return Value(false);
            case TokenType::BARE_LIT:   return Value();
            default:                    return Value();
        }
    }

    Value VirtualMachine::evaluateBinary(BinaryExprNode* node) {
        Value left = evaluate(node->left.get());
        Value right = evaluate(node->right.get());
        return applyOperator(left, node->op, right);
    }

    Value VirtualMachine::evaluateUnary(UnaryExprNode* node) {
        // --- Pointer Palette Handling (& and *) ---
        // These are handled first because they operate on the variable name/address, not just the value.
        switch (node->op.type) {
            case TokenType::AMPERSAND: { // & (Address-of)
                if (node->right->nodeType == ASTNodeType::VARIABLE) {
                    auto* var = static_cast<VariableExprNode*>(node->right.get());
                    
                    // Stores the string in the pool so it's managed by the VM lifecycle
                    auto& storedAddr = addressPool.emplace_back(std::make_unique<std::string>(var->name.lexeme));
                    return Value((void*)storedAddr.get()); 
                }
                throw std::runtime_error("Error: Cannot take address of non-variable.");
            }
            case TokenType::STAR: { // * (Dereference)
                Value addr = evaluate(node->right.get());
                if (addr.type == ValueType::REF) {
                    std::string* varName = static_cast<std::string*>(std::get<void*>(addr.as));
                    return globals.get(*varName);
                }
                throw std::runtime_error("Error: Cannot dereference non-pointer type.");
            }
            default: break;
        }

        // --- Standard Value Extraction ---
        if (node->right->nodeType != ASTNodeType::VARIABLE) return Value();
        auto* var = static_cast<VariableExprNode*>(node->right.get());
        Value current = globals.get(var->name.lexeme);
        Value result = current;

        // --- Type-Specific Logic Dispatch ---
        switch (current.type) {
            // --- 128-bit Systems Unary ---
            case ValueType::I128: {
                i128 val = std::get<i128>(current.as);
                switch (node->op.type) {
                    case TokenType::PLUS_PLUS:   val = val + i128{0, 1}; break;
                    case TokenType::MINUS_MINUS: val = val - i128{0, 1}; break;
                    case TokenType::TILDE:       val = ~val; break;
                    case TokenType::MINUS:       val = i128{0, 0} - val; break;
                    case TokenType::BANG:        return Value(!(val.high || val.low));
                    default: break;
                }
                result = Value(val);
                break;
            }
            case ValueType::UI128: {
                ui128 val = std::get<ui128>(current.as);
                switch (node->op.type) {
                    case TokenType::PLUS_PLUS:   val = val + ui128{0, 1}; break;
                    case TokenType::MINUS_MINUS: val = val - ui128{0, 1}; break;
                    case TokenType::TILDE:       val = ~val; break;
                    case TokenType::BANG:        return Value(!(val.high || val.low));
                    default: break;
                }
                result = Value(val);
                break;
            }

            // --- Standard Integer Unary (i32, i64, ui64) ---
            case ValueType::I32: {
                int32_t n = std::get<int32_t>(current.as);
                switch (node->op.type) {
                    case TokenType::PLUS_PLUS:   n++; break;
                    case TokenType::MINUS_MINUS: n--; break;
                    case TokenType::TILDE:       n = ~n; break;
                    case TokenType::MINUS:       n = -n; break;
                    case TokenType::BANG:        return Value(!n);
                    default: break;
                }
                result = Value(n);
                break;
            }
            case ValueType::I64: {
                int64_t n = std::get<int64_t>(current.as);
                switch (node->op.type) {
                    case TokenType::PLUS_PLUS:   n++; break;
                    case TokenType::MINUS_MINUS: n--; break;
                    case TokenType::TILDE:       n = ~n; break;
                    case TokenType::MINUS:       n = -n; break;
                    case TokenType::BANG:        return Value(!n);
                    default: break;
                }
                result = Value(n);
                break;
            }
            case ValueType::UI64: {
                uint64_t n = std::get<uint64_t>(current.as);
                switch (node->op.type) {
                    case TokenType::PLUS_PLUS:   n++; break;
                    case TokenType::MINUS_MINUS: n--; break;
                    case TokenType::TILDE:       n = ~n; break;
                    case TokenType::BANG:        return Value(!n);
                    default: break;
                }
                result = Value(n);
                break;
            }

            case ValueType::BOOL: {
                switch (node->op.type) {
                    case TokenType::BANG: return Value(!std::get<bool>(current.as));
                    default: break;
                }
                break;
            }

            // --- Pointer Modifiers (unique* / vol*) ---
            case ValueType::REF: {
                switch (node->op.type) {
                    case TokenType::BANG: return Value(std::get<void*>(current.as) == nullptr);
                    // vol* logic: inherent in the VM as we fetch from SymbolTable every call.
                    default: break;
                }
                break;
            }

            default:
                return current;
        }

        // --- Mutation Application ---
        if (node->op.type == TokenType::PLUS_PLUS || node->op.type == TokenType::MINUS_MINUS) {
            // Always update the variable in the symbol table with the new result
            globals.assign(var->name.lexeme, result);
            
            // If it was postfix (i++), return the original value (0) instead of the new (1)
            if (node->isPostfix)
                return current; 
        }
        
        return result;
    }

    Value VirtualMachine::evaluateIndexing(IndexExprNode* node) {
        if (node->target->nodeType == ASTNodeType::VARIABLE) {
            auto* var = static_cast<VariableExprNode*>(node->target.get());
            Value indexVal = evaluate(node->index.get());
            
            // Resolve mangled name: "arr[0]"
            std::string mangledName = var->name.lexeme + "[" + indexVal.toString() + "]";
            return globals.get(mangledName);
        }
        throw std::runtime_error("Error: Unsupported indexing target.");
    }

    Value VirtualMachine::evaluateCollection(CollectionLiteralNode* node) {
        size_t count = node->elements.size();
        
        // Allocate the internal wrapper and raw buffer for the array
        auto arrObj = std::make_unique<GlowArray>(count);
        GlowArray* rawPtr = arrObj.get();

        // Populate
        for (size_t i = 0; i < count; ++i)
            rawPtr->data[i] = evaluate(node->elements[i].get());

        // Register in the GC pool and return the pointer as a Value
        arrayPool.push_back(std::move(arrObj));
        return Value(rawPtr);
    }

    Value VirtualMachine::evaluateStringInterpolation(StringInterpolationNode* node) {
        std::string buffer = "";
        size_t expressionIdx = 0;

        for (auto& part : node->parts) {
            if (part->nodeType == ASTNodeType::LITERAL)
                // Just append the text fragment
                buffer += evaluate(part.get()).toString();
            else {
                // Evaluate expression and check safety bit
                Value val = evaluate(part.get());
                bool safe = node->isSafe[expressionIdx++];

                if (safe) buffer += val.toString();    // ${} logic
                else      buffer += val.toRawString(); // $() logic
            }
        }
        return Value(buffer);
    }

    Value VirtualMachine::evaluateCall(CallExprNode* node) {
        // Resolve the target function/method
        Value callee = evaluate(node->callee.get());
        if (callee.type != ValueType::REF) [[unlikely]]
            throw std::runtime_error("Error: Identifier is not a callable function.");

        void* rawPtr = std::get<void*>(callee.as);
        if (!rawPtr) [[unlikely]]
            throw std::runtime_error("Error: Null function pointer reference.");
        auto* func = static_cast<FunctionNode*>(rawPtr);

        // --- Safety Check: Interface vs Implementation ---
        // Prevents calling stubs defined in .glh files without a matching .glw body
        if (!func->body) [[unlikely]]
            executePanic("Runtime Error: Function '" + func->name.lexeme + "' has no implementation (header only).");
        
        // Argument Evaluation
        // We evaluate arguments BEFORE entering the new scope to ensure expressions
        // are resolved in the caller's context, not the callee's.
        std::vector<Value> args;
        args.reserve(node->arguments.size());
        for (auto& arg : node->arguments)
            args.push_back(evaluate(arg.get()));

        // Setup Call Frame (Scope)
        globals.enterScope();

        // --- The 'my' Pointer Binding (this) ---
        // Optimized switch to bind instance context for method calls (-> or .)
        switch (node->op.type) {
            case TokenType::ARROW:
            case TokenType::DOT: {
                // Re-evaluate the callee base to get the instance pointer (REF)
                Value instance = evaluate(node->callee.get());
                
                // Bind 'my' as a first-class variable in the function scope.
                // This allows syntax like: someFunc(my) or print(my->health)
                globals.define("my", instance);
                break;
            }
            default:
                // Standard global function call; no 'my' context required
                break;
        }

        // Parameter Binding
        for (size_t i = 0; i < func->parameters.size(); ++i) 
            if (i < args.size())
                globals.define(func->parameters[i]->name.lexeme, args[i]);
            
        // Execution Logic with Return Handling
        Value result;
        try {
            executeBlock(func->body.get());
        }
        catch (const ReturnException& e) {
            // Capture the value passed to the 'ret' keyword
            result = e.value;
        }

        // Scope Cleanup and Automatic GC
        // We exit the scope first so the GC sees the local references as "dead"
        globals.exitScope(); 
        
        // The internal auto-detect clear-up
        collectGarbage(); 

        return result;
    }

    Value VirtualMachine::evaluateTernary(TernaryExprNode* node) {
        Value cond = evaluate(node->condition.get());
        // Lazy evaluation: only evaluate the branch that is actually taken.
        return isTruthy(cond) ? evaluate(node->trueExpr.get()) : evaluate(node->falseExpr.get());
    }

    Value VirtualMachine::evaluateCast(CastExprNode* node) {
        Value val = evaluate(node->expression.get());

        switch (node->targetType.type) {
            case TokenType::I128: 
                return Value(string_to_i128(val.toString()));
            case TokenType::UI128: 
                return Value(string_to_ui128(val.toString()));
            case TokenType::I32: 
                return Value(static_cast<int32_t>(std::stoll(val.toString())));
            case TokenType::STR: 
                return Value(val.toString());
            case TokenType::F64: 
                return Value(std::stod(val.toString()));
            default: 
                return val; 
        }
    }

    Value VirtualMachine::evaluateMemberAccess(MemberAccessNode* node) {
        Value base = evaluate(node->structure.get());

        if (base.type != ValueType::REF)
            throw std::runtime_error("Error: Member access attempted on non-pointer type.");

        void* ptr = std::get<void*>(base.as);
        if (!ptr) throw std::runtime_error("Error: Null reference access.");

        // Resolve Instance Name via the new HashMap Registry
        std::string instanceName;
        auto it = instanceRegistry.find(ptr);
        if (it != instanceRegistry.end())
            instanceName = it->second;
        else if (base.type == ValueType::STR)
            // Fallback for string-based chaining
            instanceName = std::get<std::string>(base.as);

        if (instanceName.empty()) {
            // Static/Blueprint Check
            auto* potentialNode = static_cast<ASTNode*>(ptr);
            if (potentialNode->nodeType == ASTNodeType::UNIT)
                throw std::runtime_error("Error: Static access to Unit members not yet implemented.");
                
            throw std::runtime_error("Error: Could not resolve instance name for member access.");
        }

        // Global Lookup for mangled "Instance::Member"
        std::string fullName = instanceName + "::" + node->member.lexeme;

        if (!globals.exists(fullName))
            throw std::runtime_error("Error: Member '" + node->member.lexeme + 
                "' not found in instance '" + instanceName + "'.");
                
        Value result = globals.get(fullName);

        // Automated Chaining Logic
        if (result.type == ValueType::STR) {
            std::string strVal = std::get<std::string>(result.as);
            if (globals.exists(strVal + "::[instance_marker]")) { 
                // Wrap the string address into the pool for the next access chain
                auto& storedName = addressPool.emplace_back(std::make_unique<std::string>(strVal));
                void* newPtr = static_cast<void*>(storedName.get());
                instanceRegistry[newPtr] = strVal; // Ensure it is registered for the next chain link
                return Value(newPtr);
            }
        }

        return result;
    }

    Value VirtualMachine::evaluateQualifiedID(QualifiedIDNode* node) {
        // reconstruct the full name from the path parts
        std::string fullName = "";
        for (size_t i = 0; i < node->path.size(); ++i) {
            fullName += node->path[i].lexeme;
            if (i < node->path.size() - 1)
                fullName += "::"; // Re-add the scope separators
        }

        // lookup alias in the global symbol table
        return globals.get(fullName);
    }

    Value VirtualMachine::lookUpVariable(const Token& name) {
        return globals.get(name.lexeme);
    }

    // --- Core Engine Helpers ---

    bool VirtualMachine::typeMatch(TokenType expected, ValueType actual) {
        // Universal bypass for arrays and references
        if (actual == ValueType::ARRAY || actual == ValueType::REF) return true;

        switch (expected) {
            // --- Signed Integer Group ---
            case TokenType::I16:
            case TokenType::I32:
            case TokenType::I64:
                return actual == ValueType::I16 || actual == ValueType::I32 || actual == ValueType::I64;

            // --- Unsigned Integer Group ---
            case TokenType::UI16:
            case TokenType::UI32:
            case TokenType::UI64:
                return actual == ValueType::UI16 || actual == ValueType::UI32 || actual == ValueType::UI64;

            // --- 128-bit Group (Strict) ---
            case TokenType::I128:  return actual == ValueType::I128;
            case TokenType::UI128: return actual == ValueType::UI128;

            // --- Floating Point Group ---
            case TokenType::F32:
            case TokenType::F64:
                // Most math results default to F64 in the VM
                return actual == ValueType::F32 || actual == ValueType::F64;

            // --- Primitive Group ---
            case TokenType::STR:   return actual == ValueType::STR;
            case TokenType::BOOL:  return actual == ValueType::BOOL;
            case TokenType::CHAR:  return actual == ValueType::CHAR;
            case TokenType::HOLLOW: return actual == ValueType::BARE;

            default:
                // Handles 'auto' or custom Unit types which are REFs
                return true; 
        }
    }

    Value VirtualMachine::applyOperator(Value left, Token op, Value right) {
        // --- 128-bit Promotion Palette (i128 / ui128) ---
        auto toI128 = [](const Value& v) -> i128 {
            i128 res;
            switch (v.type) {
                case ValueType::I128:
                    return std::get<i128>(v.as);

                case ValueType::UI128: {
                    ui128 u = std::get<ui128>(v.as);
                    res.low = u.low;
                    res.high = static_cast<int64_t>(u.high); // Manual assignment bypasses constructor strictness
                    return res;
                }

                case ValueType::I32: {
                    int32_t val = std::get<int32_t>(v.as);
                    res.low = static_cast<uint64_t>(val);
                    res.high = (val < 0) ? -1 : 0; // Proper sign-extension
                    return res;
                }

                case ValueType::I64: {
                    int64_t val = std::get<int64_t>(v.as);
                    res.low = static_cast<uint64_t>(val);
                    res.high = (val < 0) ? -1 : 0;
                    return res;
                }

                case ValueType::UI64:
                    res.low = std::get<uint64_t>(v.as);
                    res.high = 0;
                    return res;

                default:
                    return string_to_i128(v.toString());
            }
        };

        i128 l = toI128(left);
        i128 r = toI128(right);

        switch (op.type) {
            case TokenType::PLUS:          return Value(l + r);
            case TokenType::MINUS:         return Value(l - r);
            case TokenType::STAR:          return Value(l * r);
            case TokenType::SLASH:         return Value(l / r);
            case TokenType::PIPE:          return Value(l | r);
            case TokenType::AMPERSAND:     return Value(l & r);
            case TokenType::TILDE:         return Value(~l);
            case TokenType::NAND:          return Value(~(l & r));
            case TokenType::NOR:           return Value(~(l | r));
            case TokenType::LESS:          return Value(l < r);
            case TokenType::GREATER:       return Value(l > r);
            case TokenType::EQUAL_EQUAL:   return Value(l == r);
            case TokenType::BANG_EQUAL:    return Value(l != r);
            case TokenType::LESS_EQUAL:    return Value(l <= r);
            case TokenType::GREATER_EQUAL: return Value(l >= r);
            default: break;
        }
        

        // --- 64-bit Signed Promotion Palette (Handles i16, i32, i64) ---
        if (isSignedInt(left.type) && isSignedInt(right.type)) {
            int64_t l = asI64(left);
            int64_t r = asI64(right);
            
            switch (op.type) {
                case TokenType::PLUS:          return Value(l + r);
                case TokenType::MINUS:         return Value(l - r);
                case TokenType::STAR:          return Value(l * r);
                case TokenType::SLASH:         return Value(l / r);
                case TokenType::PERCENT:       return Value(l % r);
                case TokenType::PIPE:          return Value(l | r);
                case TokenType::AMPERSAND:     return Value(l & r);
                case TokenType::TILDE:         return Value(~l);
                case TokenType::LESS:          return Value(l < r);
                case TokenType::GREATER:       return Value(l > r);
                case TokenType::EQUAL_EQUAL:   return Value(l == r);
                case TokenType::LESS_EQUAL:    return Value(l <= r);
                case TokenType::GREATER_EQUAL: return Value(l >= r);
                case TokenType::BANG_EQUAL:    return Value(l != r);
                default: break;
            }
        }

        // --- 64-bit Unsigned Promotion (Handles ui16, ui32, ui64) ---
        if (isUnsignedInt(left.type) && isUnsignedInt(right.type)) {
            uint64_t l = asUI64(left); 
            uint64_t r = asUI64(right);
            
            switch (op.type) {
                case TokenType::PLUS:          return Value(l + r);
                case TokenType::MINUS:         return Value(l - r);
                case TokenType::STAR:          return Value(l * r);
                case TokenType::SLASH:         return Value(l / r);
                case TokenType::PIPE:          return Value(l | r);
                case TokenType::AMPERSAND:     return Value(l & r);
                case TokenType::LESS:          return Value(l < r);
                case TokenType::GREATER:       return Value(l > r);
                case TokenType::EQUAL_EQUAL:   return Value(l == r);
                case TokenType::LESS_EQUAL:    return Value(l <= r);
                case TokenType::GREATER_EQUAL: return Value(l >= r);
                case TokenType::BANG_EQUAL:    return Value(l != r);
                default: break;
            }
        }

        // --- Floating Point Palette (f32, f64) ---
        if (left.type == ValueType::F32 || left.type == ValueType::F64 || 
            right.type == ValueType::F32 || right.type == ValueType::F64) {
            double l = asF64(left);
            double r = asF64(right);
            
            switch (op.type) {
                case TokenType::PLUS:          return Value(l + r);
                case TokenType::MINUS:         return Value(l - r);
                case TokenType::STAR:          return Value(l * r);
                case TokenType::SLASH:         return Value(l / r);
                case TokenType::LESS:          return Value(l < r);
                case TokenType::GREATER:       return Value(l > r);
                case TokenType::EQUAL_EQUAL:   return Value(l == r);
                case TokenType::LESS_EQUAL:    return Value(l <= r);
                case TokenType::GREATER_EQUAL: return Value(l >= r);
                case TokenType::BANG_EQUAL:    return Value(l != r);
                default: break;
            }
        }

        // --- String Concatenation ---
        if (op.type == TokenType::PLUS) {
            // If either side is a string, promote the operation to string concatenation
            if (left.type == ValueType::STR || right.type == ValueType::STR) {
                return Value(left.toString() + right.toString());
            }
        }
        return Value(); 
    }

    bool VirtualMachine::isTruthy(const Value& v) {
        switch (v.type) {
            case ValueType::BOOL: 
                return std::get<bool>(v.as);

            // --- Signed Integers ---
            case ValueType::I16:  return std::get<int16_t>(v.as) != 0;
            case ValueType::I32:  return std::get<int32_t>(v.as) != 0;
            case ValueType::I64:  return std::get<int64_t>(v.as) != 0;
            case ValueType::I128: {
                auto val = std::get<i128>(v.as);
                return (val.high != 0 || val.low != 0);
            }

            // --- Unsigned Integers ---
            case ValueType::UI16: return std::get<uint16_t>(v.as) != 0;
            case ValueType::UI32: return std::get<uint32_t>(v.as) != 0;
            case ValueType::UI64: return std::get<uint64_t>(v.as) != 0;
            case ValueType::UI128: {
                auto val = std::get<ui128>(v.as);
                return (val.high != 0 || val.low != 0);
            }

            // --- Floating Point ---
            case ValueType::F32:  return std::get<float>(v.as) != 0.0f;
            case ValueType::F64:  return std::get<double>(v.as) != 0.0;

            // --- Objects & Strings ---
            case ValueType::CHAR: return std::get<char>(v.as) != '\0';
            case ValueType::STR:  return !std::get<std::string>(v.as).empty();
            case ValueType::REF:  return std::get<void*>(v.as) != nullptr;

            // --- Systems Types ---
            case ValueType::BARE:
            case ValueType::NEWLINE:
            case ValueType::TAB:
            case ValueType::FLUSH:
            default: 
                return false;
        }
    }

    void VirtualMachine::processGlowFile(const std::string& path) {
        // Initialize an empty buffer
        SourceBuffer sBuffer; 
        
        // Load the file path directly (this populates the internal string)
        if (!sBuffer.loadFile(path)) {
            executePanic("IO Error: Could not open file at " + path);
            return;
        }

        // 3. Hand over to Lexer and Parser
        Lexer lexer(sBuffer);
        auto tokens = lexer.tokenize();

        Parser parser(lexer, tokens, sBuffer);
        auto ast = parser.parseProgram();

        if (ast) executeBlock(ast.get());
    }

    void VirtualMachine::loadModule(const std::string& moduleName) {
        // Priority: .glh (Interface/Header) -> .glw (Implementation/Source)
        const std::vector<std::string> extensions = { ".glh", ".glw" };
        bool found = false;

        for (const auto& ext : extensions) {
            std::string fullPath = moduleSearchPath + moduleName + ext;
            std::ifstream file(fullPath);
            
            if (file.is_open()) {
                processGlowFile(fullPath);
                found = true;
                break; // Stop once the "best" version of the module is loaded
            }
        }

        if (!found) [[unlikely]]
            executePanic("Module Error: No .glh or .glw found for '" + moduleName + "'");
    }
}
