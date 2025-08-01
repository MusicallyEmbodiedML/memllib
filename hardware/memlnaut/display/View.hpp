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
    virtual void OnDraw() = 0;  
    virtual void OnTouchDown(size_t x, size_t y) = 0;  
    bool NeedRedraw();
    inline String GetName() const { return name_; } // Changed return type
    virtual void HandleRelease() = 0; // Handle release events
    inline void SetGrid(const GridDef &grid) { grid_ = grid; }
    void setBounds(rect newBounds) {
        area = newBounds;
    }
    
    inline void redraw() {
        needRedraw_ = true;
        for(auto& subview: subviews) {
            subview->redraw();
        }
    }

    void Draw() {
        OnDraw();
        for(auto& subview: subviews) {
            if (subview->NeedRedraw()) {
                subview->Draw();
            }
        }
    }

    void HandleTouch(size_t x, size_t y) {
        Serial.print("HandleTouch at: ");
        Serial.print(x);
        Serial.print(", ");     
        Serial.println(y);
        for(auto& subview: subviews) {
            if (subview->area.x <= x && x < subview->area.x + subview->area.w &&
                subview->area.y <= y && y < subview->area.y + subview->area.h) {
                Serial.println("Subview touched");
                subview->HandleTouch(x,y);
            }
        }
        OnTouchDown(x,y);
    }

    void AddSubView(const std::shared_ptr<ViewBase> &view, rect bounds)
    {
        subviews.push_back(view);
        Serial.println("Added sub view");
        if (scr) {
            view->Setup(scr, bounds);
            Serial.println("View setup");
        }
        redraw();
    }    
    String name_;      // 1st initialized

protected:
    explicit ViewBase(String &name)  // Changed parameter type
            : name_(name)
            , grid_({0, 0, 0, 0}) // Default grid definition
            , needRedraw_(true)
            , scr(nullptr),
            area{0,0,1,1} {}  // Initialize TFT pointer
    GridDef grid_;         // 2nd initialized
    bool needRedraw_;      // 3rd initialized
    TFT_eSPI* scr;       // 4th initialized
    rect area;
    std::vector<std::shared_ptr<ViewBase>> subviews;

};


#endif // __VIEW_HPP__
