#ifndef __SYSTEM_VIEW_HPP__
#define __SYSTEM_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class SystemView : public ViewBase {
public:
    SystemView(String name)
        : ViewBase(name)
    {}

    void OnSetup() override {
    }  
    
    void OnDraw() override {
        TFT_eSprite textSprite(scr);
        textSprite.createSprite(320, 20);
        textSprite.setTextFont(2);
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);
        constexpr int32_t lineheight = 20;
        textSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        std::deque<String> lines;
        lines.push_back("MEMLNaut v1.0.0");
        lines.push_back("");
        lines.push_back("Info: https://musicallyembodiedml.github.io");
        lines.push_back("");
        lines.push_back("Build: " + String(__DATE__) + " " + String(__TIME__));
        uint32_t sys_clk = clock_get_hz(clk_sys);        
        lines.push_back("System Clock: " + String(sys_clk) + " Hz");
        lines.push_back("");
        lines.push_back("Made by Chris Kiefer and Andrea Martelloni");
        lines.push_back("Emute Lab, University of Sussex, UK");

        for(size_t i=0; i < lines.size(); i++) {
            textSprite.fillRect(0,0,320,20,TFT_BLACK);
            textSprite.drawString(lines[i].c_str(), 0, 0);
            textSprite.pushSprite(area.x + 10,area.y + (i*lineheight));
        }
    Serial.println("Firmware Build Info:");
    Serial.print("Build Date: ");
    Serial.println(__DATE__);
    Serial.print("Build Time: ");
    Serial.println(__TIME__);
    Serial.print("Combined: ");
    Serial.print(__DATE__);
    Serial.print(" ");
    Serial.println(__TIME__);        

    }  


    void HandleTouch(size_t x, size_t y) override {

    }
    void HandleRelease() override 
    {

    }


private:
};

#endif 
