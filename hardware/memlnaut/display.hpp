#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <TFT_eSPI.h>
#include <deque>

#define DISPLAY_MEM __not_in_flash("display")


class display {
public:
    void setup();

    void post(String str);

    void update();
private:
    std::deque<String> lines;
    bool redraw=false;
};

#endif