#ifndef __DEBOUNCE_HPP__
#define __DEBOUNCE_HPP__

#include <Arduino.h>

class ButtonDebounce {
private:
    static constexpr unsigned long kHaltTime_ms = 25;
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
};

class ToggleDebounce {
private:
    static constexpr unsigned long kHaltTime_ms = 25;
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
};

#endif // __DEBOUNCE_HPP__
