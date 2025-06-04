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
    grid_.widthElements = kGridWidthElements;
    grid_.heightElements = kGridHeightElements;
    grid_.widthStep = screenWidth_ / grid_.widthElements;
    grid_.heightStep = screenHeight_ / grid_.heightElements;

    // Set up views
    currentViewIndex_ = 0;
    for (auto &view : views_) {
        view->SetGrid(grid_);
        view->Setup();
    }

    // Set up internal view

    // Trigger initial redraw
    redraw_internal_ = true;
}

void DisplayDriver::Draw() {
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
        tft_.fillScreen(TFT_BLACK);
        tft_.setTextColor(TFT_WHITE);

        // Draw back arrow in first column of first row if not on first view
        if (currentViewIndex_ > 0) {
            tft_.drawString("<", grid_.widthStep / 2, grid_.heightStep / 2, 2);
        }
        // Draw forward arrow in last column of first row if not on last view
        if (currentViewIndex_ < views_.size() - 1) {
            tft_.drawString(">", (grid_.widthElements - 1) * grid_.widthStep + grid_.widthStep / 2, grid_.heightStep / 2, 2);
        }
        // Draw title of current view between arrows
        if (currentViewIndex_ < views_.size()) {
            const std::string &viewName = views_[currentViewIndex_]->GetName();
            tft_.drawString(viewName.c_str(), grid_.widthStep, grid_.heightStep / 2, 2);
        } else {
            tft_.drawString("No View", grid_.widthStep, grid_.heightStep / 2, 2);
        }

        // Draw current view
        if (currentViewIndex_ < views_.size()) {
            views_[currentViewIndex_]->Draw(&tft_);
        }
    }
}

void DisplayDriver::PollTouch()
{
    lastTouchTime_ = millis();

    // Get touch coordinates
    uint16_t x, y;
    if (tft_.getTouch(&x, &y)) {
        // Check if touch is within grid bounds
        if (x < screenWidth_ && y < screenHeight_) {
            // Calculate grid position
            size_t gridX = x / grid_.widthStep;
            size_t gridY = y / grid_.heightStep;

            // If within first row, handle internally
            if (gridY == 0) {
                // If first column, switch to previous view
                if (gridX == 0 && currentViewIndex_ > 0) {
                    currentViewIndex_--;
                }
                // If last column, switch to next view
                else if (gridX == kGridWidthElements - 1 && currentViewIndex_ < views_.size() - 1) {
                    currentViewIndex_++;
                }
                redraw_internal_ = true;
                return; // Exit early to avoid handling touch in the current view
            }

            // Handle touch event for the current view
            if (currentViewIndex_ < views_.size()) {
                views_[currentViewIndex_]->HandleTouch(gridX, gridY);
            }
        }
    }
}
