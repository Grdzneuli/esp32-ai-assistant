#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

class StatusLED {
public:
    enum class Mode {
        OFF,
        SOLID,
        BREATHING,
        PULSE,
        RAINBOW
    };

    StatusLED();

    void begin();
    void update();  // Call in loop for animations

    // Simple controls
    void setColor(uint32_t color);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void off();

    // State-based colors
    void setIdle();
    void setListening();
    void setThinking();
    void setSpeaking();
    void setError();
    void setConnecting();

    // Animation modes
    void setMode(Mode mode);

private:
    Adafruit_NeoPixel _led;
    Mode _mode;
    uint32_t _color;
    uint32_t _lastUpdate;
    uint8_t _brightness;
    int _animStep;
    bool _animDirection;

    void applyColor();
};

#endif // LED_H
