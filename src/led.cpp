#include "led.h"
#include "config.h"

StatusLED::StatusLED()
    : _led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800)
    , _mode(Mode::SOLID)
    , _color(LED_COLOR_IDLE)
    , _lastUpdate(0)
    , _brightness(50)
    , _animStep(0)
    , _animDirection(true)
{
}

void StatusLED::begin() {
    _led.begin();
    _led.setBrightness(_brightness);
    _led.show();

    Serial.println("[LED] Initialized");
}

void StatusLED::update() {
    uint32_t now = millis();

    switch (_mode) {
        case Mode::OFF:
            break;

        case Mode::SOLID:
            // Nothing to animate
            break;

        case Mode::BREATHING:
            if (now - _lastUpdate > 20) {
                _lastUpdate = now;

                if (_animDirection) {
                    _animStep += 2;
                    if (_animStep >= 100) {
                        _animStep = 100;
                        _animDirection = false;
                    }
                } else {
                    _animStep -= 2;
                    if (_animStep <= 10) {
                        _animStep = 10;
                        _animDirection = true;
                    }
                }

                _led.setBrightness((_brightness * _animStep) / 100);
                applyColor();
            }
            break;

        case Mode::PULSE:
            if (now - _lastUpdate > 50) {
                _lastUpdate = now;
                _animStep = (_animStep + 5) % 100;

                int pulseValue = abs(50 - _animStep) * 2;
                _led.setBrightness((_brightness * pulseValue) / 100);
                applyColor();
            }
            break;

        case Mode::RAINBOW:
            if (now - _lastUpdate > 30) {
                _lastUpdate = now;
                _animStep = (_animStep + 1) % 256;

                // Rainbow wheel
                uint32_t c;
                uint8_t pos = _animStep;
                if (pos < 85) {
                    c = _led.Color(pos * 3, 255 - pos * 3, 0);
                } else if (pos < 170) {
                    pos -= 85;
                    c = _led.Color(255 - pos * 3, 0, pos * 3);
                } else {
                    pos -= 170;
                    c = _led.Color(0, pos * 3, 255 - pos * 3);
                }

                _led.setPixelColor(0, c);
                _led.show();
            }
            break;
    }
}

void StatusLED::setColor(uint32_t color) {
    _color = color;
    _mode = Mode::SOLID;
    _led.setBrightness(_brightness);
    applyColor();
}

void StatusLED::setColor(uint8_t r, uint8_t g, uint8_t b) {
    setColor(_led.Color(r, g, b));
}

void StatusLED::off() {
    _mode = Mode::OFF;
    _led.setPixelColor(0, 0);
    _led.show();
}

void StatusLED::setIdle() {
    _color = _led.Color(0, 32, 0);  // Dim green
    _mode = Mode::BREATHING;
}

void StatusLED::setListening() {
    _color = _led.Color(0, 0, 255);  // Blue
    _mode = Mode::PULSE;
}

void StatusLED::setThinking() {
    _color = _led.Color(0, 255, 255);  // Cyan
    _mode = Mode::BREATHING;
}

void StatusLED::setSpeaking() {
    _color = _led.Color(255, 255, 0);  // Yellow
    _mode = Mode::SOLID;
    applyColor();
}

void StatusLED::setError() {
    _color = _led.Color(255, 0, 0);  // Red
    _mode = Mode::PULSE;
}

void StatusLED::setConnecting() {
    _mode = Mode::RAINBOW;
}

void StatusLED::setMode(Mode mode) {
    _mode = mode;
    _animStep = 0;
    _animDirection = true;
}

void StatusLED::applyColor() {
    _led.setPixelColor(0, _color);
    _led.show();
}
