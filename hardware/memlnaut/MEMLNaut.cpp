#include "MEMLNaut.hpp"

MEMLNaut* MEMLNaut::instance = nullptr;

static const int nButtons = 7;
static const int nToggles = 5;

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
    // Returns true only when state has changed AND debounce time has passed
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

    // Get the current stable state
    bool getState() const {
        return lastState;
    }
};

ButtonDebounce debouncers[nButtons];
ToggleDebounce toggleDebouncers[nToggles];


// Static interrupt handlers implementation
void MEMLNaut::handleMomA1() {
    if (debouncers[0].debounce() && instance && instance->momA1Callback) {
        instance->momA1Callback();
    }
}
void MEMLNaut::handleMomA2() { if(instance && instance->checkDebounce(1) && instance->momA2Callback) instance->momA2Callback(); }
void MEMLNaut::handleMomB1() { if(instance && instance->checkDebounce(2) && instance->momB1Callback) instance->momB1Callback(); }
void MEMLNaut::handleMomB2() { if(instance && instance->checkDebounce(3) && instance->momB2Callback) instance->momB2Callback(); }
void MEMLNaut::handleReSW() { if(instance && instance->checkDebounce(4) && instance->reSWCallback) instance->reSWCallback(); }
void MEMLNaut::handleReA() { if(instance && instance->checkDebounce(5) && instance->reACallback) instance->reACallback(); }
void MEMLNaut::handleReB() { if(instance && instance->checkDebounce(6) && instance->reBCallback) instance->reBCallback(); }

void MEMLNaut::handleTogA1() {
    bool should_update = toggleDebouncers[0].debounce(digitalRead(Pins::TOG_A1) == LOW);
    if (should_update && instance && instance->togA1Callback) {
        bool val = toggleDebouncers[0].getState();
        instance->togA1Callback(val);
    }
}
void MEMLNaut::handleTogA2() { if(instance && instance->checkDebounce(8) && instance->togA2Callback) instance->togA2Callback(digitalRead(Pins::TOG_A2) == LOW); }
void MEMLNaut::handleTogB1() { if(instance && instance->checkDebounce(9) && instance->togB1Callback) instance->togB1Callback(digitalRead(Pins::TOG_B1) == LOW); }
void MEMLNaut::handleTogB2() { if(instance && instance->checkDebounce(10) && instance->togB2Callback) instance->togB2Callback(digitalRead(Pins::TOG_B2) == LOW); }
void MEMLNaut::handleJoySW() { if(instance && instance->checkDebounce(11) && instance->joySWCallback) instance->joySWCallback(digitalRead(Pins::JOY_SW) == LOW); }

MEMLNaut::MEMLNaut() {
    instance = this;
    loopCallback = nullptr;
    // Initialize median filters
    for(auto& filter : adcFilters) {
        filter.init(FILTER_SIZE);
    }

    // Initialise all pins
    Pins::initializePins();

    // Attach momentary switch interrupts (FALLING edge)
    attachInterrupt(digitalPinToInterrupt(Pins::MOM_A1), handleMomA1, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::MOM_A2), handleMomA2, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::MOM_B1), handleMomB1, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::MOM_B2), handleMomB2, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::RE_SW), handleReSW, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::RE_A), handleReA, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::RE_B), handleReB, FALLING);

    // Attach toggle switch interrupts (CHANGE)
    attachInterrupt(digitalPinToInterrupt(Pins::TOG_A1), handleTogA1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Pins::TOG_A2), handleTogA2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Pins::TOG_B1), handleTogB1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Pins::TOG_B2), handleTogB2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Pins::JOY_SW), handleJoySW, CHANGE);
}

// Momentary switch callback setters
void MEMLNaut::setMomA1Callback(ButtonCallback cb) { momA1Callback = cb; }
void MEMLNaut::setMomA2Callback(ButtonCallback cb) { momA2Callback = cb; }
void MEMLNaut::setMomB1Callback(ButtonCallback cb) { momB1Callback = cb; }
void MEMLNaut::setMomB2Callback(ButtonCallback cb) { momB2Callback = cb; }
void MEMLNaut::setReSWCallback(ButtonCallback cb) { reSWCallback = cb; }
void MEMLNaut::setReACallback(ButtonCallback cb) { reACallback = cb; }
void MEMLNaut::setReBCallback(ButtonCallback cb) { reBCallback = cb; }

// Toggle switch callback setters
void MEMLNaut::setTogA1Callback(ToggleCallback cb) { togA1Callback = cb; }
void MEMLNaut::setTogA2Callback(ToggleCallback cb) { togA2Callback = cb; }
void MEMLNaut::setTogB1Callback(ToggleCallback cb) { togB1Callback = cb; }
void MEMLNaut::setTogB2Callback(ToggleCallback cb) { togB2Callback = cb; }
void MEMLNaut::setJoySWCallback(ToggleCallback cb) { joySWCallback = cb; }

// ADC callback setters
void MEMLNaut::setJoyXCallback(AnalogCallback cb, uint16_t threshold) {
    adcStates[0] = {analogRead(Pins::JOY_X) / ADC_SCALE, threshold, cb};
}
void MEMLNaut::setJoyYCallback(AnalogCallback cb, uint16_t threshold) {
    adcStates[1] = {analogRead(Pins::JOY_Y) / ADC_SCALE, threshold, cb};
}
void MEMLNaut::setJoyZCallback(AnalogCallback cb, uint16_t threshold) {
    adcStates[2] = {analogRead(Pins::JOY_Z) / ADC_SCALE, threshold, cb};
}
void MEMLNaut::setRVGain1Callback(AnalogCallback cb, uint16_t threshold) {
    adcStates[3] = {analogRead(Pins::RV_GAIN1) / ADC_SCALE, threshold, cb};
}
void MEMLNaut::setRVZ1Callback(AnalogCallback cb, uint16_t threshold) {
    adcStates[4] = {analogRead(Pins::RV_Z1) / ADC_SCALE, threshold, cb};
}
void MEMLNaut::setRVY1Callback(AnalogCallback cb, uint16_t threshold) {
    adcStates[5] = {analogRead(Pins::RV_Y1) / ADC_SCALE, threshold, cb};
}
void MEMLNaut::setRVX1Callback(AnalogCallback cb, uint16_t threshold) {
    adcStates[6] = {analogRead(Pins::RV_X1) / ADC_SCALE, threshold, cb};
}

void MEMLNaut::setLoopCallback(LoopCallback cb) {
    loopCallback = cb;
}

bool MEMLNaut::checkDebounce(size_t index) {
    unsigned long now = millis();
    if (now - lastDebounceTime[index] >= DEBOUNCE_TIME) {
        lastDebounceTime[index] = now;
        return true;
    }
    return false;
}

void MEMLNaut::loop() {
    const uint8_t adcPins[NUM_ADCS] = {
        Pins::JOY_X, Pins::JOY_Y, Pins::JOY_Z,
        Pins::RV_GAIN1, Pins::RV_Z1, Pins::RV_Y1, Pins::RV_X1
    };

    for (size_t i = 0; i < NUM_ADCS; i++) {
        uint16_t rawValue = analogRead(adcPins[i]);
        uint16_t filteredValue = adcFilters[i].process(rawValue);
        float currentValue = filteredValue / ADC_SCALE;
        auto& state = adcStates[i];

        if (state.callback && abs(static_cast<int>(filteredValue - (state.lastValue * ADC_SCALE))) > state.threshold) {
            state.callback(currentValue);
            state.lastValue = currentValue;
        }
    }

    if (loopCallback) {
        loopCallback();
    }
}
