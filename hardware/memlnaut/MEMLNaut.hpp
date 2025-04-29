#ifndef __MEMLNAUT_HPP__
#define __MEMLNAUT_HPP__

#include "Pins.hpp"
#include "../../utils/Debounce.hpp"
#include "../../utils/MedianFilter.h"
#include <functional>
#include <array>

class MEMLNaut {
public:
    using ButtonCallback = std::function<void(void)>;
    using ToggleCallback = std::function<void(bool)>;
    using AnalogCallback = std::function<void(float)>;
    using LoopCallback = std::function<void(void)>;

private:
    static MEMLNaut* instance;
    static constexpr size_t NUM_ADCS = 7;
    static constexpr uint16_t DEFAULT_THRESHOLD = 100;
    static constexpr size_t FILTER_SIZE = 5;
    static constexpr float ADC_SCALE = 4128.7f;

    static constexpr size_t NUM_BUTTONS = 7;
    static constexpr size_t NUM_TOGGLES = 5;

    struct ADCState {
        float lastValue = 0.0f;
        uint16_t threshold = DEFAULT_THRESHOLD;
        AnalogCallback callback = nullptr;
    };

    std::array<ADCState, NUM_ADCS> adcStates;
    std::array<MedianFilter<uint16_t>, NUM_ADCS> adcFilters;

    std::array<ButtonDebounce, NUM_BUTTONS> debouncers;
    std::array<ToggleDebounce, NUM_TOGGLES> toggleDebouncers;

    // Callback storage
    ButtonCallback momA1Callback;
    ButtonCallback momA2Callback;
    ButtonCallback momB1Callback;
    ButtonCallback momB2Callback;
    ButtonCallback reSWCallback;
    ButtonCallback reACallback;
    ButtonCallback reBCallback;

    ToggleCallback togA1Callback;
    ToggleCallback togA2Callback;
    ToggleCallback togB1Callback;
    ToggleCallback togB2Callback;
    ToggleCallback joySWCallback;

    LoopCallback loopCallback;

    // Static interrupt handlers
    static void handleMomA1();
    static void handleMomA2();
    static void handleMomB1();
    static void handleMomB2();
    static void handleReSW();
    static void handleReA();
    static void handleReB();

    static void handleTogA1();
    static void handleTogA2();
    static void handleTogB1();
    static void handleTogB2();
    static void handleJoySW();

public:
    static MEMLNaut* Instance() {
        return instance;
    }

    static void Initialize() {
        if (!instance) {
            instance = new MEMLNaut();
        }
    }

    // Delete copy constructor and assignment operator
    MEMLNaut(const MEMLNaut&) = delete;
    MEMLNaut& operator=(const MEMLNaut&) = delete;

    MEMLNaut();

    // Set callbacks for momentary switches
    void setMomA1Callback(ButtonCallback cb);
    void setMomA2Callback(ButtonCallback cb);
    void setMomB1Callback(ButtonCallback cb);
    void setMomB2Callback(ButtonCallback cb);
    void setReSWCallback(ButtonCallback cb);
    void setReACallback(ButtonCallback cb);
    void setReBCallback(ButtonCallback cb);

    // Set callbacks for toggle switches
    void setTogA1Callback(ToggleCallback cb);
    void setTogA2Callback(ToggleCallback cb);
    void setTogB1Callback(ToggleCallback cb);
    void setTogB2Callback(ToggleCallback cb);
    void setJoySWCallback(ToggleCallback cb);

    // ADC callback setters
    void setJoyXCallback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);
    void setJoyYCallback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);
    void setJoyZCallback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);
    void setRVGain1Callback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);
    void setRVZ1Callback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);
    void setRVY1Callback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);
    void setRVX1Callback(AnalogCallback cb, uint16_t threshold = DEFAULT_THRESHOLD);

    // Main loop callback setter
    void setLoopCallback(LoopCallback cb);

    void loop();
};

#endif // __MEMLNAUT_HPP__
