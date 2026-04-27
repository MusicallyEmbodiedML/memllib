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
    frames_[0].index = 0;
    for (auto &view : frames_[0].views) {
        view->SetGrid(grid_);
        view->Setup(&tft_, mainArea);
    }

    // Set up internal view

    // Trigger initial redraw
    redraw_internal_ = true;
}

void DisplayDriver::Draw() {
    lastDrawTime_ = millis();

    auto& frame = currentFrame();

    if (redraw_internal_) {
        tft_.fillScreen(TFT_BLACK);
        tft_.fillRect(0, 0, tft_.width(), 30, TFT_WHITE);
        redraw_internal_ = false;

        leftButton.fillSprite(TFT_WHITE);
        rightButton.fillSprite(TFT_WHITE);

        if (stackDepth_ > 0) {
            // Inside a section: left button is Back
            leftButton.setTextFont(4);
            leftButton.setTextColor(TFT_BLUE, TFT_WHITE);
            leftButton.setTextDatum(TL_DATUM);
            leftButton.drawString("^", 3, 3);
            leftButton.pushSprite(0, 0);

            // Right arrow if not at last child
            if (frame.index < frame.views.size() - 1) {
                rightButton.pushSprite(tft_.width() - rightButton.width(), 0);
            }

            // Title: "Section > View"
            title.fillSprite(TFT_WHITE);
            title.setTextFont(4);
            title.setTextColor(TFT_BLUE, TFT_WHITE);
            title.setTextDatum(TL_DATUM);
            String titleStr = frame.sectionName + ">" + frame.views[frame.index]->GetName();
            title.drawString(titleStr.c_str(), 3, 3);
            title.pushSprite(40, 0);
        } else {
            // Top level: existing behaviour
            if (frame.index > 0) {
                leftButton.pushSprite(0, 0);
            }
            if (frame.index < frame.views.size() - 1) {
                rightButton.pushSprite(tft_.width() - rightButton.width(), 0);
            }
            title.fillSprite(TFT_WHITE);
            if (frame.index < frame.views.size()) {
                title.drawString(frame.views[frame.index]->GetName().c_str(), 3, 3);
            } else {
                title.drawString("No View", 3, 3);
            }
            title.pushSprite(40, 0);
        }

        if (frame.index < frame.views.size()) {
            frame.views[frame.index]->redraw();
        }
    }
    if (frame.index < frame.views.size()) {
        frame.views[frame.index]->Draw();
    }
}

void DisplayDriver::NavigateToView(const std::shared_ptr<ViewBase>& target) {
    // Search top-level views first
    auto& topViews = frames_[0].views;
    for (size_t i = 0; i < topViews.size(); i++) {
        if (topViews[i] == target) {
            // Hide current view (wherever we are)
            auto& cf = currentFrame();
            if (cf.index < cf.views.size()) {
                cf.views[cf.index]->setVisible(false);
                cf.views[cf.index]->removeFocus();
                cf.views[cf.index]->OnHide();
            }
            if (stackDepth_ > 0) {
                frames_[1].views.clear();
                stackDepth_ = 0;
            }
            frames_[0].index = i;
            topViews[i]->setVisible(true);
            topViews[i]->OnDisplay();
            redraw_internal_ = true;
            return;
        }
        // Search section children
        auto* children = topViews[i]->getSectionChildren();
        if (children) {
            for (size_t j = 0; j < children->size(); j++) {
                if ((*children)[j] == target) {
                    navigateInto(*children, topViews[i]->GetName(), j);
                    return;
                }
            }
        }
    }
}

void DisplayDriver::navigateInto(const std::vector<std::shared_ptr<ViewBase>>& children,
                                  const String& sectionName, size_t startIndex) {
    if (children.empty()) return;

    // Hide current view
    auto& cf = currentFrame();
    if (cf.index < cf.views.size()) {
        cf.views[cf.index]->setVisible(false);
        cf.views[cf.index]->removeFocus();
        cf.views[cf.index]->OnHide();
    }

    frames_[1].views = children;
    frames_[1].sectionName = sectionName;
    frames_[1].index = constrain((int)startIndex, 0, (int)children.size() - 1);
    stackDepth_ = 1;

    children[frames_[1].index]->setVisible(true);
    children[frames_[1].index]->OnDisplay();
    redraw_internal_ = true;
}

void DisplayDriver::navigateBack() {
    if (stackDepth_ == 0) return;

    auto& frame = frames_[1];
    if (frame.index < frame.views.size()) {
        frame.views[frame.index]->setVisible(false);
        frame.views[frame.index]->removeFocus();
        frame.views[frame.index]->OnHide();
    }
    frame.views.clear();
    stackDepth_ = 0;

    auto& top = frames_[0];
    if (top.index < top.views.size()) {
        top.views[top.index]->setVisible(true);
        top.views[top.index]->OnDisplay();
    }
    redraw_internal_ = true;
}

void DisplayDriver::ChangeView(int delta) {
    if (redraw_internal_) return;
    auto& frame = currentFrame();
    size_t lastIndex = frame.index;
    bool viewChange = false;
    if (delta < 0 && frame.index > 0) {
        frame.index--;
        viewChange = true;
    } else if (delta > 0 && frame.index < frame.views.size() - 1) {
        frame.index++;
        viewChange = true;
    }
    if (viewChange) {
        redraw_internal_ = true;
        frame.views[lastIndex]->setVisible(false);
        frame.views[lastIndex]->removeFocus();
        frame.views[lastIndex]->OnHide();
        frame.views[frame.index]->setVisible(true);
        frame.views[frame.index]->OnDisplay();
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

    auto& frame = currentFrame();
    if (frame.index < frame.views.size()) {
        if (pressed && !isTouchPressed_) {
            if (y <= (uint16_t)topBarHeight) {
                if (x < (uint16_t)leftButton.width()) {
                    if (stackDepth_ > 0) navigateBack();
                    else ChangeView(-1);
                } else if (x > (uint16_t)(tft_.width() - rightButton.width())) {
                    ChangeView(1);
                }
            } else {
                frame.views[frame.index]->HandleTouch(lastTouchX, lastTouchY);
            }
            isTouchPressed_ = true;
        } else if (isTouchPressed_) {
            if (pressed) {
                frame.views[frame.index]->HandleTouchDrag(lastTouchX, lastTouchY);
            }
        }
        if (!pressed && isTouchPressed_) {
            frame.views[frame.index]->HandleTouchRelease(lastTouchX, lastTouchY);
            isTouchPressed_ = false;
        }
    }

}
