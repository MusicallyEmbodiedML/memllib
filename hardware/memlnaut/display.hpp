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
        if(lines.size() > 10) {
            lines.pop_front();
        }
    }

    void statusPost(String str, size_t pos) {
        if (pos >= kNStatuses) {
            return; // Invalid position
        }
        status_text[pos] = str;
        status_redraw = true;
    }

    void update() {
        if (redraw) {
            redraw = false;
            status_redraw = true;
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

        if (status_redraw) {
            status_redraw = false;
            tft.fillRect(0, 220, 320, 20, TFT_BLACK);
            textSprite.fillRect(0,0,320,20,TFT_BLACK);
            textSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
            textSprite.drawString(status_text[0].c_str(), 0, 0);
            textSprite.setTextColor(TFT_GREEN, TFT_BLACK);
            textSprite.drawString(status_text[1].c_str(), 160, 0);
            textSprite.pushSprite(10,220);
            textSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        }
    }
private:
    std::deque<String> lines;
    bool redraw=false;
    bool status_redraw = false;
    static constexpr size_t kNStatuses = 2;
    String status_text[kNStatuses];
    TFT_eSprite textSprite;
};

#endif
