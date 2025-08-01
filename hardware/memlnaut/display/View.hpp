#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <TFT_eSPI.h>
#include "UIElements.hpp"


class ViewBase {

public:
    ViewBase() = delete; // Prevent instantiation of base class
    virtual ~ViewBase() = default; // Virtual destructor
    void Setup(TFT_eSPI* tft, rect bounds);  // No longer virtual
    virtual void OnSetup() = 0;  // New virtual setup hook
    virtual void Draw() = 0;  // No longer needs TFT pointer
    bool NeedRedraw();
    inline const char* GetName() const { return name_; } // Changed return type
    virtual void HandleTouch(size_t x, size_t y) = 0; // Handle touch events
    virtual void HandleRelease() = 0; // Handle release events
    inline void SetGrid(const GridDef &grid) { grid_ = grid; }
    void setBounds(rect newBounds) {
        area = newBounds;
    }
    inline void redraw() {
        needRedraw_ = true;
    }

protected:
    explicit ViewBase(const char* name)  // Changed parameter type
            : name_(name)
            , grid_({0, 0, 0, 0}) // Default grid definition
            , needRedraw_(true)
            , scr(nullptr),
            area{0,0,1,1} {}  // Initialize TFT pointer
    const char* name_;      // 1st initialized
    GridDef grid_;         // 2nd initialized
    bool needRedraw_;      // 3rd initialized
    TFT_eSPI* scr;       // 4th initialized
    rect area;
};


#endif // __VIEW_HPP__
