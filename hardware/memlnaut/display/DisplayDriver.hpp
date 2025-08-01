#ifndef __DISPLAY_DRIVER_HPP__
#define __DISPLAY_DRIVER_HPP__

#include "View.hpp"
#include <vector>
#include <memory>
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
        views_.push_back(view);
        view->SetGrid(grid_);
        Serial.println("Added view");
        if (tft_initialized_) {
            view->Setup(&tft_, mainArea);
            Serial.println("View setup");
        }
    }
    void PollTouch();
    unsigned long GetLastTouchTime() const { return lastTouchTime_; }
    unsigned long GetLastDrawTime() const { return lastDrawTime_; }

private:
    // Internal TFT hardware instance
    TFT_eSPI tft_;
    

    // Views
    std::vector<std::shared_ptr<ViewBase>> views_;
    size_t currentViewIndex_;

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

};


#endif // __DISPLAY_DRIVER_HPP__
