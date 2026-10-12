#ifndef __BARGRAPH_VIEW_HPP__
#define __BARGRAPH_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


// Per-value graph that only pushes the pixels that changed, to keep SPI traffic (and so
// core-0 time) low:
//  - a bar whose quantised height hasn't moved since the last frame is skipped
//  - BARS style draws only the strip between the old and new height
//  - the backdrop (gridlines, group labels) is drawn once after a screen clear, and only
//    the gridline pixels a shrinking bar uncovers are restored
class BarGraphView : public ViewBase {
public:
    enum class Style : uint8_t {
        DOTS,  // a 4px marker at each value (original look)
        BARS,  // solid bars up from the baseline, one fixed colour per bar
    };

    BarGraphView(String name, size_t nOutputs, int barwidth = 2, int colour = TFT_GREEN, float rangeLow=0.f, float rangeHigh=1.f)
        : ViewBase(name), colour(colour), barwidth(barwidth), newValues(nOutputs, 0.0f),
          rangeLow(rangeLow), rangeHigh(rangeHigh), runningMax(nOutputs,0), runningMin(nOutputs,0)
    {

    }

    void OnSetup() override {
        rangeTotal = rangeHigh - rangeLow;
        rangeTotalInv = 1.f / rangeTotal;
        layout();
    }

    void OnDisplay() override {
    };

    void OnScreenCleared() override {
        fullRepaint_ = true;
    }

    // BARS: fill width is derived from the bar spacing (barwidth is used for DOTS only).
    void setStyle(Style s) {
        style_ = s;
        layout();
        clearArea_ = true;
        redraw();
    }

    // BARS: cap the bar width (0 = none). With only a few bars they are packed together
    // and centred rather than spread across the whole width.
    void setMaxBarWidth(int w) {
        maxBarW_ = w;
        layout();
        clearArea_ = true;
        redraw();
    }

    // Dark gridlines at 0, 0.25, 0.5, 0.75 and 1 of the range.
    void setGridEnabled(bool on) {
        grid_on_ = on;
        layout();
        clearArea_ = true;
        redraw();
    }

    // Split the bars into labelled groups: starts[g] is the first bar index of group g
    // (ascending, starts[0] normally 0). Groups are separated by a gap and labelled
    // underneath, in a row taken from the bottom of the view's area.
    void setGroups(const std::vector<size_t>& starts, const std::vector<String>& labels) {
        groupStarts_ = starts;
        groupLabels_ = labels;
        layout();
        clearArea_ = true;
        redraw();
    }

    // BARS: low->high gradient across the bars (by index, so each bar's colour is fixed).
    // DOTS: gradient by value.
    void setSpectrumColors(uint16_t low, uint16_t high) {
        if (useSpectrum_ && low == spectrumLow_ && high == spectrumHigh_) return;
        spectrumLow_ = low;
        spectrumHigh_ = high;
        useSpectrum_ = true;
        computeColours();
        recolour_ = true;  // BARS: repaint each bar in its new colour
        redraw();
    }

    void setNumDisplayBars(size_t n) {
        if (n == 0 || n == newValues.size()) return;
        newValues.assign(n, 0.f);
        runningMax.assign(n, 0.f);
        runningMin.assign(n, 0.f);
        layout();
        // Cleared in OnDraw, which only runs while this view is on screen: when hidden,
        // this area belongs to whichever view is showing (e.g. the Input Source selector
        // that triggered this), and the screen clear on switching views covers that case.
        clearArea_ = true;
        redraw();
    }

    void UpdateValues(const std::vector<float>& values, bool resetMinMax=false) {
        size_t n = std::min(values.size(), newValues.size());
        for(size_t i=0; i < n; i++) {
            newValues[i] = values[i];
        }
        if (resetMinMax) {
            runningMax = values;
            runningMin = values;
        }
        redraw();
    }


    void OnDraw() override {
        if (clearArea_) {
            scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);
            clearArea_ = false;
            fullRepaint_ = true;
        }
        if (fullRepaint_) {
            drawBackdrop();
            // Nothing of the old bars is left on screen.
            drawnH_.assign(newValues.size(), style_ == Style::BARS ? 0 : -1);
            fullRepaint_ = false;
            recolour_ = false;
        }
        // BARS: colours changed, so every bar is repainted in full this frame.
        const bool recolour = recolour_ && style_ == Style::BARS;
        recolour_ = false;

        const size_t n = std::min(newValues.size(), barX_.size());
        for (size_t i = 0; i < n; i++) {
            float norm = (newValues[i] - rangeLow) * rangeTotalInv;
            if (norm < 0.f) norm = 0.f;
            else if (norm > 1.f) norm = 1.f;
            const int16_t h = static_cast<int16_t>(norm * barH_);
            const int16_t old = drawnH_[i];
            if (h == old && !recolour) continue;
            const int x = barX_[i];

            if (recolour) {
                if (old > h) {
                    scr->fillRect(x, baseY_ - old, barW_, old - h, TFT_BLACK);
                    restoreGrid(x, barW_, baseY_ - old, baseY_ - h);
                }
                scr->fillRect(x, baseY_ - h, barW_, h, colours_[i]);
            } else if (style_ == Style::BARS) {
                if (h > old) {
                    scr->fillRect(x, baseY_ - h, barW_, h - old, colours_[i]);
                } else {
                    scr->fillRect(x, baseY_ - old, barW_, old - h, TFT_BLACK);
                    restoreGrid(x, barW_, baseY_ - old, baseY_ - h);
                }
            } else {
                if (old >= 0) {
                    scr->fillRect(x, baseY_ - old, barW_, kDotH, TFT_BLACK);
                    restoreGrid(x, barW_, baseY_ - old, baseY_ - old + kDotH);
                }
                const uint16_t c = useSpectrum_
                    ? lerpRGB565(spectrumLow_, spectrumHigh_, norm)
                    : static_cast<uint16_t>(colour);
                scr->fillRect(x, baseY_ - h, barW_, kDotH, c);
            }
            drawnH_[i] = h;
        }
    }



private:
    static constexpr int16_t kDotH = 4;
    static constexpr int kGroupGap = 6;       // px between groups
    static constexpr int kLabelRowH = 10;     // font 1 is 8px tall
    static constexpr int kNumGridLines = 5;
    static constexpr uint16_t kGridColour = 0x2104;      // very dark grey
    static constexpr uint16_t kBaselineColour = 0x4208;  // dark grey
    static constexpr uint16_t kLabelColour = TFT_DARKGREY;

    static uint16_t lerpRGB565(uint16_t c1, uint16_t c2, float t) {
        int r = ((c1 >> 11) & 0x1F) + static_cast<int>((static_cast<int>((c2 >> 11) & 0x1F) - static_cast<int>((c1 >> 11) & 0x1F)) * t);
        int g = ((c1 >>  5) & 0x3F) + static_cast<int>((static_cast<int>((c2 >>  5) & 0x3F) - static_cast<int>((c1 >>  5) & 0x3F)) * t);
        int b = ( c1        & 0x1F) + static_cast<int>((static_cast<int>( c2        & 0x1F) - static_cast<int>( c1        & 0x1F)) * t);
        return (static_cast<uint16_t>(r) << 11) | (static_cast<uint16_t>(g) << 5) | static_cast<uint16_t>(b);
    }

    size_t numGapsBefore(size_t i) const {
        size_t g = 0;
        for (size_t k = 1; k < groupStarts_.size(); k++)
            if (groupStarts_[k] > 0 && groupStarts_[k] <= i) g++;
        return g;
    }

    // Integer layout so every bar and gap is the same width (a float pitch makes the gaps
    // visibly uneven at ~3px per bar). The leftover is split either side to centre it.
    void layout() {
        const size_t n = newValues.size();
        if (n == 0 || area.w <= 1) return;
        const int offsetX = 5, offsetY = 5;
        const bool labels = !groupLabels_.empty();
        const int nGaps = static_cast<int>(numGapsBefore(n));
        const int usable = area.w - 2 * offsetX - nGaps * kGroupGap;
        int pitch = std::max(1, usable / static_cast<int>(n));
        barW_ = (style_ == Style::BARS) ? std::max(1, pitch - std::max(1, pitch / 3))
                                        : std::min(barwidth, pitch);
        if (style_ == Style::BARS && maxBarW_ > 0 && barW_ > maxBarW_) {
            barW_ = maxBarW_;
            pitch = barW_ + std::max(2, barW_ / 2);
        }
        const int used = pitch * static_cast<int>(n) + nGaps * kGroupGap - (pitch - barW_);
        const int x0 = area.x + std::max(offsetX, (area.w - used) / 2);
        barX_.resize(n);
        for (size_t i = 0; i < n; i++)
            barX_[i] = x0 + static_cast<int>(i) * pitch + static_cast<int>(numGapsBefore(i)) * kGroupGap;
        graphX0_ = x0;
        graphX1_ = barX_[n - 1] + barW_;

        baseY_ = area.y + area.h - offsetY - (labels ? kLabelRowH : 0);
        barH_ = baseY_ - (area.y + offsetY);
        if (style_ == Style::DOTS) baseY_ -= kDotH;  // keep h=0 dots inside the area
        for (int k = 0; k < kNumGridLines; k++)
            gridY_[k] = baseY_ - (barH_ * k) / (kNumGridLines - 1);
        computeColours();
        drawnH_.assign(n, style_ == Style::BARS ? 0 : -1);
    }

    void computeColours() {
        const size_t n = newValues.size();
        colours_.resize(n);
        for (size_t i = 0; i < n; i++) {
            colours_[i] = useSpectrum_
                ? lerpRGB565(spectrumLow_, spectrumHigh_, n > 1 ? static_cast<float>(i) / (n - 1) : 0.f)
                : static_cast<uint16_t>(colour);
        }
    }

    // One gridline segment under each group, so the gaps stay clean.
    void drawGridLine(int k) {
        const uint16_t c = (k == 0) ? kBaselineColour : kGridColour;
        if (groupStarts_.size() < 2) {
            scr->drawFastHLine(graphX0_, gridY_[k], graphX1_ - graphX0_, c);
            return;
        }
        for (size_t g = 0; g < groupStarts_.size(); g++) {
            size_t first, last;
            if (!groupRange(g, first, last)) continue;
            scr->drawFastHLine(barX_[first], gridY_[k], barX_[last] + barW_ - barX_[first], c);
        }
    }

    bool groupRange(size_t g, size_t& first, size_t& last) const {
        const size_t n = std::min(newValues.size(), barX_.size());
        first = groupStarts_[g];
        last = (g + 1 < groupStarts_.size() ? groupStarts_[g + 1] : n);
        if (first >= n || last <= first) return false;
        last = std::min(last, n) - 1;
        return true;
    }

    void drawBackdrop() {
        if (barX_.empty()) return;
        if (grid_on_)
            for (int k = 0; k < kNumGridLines; k++) drawGridLine(k);
        if (!groupLabels_.empty()) {
            scr->setTextFont(1);
            scr->setTextColor(kLabelColour, TFT_BLACK);
            scr->setTextDatum(TC_DATUM);
            const int y = baseY_ + (style_ == Style::DOTS ? kDotH : 0) + 2;
            for (size_t g = 0; g < groupStarts_.size() && g < groupLabels_.size(); g++) {
                size_t first, last;
                if (!groupRange(g, first, last)) continue;
                const int cx = (barX_[first] + barX_[last] + barW_) / 2;
                scr->drawString(groupLabels_[g].c_str(), cx, y);
            }
            scr->setTextDatum(TL_DATUM);
        }
    }

    // Re-draw the gridline pixels in rows [yTop, yBottom) of one bar's column.
    void restoreGrid(int x, int w, int yTop, int yBottom) {
        if (!grid_on_) return;
        for (int k = 0; k < kNumGridLines; k++) {
            if (gridY_[k] >= yTop && gridY_[k] < yBottom)
                scr->drawFastHLine(x, gridY_[k], w, k == 0 ? kBaselineColour : kGridColour);
        }
    }

    std::vector<float> newValues;
    int colour = TFT_GREEN;
    int barwidth = 2;
    bool useSpectrum_{false};
    uint16_t spectrumLow_{TFT_GREEN};
    uint16_t spectrumHigh_{TFT_BLUE};
    float rangeLow = 0.0f;
    float rangeHigh = 1.0f;
    float rangeTotal=1.f;
    float rangeTotalInv=1.f;

    Style style_{Style::DOTS};
    bool grid_on_{false};
    int maxBarW_{0};
    std::vector<size_t> groupStarts_;
    std::vector<String> groupLabels_;

    // Layout (from layout()) and what is currently on screen.
    std::vector<int16_t> barX_;
    std::vector<uint16_t> colours_;
    std::vector<int16_t> drawnH_;  // drawn height per bar; DOTS: -1 = no dot on screen
    int barW_{1};
    int baseY_{0};
    int barH_{1};
    int graphX0_{0}, graphX1_{0};
    int gridY_[kNumGridLines]{};

    bool clearArea_{false};    // wipe the area, then full repaint (layout changed)
    bool fullRepaint_{true};   // screen was cleared: backdrop + all bars
    bool recolour_{false};     // colours changed: repaint bars in place

    std::vector<float> runningMax, runningMin;

};

#endif
