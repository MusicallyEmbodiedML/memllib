#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <string>
#include <TFT_eSPI.h>

class ViewBase {

public:
    ViewBase() = delete; // Prevent instantiation of base class
    virtual void Setup() = 0; // Pure virtual function for setup
    virtual void Draw(TFT_eSPI *tft) = 0; // Pure virtual function for drawing
    virtual bool NeedRedraw() const = 0; // Check if redraw is needed
    virtual const std::string &GetName() const = 0; // Get view name
    virtual void HandleTouch(size_t x, size_t y) = 0; // Handle touch events
};


#endif // __VIEW_HPP__
