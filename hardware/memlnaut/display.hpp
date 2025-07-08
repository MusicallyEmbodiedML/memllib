#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <TFT_eSPI.h>
#include <deque>

#define DISPLAY_MEM __not_in_flash("display")

extern TFT_eSPI tft;  // Invoke custom library


class display {
public:
    display() : textSprite(&tft) {

    }

    void setup() {
        tft.begin();
        tft.setRotation(1);
        tft.fillScreen(TFT_BLACK);
        textSprite.setFreeFont(&FreeMono9pt7b);
        // tft.setTextFont(4);
        // tft.setTextColor(0xFC9F);
        textSprite.createSprite(320, 20);

    }

    void post(String str) {
        redraw = true;
        lines.push_back(str);
        if(lines.size() > 11) {
            lines.pop_front();
        }
    }

    void update() {
        if (redraw) {
            redraw = false;
            tft.fillScreen(TFT_BLACK);
            constexpr int32_t lineheight = 20;
            // tft.fillRect(10, 10, 250, 30, TFT_BLUE);
            // tft.fillRect(100, 100, 200, 100, TFT_RED);
            for(size_t i=0; i < lines.size(); i++) {
                textSprite.fillRect(0,0,320,20,TFT_BLACK);
                textSprite.drawString(lines[i].c_str(), 0, 0);
                textSprite.pushSprite(10,i*lineheight);
            }
        }
    }
private:
    std::deque<String> lines;
    bool redraw=false;
    TFT_eSprite textSprite;
};

#endif
