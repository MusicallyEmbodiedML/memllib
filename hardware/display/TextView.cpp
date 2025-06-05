#include "TextView.hpp"

TextView::TextView(const char* name, const char* content, uint16_t color)
    : ViewBase(name)
    , content_(content)
    , color_(color)
    , button_("Press Me", 0x041f, false)
    , toggle_("Toggle Me", 0x041f, true)
{}

void TextView::OnSetup() {
    button_.SetGrid(grid_, 0, 3);
    button_.Setup(tft_);
    button_.SetCallback([this](bool state) { OnButtonPressed(state); });

    toggle_.SetGrid(grid_, 3, 3);
    toggle_.Setup(tft_);
    toggle_.SetCallback([this](bool state) { OnTogglePressed(state); });
}

void TextView::Draw() {
    tft_->setTextColor(color_);
    // Draw the content at the third row, first column of the grid
    size_t gridX = 0;
    size_t gridY = 2; // Third row
    tft_->drawString(content_, gridX * grid_.widthStep, gridY * grid_.heightStep, 2);
    needRedraw_ = false;
    // Draw the button
    button_.Draw();
    // Draw the toggle button
    toggle_.Draw();
}

void TextView::HandleTouch(size_t x, size_t y) {
    // Serial.print("TextView HandleTouch at: ");
    // Serial.print(x);
    // Serial.print(", ");
    // Serial.println(y);
    // Pass touch coordinates to the button for interaction
    button_.Interact(x, y);
    toggle_.Interact(x, y);
}

void TextView::HandleRelease() {
    // Serial.println("TextView HandleRelease");
    // Handle release events for the button
    //button_.Interact(0, 0); // Pass dummy coordinates since release doesn't need them
    button_.Release();
    toggle_.Release();
}

void TextView::OnButtonPressed(bool state) {
    Serial.print("Button pressed: ");
    Serial.println(state ? "TRUE" : "FALSE");
}
