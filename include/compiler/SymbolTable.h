#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>
#include "common/Token.h"
#include "vm/Value.h"

namespace Glow {

    struct Symbol {
        Value value;
        bool isConstant;
        TokenType visibility = TokenType::PRIV;
    };

    class SymbolTable {
    public:
        SymbolTable();

        // --- Scoping ---
        void enterScope(); // Pushes a new local map onto the stack
        void exitScope();  // Pops the innermost map

        // --- Mutation & Access ---
        // Registers a new symbol in the current innermost scope
        void define(const std::string& name, Value val, bool isConst = false);
        
        // Updates an existing variable, respecting constants
        void assign(const std::string& name, Value val);
        
        // Searches from local scope up to global
        Value get(const std::string& name);

        // helper to extract types 
        ValueType getType(const std::string& name) {
            return get(name).type;
        }

        Value remove(const std::string& name); // manual memory freeing
        Value getFromScope(int scopeIndex, const std::string& name);

        // --- Systems Extensions ---
        // Checks if a symbol exists anywhere in the stack
        bool exists(const std::string& name) const;
        
        // Explicitly registers a symbol in the base (global) scope
        void defineGlobal(const std::string& name, Value val, bool isConst = false);

        // Exposes the current (global) scope for the entry-point search
        const std::unordered_map<std::string, Symbol>& getValues() const {
            return scopes.back(); 
        }

        // --- GC SUPPORT ---
        // Provides access to the entire stack for the Mark phase
        const std::vector<std::unordered_map<std::string, Symbol>>& getAllScopes() const {
            return scopes;
        }

    private:
        // Index 0 is always the Global Scope; scopes.back() is the Local Scope
        std::vector<std::unordered_map<std::string, Symbol>> scopes;
    };
}
