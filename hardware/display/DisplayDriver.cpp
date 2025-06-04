#include "DisplayDriver.hpp"

void DisplayDriver::Setup() {
    // Clear screen
    tft_.init();
    tft_.setRotation(1);
    tft_.fillScreen(TFT_BLACK);

    // Set up touch
    tft_.setTouch(calData_);

    // Set up grid dimensions
    screenWidth_ = tft_.width();
    screenHeight_ = tft_.height();
    gridWidthStep_ = screenWidth_ / kGridWidthElements;
    gridHeightStep_ = screenHeight_ / kGridHeightElements;

    // Set up views
    currentViewIndex_ = 0;
    for (auto &view : views_) {
        view->Setup();
    }

    // Set up internal view
}

void DisplayDriver::Draw() {
}
