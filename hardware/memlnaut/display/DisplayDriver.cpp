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

    // Top bar is drawn directly in Draw() (no sprite buffers) to save RAM.

    tft_.fillScreen(TFT_BLACK);

    mainArea = {0, kMainTop, screenWidth_, screenHeight_ - kMainTop};

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

        // Clear the redraw flag now that screen is cleared
        // This allows rapid view changes while ensuring old content is removed
        redraw_internal_ = false;

        drawHeader();
        if (dialogView_) {
            dialogView_->invalidate();
        } else if (currentViewIndex_ < views_.size()) {
            views_[currentViewIndex_]->invalidate();
        }
    }
    // Encoder focus can change without a view change: repaint just the accent line.
    if (currentFocused() != accentFocused_) drawAccent();
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
            if (y <= topBarHeight) {
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
                    constexpr int kNavButtonWidth = 30;  // nav-arrow touch zone (was sprite width)
                    if (x < kNavButtonWidth) {
                        ChangeView(-1);
                    }
                    else if (x > tft_.width() - kNavButtonWidth) {
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


// ─── Header ─────────────────────────────────────────────────────────────────────────
// Dark bar: nav arrows (dimmed at the ends), centred title, a dot per screen with the
// current one lit, and an accent line underneath that turns yellow while the current
// view holds the encoder focus. Drawn only on a screen change (plus the accent line on a
// focus change), so it costs nothing per frame.

bool DisplayDriver::currentFocused() const {
    if (dialogView_ || currentViewIndex_ >= views_.size()) return false;
    return views_[currentViewIndex_]->isFocused();
}

void DisplayDriver::drawAccent() {
    accentFocused_ = currentFocused();
    tft_.fillRect(0, kHeaderH, screenWidth_, kAccentH,
                  accentFocused_ ? kAccentFocusColour : kAccentColour);
}

void DisplayDriver::drawHeader() {
    const int w = screenWidth_;

    // Rendered into a 4-bit sprite and pushed in one go: text drawn straight to the panel
    // came out corrupted. Palette indices below; ~4 KB, freed again after the push.
    enum : uint8_t { kBg = 0, kWhite, kArrow, kArrowDim, kDot, kDotCurrent };
    uint16_t palette[16] = {kHeaderBg, TFT_WHITE, kArrowColour, kArrowDimColour, kDotColour,
                            kDotCurrentColour};
    TFT_eSprite spr(&tft_);
    spr.setColorDepth(4);
    if (!spr.createSprite(w, kHeaderH)) {  // out of RAM: just clear the bar
        tft_.fillRect(0, 0, w, kHeaderH, kHeaderBg);
        drawAccent();
        return;
    }
    spr.createPalette(palette, 16);
    spr.fillSprite(kBg);

    String title;
    if (dialogView_) {
        title = dialogView_->GetName();
    } else {
        title = currentViewIndex_ < views_.size() ? views_[currentViewIndex_]->GetName() : String("No View");

        // Nav arrows: filled triangles, dimmed when there is nowhere to go.
        const int cy = kTitleY;
        const uint8_t backCol = currentViewIndex_ > 0 ? kArrow : kArrowDim;
        const uint8_t fwdCol = currentViewIndex_ + 1 < views_.size() ? kArrow : kArrowDim;
        spr.fillTriangle(6, cy, 15, cy - 7, 15, cy + 7, backCol);
        spr.fillTriangle(w - 7, cy, w - 16, cy - 7, w - 16, cy + 7, fwdCol);

        // Position dots, centred under the title.
        const int n = static_cast<int>(views_.size());
        const int x0 = w / 2 - ((n - 1) * kDotPitch) / 2;
        for (int i = 0; i < n; i++) {
            const bool cur = i == static_cast<int>(currentViewIndex_);
            spr.fillRect(x0 + i * kDotPitch - 1, kDotY - 1, 3, 3, cur ? kDotCurrent : kDot);
        }
    }

    // Orbitron, in capitals (no descenders to run into the dots), on a fixed baseline.
    // It is wide, so a title that won't fit between the arrows falls back to mixed case.
    spr.setFreeFont(&Orbitron_Light_24);
    spr.setTextColor(kWhite);
    spr.setTextDatum(C_BASELINE);
    String caps = title;
    caps.toUpperCase();
    spr.drawString((spr.textWidth(caps) <= w - 2 * kTitleMargin ? caps : title).c_str(),
                   w / 2, kTitleBaseline);

    spr.pushSprite(0, 0);
    spr.deleteSprite();

    drawAccent();
}
