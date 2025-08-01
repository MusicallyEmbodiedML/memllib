#ifndef __BUTTON_VIEW_HPP__
#define __BUTTON_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class ButtonView : public ViewBase {
public:
    ButtonView(const char* name)
        : ViewBase(name)
    {
    }

    void OnSetup() override {
    }  

    void OnDraw() override {
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_RED);
        scr->setTextColor(TFT_WHITE);
        scr->drawString("B", area.x + 10, area.y + 10);
    }  


    void HandleTouch(size_t x, size_t y) override {

    }
    void HandleRelease() override 
    {

    }


private:
};

#endif 
