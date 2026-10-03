#ifndef __RL_VIEW_HPP__
#define __RL_VIEW_HPP__

#include "View.hpp"
#include "BarGraphView.hpp"
#include <array>
#include <cmath>

// RL screen: the network's outputs as bars, over a status line of
//   [loss trace] [yes/no memory split + counts] [last action, flashed on change]
// Each status field repaints only when its value changes; the loss trace adds one
// column per kLossColumnMs, oscilloscope style, so it never has to scroll.
class RLView : public ViewBase {
public:
    static constexpr int kStatusBarHeight = 20;

    RLView(String name, size_t nOutputs, int barwidth = 2, int colour = TFT_GREEN,
           float rangeLow = 0.f, float rangeHigh = 1.f)
        : ViewBase(name)
        , barGraph(std::make_shared<BarGraphView>(name, nOutputs, barwidth, colour, rangeLow, rangeHigh))
    {
        barGraph->setStyle(BarGraphView::Style::BARS);
        barGraph->setGridEnabled(true);
        lossLog_.fill(NAN);
    }

    void OnSetup() override {
        rect barArea = getBarGraphArea();
        AddSubView(barGraph, barArea);
        barGraph->setSpectrumColors(TFT_GREEN, TFT_BLUE);
    }

    // Label groups of outputs (e.g. per voice): starts[g] = first output index of group g.
    void setGroups(const std::vector<size_t>& starts, const std::vector<String>& labels) {
        barGraph->setGroups(starts, labels);
    }

    // View re-shown (driver cleared the screen): repaint every field.
    void OnDisplay() override {
        OnScreenCleared();
        needRedraw_ = true;
        for (auto& subview : subviews) subview->OnDisplay();
    }

    void OnScreenCleared() override {
        lossDirty_ = countsDirty_ = actionDirty_ = true;
        lossFullRepaint_ = true;
    }

    void OnDraw() override {
        const int barY = area.y + area.h - kStatusBarHeight;
        scr->setTextFont(1);
        scr->setTextDatum(TL_DATUM);

        // Loss: append one column per kLossColumnMs (mean of the losses reported since).
        const uint32_t now = millis();
        if (lossCount_ > 0 && now - lastLossColumnMs_ >= kLossColumnMs) {
            const float mean = lossSum_ / lossCount_;
            lossSum_ = 0.f;
            lossCount_ = 0;
            lastLossColumnMs_ = now;
            lossCursor_ = (lossCursor_ + 1) % kLossTraceW;
            lossLog_[lossCursor_] = log10f(mean);
            if (rescaleLoss()) lossFullRepaint_ = true;  // trace re-plotted on the new scale
            if (!lossFullRepaint_) {
                drawLossColumn(barY, lossCursor_);
                drawLossColumn(barY, lossHead());                       // blank write head
                drawLossColumn(barY, (lossHead() + 1) % kLossTraceW);   // drop its link to it
            }
        }
        if (lossFullRepaint_) {
            scr->setTextColor(TFT_DARKGREY, TFT_BLACK);
            scr->drawString("L", area.x + 2, barY + 6);
            scr->fillRect(area.x + kLossX, barY + 2, kLossTraceW, kLossTraceH, TFT_BLACK);
            for (size_t i = 0; i < kLossTraceW; i++) drawLossColumn(barY, i);
            lossFullRepaint_ = false;
        }
        lossDirty_ = false;

        if (countsDirty_) {
            // Proportion bar: green = liked memories, red = disliked.
            const int bx = area.x + kCountsX, by = barY + 7;
            const size_t total = posCount_ + negCount_;
            if (total == 0) {
                scr->fillRect(bx, by, kCountsBarW, 6, kEmptyColour);
            } else {
                const int gw = static_cast<int>((kCountsBarW * posCount_ + total / 2) / total);
                if (gw > 0) scr->fillRect(bx, by, gw, 6, TFT_GREEN);
                if (gw < kCountsBarW) scr->fillRect(bx + gw, by, kCountsBarW - gw, 6, TFT_RED);
            }
            // Padded text overwrites the old value in one pass: no clear, no flicker.
            scr->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            scr->setTextPadding(kCountsTextW);
            scr->drawString((String(posCount_) + "/" + String(negCount_)).c_str(),
                            bx + kCountsBarW + 4, barY + 6);
            scr->setTextPadding(0);
            countsDirty_ = false;
        }

        if (actionDirty_) {
            const int ax = area.x + kActionX, aw = area.w - kActionX - 2;
            const bool flash = flashing_ && now < flashUntilMs_;
            if (!flash) flashing_ = false;
            const uint16_t bg = flash ? actionColour(lastAction_) : TFT_BLACK;
            const uint16_t fg = flash ? TFT_BLACK : TFT_DARKGREY;
            scr->fillRoundRect(ax, barY + 2, aw, kStatusBarHeight - 4, 3, bg);
            scr->setTextColor(fg, bg);
            scr->setTextDatum(MC_DATUM);
            scr->drawString(lastAction_.c_str(), ax + aw / 2, barY + kStatusBarHeight / 2);
            scr->setTextDatum(TL_DATUM);
            actionDirty_ = false;
        }
    }

    void UpdateValues(const std::vector<float>& values, bool resetMinMax = false) {
        barGraph->UpdateValues(values, resetMinMax);
        // Called every inference step: a cheap place to end the action flash on time.
        if (flashing_ && millis() >= flashUntilMs_) { actionDirty_ = true; needRedraw_ = true; }
    }

    // Safe to call from ISR — only updates state and sets dirty/redraw flags, no direct SPI.
    // Each setter marks only its own field so OnDraw repaints just that part of the bar.
    // Zero means no training happened (no liked memories in the batch): not plotted.
    void setLoss(float v) {
        if (!(v > 0.f) || !std::isfinite(v)) return;
        lossSum_ += v;
        lossCount_++;
        lossDirty_ = true;
        needRedraw_ = true;
    }
    void setMemoryCounts(size_t pos, size_t neg) {
        if (pos != posCount_ || neg != negCount_) {
            posCount_ = pos;
            negCount_ = neg;
            countsDirty_ = true;
            needRedraw_ = true;
        }
    }
    // Flashes the action field even when the same action repeats (e.g. several 'yes').
    void setLastAction(const String& a) {
        lastAction_ = a;
        flashing_ = true;
        flashUntilMs_ = millis() + kFlashMs;
        actionDirty_ = true;
        needRedraw_ = true;
    }
    void setNoiseActive(bool active) {
        if (active != noiseActive_) {
            noiseActive_ = active;
            if (active) {
                barGraph->setSpectrumColors(TFT_RED, TFT_YELLOW);
            } else {
                barGraph->setSpectrumColors(TFT_GREEN, TFT_BLUE);
            }
        }
    }

protected:
    virtual rect getBarGraphArea() const {
        return {area.x, area.y, area.w, area.h - kStatusBarHeight};
    }

private:
    // Status line layout (x offsets from area.x)
    static constexpr int kLossX = 10;
    static constexpr size_t kLossTraceW = 88;
    static constexpr int kLossTraceH = 16;
    static constexpr int kCountsX = 106;
    static constexpr int kCountsBarW = 50;
    static constexpr int kCountsTextW = 42;
    static constexpr int kActionX = 206;
    static constexpr uint16_t kEmptyColour = 0x4208;
    static constexpr uint16_t kLossColour = TFT_CYAN;

    static constexpr uint32_t kLossColumnMs = 150;  // 88 columns ~ 13s of history
    static constexpr uint32_t kFlashMs = 700;
    // Loss is plotted on a log scale fitted to the samples on screen, snapped to
    // quarter decades (so it only changes, forcing a re-plot, when the range really moves)
    // and at least half a decade tall (so a flat loss isn't blown up into noise).
    static constexpr float kLossScaleStep = 0.25f;
    static constexpr float kLossMinSpan = 0.5f;

    // Returns true if the scale changed.
    bool rescaleLoss() {
        float lo = INFINITY, hi = -INFINITY;
        for (float v : lossLog_) {
            if (std::isnan(v)) continue;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        if (lo > hi) return false;
        lo = floorf(lo / kLossScaleStep) * kLossScaleStep;
        hi = ceilf(hi / kLossScaleStep) * kLossScaleStep;
        if (hi - lo < kLossMinSpan) {
            const float mid = roundf((lo + hi) * 0.5f / kLossScaleStep) * kLossScaleStep;
            lo = mid - kLossMinSpan * 0.5f;
            hi = mid + kLossMinSpan * 0.5f;
        }
        if (lo == lossScaleLo_ && hi == lossScaleHi_) return false;
        lossScaleLo_ = lo;
        lossScaleHi_ = hi;
        return true;
    }

    int lossToY(float logLoss) const {
        float t = (logLoss - lossScaleLo_) / (lossScaleHi_ - lossScaleLo_);
        if (t < 0.f) t = 0.f;
        else if (t > 1.f) t = 1.f;
        return static_cast<int>((1.f - t) * (kLossTraceH - 1));  // row from the top
    }

    // Column i of the trace: a vertical segment joining its sample to the previous one, so
    // the trace reads as a line. The column after the cursor is kept blank as the write head.
    size_t lossHead() const { return (lossCursor_ + 1) % kLossTraceW; }

    void drawLossColumn(int barY, size_t i) {
        const int x = area.x + kLossX + static_cast<int>(i);
        const int top = barY + 2;
        scr->drawFastVLine(x, top, kLossTraceH, TFT_BLACK);
        if (i == lossHead() || std::isnan(lossLog_[i])) return;
        const int y = lossToY(lossLog_[i]);
        const size_t prev = (i + kLossTraceW - 1) % kLossTraceW;
        const int py = (prev == lossHead() || std::isnan(lossLog_[prev])) ? y : lossToY(lossLog_[prev]);
        const int y0 = std::min(y, py), y1 = std::max(y, py);
        scr->drawFastVLine(x, top + y0, y1 - y0 + 1, kLossColour);
    }

    static uint16_t actionColour(const String& a) {
        if (a == "yes" || a == "drop") return TFT_GREEN;
        if (a == "no") return TFT_RED;
        if (a == "forget") return TFT_ORANGE;
        return TFT_SKYBLUE;  // scramble, drag, ...
    }

    std::shared_ptr<BarGraphView> barGraph;
    size_t posCount_{0};
    size_t negCount_{0};
    String lastAction_{""};
    bool noiseActive_{false};
    bool lossDirty_{true};
    bool countsDirty_{true};
    bool actionDirty_{true};

    std::array<float, kLossTraceW> lossLog_;  // log10(loss) per column; NaN = none
    float lossScaleLo_{-4.f}, lossScaleHi_{0.f};
    size_t lossCursor_{0};
    float lossSum_{0.f};
    uint32_t lossCount_{0};
    uint32_t lastLossColumnMs_{0};
    bool lossFullRepaint_{true};

    bool flashing_{false};
    uint32_t flashUntilMs_{0};
};

#endif // __RL_VIEW_HPP__
