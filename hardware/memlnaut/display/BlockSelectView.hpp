#ifndef __BLOCK_SELECT_VIEW_HPP__
#define __BLOCK_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "ButtonView.hpp"


class BlockSelectView : public ViewBase {
public:
    BlockSelectView(const char* name)
        : ViewBase(name)
    {}

    void OnSetup() override {
        // Initialize button1 with a specific position and size
        button1 = std::make_shared<ButtonView>("Button 1");
        rect bounds = { area.x + 10, area.y + 10, 100, 50 }; // Example bounds
        AddSubView(button1, bounds);
    }  

    void OnDraw() override {
    }  


    void HandleTouch(size_t x, size_t y) override {

    }
    void HandleRelease() override 
    {

    }


private:
    std::shared_ptr<ButtonView> button1;

};

#endif 
