#include "display.hpp"

// Define tft as static to this translation unit (display.cpp)
// The DISPLAY_MEM macro is applied here.
namespace {
    DISPLAY_MEM TFT_eSPI tft_instance = TFT_eSPI();
}

void display::setup() {
    tft_instance.begin();
    tft_instance.setRotation(1);
    tft_instance.fillScreen(TFT_BLACK);
    tft_instance.setFreeFont(&FreeSans9pt7b);
    tft_instance.setTextColor(0xFC9F);
}

void display::post(String str) {
    redraw = true;
    lines.push_back(str);
    if(lines.size() > 11) {
        lines.pop_front();
    }
}

void display::update() {
    if (redraw) {
        redraw = false;
        tft_instance.fillScreen(TFT_BLACK);
        constexpr int32_t lineheight = 20;
        for(size_t i=0; i < lines.size(); i++) {
            tft_instance.drawString(lines[i].c_str(), 10,i*lineheight);
        }
    }
}
