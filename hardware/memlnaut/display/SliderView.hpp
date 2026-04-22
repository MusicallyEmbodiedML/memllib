#ifndef __SLIDER_VIEW_HPP__
#define __SLIDER_VIEW_HPP__

#include "View.hpp"
#include <functional>

class SliderView : public ViewBase {
public:
    using ValueChangedCallback = std::function<void(int)>;

    SliderView(String name) : ViewBase(name) {}

    void setRange(int minVal, int maxVal, int step) {
        minVal_ = minVal;
        maxVal_ = maxVal;
        step_ = step;
        currentVal_ = constrain(currentVal_, minVal_, maxVal_);
    }

    void setLabel(const String& label) { label_ = label; }

    void setValue(int v) {
        v = constrain(v, minVal_, maxVal_);
        if (v != currentVal_) {
            currentVal_ = v;
            if (onValueChanged_) onValueChanged_(currentVal_);
            redraw();
        }
    }

    int getValue() const { return currentVal_; }
    int getStep() const { return step_; }

    void setActive(bool active) {
        if (active != active_) {
            active_ = active;
            redraw();
        }
    }

    void setValueChangedCallback(ValueChangedCallback cb) {
        onValueChanged_ = std::move(cb);
    }

    void OnSetup() override {}

    void OnDraw() override {
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);

        uint16_t borderColour = active_ ? TFT_YELLOW : TFT_DARKGREY;
        scr->drawRect(area.x, area.y, area.w, area.h, borderColour);

        // Label
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->setTextDatum(TC_DATUM);
        scr->setTextFont(2);
        scr->drawString(label_.c_str(), area.x + area.w / 2, area.y + 4);

        // Track
        constexpr int trackPad = 12;
        constexpr int trackH = 8;
        int trackX = area.x + trackPad;
        int trackW = area.w - 2 * trackPad;
        int trackY = area.y + area.h / 2 - trackH / 2;

        scr->fillRect(trackX, trackY, trackW, trackH, TFT_DARKGREY);

        float t = (float)(currentVal_ - minVal_) / (float)(maxVal_ - minVal_);
        int filledW = (int)(t * trackW);
        if (filledW > 0) {
            scr->fillRect(trackX, trackY, filledW, trackH, active_ ? TFT_CYAN : TFT_BLUE);
        }

        // Thumb
        constexpr int thumbW = 8;
        constexpr int thumbH = 20;
        int thumbX = constrain(trackX + filledW - thumbW / 2, trackX, trackX + trackW - thumbW);
        int thumbY = area.y + area.h / 2 - thumbH / 2;
        scr->fillRect(thumbX, thumbY, thumbW, thumbH, active_ ? TFT_WHITE : TFT_SILVER);

        // Value
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", currentVal_);
        scr->setTextColor(active_ ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
        scr->setTextDatum(BC_DATUM);
        scr->setTextFont(4);
        scr->drawString(buf, area.x + area.w / 2, area.y + area.h - 2);

        scr->setTextDatum(TL_DATUM);
        scr->setTextFont(1);
    }

    void OnTouchDown(size_t x, size_t y) override { applyTouch(x); }
    void OnTouchDrag(size_t x, size_t y) override { applyTouch(x); }

    bool acceptsFocus() override { return false; }

private:
    void applyTouch(size_t x) {
        constexpr int trackPad = 12;
        int trackX = area.x + trackPad;
        int trackW = area.w - 2 * trackPad;
        float t = constrain((float)((int)x - trackX) / (float)trackW, 0.f, 1.f);
        setValue(minVal_ + (int)roundf(t * (float)(maxVal_ - minVal_)));
    }

    int minVal_ = 30;
    int maxVal_ = 200;
    int step_ = 1;
    int currentVal_ = 107;
    bool active_ = false;
    String label_;
    ValueChangedCallback onValueChanged_;
};

#endif
