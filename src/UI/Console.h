#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <glm/glm.hpp>

class Shader;

// LogEntry structure - public so UIManager can access it
struct LogEntry {
    std::string text;
    glm::vec4 color;
};

/**
 * In-game debug console
 * - Toggle with ~ key
 * - Shows all log output
 * - Supports text selection and copy (Ctrl+C)
 * - Command input with history
 */
class Console {
public:
    static Console& instance() {
        static Console console;
        return console;
    }
    
    // Console visibility
    void toggle() { m_visible = !m_visible; }
    void show() { m_visible = true; }
    void hide() { m_visible = false; }
    bool isVisible() const { return m_visible; }
    
    // Add a log message to the console
    void addMessage(const std::string& message, const glm::vec4& color = glm::vec4(1.0f));
    void addInfo(const std::string& message);
    void addWarning(const std::string& message);
    void addError(const std::string& message);
    void addDebug(const std::string& message);
    
    // Clear all messages
    void clear();
    
    // Input handling
    void handleKeyInput(int key, int action, int mods);
    void handleCharInput(unsigned int codepoint);
    
    // Copy selected text or all text to clipboard
    void copyToClipboard();
    void pasteFromClipboard();
    
    // Scrolling
    void scrollUp(int lines = 3);
    void scrollDown(int lines = 3);
    void scrollToBottom();
    void scrollToTop();
    
    // Command callback - called when user presses Enter
    using CommandCallback = std::function<void(const std::string&)>;
    void setCommandCallback(CommandCallback callback) { m_commandCallback = callback; }
    
    // Get current input text (for external processing)
    const std::string& getInputText() const { return m_inputText; }
    
    // Accessors for UIManager rendering
    const std::deque<LogEntry>& getMessages() const { return m_messages; }
    int getScrollOffset() const { return m_scrollOffset; }
    size_t getCursorPos() const { return m_cursorPos; }
    
    // Selection
    void selectAll();
    bool hasSelection() const { return m_selectionStart != m_selectionEnd; }
    
    // Configuration
    void setMaxLines(size_t max) { m_maxLines = max; }
    void setFontScale(float scale) { m_fontScale = scale; }
    
    // Mouse handling for text selection
    void handleMouseButton(int button, int action, double mouseX, double mouseY, int screenHeight);
    void handleMouseMove(double mouseX, double mouseY, int screenHeight);
    void handleScroll(double yOffset);
    
    // Get selection state for rendering
    bool isSelecting() const { return m_selecting; }
    size_t getSelectionStart() const { return m_selectionStart; }
    size_t getSelectionEnd() const { return m_selectionEnd; }
    
private:
    Console();
    ~Console() = default;
    
    // Prevent copying
    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;
    
    // Message buffer
    std::deque<LogEntry> m_messages;
    std::mutex m_messageMutex;
    size_t m_maxLines = 500;
    
    // Scroll state
    int m_scrollOffset = 0;
    int m_visibleLines = 20;
    
    // Input state
    std::string m_inputText;
    size_t m_cursorPos = 0;
    std::vector<std::string> m_commandHistory;
    int m_historyIndex = -1;
    
    // Selection state
    size_t m_selectionStart = 0;
    size_t m_selectionEnd = 0;
    bool m_selecting = false;
    
    // Visibility
    bool m_visible = false;
    
    // Rendering
    float m_fontScale = 0.45f;
    float m_lineHeight = 16.0f;
    float m_padding = 8.0f;
    float m_consoleHeight = 0.4f; // 40% of screen height
    
    // Callback
    CommandCallback m_commandCallback;
    
    // Helpers
    std::string getSelectedText() const;
    void executeCommand();
};
