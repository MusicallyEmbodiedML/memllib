#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <string>
#include <TFT_eSPI.h>
#include "UIElements.hpp"


class ViewBase {

public:
    ViewBase() = delete; // Prevent instantiation of base class
    virtual ~ViewBase() = default; // Virtual destructor
    virtual void Setup() = 0; // Pure virtual function for setup
    virtual void Draw(TFT_eSPI *tft) = 0; // Pure virtual function for drawing
    bool NeedRedraw()
    {
        bool ret = needRedraw_;
        needRedraw_ = false;
        return ret;
    }
    inline const std::string &GetName() const { return name_; } // Get view name
    virtual void HandleTouch(size_t x, size_t y) = 0; // Handle touch events
    inline void SetGrid(const GridDef &grid)
    {
        grid_ = grid;
    }

protected:
    explicit ViewBase(const std::string& name)
            : name_(name),
            grid_({0, 0, 0, 0}), // Default grid definition
            needRedraw_(true) {}
    const std::string name_;
    GridDef grid_; // Grid definition for layout
    bool needRedraw_;
};


#endif // __VIEW_HPP__
