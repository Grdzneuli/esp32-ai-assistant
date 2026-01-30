#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>

// UI Colors
#define COLOR_BG          TFT_BLACK
#define COLOR_TEXT        TFT_WHITE
#define COLOR_TEXT_DIM    TFT_DARKGREY
#define COLOR_ACCENT      TFT_CYAN
#define COLOR_USER_MSG    TFT_GREEN
#define COLOR_AI_MSG      TFT_YELLOW
#define COLOR_ERROR       TFT_RED
#define COLOR_STATUS_BAR  0x2104  // Dark gray

class Display {
public:
    enum class Screen {
        SPLASH,
        STATUS,
        CHAT,
        SETTINGS
    };

    enum class AssistantState {
        IDLE,
        LISTENING,
        THINKING,
        SPEAKING,
        ERROR
    };

    Display();

    void begin();
    void setBacklight(bool on);
    void setBrightness(uint8_t level);  // 0-255

    // Screen management
    void showSplash();
    void showStatus(const String& wifiStatus, const String& ip);
    void showChat();
    void showError(const String& message);

    // Chat interface
    void setAssistantState(AssistantState state);
    void showUserMessage(const String& message);
    void showAIMessage(const String& message);
    void showThinking();
    void clearChat();

    // Status bar
    void updateStatusBar(int8_t rssi, int volume, bool listening);

    // Utilities
    void update();  // Call in loop for animations
    Screen getCurrentScreen() const { return _currentScreen; }

private:
    TFT_eSPI _tft;
    Screen _currentScreen;
    AssistantState _state;

    // Chat scrolling
    int _chatScrollY;
    std::vector<String> _chatHistory;

    // Animation
    uint32_t _lastAnimTime;
    int _animFrame;

    void drawStatusBar(int8_t rssi, int volume, bool listening);
    void drawStateIndicator();
    void wrapText(const String& text, int x, int y, int maxWidth, uint16_t color);
    int getTextHeight(const String& text, int maxWidth);
};

#endif // DISPLAY_H
