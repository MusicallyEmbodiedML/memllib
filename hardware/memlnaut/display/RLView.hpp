#ifndef __RL_VIEW_HPP__
#define __RL_VIEW_HPP__

#include "View.hpp"
#include "BarGraphView.hpp"

class RLView : public ViewBase {
public:
    static constexpr int kStatusBarHeight = 20;

    RLView(String name, size_t nOutputs, int barwidth = 2, int colour = TFT_GREEN,
           float rangeLow = 0.f, float rangeHigh = 1.f)
        : ViewBase(name)
        , barGraph(std::make_shared<BarGraphView>(name, nOutputs, barwidth, colour, rangeLow, rangeHigh))
    {}

    void OnSetup() override {
        rect barArea = {area.x, area.y, area.w, area.h - kStatusBarHeight};
        AddSubView(barGraph, barArea);
    }

    void OnDraw() override {
        int barY = area.y + area.h - kStatusBarHeight;
        scr->fillRect(area.x, barY, area.w, kStatusBarHeight, TFT_BLACK);
        scr->setTextFont(1);
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->drawString(("L:" + String(loss_, 4)).c_str(), area.x + 2, barY + 4);
        scr->drawString(("M:" + String(memSize_)).c_str(), area.x + 100, barY + 4);
        scr->drawString(lastAction_.c_str(), area.x + 200, barY + 4);
    }

    void UpdateValues(const std::vector<float>& values, bool resetMinMax = false) {
        barGraph->UpdateValues(values, resetMinMax);
    }

    void setLoss(float v) {
        if (v != loss_) { loss_ = v; drawStatusItem(0, 98, "L:" + String(v, 4)); }
    }
    void setMemorySize(size_t n) {
        if (n != memSize_) { memSize_ = n; drawStatusItem(100, 98, "M:" + String(n)); }
    }
    void setLastAction(const String& a) {
        if (a != lastAction_) { lastAction_ = a; drawStatusItem(200, area.w - 200, a); }
    }

private:
    void drawStatusItem(int xOffset, int w, const String& text) {
        if (!scr) return;
        int barY = area.y + area.h - kStatusBarHeight;
        scr->fillRect(area.x + xOffset, barY, w, kStatusBarHeight, TFT_BLACK);
        scr->setTextFont(1);
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->drawString(text.c_str(), area.x + xOffset + 2, barY + 4);
    }

    std::shared_ptr<BarGraphView> barGraph;
    float loss_{0.f};
    size_t memSize_{0};
    String lastAction_{""};
};

#endif // __RL_VIEW_HPP__
