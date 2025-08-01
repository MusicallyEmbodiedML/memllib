#ifndef __MESSAGE_VIEW_HPP__
#define __MESSAGE_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class MessageView : public ViewBase {
public:
    MessageView(const char* name)
        : ViewBase(name)
    {}

    void OnSetup() override {
    }  
    
    void Draw() override {
        TFT_eSprite textSprite(scr);
        textSprite.createSprite(320, 20);
        textSprite.setFreeFont(&FreeMono9pt7b);
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);
        constexpr int32_t lineheight = 20;
        textSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        for(size_t i=0; i < lines.size(); i++) {
            textSprite.fillRect(0,0,320,20,TFT_BLACK);
            textSprite.drawString(lines[i].c_str(), 0, 0);
            textSprite.pushSprite(area.x + 10,area.y + (i*lineheight));
            Serial.print("MessageView line ");
            Serial.print(i);
            Serial.print(": ");            Serial.println(lines[i]);
            // scr->drawString(lines[i].c_str(), area.x + 10, area.y + (i * lineheight), 2);

        }

    }  

    void post(String str) {
        lines.push_back(str);
        if(lines.size() > 9) {
            lines.pop_front();
        }
        redraw();
    }


    void HandleTouch(size_t x, size_t y) override {

    }
    void HandleRelease() override 
    {

    }


private:
    std::deque<String> lines;
};

#endif 
