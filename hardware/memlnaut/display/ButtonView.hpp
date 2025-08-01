#ifndef __BUTTON_VIEW_HPP__
#define __BUTTON_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class ButtonView : public ViewBase {
public:

    using ButtonCallback = std::function<void(size_t)>;

    ButtonView(String name, size_t _id_, int _fillcolour_ = TFT_BLUE)
        : ViewBase(name), fillColour(_fillcolour_),
        id(_id_)
    {
        Serial.print("Creating ButtonView: ");
        Serial.println(name);
    }

    void SetReleaseCallback(ButtonCallback __callback) {
        callback = __callback;
    }

    void OnSetup() override {
    }  

    void OnDraw() override {
        scr->fillRect(area.x, area.y, area.w, area.h, fillColour);
        if (pressed) {
            scr->drawRect(area.x, area.y, area.w, area.h, TFT_RED);
        } else {
            scr->drawRect(area.x, area.y, area.w, area.h, TFT_WHITE);
        }
        scr->setTextColor(TFT_WHITE);
        scr->setTextFont(4);
        Serial.print("Drawing ButtonView: ");
        Serial.println(name_);
        scr->drawString(this->name_, area.x + 10, area.y + 10);
        // scr->drawString("1", area.x + 10, area.y + 10);
    }  


    void OnTouchDown(size_t x, size_t y) override {
        pressed = true;
        redraw();
        Serial.print("ButtonView OnTouchDown at: ");
        Serial.print(x);
        Serial.print(", ");     
        Serial.println(y);
        // Check if the touch is within the button area
    }

    void OnTouchUp(size_t x, size_t y) override {
        Serial.print("ButtonView OnTouchUp at: ");
        Serial.print(x);
        Serial.print(", ");     
        Serial.println(y);
        if (callback) {
            callback(id);
        }
        pressed = false;
        redraw();
    }


private:
    int fillColour;
    size_t id;

    ButtonCallback callback = nullptr;

    bool pressed = false;
};

#endif 
