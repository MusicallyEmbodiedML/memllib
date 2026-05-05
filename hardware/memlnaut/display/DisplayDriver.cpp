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

    leftButton.createSprite(30,topBarHeight);
    title.createSprite(180,topBarHeight);
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
    if (redraw_internal_) {

        // Clear screen
        tft_.fillScreen(TFT_BLACK);
        // tft_.setTextColor(TFT_WHITE);
        tft_.fillRect(0, 0, tft_.width(), 30, TFT_WHITE);

        // Clear the redraw flag now that screen is cleared
        // This allows rapid view changes while ensuring old content is removed
        redraw_internal_ = false;

        // tft_.setFreeFont(&FreeSansBoldOblique24pt7b);
        // tft_.setTextFont(4);

        title.fillSprite(TFT_WHITE);
        if (dialogView_) {
            // Dialog active: no nav arrows, show dialog title
            title.drawString(dialogView_->GetName().c_str(), 3, 3);
            title.pushSprite(40, 0);
            dialogView_->redraw();
        } else {
            // Draw back arrow in first column of first row if not on first view
            if (currentViewIndex_ > 0) {
                leftButton.pushSprite(0, 0);
            }
            // Draw forward arrow in last column of first row if not on last view
            if (currentViewIndex_ < views_.size() - 1) {
                rightButton.pushSprite(tft_.width() - rightButton.width(), 0);
            }
            // Draw title of current view between arrows
            if (currentViewIndex_ < views_.size()) {
                title.drawString(views_[currentViewIndex_]->GetName().c_str(), 3, 3);
            } else {
                title.drawString("No View", 3, 3);
            }
            title.pushSprite(40, 0);
            views_[currentViewIndex_]->redraw();
        }
    }
    if (dialogView_) {
        dialogView_->Draw();
    } else if (currentViewIndex_ < views_.size()) {
        views_[currentViewIndex_]->Draw();
    }


}

void DisplayDriver::NavigateToView(const std::shared_ptr<ViewBase>& target) {
    auto it = std::find(views_.begin(), views_.end(), target);
    if (it == views_.end()) return;
    size_t targetIndex = static_cast<size_t>(it - views_.begin());
    if (targetIndex == currentViewIndex_) return;
    views_[currentViewIndex_]->setVisible(false);
    views_[currentViewIndex_]->removeFocus();
    views_[currentViewIndex_]->OnHide();
    currentViewIndex_ = targetIndex;
    views_[currentViewIndex_]->setVisible(true);
    views_[currentViewIndex_]->OnDisplay();
    redraw_internal_ = true;
}

void DisplayDriver::ShowDialog(const std::shared_ptr<ViewBase>& dialog) {
    dialogView_ = dialog;
    dialog->OnDisplay();
    redraw_internal_ = true;
}

void DisplayDriver::DismissDialog() {
    if (dialogView_) {
        dialogView_->OnHide();
        dialogView_ = nullptr;
        redraw_internal_ = true;
    }
}

void DisplayDriver::ChangeView(int delta) {
    if (dialogView_) return;
    if (!redraw_internal_) { //wait until the current redraw is finished
        auto lastViewIndex = currentViewIndex_;
        bool viewChange = false;
        if (delta < 0 && currentViewIndex_ > 0) {
            currentViewIndex_--;
            viewChange = true;
        } else if (delta > 0 && currentViewIndex_ < views_.size() - 1) {
            currentViewIndex_++;
            viewChange = true;
        }
        if (viewChange) {
            // If view changed, redraw the new view
            redraw_internal_ = true;
            views_[lastViewIndex]->setVisible(false);
            views_[lastViewIndex]->removeFocus();
            views_[lastViewIndex]->OnHide();  // Call OnHide for the old view
            views_[currentViewIndex_]->setVisible(true);
            views_[currentViewIndex_]->OnDisplay();  // Call OnDisplay for the new view
        }
    }
}

void DisplayDriver::PollTouch() {
    // lastTouchTime_ = millis();

    uint16_t x, y;
    bool pressed = tft_.getTouch(&x, &y, 20);
    if(pressed) {
        lastTouchX = x;
        lastTouchY = y;
    }

    if (currentViewIndex_ < views_.size()) {
        bool viewChange = false;
        if (pressed && !isTouchPressed_) {
            auto lastViewIndex = currentViewIndex_;
            // If within first row, handle navigation
            if (y <= leftButton.height()) {
                // if (x < leftButton.width() && currentViewIndex_ > 0) {
                //     // Navigate to previous view
                //     currentViewIndex_--;
                //     redraw_internal_ = true;
                //     viewChange = true;
                // }
                // else if (x > tft_.width() - rightButton.width() && currentViewIndex_ < views_.size() - 1) {
                //     // Navigate to next view
                //     currentViewIndex_++;
                //     redraw_internal_ = true;
                //     viewChange = true;
                // }
                if (!dialogView_) {
                    if (x < leftButton.width()) {
                        ChangeView(-1);
                    }
                    else if (x > tft_.width() - rightButton.width()) {
                        ChangeView(1);
                    }
                }
            } else {
                auto& activeView = dialogView_ ? dialogView_ : views_[currentViewIndex_];
                activeView->HandleTouch(lastTouchX, lastTouchY);
            }
            // if (viewChange) {
            //     // If view changed, redraw the new view
            //     views_[lastViewIndex]->setVisible(false);
            //     views_[lastViewIndex]->OnHide();  // Call OnHide for the old view
            //     views_[currentViewIndex_]->setVisible(true);
            //     views_[currentViewIndex_]->OnDisplay();  // Call OnDisplay for the new view
            // }
            isTouchPressed_ = true;
        } else if (isTouchPressed_) {
            //drag event
        } 
        if (!pressed && isTouchPressed_) {
            // If touch was released, handle release
            // views_[currentViewIndex_]->HandleRelease();
            // If released, check if any button was released
            auto& activeView = dialogView_ ? dialogView_ : views_[currentViewIndex_];
            activeView->HandleTouchRelease(lastTouchX, lastTouchY);
            Serial.println("Touch released");   
            Serial.print("x: ");
            Serial.print(x);
            Serial.print(", y: ");
            Serial.println(y);
            isTouchPressed_ = false;
        }
    }

}
