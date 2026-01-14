#include "compiler/SymbolTable.h"

namespace Glow {

    SymbolTable::SymbolTable() {
        // Initialize the base Global Scope
        enterScope();
    }

    void SymbolTable::enterScope() {
        scopes.push_back(std::unordered_map<std::string, Symbol>());
    }

    void SymbolTable::exitScope() {
        if (scopes.size() > 1) scopes.pop_back();
    }

    void SymbolTable::define(const std::string& name, Value val, bool isConst) {
        // Registers symbols in the local frame. 
        // For 'units', this includes method pointers and fields.
        scopes.back()[name] = {val, isConst};
    }

    void SymbolTable::defineGlobal(const std::string& name, Value val, bool isConst) {
        // Used by Enums (e.g., State::IDLE) and Entry points.
        scopes.front()[name] = {val, isConst};
    }

    Value SymbolTable::get(const std::string& name) {
        // Check for 'my->' context first if it's a member access
        // Standard Lexical Scoping: Search from Local (back) to Global (front)
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end())
                return found->second.value;
        }
        throw std::runtime_error("Systems Error: Undefined identifier '" + name + "'");
    }

    // --- Targeted Access for Units ---
    Value SymbolTable::getFromScope(int scopeIndex, const std::string& name) {
        if (scopeIndex < 0 || scopeIndex >= (int)scopes.size()) 
             throw std::runtime_error("Systems Error: Invalid scope access.");
             
        auto found = scopes[scopeIndex].find(name);
        if (found != scopes[scopeIndex].end()) return found->second.value;
        
        throw std::runtime_error("Systems Error: Member '" + name + "' not found in instance.");
    }

    void SymbolTable::assign(const std::string& name, Value val) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                // Systems Check: Const protection
                if (found->second.isConstant) [[unlikely]]
                    throw std::runtime_error("Systems Error: Cannot modify constant '" + name + "'");
                
                // Actual memory update
                found->second.value = val;
                return;
            }
        }
        throw std::runtime_error("Runtime Error: Cannot assign to undefined variable '" + name + "'");
    }

    bool SymbolTable::exists(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
            if (it->find(name) != it->end()) return true;
        return false;
    }

    Value SymbolTable::remove(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                if (found->second.isConstant)
                    throw std::runtime_error("Error: Cannot delete constant '" + name + "'");
                
                Value val = found->second.value; // Capture the value
                it->erase(found);                // Remove from table
                return val;                      // Return for VM cleanup
            }
        }
        return Value(); // Return empty if nothing found
    }
}
