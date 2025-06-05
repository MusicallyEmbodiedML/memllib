#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <TFT_eSPI.h>
#include "UIElements.hpp"


class ViewBase {

public:
    ViewBase() = delete; // Prevent instantiation of base class
    virtual ~ViewBase() = default; // Virtual destructor
    void Setup(TFT_eSPI* tft) {  // No longer virtual
        tft_ = tft;
        needRedraw_ = true;
        OnSetup();  // Call virtual setup hook
    }
    virtual void OnSetup() = 0;  // New virtual setup hook
    virtual void Draw() = 0;  // No longer needs TFT pointer
    bool NeedRedraw()
    {
        bool ret = needRedraw_;
        needRedraw_ = false;
        return ret;
    }
    inline const char* GetName() const { return name_; } // Changed return type
    virtual void HandleTouch(size_t x, size_t y) = 0; // Handle touch events
    virtual void HandleRelease() = 0; // Handle release events
    inline void SetGrid(const GridDef &grid)
    {
        grid_ = grid;
    }

protected:
    explicit ViewBase(const char* name)  // Changed parameter type
            : name_(name),
            grid_({0, 0, 0, 0}), // Default grid definition
            needRedraw_(true),
            tft_(nullptr) {}  // Initialize TFT pointer
    const char* name_;  // Changed member type
    GridDef grid_; // Grid definition for layout
    bool needRedraw_;
    TFT_eSPI* tft_;  // Store TFT pointer
};


#endif // __VIEW_HPP__
