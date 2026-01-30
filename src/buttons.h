#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>
#include <functional>

enum class ButtonEvent {
    NONE,
    PRESSED,
    RELEASED,
    LONG_PRESS,
    DOUBLE_CLICK
};

enum class Button {
    BOOT,      // Talk/Interrupt button
    VOL_UP,
    VOL_DOWN
};

class Buttons {
public:
    using ButtonCallback = std::function<void(Button button, ButtonEvent event)>;

    Buttons();

    void begin();
    void setCallback(ButtonCallback callback);

    // Call in loop
    void update();

    // Direct state query
    bool isPressed(Button button);
    bool isHeld(Button button);  // True if held for > 500ms

private:
    struct ButtonState {
        int pin;
        bool lastState;
        bool currentState;
        uint32_t lastChangeTime;
        uint32_t pressTime;
        bool longPressTriggered;
        uint8_t clickCount;
        uint32_t lastClickTime;
    };

    ButtonState _buttons[3];
    ButtonCallback _callback;

    void processButton(int index, Button button);
};

#endif // BUTTONS_H
