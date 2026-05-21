#ifndef __CC_SELECT_VIEW_HPP__
#define __CC_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include <vector>
#include <algorithm>
#include <functional>

class CCSelectView : public ViewBase {
public:
    using OnChangeCallback = std::function<void(const std::vector<uint8_t>&)>;

    CCSelectView(size_t maxActive, String name = "MIDI CC Out")
        : ViewBase(name), maxActive_(maxActive) {}

    void setOnChangeCallback(OnChangeCallback cb) { cb_ = cb; }

    size_t getMaxActive() const { return maxActive_; }
    const std::vector<uint8_t>& getSelectedCCs() const { return selectedCCs_; }

    void setSelectedCCs(const std::vector<uint8_t>& ccs) {
        selectedCCs_ = ccs;
        std::sort(selectedCCs_.begin(), selectedCCs_.end());
    }

    void OnSetup() override {}

    void OnDisplay() override {}

    void OnDraw() override {
        for (int col = 0; col < 8; col++) {
            for (int row = 0; row < 4; row++) {
                uint8_t ccNum = static_cast<uint8_t>(col * 4 + row + 1);
                bool sel = isSelected(ccNum);
                int x = area.x + 10 + col * 40;
                int y = area.y + 10 + row * 45;
                scr->fillRect(x, y, 30, 35, sel ? TFT_GREEN : TFT_DARKGREY);
                scr->drawRect(x, y, 30, 35, TFT_WHITE);
                scr->setTextColor(sel ? TFT_BLACK : TFT_WHITE, sel ? TFT_GREEN : TFT_DARKGREY);
                scr->setTextFont(2);
                scr->drawString(String(ccNum), x + 5, y + 10);
            }
        }
        scr->fillRect(area.x, area.y + area.h - 20, area.w, 20, TFT_BLACK);
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->setTextFont(2);
        String status = String(selectedCCs_.size()) + "/" + String(maxActive_) + " active";
        if (selectedCCs_.size() >= maxActive_) status += " (max)";
        scr->drawString(status, area.x + 5, area.y + area.h - 18);
    }

    void OnTouchUp(size_t x, size_t y) override {
        for (int col = 0; col < 8; col++) {
            for (int row = 0; row < 4; row++) {
                int bx = area.x + 10 + col * 40;
                int by = area.y + 10 + row * 45;
                if ((int)x >= bx && (int)x < bx + 30 && (int)y >= by && (int)y < by + 35) {
                    toggle(static_cast<uint8_t>(col * 4 + row + 1));
                    return;
                }
            }
        }
    }

private:
    size_t maxActive_;
    std::vector<uint8_t> selectedCCs_;
    OnChangeCallback cb_;

    bool isSelected(uint8_t cc) const {
        return std::find(selectedCCs_.begin(), selectedCCs_.end(), cc) != selectedCCs_.end();
    }

    void toggle(uint8_t cc) {
        auto it = std::find(selectedCCs_.begin(), selectedCCs_.end(), cc);
        if (it != selectedCCs_.end()) {
            selectedCCs_.erase(it);
        } else {
            if (selectedCCs_.size() >= maxActive_) {
                redraw();
                return;
            }
            selectedCCs_.push_back(cc);
            std::sort(selectedCCs_.begin(), selectedCCs_.end());
        }
        redraw();
        if (cb_) cb_(selectedCCs_);
    }
};

#endif
