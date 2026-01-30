#include "display.h"
#include "config.h"

Display::Display()
    : _currentScreen(Screen::SPLASH)
    , _state(AssistantState::IDLE)
    , _chatScrollY(30)
    , _lastAnimTime(0)
    , _animFrame(0)
{
}

void Display::begin() {
    _tft.init();
    _tft.setRotation(0);  // Portrait mode
    _tft.fillScreen(COLOR_BG);
    _tft.setTextColor(COLOR_TEXT, COLOR_BG);
    _tft.setTextWrap(false);

    // Enable backlight
    pinMode(TFT_BL_PIN, OUTPUT);
    setBacklight(true);

    Serial.println("[Display] Initialized " + String(TFT_WIDTH) + "x" + String(TFT_HEIGHT));
}

void Display::setBacklight(bool on) {
    digitalWrite(TFT_BL_PIN, on ? HIGH : LOW);
}

void Display::setBrightness(uint8_t level) {
    analogWrite(TFT_BL_PIN, level);
}

void Display::showSplash() {
    _currentScreen = Screen::SPLASH;
    _tft.fillScreen(COLOR_BG);

    // Title
    _tft.setTextSize(2);
    _tft.setTextColor(COLOR_ACCENT);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString("ESP32-S3", TFT_WIDTH / 2, TFT_HEIGHT / 2 - 40);
    _tft.drawString("AI Assistant", TFT_WIDTH / 2, TFT_HEIGHT / 2 - 10);

    // Subtitle
    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT_DIM);
    _tft.drawString("Powered by Gemini", TFT_WIDTH / 2, TFT_HEIGHT / 2 + 30);

    _tft.setTextDatum(TL_DATUM);
}

void Display::showStatus(const String& wifiStatus, const String& ip) {
    _currentScreen = Screen::STATUS;
    _tft.fillScreen(COLOR_BG);

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);

    int y = 10;
    _tft.drawString("System Status", 10, y);
    y += 25;

    _tft.setTextColor(COLOR_TEXT_DIM);
    _tft.drawString("WiFi:", 10, y);
    _tft.setTextColor(wifiStatus == "Connected" ? COLOR_USER_MSG : COLOR_ERROR);
    _tft.drawString(wifiStatus, 60, y);
    y += 15;

    if (ip.length() > 0) {
        _tft.setTextColor(COLOR_TEXT_DIM);
        _tft.drawString("IP:", 10, y);
        _tft.setTextColor(COLOR_TEXT);
        _tft.drawString(ip, 60, y);
    }
}

void Display::showChat() {
    _currentScreen = Screen::CHAT;
    _tft.fillScreen(COLOR_BG);

    // Draw status bar at top
    drawStatusBar(0, 70, false);

    // Draw state indicator
    drawStateIndicator();
}

void Display::showError(const String& message) {
    _tft.fillRect(0, TFT_HEIGHT - 60, TFT_WIDTH, 60, COLOR_ERROR);
    _tft.setTextColor(TFT_WHITE);
    _tft.setTextSize(1);
    wrapText(message, 5, TFT_HEIGHT - 55, TFT_WIDTH - 10, TFT_WHITE);
}

void Display::setAssistantState(AssistantState state) {
    _state = state;
    if (_currentScreen == Screen::CHAT) {
        drawStateIndicator();
    }
}

void Display::showUserMessage(const String& message) {
    _chatHistory.push_back("You: " + message);

    if (_currentScreen == Screen::CHAT) {
        // Clear chat area
        _tft.fillRect(0, 30, TFT_WIDTH, TFT_HEIGHT - 80, COLOR_BG);

        int y = 35;
        _tft.setTextSize(1);

        // Show last few messages
        int startIdx = max(0, (int)_chatHistory.size() - 6);
        for (int i = startIdx; i < _chatHistory.size(); i++) {
            uint16_t color = _chatHistory[i].startsWith("You:") ? COLOR_USER_MSG : COLOR_AI_MSG;
            wrapText(_chatHistory[i], 5, y, TFT_WIDTH - 10, color);
            y += getTextHeight(_chatHistory[i], TFT_WIDTH - 10) + 8;

            if (y > TFT_HEIGHT - 80) break;
        }
    }
}

void Display::showAIMessage(const String& message) {
    _chatHistory.push_back("AI: " + message);

    if (_currentScreen == Screen::CHAT) {
        // Clear chat area
        _tft.fillRect(0, 30, TFT_WIDTH, TFT_HEIGHT - 80, COLOR_BG);

        int y = 35;
        _tft.setTextSize(1);

        // Show last few messages
        int startIdx = max(0, (int)_chatHistory.size() - 6);
        for (int i = startIdx; i < _chatHistory.size(); i++) {
            uint16_t color = _chatHistory[i].startsWith("You:") ? COLOR_USER_MSG : COLOR_AI_MSG;
            wrapText(_chatHistory[i], 5, y, TFT_WIDTH - 10, color);
            y += getTextHeight(_chatHistory[i], TFT_WIDTH - 10) + 8;

            if (y > TFT_HEIGHT - 80) break;
        }
    }
}

void Display::showThinking() {
    if (_currentScreen == Screen::CHAT) {
        _tft.fillRect(0, TFT_HEIGHT - 50, TFT_WIDTH, 20, COLOR_BG);
        _tft.setTextColor(COLOR_ACCENT);
        _tft.setTextSize(1);
        _tft.drawString("Thinking...", 10, TFT_HEIGHT - 45);
    }
}

void Display::clearChat() {
    _chatHistory.clear();
    if (_currentScreen == Screen::CHAT) {
        _tft.fillRect(0, 30, TFT_WIDTH, TFT_HEIGHT - 80, COLOR_BG);
    }
}

void Display::updateStatusBar(int8_t rssi, int volume, bool listening) {
    if (_currentScreen == Screen::CHAT) {
        drawStatusBar(rssi, volume, listening);
    }
}

void Display::update() {
    // Handle animations
    if (millis() - _lastAnimTime > 500) {
        _lastAnimTime = millis();
        _animFrame = (_animFrame + 1) % 4;

        if (_state == AssistantState::THINKING || _state == AssistantState::LISTENING) {
            drawStateIndicator();
        }
    }
}

void Display::drawStatusBar(int8_t rssi, int volume, bool listening) {
    _tft.fillRect(0, 0, TFT_WIDTH, 25, COLOR_STATUS_BAR);

    _tft.setTextSize(1);
    _tft.setTextColor(COLOR_TEXT);

    // WiFi signal strength indicator
    int bars = 0;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else if (rssi > -80) bars = 1;

    for (int i = 0; i < 4; i++) {
        int h = 4 + i * 3;
        uint16_t color = (i < bars) ? COLOR_ACCENT : COLOR_TEXT_DIM;
        _tft.fillRect(5 + i * 5, 20 - h, 3, h, color);
    }

    // Volume indicator
    _tft.drawString("Vol:" + String(volume) + "%", 30, 8);

    // Listening indicator
    if (listening) {
        _tft.fillCircle(TFT_WIDTH - 15, 12, 6, TFT_RED);
    }
}

void Display::drawStateIndicator() {
    int y = TFT_HEIGHT - 25;
    _tft.fillRect(0, y, TFT_WIDTH, 25, COLOR_STATUS_BAR);

    _tft.setTextSize(1);
    String stateText;
    uint16_t stateColor;

    switch (_state) {
        case AssistantState::IDLE:
            stateText = "Ready - Press BOOT to talk";
            stateColor = COLOR_TEXT_DIM;
            break;
        case AssistantState::LISTENING:
            stateText = "Listening" + String(".").substring(0, (_animFrame % 3) + 1);
            stateColor = TFT_RED;
            break;
        case AssistantState::THINKING:
            stateText = "Thinking" + String(".").substring(0, (_animFrame % 3) + 1);
            stateColor = COLOR_ACCENT;
            break;
        case AssistantState::SPEAKING:
            stateText = "Speaking...";
            stateColor = COLOR_AI_MSG;
            break;
        case AssistantState::ERROR:
            stateText = "Error occurred";
            stateColor = COLOR_ERROR;
            break;
    }

    _tft.setTextColor(stateColor);
    _tft.setTextDatum(MC_DATUM);
    _tft.drawString(stateText, TFT_WIDTH / 2, y + 12);
    _tft.setTextDatum(TL_DATUM);
}

void Display::wrapText(const String& text, int x, int y, int maxWidth, uint16_t color) {
    _tft.setTextColor(color);

    String remaining = text;
    int lineY = y;
    int charWidth = 6;  // Approximate character width for size 1

    while (remaining.length() > 0) {
        int maxChars = maxWidth / charWidth;
        if (remaining.length() <= maxChars) {
            _tft.drawString(remaining, x, lineY);
            break;
        }

        // Find last space within maxChars
        int breakPoint = maxChars;
        for (int i = maxChars; i > 0; i--) {
            if (remaining.charAt(i) == ' ') {
                breakPoint = i;
                break;
            }
        }

        _tft.drawString(remaining.substring(0, breakPoint), x, lineY);
        remaining = remaining.substring(breakPoint + 1);
        lineY += 12;

        if (lineY > TFT_HEIGHT - 30) break;  // Stop if running out of screen
    }
}

int Display::getTextHeight(const String& text, int maxWidth) {
    int charWidth = 6;
    int maxChars = maxWidth / charWidth;
    int lines = (text.length() / maxChars) + 1;
    return lines * 12;
}
