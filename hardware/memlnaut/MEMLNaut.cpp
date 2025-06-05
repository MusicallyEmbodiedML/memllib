#include <iterator>
#include "MEMLNaut.hpp"

MEMLNaut* MEMLNaut::instance = nullptr;

// Static interrupt handlers implementation
void MEMLNaut::handleMomA1() {
    if (instance && instance->debouncers[0].debounce() && instance->momA1Callback) {
        instance->momA1Callback();
    }
}
void MEMLNaut::handleMomA2() {
    if (instance && instance->debouncers[1].debounce() && instance->momA2Callback) {
        instance->momA2Callback();
    }
}
void MEMLNaut::handleMomB1() {
    if (instance && instance->debouncers[2].debounce() && instance->momB1Callback) {
        instance->momB1Callback();
    }
}
void MEMLNaut::handleMomB2() {
    if (instance && instance->debouncers[3].debounce() && instance->momB2Callback) {
        instance->momB2Callback();
    }
}
void MEMLNaut::handleReSW() {
    if (instance && instance->debouncers[4].debounce() && instance->reSWCallback) {
        instance->reSWCallback();
    }
}

// Update ReA/B handlers to only handle encoder
void MEMLNaut::handleReA() {
    if (instance && instance->encoderCallback) {
        int8_t change = instance->readRotary();
        if (change != 0) {
            instance->encoderCallback(change);
        }
    }
}

void MEMLNaut::handleReB() {
    if (instance && instance->encoderCallback) {
        int8_t change = instance->readRotary();
        if (change != 0) {
            instance->encoderCallback(change);
        }
    }
}

void MEMLNaut::handleTogA1() {
    if (instance) {
        bool should_update = instance->toggleDebouncers[0].debounce(digitalRead(Pins::TOG_A1) == LOW);
        if (should_update && instance->togA1Callback) {
            bool val = instance->toggleDebouncers[0].getState();
            instance->togA1Callback(val);
        }
    }
}
void MEMLNaut::handleTogA2() {
    if (instance) {
        bool should_update = instance->toggleDebouncers[1].debounce(digitalRead(Pins::TOG_A2) == LOW);
        if (should_update && instance->togA2Callback) {
            bool val = instance->toggleDebouncers[1].getState();
            instance->togA2Callback(val);
        }
    }
}
void MEMLNaut::handleTogB1() {
    if (instance) {
        bool should_update = instance->toggleDebouncers[2].debounce(digitalRead(Pins::TOG_B1) == LOW);
        if (should_update && instance->togB1Callback) {
            bool val = instance->toggleDebouncers[2].getState();
            instance->togB1Callback(val);
        }
    }
}
void MEMLNaut::handleTogB2() {
    if (instance) {
        bool should_update = instance->toggleDebouncers[3].debounce(digitalRead(Pins::TOG_B2) == LOW);
        if (should_update && instance->togB2Callback) {
            bool val = instance->toggleDebouncers[3].getState();
            instance->togB2Callback(val);
        }
    }
}
void MEMLNaut::handleJoySW() {
    if (instance) {
        bool should_update = instance->toggleDebouncers[4].debounce(digitalRead(Pins::JOY_SW) == LOW);
        if (should_update && instance->joySWCallback) {
            bool val = instance->toggleDebouncers[4].getState();
            instance->joySWCallback(val);
        }
    }
}

int8_t MEMLNaut::readRotary() {
    static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

    prevNextCode <<= 2;
    if (digitalRead(Pins::RE_B)) prevNextCode |= 0x02;
    if (digitalRead(Pins::RE_A)) prevNextCode |= 0x01;
    prevNextCode &= 0x0f;
    Serial.println(prevNextCode);

    if (rot_enc_table[prevNextCode]) {
        encoderStore <<= 4;
        encoderStore |= prevNextCode;
        if ((encoderStore & 0xff) == 0x2b) return -1;
        if ((encoderStore & 0xff) == 0x17) return 1;
    }
    return 0;
}

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

    prevNextCode = 0;
    encoderStore = 0;
    encoderCallback = nullptr;
}

// Momentary switch callback setters
void MEMLNaut::setMomA1Callback(ButtonCallback cb) { momA1Callback = cb; }
void MEMLNaut::setMomA2Callback(ButtonCallback cb) { momA2Callback = cb; }
void MEMLNaut::setMomB1Callback(ButtonCallback cb) { momB1Callback = cb; }
void MEMLNaut::setMomB2Callback(ButtonCallback cb) { momB2Callback = cb; }
void MEMLNaut::setReSWCallback(ButtonCallback cb) { reSWCallback = cb; }

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

void MEMLNaut::setEncoderCallback(EncoderCallback cb) {
    encoderCallback = cb;
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
