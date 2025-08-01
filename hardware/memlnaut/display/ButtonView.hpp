#ifndef __BUTTON_VIEW_HPP__
#define __BUTTON_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class ButtonView : public ViewBase {
public:
    ButtonView(String name, size_t _id_, int _fillcolour_ = TFT_BLUE)
        : ViewBase(name), fillColour(_fillcolour_),
        id(_id_)
    {
        Serial.print("Creating ButtonView: ");
        Serial.println(name);
    }

    void OnSetup() override {
    }  

    void OnDraw() override {
        scr->fillRect(area.x, area.y, area.w, area.h, fillColour);
        scr->setTextColor(TFT_WHITE);
        scr->setTextFont(4);
        Serial.print("Drawing ButtonView: ");
        Serial.println(name_);
        scr->drawString(this->name_, area.x + 10, area.y + 10);
        // scr->drawString("1", area.x + 10, area.y + 10);
    }  


    void OnTouchDown(size_t x, size_t y) override {

    }
    void HandleRelease() override 
    {

    }


private:
    int fillColour;
    size_t id;
};

#endif 
