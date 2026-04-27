#ifndef __DISPLAY_DRIVER_HPP__
#define __DISPLAY_DRIVER_HPP__

#include "View.hpp"
#include <vector>
#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstddef>


#include <TFT_eSPI.h>
#include <TFT_eWidget.h>


class DisplayDriver {
public:
    DisplayDriver() :        
        leftButton(&tft_),
        title(&tft_),
        rightButton(&tft_)
    {
    }

    void Setup();
    void Draw();
    inline void AddView(const std::shared_ptr<ViewBase> &view)
    {
        frames_[0].views.push_back(view);
        view->SetGrid(grid_);
        if (tft_initialized_) {
            view->Setup(&tft_, mainArea);
        }
        redraw_internal_ = true;
    }
    inline void InsertViewAfter(const std::shared_ptr<ViewBase> &existingView, const std::shared_ptr<ViewBase> &newView)
    {
        auto& topViews = frames_[0].views;
        auto it = std::find(topViews.begin(), topViews.end(), existingView);
        if (it != topViews.end()) {
            topViews.insert(it + 1, newView);
            newView->SetGrid(grid_);
            if (tft_initialized_) {
                newView->Setup(&tft_, mainArea);
            }
            redraw_internal_ = true;
        }
    }
    void PollTouch();
    unsigned long GetLastTouchTime() const { return lastTouchTime_; }
    unsigned long GetLastDrawTime() const { return lastDrawTime_; }

    void ChangeView(int delta);
    void NavigateToView(const std::shared_ptr<ViewBase>& target);
    void navigateInto(const std::vector<std::shared_ptr<ViewBase>>& children,
                      const String& sectionName, size_t startIndex = 0);
    void navigateBack();

    void RotaryIncEvent(int delta) {
        auto& frame = currentFrame();
        if (frame.index < frame.views.size()) {
            auto& currentView = frame.views[frame.index];
            if (currentView->isFocused()) {
                currentView->HandleRotaryEncChange(delta);
            } else {
                ChangeView(delta);
            }
        }
    }
    void RotarySwitchEvent() {
        auto& frame = currentFrame();
        if (frame.index < frame.views.size()) {
            auto& currentView = frame.views[frame.index];
            if (currentView->isFocused()) {
                currentView->HandleRotaryEncSwitch();
            } else {
                currentView->setFocus();
            }
        }
    }

private:
    struct NavFrame {
        std::vector<std::shared_ptr<ViewBase>> views;
        size_t index = 0;
        String sectionName;
    };

    NavFrame& currentFrame() { return frames_[stackDepth_]; }

    // Internal TFT hardware instance
    TFT_eSPI tft_;

    // Navigation stack (depth 0 = top level, depth 1 = inside section)
    NavFrame frames_[2];
    int stackDepth_ = 0;

    // Calibration
    uint16_t calData_[5] = { 421, 3470, 270, 3492, 7 };

    // Grid
    static constexpr size_t kGridWidthElements = 16;
    static constexpr size_t kGridHeightElements = 12;
    int screenWidth_;
    int screenHeight_;
    GridDef grid_;

    bool redraw_internal_{false};
    unsigned long lastTouchTime_{0};
    unsigned long lastDrawTime_{0};
    bool tft_initialized_{false};
    bool isTouchPressed_{false};

    //top bar
    TFT_eSprite leftButton, title, rightButton;
    rect mainArea;

    int lastTouchX, lastTouchY;

    const int topBarHeight = 30;

};


#endif // __DISPLAY_DRIVER_HPP__
