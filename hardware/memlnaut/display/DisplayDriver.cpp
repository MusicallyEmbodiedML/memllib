#include "DisplayDriver.hpp"

void DisplayDriver::Setup() {
    // Clear screen
    Serial.println("display setup");
    Serial.println("init tft...");
    tft_.init();
    tft_.setRotation(1);
    tft_.fillScreen(TFT_BLACK);
    tft_initialized_ = true;
    Serial.println("display init");

    // Set up touch
    tft_.setTouch(calData_);
    isTouchPressed_ = false;
    Serial.println("touch init");

    // Set up grid dimensions
    screenWidth_ = tft_.width();
    screenHeight_ = tft_.height();
    grid_.widthElements = kGridWidthElements;
    grid_.heightElements = kGridHeightElements;
    grid_.widthStep = screenWidth_ / grid_.widthElements;
    grid_.heightStep = screenHeight_ / grid_.heightElements;

    constexpr int topBarHeight = 30;
    leftButton.createSprite(30,topBarHeight);
    title.createSprite(150,topBarHeight);
    rightButton.createSprite(30,topBarHeight);

    leftButton.setTextFont(4);
    leftButton.setTextColor(TFT_BLUE, TFT_WHITE);
    leftButton.setTextDatum(TL_DATUM);
    leftButton.fillSprite(TFT_WHITE);
    leftButton.drawString("<", 3,3);

    rightButton.setTextFont(4);
    rightButton.setTextColor(TFT_BLUE, TFT_WHITE);
    rightButton.setTextDatum(TL_DATUM);
    rightButton.fillSprite(TFT_WHITE);
    rightButton.drawString(">", 3,3);

    title.setTextFont(4);
    title.setTextColor(TFT_BLUE, TFT_WHITE);
    title.setTextDatum(TL_DATUM);
        
    tft_.fillScreen(TFT_BLACK);

    mainArea = {0, topBarHeight + 5, screenWidth_, screenHeight_ - topBarHeight - 5};

    // Set up views
    currentViewIndex_ = 0;
    for (auto &view : views_) {
        view->SetGrid(grid_);
        view->Setup(&tft_, mainArea);
    }

    // Set up internal view

    // Trigger initial redraw
    redraw_internal_ = true;
}

void DisplayDriver::Draw() {
    // Serial.println("display draw");
    lastDrawTime_ = millis();

    // Check if any of the views need redrawing
    bool needRedraw = false;
    for (const auto &view : views_) {
        if (view->NeedRedraw()) {
            needRedraw = true;
            break;
        }
    }
    if (needRedraw || redraw_internal_) {
        redraw_internal_ = false;

        // Clear screen
        // tft_.fillScreen(TFT_BLACK);
        // tft_.setTextColor(TFT_WHITE);
        tft_.fillRect(0, 0, tft_.width(), 30, TFT_WHITE);

        // tft_.setFreeFont(&FreeSansBoldOblique24pt7b);
        // tft_.setTextFont(4);

        // Draw back arrow in first column of first row if not on first view
        if (currentViewIndex_ > 0) {
            // tft_.drawString("<", grid_.widthStep / 2, grid_.heightStep / 2);
            leftButton.pushSprite(0, 0);

        }
        // Draw forward arrow in last column of first row if not on last view
        if (currentViewIndex_ < views_.size() - 1) {
            // tft_.drawString(">", (grid_.widthElements - 1) * grid_.widthStep + grid_.widthStep / 2, grid_.heightStep / 2);
            rightButton.pushSprite(tft_.width() - rightButton.width(), 0);

        }
        title.fillSprite(TFT_WHITE);
        // Draw title of current view between arrows
        if (currentViewIndex_ < views_.size()) {
            const std::string &viewName = views_[currentViewIndex_]->GetName();
            title.drawString(viewName.c_str(), 3,3);

            // tft_.drawString(viewName.c_str(), grid_.widthStep, grid_.heightStep / 2);
        } else {
            title.drawString("No View", 3,3);
            // tft_.drawString("No View", grid_.widthStep, grid_.heightStep / 2, 2);
        }
        title.pushSprite(40, 0);

        // Draw current view
        if (currentViewIndex_ < views_.size()) {
            views_[currentViewIndex_]->Draw();  // No longer passing tft_
        }
    }
}

void DisplayDriver::PollTouch() {
    lastTouchTime_ = millis();

    uint16_t x, y;
    bool pressed = tft_.getTouch(&x, &y);

    if (currentViewIndex_ < views_.size()) {
        // Calculate grid position
        size_t gridX = x / grid_.widthStep;
        size_t gridY = y / grid_.heightStep;

        if (pressed && !isTouchPressed_) {
            // If within first row, handle navigation
            if (y <= leftButton.height()) {
                if (x < leftButton.width() && currentViewIndex_ > 0) {
                    // Send a handle touch outside the screen and handle release to current view
                    views_[currentViewIndex_]->HandleTouch(9999,9999);
                    views_[currentViewIndex_]->HandleRelease();
                    // Navigate to previous view
                    currentViewIndex_--;
                    redraw_internal_ = true;
                }
                else if (x > tft_.width() - rightButton.width() && currentViewIndex_ < views_.size() - 1) {
                    // Send a handle touch outside the screen and handle release to current view
                    views_[currentViewIndex_]->HandleTouch(9999,9999);
                    views_[currentViewIndex_]->HandleRelease();
                    // Navigate to next view
                    currentViewIndex_++;
                    redraw_internal_ = true;
                }
            } else {
                // Handle touch in the current view
                if (gridX < grid_.widthElements && gridY < grid_.heightElements) {
                    // Pass raw coordinates to the current view for precise detection
                    views_[currentViewIndex_]->HandleTouch(x, y);
                }
            }
        } else if (isTouchPressed_) {
            // If released, check if any button was released
            views_[currentViewIndex_]->HandleRelease();
        }
    }

    isTouchPressed_ = pressed;
}
