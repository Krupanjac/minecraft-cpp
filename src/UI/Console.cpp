#include "Console.h"
#include "../Render/Shader.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

Console::Console() {
    // Initialize with welcome message
    addMessage("=== Debug Console ===", glm::vec4(0.3f, 0.8f, 1.0f, 1.0f));
    addMessage("Press ~ to toggle console", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    addMessage("Ctrl+C to copy, Ctrl+V to paste, Ctrl+A to select all", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    addMessage("PageUp/PageDown or mouse wheel to scroll", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    addMessage("", glm::vec4(1.0f));
}

void Console::addMessage(const std::string& message, const glm::vec4& color) {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    
    // Split multi-line messages
    std::istringstream stream(message);
    std::string line;
    while (std::getline(stream, line)) {
        m_messages.push_back({line, color});
        
        // Remove old messages if we exceed max
        while (m_messages.size() > m_maxLines) {
            m_messages.pop_front();
        }
    }
    
    // Auto-scroll to bottom if near bottom
    if (m_scrollOffset <= 3) {
        m_scrollOffset = 0;
    }
}

void Console::addInfo(const std::string& message) {
    addMessage(message, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)); // White
}

void Console::addWarning(const std::string& message) {
    addMessage(message, glm::vec4(1.0f, 0.8f, 0.2f, 1.0f)); // Yellow/Orange
}

void Console::addError(const std::string& message) {
    addMessage(message, glm::vec4(1.0f, 0.3f, 0.3f, 1.0f)); // Red
}

void Console::addDebug(const std::string& message) {
    addMessage(message, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
}

void Console::clear() {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    m_messages.clear();
    m_scrollOffset = 0;
    addMessage("Console cleared", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
}

void Console::handleKeyInput(int key, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;
    
    // Ctrl+C - Copy
    if (ctrl && key == GLFW_KEY_C) {
        copyToClipboard();
        return;
    }
    
    // Ctrl+V - Paste
    if (ctrl && key == GLFW_KEY_V) {
        pasteFromClipboard();
        return;
    }
    
    // Ctrl+A - Select All
    if (ctrl && key == GLFW_KEY_A) {
        selectAll();
        return;
    }
    
    // Enter - Execute command
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
        executeCommand();
        return;
    }
    
    // Backspace
    if (key == GLFW_KEY_BACKSPACE) {
        if (!m_inputText.empty() && m_cursorPos > 0) {
            m_inputText.erase(m_cursorPos - 1, 1);
            m_cursorPos--;
        }
        return;
    }
    
    // Delete
    if (key == GLFW_KEY_DELETE) {
        if (m_cursorPos < m_inputText.length()) {
            m_inputText.erase(m_cursorPos, 1);
        }
        return;
    }
    
    // Left arrow
    if (key == GLFW_KEY_LEFT) {
        if (m_cursorPos > 0) m_cursorPos--;
        return;
    }
    
    // Right arrow
    if (key == GLFW_KEY_RIGHT) {
        if (m_cursorPos < m_inputText.length()) m_cursorPos++;
        return;
    }
    
    // Home
    if (key == GLFW_KEY_HOME) {
        m_cursorPos = 0;
        return;
    }
    
    // End
    if (key == GLFW_KEY_END) {
        m_cursorPos = m_inputText.length();
        return;
    }
    
    // Up arrow - Command history
    if (key == GLFW_KEY_UP) {
        if (!m_commandHistory.empty()) {
            if (m_historyIndex < (int)m_commandHistory.size() - 1) {
                m_historyIndex++;
                m_inputText = m_commandHistory[m_commandHistory.size() - 1 - m_historyIndex];
                m_cursorPos = m_inputText.length();
            }
        }
        return;
    }
    
    // Down arrow - Command history
    if (key == GLFW_KEY_DOWN) {
        if (m_historyIndex > 0) {
            m_historyIndex--;
            m_inputText = m_commandHistory[m_commandHistory.size() - 1 - m_historyIndex];
            m_cursorPos = m_inputText.length();
        } else if (m_historyIndex == 0) {
            m_historyIndex = -1;
            m_inputText.clear();
            m_cursorPos = 0;
        }
        return;
    }
    
    // Page Up - Scroll
    if (key == GLFW_KEY_PAGE_UP) {
        scrollUp(m_visibleLines - 2);
        return;
    }
    
    // Page Down - Scroll
    if (key == GLFW_KEY_PAGE_DOWN) {
        scrollDown(m_visibleLines - 2);
        return;
    }
    
    // Escape - Close console
    if (key == GLFW_KEY_ESCAPE) {
        hide();
        return;
    }
}

void Console::handleCharInput(unsigned int codepoint) {
    // Only handle printable ASCII for now
    if (codepoint >= 32 && codepoint < 127) {
        m_inputText.insert(m_cursorPos, 1, static_cast<char>(codepoint));
        m_cursorPos++;
    }
}

void Console::copyToClipboard() {
#ifdef _WIN32
    std::string textToCopy;
    
    if (hasSelection()) {
        textToCopy = getSelectedText();
    } else {
        // Copy all visible messages
        std::lock_guard<std::mutex> lock(m_messageMutex);
        std::ostringstream ss;
        for (const auto& entry : m_messages) {
            ss << entry.text << "\r\n";
        }
        textToCopy = ss.str();
    }
    
    if (textToCopy.empty()) return;
    
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, textToCopy.size() + 1);
        if (hGlob) {
            char* pGlobal = static_cast<char*>(GlobalLock(hGlob));
            if (pGlobal) {
                memcpy(pGlobal, textToCopy.c_str(), textToCopy.size() + 1);
                GlobalUnlock(hGlob);
                SetClipboardData(CF_TEXT, hGlob);
            }
        }
        CloseClipboard();
        addMessage("Copied to clipboard", glm::vec4(0.5f, 1.0f, 0.5f, 1.0f));
    }
#endif
}

void Console::pasteFromClipboard() {
#ifdef _WIN32
    if (OpenClipboard(nullptr)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pText = static_cast<char*>(GlobalLock(hData));
            if (pText) {
                std::string text(pText);
                // Remove newlines for single-line input
                text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
                text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
                
                m_inputText.insert(m_cursorPos, text);
                m_cursorPos += text.length();
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
#endif
}

void Console::scrollUp(int lines) {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    m_scrollOffset += lines;
    int maxScroll = (std::max)(0, (int)m_messages.size() - m_visibleLines);
    m_scrollOffset = (std::min)(m_scrollOffset, maxScroll);
}

void Console::scrollDown(int lines) {
    m_scrollOffset -= lines;
    m_scrollOffset = (std::max)(0, m_scrollOffset);
}

void Console::scrollToBottom() {
    m_scrollOffset = 0;
}

void Console::scrollToTop() {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    m_scrollOffset = (std::max)(0, (int)m_messages.size() - m_visibleLines);
}

void Console::selectAll() {
    m_selectionStart = 0;
    m_selectionEnd = m_messages.size();
    addMessage("All text selected (Ctrl+C to copy)", glm::vec4(0.5f, 1.0f, 0.5f, 1.0f));
}

std::string Console::getSelectedText() const {
    std::ostringstream ss;
    
    if (hasSelection()) {
        // Get selected lines
        size_t start = (std::min)(m_selectionStart, m_selectionEnd);
        size_t end = (std::max)(m_selectionStart, m_selectionEnd);
        
        for (size_t i = start; i <= end && i < m_messages.size(); ++i) {
            ss << m_messages[i].text << "\n";
        }
    } else {
        // Return all messages when no selection
        for (const auto& entry : m_messages) {
            ss << entry.text << "\n";
        }
    }
    return ss.str();
}

void Console::executeCommand() {
    if (m_inputText.empty()) return;
    
    // Add to history
    m_commandHistory.push_back(m_inputText);
    if (m_commandHistory.size() > 100) {
        m_commandHistory.erase(m_commandHistory.begin());
    }
    m_historyIndex = -1;
    
    // Echo command
    addMessage("> " + m_inputText, glm::vec4(0.8f, 0.8f, 0.3f, 1.0f));
    
    // Built-in commands
    if (m_inputText == "clear") {
        clear();
    } else if (m_inputText == "help") {
        addMessage("Available commands:", glm::vec4(0.3f, 0.8f, 1.0f, 1.0f));
        addMessage("  clear - Clear console", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
        addMessage("  help - Show this help", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
        addMessage("  quit/exit - Close console", glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
    } else if (m_inputText == "quit" || m_inputText == "exit") {
        hide();
    } else if (m_commandCallback) {
        m_commandCallback(m_inputText);
    } else {
        addMessage("Unknown command: " + m_inputText, glm::vec4(1.0f, 0.5f, 0.5f, 1.0f));
    }
    
    m_inputText.clear();
    m_cursorPos = 0;
}

void Console::handleMouseButton(int button, int action, double mouseX, double mouseY, int screenHeight) {
    if (!m_visible) return;
    
    // Only handle within console area (top 40% of screen)
    float consoleHeight = screenHeight * m_consoleHeight;
    if (mouseY > consoleHeight) return;
    
    if (button == 0) { // Left mouse button
        if (action == 1) { // Press
            // Start selection
            m_selecting = true;
            
            // Calculate which line was clicked
            float lineHeight = 16.0f;
            float padding = 8.0f;
            int clickedLine = static_cast<int>((mouseY - padding) / lineHeight);
            
            // Convert to message index
            std::lock_guard<std::mutex> lock(m_messageMutex);
            int visibleLines = static_cast<int>((consoleHeight - padding * 3 - lineHeight) / lineHeight);
            int startIdx = (std::max)(0, (int)m_messages.size() - visibleLines - m_scrollOffset);
            int lineIdx = startIdx + clickedLine;
            
            if (lineIdx >= 0 && lineIdx < (int)m_messages.size()) {
                m_selectionStart = lineIdx;
                m_selectionEnd = lineIdx;
            }
        } else if (action == 0) { // Release
            m_selecting = false;
        }
    }
}

void Console::handleMouseMove(double mouseX, double mouseY, int screenHeight) {
    if (!m_visible || !m_selecting) return;
    
    float consoleHeight = screenHeight * m_consoleHeight;
    if (mouseY > consoleHeight) return;
    
    // Update selection end based on mouse position
    float lineHeight = 16.0f;
    float padding = 8.0f;
    int clickedLine = static_cast<int>((mouseY - padding) / lineHeight);
    
    std::lock_guard<std::mutex> lock(m_messageMutex);
    int visibleLines = static_cast<int>((consoleHeight - padding * 3 - lineHeight) / lineHeight);
    int startIdx = (std::max)(0, (int)m_messages.size() - visibleLines - m_scrollOffset);
    int lineIdx = startIdx + clickedLine;
    
    if (lineIdx >= 0 && lineIdx < (int)m_messages.size()) {
        m_selectionEnd = lineIdx;
    }
}

void Console::handleScroll(double yOffset) {
    if (!m_visible) return;
    
    if (yOffset > 0) {
        scrollUp(3);
    } else if (yOffset < 0) {
        scrollDown(3);
    }
}