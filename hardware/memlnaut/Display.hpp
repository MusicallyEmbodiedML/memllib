#ifndef __DISPLAY_H
#define __DISPLAY_H

#include <TFT_eSPI.h>
#include <deque>

#define DISPLAY_MEM __not_in_flash("display")

extern TFT_eSPI DISPLAY_MEM tft;

/**
 * @brief Display class for handling text output on TFT screen
 *
 * This class provides a simple text console-like interface for the TFT display.
 * It maintains a scrolling buffer of text lines and handles automatic redrawing
 * when content changes.
 */
class Display {
public:
    /**
     * @brief Initialize the display
     *
     * Sets up the TFT display with default parameters:
     * - Landscape orientation (rotation 1)
     * - Black background
     * - FreeSans9pt7b font
     * - Orange text color (0xFC9F)
     */
    void setup();

    /**
     * @brief Add a new line of text to the display
     *
     * @param str String to be displayed
     *
     * The string will be added to the bottom of the display.
     * If more than 11 lines are present, the oldest line will be removed.
     */
    void post(String str);

    /**
     * @brief Update the display if content has changed
     *
     * This method should be called regularly in the main loop.
     * It will only redraw the display if content has changed.
     */
    void update();

private:
    std::deque<String> lines;  ///< Buffer containing lines of text
    bool redraw{false};        ///< Flag indicating if display needs updating
};

#endif