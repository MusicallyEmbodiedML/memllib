#include "TextView.hpp"

TextView::TextView(const char* name,    // Changed parameter type
                   const char* content,   // Changed parameter type
                   uint16_t color)
    : ViewBase(name), content_(content),
      color_(color),
      button_("Press Me", 0x041f)  // Initialize button with label and color
{}

void TextView::OnSetup() {
    button_.SetGrid(grid_, 0, 3);
    button_.Setup(tft_);
    button_.SetCallback([this](bool state) { OnButtonPressed(state); });
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
}

void TextView::HandleTouch(size_t x, size_t y) {
    Serial.print("TextView HandleTouch at: ");
    Serial.print(x);
    Serial.print(", ");
    Serial.println(y);
    // Pass touch coordinates to the button for interaction
    button_.Interact(x, y);
}

void TextView::OnButtonPressed(bool state) {
    Serial.print("Button pressed: ");
    Serial.println(state ? "TRUE" : "FALSE");
}
