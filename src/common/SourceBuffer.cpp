#include "common/SourceBuffer.h"
#include <fstream>
#include <sstream>

namespace Glow {

    bool SourceBuffer::loadFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        content = buffer.str();
        filePath = path;
        
        // Strictly reset state for new file to avoid coordinate pollution
        cursor = 0;
        line = 1;
        column = 1;
        return true;
    }

    char SourceBuffer::peek() const {
        if (isAtEnd()) return '\0';
        return content[cursor];
    }

    char SourceBuffer::peekNext() const {
        if (cursor + 1 >= content.length()) return '\0';
        return content[cursor + 1];
    }

    char SourceBuffer::advance() {
        if (isAtEnd()) return '\0';
        char current = content[cursor++];
        
        // Standard coordinate tracking for Glow's diagnostic system
        if (current == '\n') {
            line++;
            column = 1;
        }
        else column++;
        return current;
    }

    char SourceBuffer::previous() const {
        if (cursor == 0) return '\0';
        return content[cursor - 1];
    }

    bool SourceBuffer::isAtEnd() const {
        return cursor >= content.length();
    }

    std::string SourceBuffer::getLineText(int lineNum) const {
        std::stringstream ss(content); 
        std::string line;
        int currentLine = 1;
        
        while (std::getline(ss, line)) {
            if (currentLine == lineNum) return line;
            currentLine++;
        }
        return "";
    }
}
