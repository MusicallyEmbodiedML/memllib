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
        rect barArea = getBarGraphArea();
        AddSubView(barGraph, barArea);
        barGraph->setSpectrumColors(TFT_GREEN, TFT_BLUE);
    }

    void OnDraw() override {
        int barY = area.y + area.h - kStatusBarHeight;
        scr->fillRect(area.x, barY, area.w, kStatusBarHeight, TFT_BLACK);
        scr->setTextFont(1);
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->drawString(("L:" + String(loss_, 4)).c_str(), area.x + 2, barY + 4);
        scr->drawString(("y:" + String(posCount_) + " n:" + String(negCount_) + " x" + String(multiplier_)).c_str(), area.x + 100, barY + 4);
        scr->drawString(lastAction_.c_str(), area.x + 200, barY + 4);
    }

    void UpdateValues(const std::vector<float>& values, bool resetMinMax = false) {
        barGraph->UpdateValues(values, resetMinMax);
    }

    // Safe to call from ISR — only updates state and sets redraw flag, no direct SPI
    void setLoss(float v) {
        if (v != loss_) { loss_ = v; redraw(); }
    }
    void setMemoryCounts(size_t pos, size_t neg) {
        if (pos != posCount_ || neg != negCount_) {
            posCount_ = pos;
            negCount_ = neg;
            redraw();
        }
    }
    void setDislikeMultiplier(size_t m) {
        if (m != multiplier_) { multiplier_ = m; redraw(); }
    }
    void setLastAction(const String& a) {
        if (a != lastAction_) { lastAction_ = a; redraw(); }
    }
    void setNoiseActive(bool active) {
        if (active != noiseActive_) {
            noiseActive_ = active;
            if (active) {
                barGraph->setSpectrumColors(TFT_RED, TFT_YELLOW);
            } else {
                barGraph->setSpectrumColors(TFT_GREEN, TFT_BLUE);
            }
            barGraph->redraw();
        }
    }

protected:
    virtual rect getBarGraphArea() const {
        return {area.x, area.y, area.w, area.h - kStatusBarHeight};
    }

private:
    std::shared_ptr<BarGraphView> barGraph;
    float loss_{0.f};
    size_t posCount_{0};
    size_t negCount_{0};
    size_t multiplier_{1};
    String lastAction_{""};
    bool noiseActive_{false};
};

#endif // __RL_VIEW_HPP__
