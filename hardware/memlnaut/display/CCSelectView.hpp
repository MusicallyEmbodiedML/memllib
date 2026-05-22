#ifndef __CC_SELECT_VIEW_HPP__
#define __CC_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include <vector>
#include <algorithm>
#include <functional>

struct CCOption {
    uint8_t num;
    String  name;
};

// Scrollable named-CC selector.
// Shows a list of CCOption entries; allows selecting up to maxActive_.
// Selection order is preserved — index in selectedCCs_ == NN output index.
// Navigate with rotary encoder (scroll) or touch (tap row). Press encoder to toggle.
class CCSelectView : public ViewBase {
public:
    using OnChangeCallback = std::function<void(const std::vector<uint8_t>&)>;

    CCSelectView(size_t maxActive, String name = "MIDI CC Out")
        : ViewBase(name), maxActive_(maxActive) {}

    void setOptions(std::vector<CCOption> opts) {
        options_ = std::move(opts);
    }

    void setOnChangeCallback(OnChangeCallback cb) { cb_ = cb; }

    size_t getMaxActive() const { return maxActive_; }
    const std::vector<uint8_t>& getSelectedCCs() const { return selectedCCs_; }

    void setSelectedCCs(const std::vector<uint8_t>& ccs) {
        selectedCCs_.clear();
        for (uint8_t cc : ccs) {
            if (selectedCCs_.size() >= maxActive_) break;
            selectedCCs_.push_back(cc);
        }
        std::sort(selectedCCs_.begin(), selectedCCs_.end());
    }

    void OnSetup() override {
        // If no named options were provided, generate a generic numbered list (CC 1–127)
        if (options_.empty()) {
            for (uint8_t i = 1; i <= 127; i++)
                options_.push_back({i, "CC " + String(i)});
        }
    }

    void OnDraw() override {
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);

        // Header
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->setTextFont(2);
        String hdr = String(selectedCCs_.size()) + "/" + String(maxActive_) + " assigned";
        scr->drawString(hdr, area.x + 4, area.y + 4);

        // Rows (options + one trailing Done entry)
        size_t totalItems = options_.size() + 1;
        for (int i = 0; i < kVisibleRows; i++) {
            size_t optIdx = scrollOffset_ + (size_t)i;
            if (optIdx >= totalItems) break;

            int ry = area.y + kHeaderH + i * kRowH;
            bool isCursor = (optIdx == cursorRow_);
            bool isDone   = (optIdx == options_.size());

            if (isDone) {
                uint16_t bg = isCursor ? (uint16_t)TFT_DARKGREEN : (uint16_t)TFT_BLACK;
                scr->fillRect(area.x, ry, area.w, kRowH - 1, bg);
                scr->setTextFont(2);
                scr->setTextColor(TFT_WHITE, bg);
                scr->drawString("[ Done ]", area.x + 36, ry + 4);
                continue;
            }

            int slot = slotOf(options_[optIdx].num);
            bool isSel = slot > 0;

            uint16_t bg = (isSel && isCursor) ? (uint16_t)0x0660   // bright green: selected + cursor
                        : isSel               ? (uint16_t)0x0340   // dark green: selected
                        : isCursor            ? (uint16_t)TFT_NAVY // navy: cursor only
                                              : (uint16_t)TFT_BLACK;

            scr->fillRect(area.x, ry, area.w, kRowH - 1, bg);
            scr->setTextFont(2);

            if (isSel) {
                scr->setTextColor(TFT_YELLOW, bg);
                scr->drawString("[" + String(slot) + "]", area.x + 2, ry + 4);
            } else if (isCursor) {
                scr->setTextColor(TFT_YELLOW, bg);
                scr->drawString(" > ", area.x + 2, ry + 4);
            }

            scr->setTextColor(isSel ? TFT_WHITE : (isCursor ? TFT_YELLOW : TFT_SILVER), bg);
            scr->drawString(options_[optIdx].name, area.x + 36, ry + 4);

            scr->setTextColor(0x7BEF, bg);
            scr->drawString("CC" + String(options_[optIdx].num), area.x + area.w - 40, ry + 4);
        }

        // Scroll indicator
        if (totalItems > (size_t)kVisibleRows) {
            int barH = area.h - kHeaderH;
            int indicatorH = std::max(8, (int)(barH * kVisibleRows / (int)totalItems));
            int indicatorY = area.y + kHeaderH +
                             (int)(scrollOffset_ * (barH - indicatorH) / (totalItems - kVisibleRows));
            scr->fillRect(area.x + area.w - 4, area.y + kHeaderH, 4, barH, TFT_DARKGREY);
            scr->fillRect(area.x + area.w - 4, indicatorY, 4, indicatorH, TFT_WHITE);
        }
    }

    bool acceptsFocus() override { return true; }

    bool setFocus() override {
        redraw();
        return ViewBase::setFocus();
    }

    void removeFocus() override {
        ViewBase::removeFocus();
        redraw();
    }

    void HandleRotaryEncChange(int inc) override {
        size_t totalItems = options_.size() + 1;
        if (inc > 0 && cursorRow_ + 1 < totalItems) {
            cursorRow_++;
            if (cursorRow_ >= scrollOffset_ + (size_t)kVisibleRows)
                scrollOffset_ = cursorRow_ - kVisibleRows + 1;
        } else if (inc < 0 && cursorRow_ > 0) {
            cursorRow_--;
            if (cursorRow_ < scrollOffset_)
                scrollOffset_ = cursorRow_;
        }
        redraw();
    }

    void HandleRotaryEncSwitch() override {
        if (cursorRow_ == options_.size()) {
            removeFocus();
            redraw();
        } else {
            toggleAt(cursorRow_);
        }
    }

    void OnTouchUp(size_t tx, size_t ty) override {
        size_t totalItems = options_.size() + 1;
        for (int i = 0; i < kVisibleRows; i++) {
            int ry = area.y + kHeaderH + i * kRowH;
            if ((int)ty >= ry && (int)ty < ry + kRowH) {
                size_t optIdx = scrollOffset_ + (size_t)i;
                if (optIdx >= totalItems) return;
                cursorRow_ = optIdx;
                if (optIdx == options_.size()) {
                    removeFocus();
                    redraw();
                } else {
                    toggleAt(optIdx);
                }
                return;
            }
        }
    }

private:
    static constexpr int kRowH      = 28;
    static constexpr int kHeaderH   = 22;
    static constexpr int kVisibleRows = 6;

    size_t maxActive_;
    std::vector<CCOption> options_;
    std::vector<uint8_t> selectedCCs_;  // order = NN output index
    size_t scrollOffset_ = 0;
    size_t cursorRow_    = 0;
    OnChangeCallback cb_;

    // Returns 1-based slot number, 0 if not selected
    int slotOf(uint8_t cc) const {
        for (size_t i = 0; i < selectedCCs_.size(); i++)
            if (selectedCCs_[i] == cc) return (int)i + 1;
        return 0;
    }

    void toggleAt(size_t optIdx) {
        uint8_t ccNum = options_[optIdx].num;
        auto it = std::find(selectedCCs_.begin(), selectedCCs_.end(), ccNum);
        if (it != selectedCCs_.end()) {
            selectedCCs_.erase(it);
        } else {
            if (selectedCCs_.size() < maxActive_) {
                selectedCCs_.push_back(ccNum);
                std::sort(selectedCCs_.begin(), selectedCCs_.end());
            }
        }
        redraw();
        if (cb_) cb_(selectedCCs_);
    }
};

#endif  // __CC_SELECT_VIEW_HPP__
