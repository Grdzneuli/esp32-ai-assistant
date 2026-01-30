#include "buttons.h"
#include "config.h"

#define LONG_PRESS_MS      500
#define DOUBLE_CLICK_MS    300

Buttons::Buttons()
    : _callback(nullptr)
{
    // Initialize button states
    _buttons[0] = {BTN_BOOT_PIN, true, true, 0, 0, false, 0, 0};
    _buttons[1] = {BTN_VOL_UP_PIN, true, true, 0, 0, false, 0, 0};
    _buttons[2] = {BTN_VOL_DOWN_PIN, true, true, 0, 0, false, 0, 0};
}

void Buttons::begin() {
    // Configure button pins as input with pullup
    // BOOT button is typically active-low with internal pullup
    pinMode(BTN_BOOT_PIN, INPUT_PULLUP);
    pinMode(BTN_VOL_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_VOL_DOWN_PIN, INPUT_PULLUP);

    Serial.println("[Buttons] Initialized");
}

void Buttons::setCallback(ButtonCallback callback) {
    _callback = callback;
}

void Buttons::update() {
    processButton(0, Button::BOOT);
    processButton(1, Button::VOL_UP);
    processButton(2, Button::VOL_DOWN);
}

void Buttons::processButton(int index, Button button) {
    ButtonState& state = _buttons[index];

    bool reading = digitalRead(state.pin) == LOW;  // Active low
    uint32_t now = millis();

    // Debounce
    if (reading != state.lastState) {
        state.lastChangeTime = now;
    }

    if ((now - state.lastChangeTime) > BTN_DEBOUNCE_MS) {
        if (reading != state.currentState) {
            state.currentState = reading;

            if (state.currentState) {
                // Button pressed
                state.pressTime = now;
                state.longPressTriggered = false;

                if (_callback) {
                    _callback(button, ButtonEvent::PRESSED);
                }
            } else {
                // Button released
                if (!state.longPressTriggered) {
                    // Check for double click
                    if ((now - state.lastClickTime) < DOUBLE_CLICK_MS) {
                        state.clickCount++;
                        if (state.clickCount >= 2) {
                            if (_callback) {
                                _callback(button, ButtonEvent::DOUBLE_CLICK);
                            }
                            state.clickCount = 0;
                        }
                    } else {
                        state.clickCount = 1;
                    }
                    state.lastClickTime = now;
                }

                if (_callback) {
                    _callback(button, ButtonEvent::RELEASED);
                }
            }
        }

        // Check for long press while held
        if (state.currentState && !state.longPressTriggered) {
            if ((now - state.pressTime) > LONG_PRESS_MS) {
                state.longPressTriggered = true;
                if (_callback) {
                    _callback(button, ButtonEvent::LONG_PRESS);
                }
            }
        }
    }

    state.lastState = reading;
}

bool Buttons::isPressed(Button button) {
    int index = static_cast<int>(button);
    if (index >= 0 && index < 3) {
        return _buttons[index].currentState;
    }
    return false;
}

bool Buttons::isHeld(Button button) {
    int index = static_cast<int>(button);
    if (index >= 0 && index < 3) {
        if (_buttons[index].currentState) {
            return (millis() - _buttons[index].pressTime) > LONG_PRESS_MS;
        }
    }
    return false;
}
