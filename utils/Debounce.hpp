#ifndef __DEBOUNCE_HPP__
#define __DEBOUNCE_HPP__

#include <Arduino.h>

class ButtonDebounce {
private:
    static constexpr unsigned long kHaltTime_ms = 120;
    unsigned long lastPressTime_ms = 0;

public:
    bool debounce() {
        unsigned long currentTime = millis();
        if (currentTime - lastPressTime_ms >= kHaltTime_ms) {
            lastPressTime_ms = currentTime;
            return true;
        }
        return false;
    }
    void setState(bool pressed) {
        if (pressed) {
            lastPressTime_ms = millis() - kHaltTime_ms;
        } else {
            lastPressTime_ms = millis();
        }
    }
};

class ToggleDebounce {
private:
    static constexpr unsigned long kHaltTime_ms = 120;
    unsigned long lastChangeTime_ms = 0;
    bool lastState = false;
    bool stateChanged = false;

public:
    bool debounce(bool currentState) {
        unsigned long currentTime = millis();
        bool shouldUpdate = false;

        if (currentState != lastState &&
            currentTime - lastChangeTime_ms >= kHaltTime_ms) {
            lastChangeTime_ms = currentTime;
            lastState = currentState;
            shouldUpdate = true;
        }
        return shouldUpdate;
    }

    bool getState() const {
        return lastState;
    }
    void setState(bool state) {
        lastState = state;
        lastChangeTime_ms = millis() - kHaltTime_ms;
    }
};

#endif // __DEBOUNCE_HPP__
