#ifndef __VOICESPACESELECT_VIEW_HPP__
#define __VOICESPACESELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "../common.hpp"
#include "RotarySelectView.hpp"


class VoiceSpaceSelectView : public ViewBase {
public:
    VoiceSpaceSelectView(String name)
        : ViewBase(name)
    {}

    void OnSetup() override {
        voiceSpaceSelector = std::make_shared<RotarySelectView>("VoiceSpace", TFT_WHITE);
        AddSubView(voiceSpaceSelector, { area.x + 20, area.y + 20, 280, 140 });

    }  

    void OnDisplay() override {
    };

    
    void OnDraw() override {
    }  

    bool acceptsFocus() override {
        return true;
    }

    bool setFocus() override {
        Serial.println("VoiceSpaceSelectView::setFocus called");
        voiceSpaceSelector->setFocus();
        redraw();
        return ViewBase::setFocus();
    }

    void removeFocus() override{
        hasFocus = false;   
    }

    void HandleRotaryEncChange(int inc) override {
        voiceSpaceSelector->HandleRotaryEncChange(inc);
    }

    virtual void HandleRotaryEncSwitch() override{
        if (isFocused()) {
            voiceSpaceSelector->removeFocus();
            removeFocus();
        }
    }




private:
    std::shared_ptr<RotarySelectView> voiceSpaceSelector;

};

#endif 
