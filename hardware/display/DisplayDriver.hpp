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
    void Setup();
    void Draw();
    inline void AddView(std::shared_ptr<ViewBase> &view)
    {
        views_.push_back(view);
        view->Setup();
    }
    void PollTouch();

private:
    // Internal TFT hardware instance
    TFT_eSPI tft_;

    // Views
    std::vector<std::shared_ptr<ViewBase>> views_;
    size_t currentViewIndex_;

    // Calibration
    uint16_t calData_[5] = { 421, 3470, 270, 3492, 7 };

    // Grid
    static constexpr size_t kGridWidthElements = 4;
    static constexpr size_t kGridHeightElements = 4;
    size_t screenWidth_;
    size_t screenHeight_;
    size_t gridWidthStep_;
    size_t gridHeightStep_;

    bool redraw_internal_{false};
};


#endif // __DISPLAY_DRIVER_HPP__
