#ifndef __BLOCK_SELECT_VIEW_HPP__
#define __BLOCK_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "ButtonView.hpp"


class BlockSelectView : public ViewBase {
public:
    using OnSelectCallback = std::function<void(size_t)>;

    BlockSelectView(String name, int _buttonColour_ = TFT_BLUE)
        : ViewBase(name), buttonColour(_buttonColour_)
    {}

    void SetOnSelectCallback(OnSelectCallback _cb_) {
        cb = _cb_;
    }

    void OnSetup() override {
        int idx=1;
        for(int i=0; i < 4; i++) {
            for(int j=0; j < 2; j++) {

                auto button = std::make_shared<ButtonView>(String(idx), idx, buttonColour);
                rect bounds = { area.x + 10 + (i * 60), area.y + 10 + (j*60), 50, 50 }; // Example bounds
                AddSubView(button, bounds);
                button->SetReleaseCallback([this](size_t id) { 
                    Serial.print("Button pressed: ");
                    Serial.println(id);
                    cb(id);
                });
                buttons.push_back(button);
                idx++;
            }
        }
    }  

    void OnDraw() override {
        TFT_eSprite textSprite(scr);
        textSprite.createSprite(320, 20);
        textSprite.setTextFont(2);
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);
        constexpr int32_t lineheight = 20;
        textSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        textSprite.fillRect(0,0,320,20,TFT_BLACK);
        textSprite.drawString(msg, 3, 0);
        textSprite.pushSprite(area.x,area.y + area.h - 25);
    }  

    void SetMessage(const String &__msg) {
        msg = __msg;
        redraw();
    }



private:
    std::vector<std::shared_ptr<ButtonView>> buttons;
    int buttonColour;
    OnSelectCallback cb = nullptr;
    String msg;

};

#endif 
