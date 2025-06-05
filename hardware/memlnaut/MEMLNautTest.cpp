#include "MEMLNaut.hpp"
#include <Arduino.h>

// Button callback functions
void onMomA1() { Serial.println("MOM_A1 pressed"); }
void onMomA2() { Serial.println("MOM_A2 pressed"); }
void onMomB1() { Serial.println("MOM_B1 pressed"); }
void onMomB2() { Serial.println("MOM_B2 pressed"); }
void onReSW() { Serial.println("RE_SW pressed"); }

// Toggle callback functions
void onTogA1(bool state) { Serial.printf("TOG_A1: %s\n", state ? "ON" : "OFF"); }
void onTogA2(bool state) { Serial.printf("TOG_A2: %s\n", state ? "ON" : "OFF"); }
void onTogB1(bool state) { Serial.printf("TOG_B1: %s\n", state ? "ON" : "OFF"); }
void onTogB2(bool state) { Serial.printf("TOG_B2: %s\n", state ? "ON" : "OFF"); }
void onJoySW(bool state) { Serial.printf("JOY_SW: %s\n", state ? "ON" : "OFF"); }

// ADC callback functions
void onJoyX(float value) { Serial.printf("JOY_X: %.3f\n", value); }
void onJoyY(float value) { Serial.printf("JOY_Y: %.3f\n", value); }
void onJoyZ(float value) { Serial.printf("JOY_Z: %.3f\n", value); }
void onRVGain1(float value) { Serial.printf("RV_GAIN1: %.3f\n", value); }
void onRVZ1(float value) { Serial.printf("RV_Z1: %.3f\n", value); }
void onRVY1(float value) { Serial.printf("RV_Y1: %.3f\n", value); }
void onRVX1(float value) { Serial.printf("RV_X1: %.3f\n", value); }

// Encoder callback function
void onEncoder(int32_t value) {
    if (value > 0) {
        Serial.println("Encoder: Clockwise");
    } else if (value < 0) {
        Serial.println("Encoder: Counter-clockwise");
    }
}

namespace MEMLNautTest {
    void Setup() {
        Serial.begin(115200);
        while (!Serial) delay(10);
        Serial.println("MEMLNaut Test Starting...");

        // Set up momentary switch callbacks
        MEMLNaut::Instance()->setMomA1Callback(onMomA1);
        MEMLNaut::Instance()->setMomA2Callback(onMomA2);
        MEMLNaut::Instance()->setMomB1Callback(onMomB1);
        MEMLNaut::Instance()->setMomB2Callback(onMomB2);
        MEMLNaut::Instance()->setReSWCallback(onReSW);

        // Set up toggle switch callbacks
        MEMLNaut::Instance()->setTogA1Callback(onTogA1);
        MEMLNaut::Instance()->setTogA2Callback(onTogA2);
        MEMLNaut::Instance()->setTogB1Callback(onTogB1);
        MEMLNaut::Instance()->setTogB2Callback(onTogB2);
        MEMLNaut::Instance()->setJoySWCallback(onJoySW);

        // Set up ADC callbacks with default threshold
        MEMLNaut::Instance()->setJoyXCallback(onJoyX);
        MEMLNaut::Instance()->setJoyYCallback(onJoyY);
        MEMLNaut::Instance()->setJoyZCallback(onJoyZ);
        MEMLNaut::Instance()->setRVGain1Callback(onRVGain1);
        MEMLNaut::Instance()->setRVZ1Callback(onRVZ1);
        MEMLNaut::Instance()->setRVY1Callback(onRVY1);
        MEMLNaut::Instance()->setRVX1Callback(onRVX1);

        // Set up encoder callback
        MEMLNaut::Instance()->setEncoderCallback(onEncoder);

        Serial.println("MEMLNaut Test Setup Complete!");
    }
}
