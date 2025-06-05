#ifndef __TEXT_VIEW_HPP__
#define __TEXT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class TextView : public ViewBase {
public:
    TextView(const char* name,      // Changed parameter type
             const char* content,    // Changed parameter type
             uint16_t color);

    void OnSetup() override;  // Changed from Setup to OnSetup
    void Draw() override;  // No longer takes TFT pointer
    void HandleTouch(size_t x, size_t y) override;
    void HandleRelease() override;

private:
    void OnButtonPressed(bool state);
    void OnTogglePressed(bool state) {
        Serial.print("Toggle pressed: ");
        Serial.println(state ? "TRUE" : "FALSE");
    }
    const char* content_;
    uint16_t color_;
    Button button_{"Press Me", 0x041F, false};  // Navy blue to deep blue, non-toggle
    Button toggle_{"Toggle Me", 0x041F, true};  // Navy blue to deep blue, toggle
};

#endif // __TEXT_VIEW_HPP__
