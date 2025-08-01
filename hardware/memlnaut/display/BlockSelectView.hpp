#ifndef __BLOCK_SELECT_VIEW_HPP__
#define __BLOCK_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "ButtonView.hpp"


class BlockSelectView : public ViewBase {
public:
    BlockSelectView(String name, int _buttonColour_ = TFT_BLUE)
        : ViewBase(name), buttonColour(_buttonColour_)
    {}

    void OnSetup() override {
        int idx=1;
        for(int i=0; i < 4; i++) {
            for(int j=0; j < 2; j++) {

                auto button = std::make_shared<ButtonView>(String(idx), idx, buttonColour);
                rect bounds = { area.x + 10 + (i * 60), area.y + 10 + (j*60), 50, 50 }; // Example bounds
                AddSubView(button, bounds);
                buttons.push_back(button);
                idx++;
            }
        }
    }  

    void OnDraw() override {
    }  


    void HandleTouch(size_t x, size_t y) override {

    }
    void HandleRelease() override 
    {

    }


private:
    std::vector<std::shared_ptr<ButtonView>> buttons;
    int buttonColour;

};

#endif 
