#ifndef __ROTARY_SELECT_VIEW_HPP__
#define __ROTARY_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "fonts/Orbitron_16.h"
#include "fonts/Orbitron_12.h"


// A scroll-wheel ("drum") selector. The current option sits centred in a rounded pill
// (outlined; filled with the accent colour while the encoder is focused on it), with its
// neighbours above and below in a smaller, dimmer Orbitron, fading out further away. The
// position ("3 / 7") is top right, and an optional one-line description of the current
// option sits at the bottom. Fits its area: 5 rows when there is room, else 3.
// Each row is drawn into a small 4-bit palette sprite (exact colours, no flicker).
class RotarySelectView : public ViewBase {

public:
    using NewSelectionCallback = std::function<void(size_t)>;

    // _fillcolour_ is the accent colour (pill and focus).
    RotarySelectView(String name, int _fillcolour_ = TFT_CYAN)
        : ViewBase(name), accent_(static_cast<uint16_t>(_fillcolour_))
    {
    }

    void setNewSelectionCallback(NewSelectionCallback cb) {
        newSelCB = cb;
    }

    // Called whenever the view gives up focus: encoder switch pressed, or navigated away
    // from (focused or not). Use it to commit a selection, e.g. persist it to flash, once
    // the user has finished scrolling. Runs in the rotary/touch handler context.
    void setFocusLostCallback(std::function<void()> cb) {
        focusLostCB = cb;
    }

    void OnSetup() override {
        if (options.empty()) {
            for (int i = 1; i <= 8; i++) options.push_back("VoiceSpace " + String(i));
        }
    }

    void OnDraw() override {
        const Layout L = layout();
        const int half = L.rows / 2;
        int y = L.top;
        for (int r = 0; r < L.rows; r++) {
            const int h = L.rowH[r];
            drawRow(selectedIndex + r - half, r - half, y, h);
            y += h;
        }
        drawCount();
        if (L.descH > 0) drawDescription(area.y + area.h - L.descH, L.descH);
    }

    void OnTouchUp(size_t x, size_t y) override {
        // Tap a row above/below the selection to move there.
        const Layout L = layout();
        const int half = L.rows / 2;
        int top = L.top;
        for (int r = 0; r < L.rows; r++) {
            if (static_cast<int>(y) >= top && static_cast<int>(y) < top + L.rowH[r]) {
                const int delta = r - half;
                const int dir = delta > 0 ? 1 : -1;
                for (int s = 0; s < abs(delta); s++) HandleRotaryEncChange(dir);
                break;
            }
            top += L.rowH[r];
        }
    }

    void setOptions(std::span<String> newOptions) {
        options.assign(newOptions.begin(), newOptions.end());
        redraw();
    }

    // One short line per option (same order), shown under the list. Optional.
    void setDescriptions(std::span<String> descs) {
        descriptions.assign(descs.begin(), descs.end());
        redraw();
    }

    void setSelection(size_t idx) {
        if (idx < options.size()) selectedIndex = idx;
        redraw();
    }

    bool acceptsFocus() override {
        return true;
    }

    bool setFocus() override {
        redraw();
        return ViewBase::setFocus();
    }

    void removeFocus() override {
        ViewBase::removeFocus();
        redraw();
        if (focusLostCB) focusLostCB();
    }

    void HandleRotaryEncSwitch() override {
        removeFocus();
    }

    void HandleRotaryEncChange(int inc) override {
        if (inc > 0 && selectedIndex + 1 < options.size()) {
            selectedIndex++;
        } else if (inc < 0 && selectedIndex > 0) {
            selectedIndex--;
        } else {
            return;
        }
        if (newSelCB) newSelCB(selectedIndex);
        redraw();
    }


private:
    static constexpr int kPad = 6;
    static constexpr int kCountW = 40;     // reserved top-right for "3 / 7"
    static constexpr int kDescH = 30;      // description band (two lines of 12px)
    static constexpr uint16_t kTextColour = 0xE71C;
    static constexpr uint16_t kNearColour = 0x94B2;  // neighbours
    static constexpr uint16_t kFarColour = 0x4A49;   // further out
    static constexpr uint16_t kDarkColour = 0x0841;  // text on the filled pill
    static constexpr uint16_t kDescColour = 0x8C71;

    struct Layout {
        int rows;
        int rowH[5];
        int top;
        int descH;
    };

    Layout layout() const {
        Layout L{};
        L.descH = (!descriptions.empty() && area.h >= 150) ? kDescH : 0;
        const int avail = area.h - L.descH - kPad;
        if (avail >= 16 + 22 + 32 + 22 + 16) {
            L.rows = 5;
            const int h[5] = {16, 22, 32, 22, 16};
            for (int i = 0; i < 5; i++) L.rowH[i] = h[i];
        } else {
            L.rows = 3;
            const int h[3] = {20, 30, 20};
            for (int i = 0; i < 3; i++) L.rowH[i] = h[i];
        }
        int total = 0;
        for (int i = 0; i < L.rows; i++) total += L.rowH[i];
        L.top = area.y + kPad / 2 + (avail - total) / 2;
        return L;
    }

    // Longest prefix of s that fits maxW in the sprite's current font, with ".." if cut.
    static String fit(TFT_eSprite& spr, const String& s, int maxW) {
        if (spr.textWidth(s) <= maxW) return s;
        String t = s;
        while (t.length() && spr.textWidth(t + "..") > maxW) t.remove(t.length() - 1);
        return t + "..";
    }

    // Row for option idx at offset off from the selection (0 = the selection).
    void drawRow(int idx, int off, int y, int h) {
        enum : uint8_t { kBg = 0, kAccent, kText, kNear, kFar, kDark };
        uint16_t palette[16] = {TFT_BLACK, accent_, kTextColour, kNearColour, kFarColour, kDarkColour};
        const int w = area.w - 2 * kCountW;  // centred, clear of the count
        TFT_eSprite spr(scr);
        spr.setColorDepth(4);
        if (!spr.createSprite(w, h)) return;
        spr.createPalette(palette, 16);
        spr.fillSprite(kBg);

        if (idx >= 0 && idx < static_cast<int>(options.size())) {
            String label = options[idx];
            label.toUpperCase();
            spr.setTextDatum(MC_DATUM);
            if (off == 0) {
                const bool filled = isFocused();
                if (filled) spr.fillRoundRect(0, 1, w, h - 2, (h - 2) / 2, kAccent);
                else        spr.drawRoundRect(0, 1, w, h - 2, (h - 2) / 2, kAccent);
                spr.setTextColor(filled ? kDark : kText);
                spr.setFreeFont(&Orbitron_16);
                if (spr.textWidth(label) > w - 16) spr.setFreeFont(&Orbitron_12);
                spr.drawString(fit(spr, label, w - 16).c_str(), w / 2, h / 2);
            } else {
                spr.setTextColor(abs(off) == 1 ? kNear : kFar);
                spr.setFreeFont(&Orbitron_12);
                spr.drawString(fit(spr, label, w - 8).c_str(), w / 2, h / 2);
            }
        }
        spr.pushSprite(area.x + kCountW, y);
        spr.deleteSprite();
    }

    void drawCount() {
        uint16_t palette[16] = {TFT_BLACK, kDescColour};
        TFT_eSprite spr(scr);
        spr.setColorDepth(4);
        if (!spr.createSprite(kCountW - 2, 14)) return;
        spr.createPalette(palette, 16);
        spr.fillSprite(0);
        if (!options.empty()) {
            spr.setFreeFont(&Orbitron_12);
            spr.setTextColor(1);
            spr.setTextDatum(TR_DATUM);
            spr.drawString((String(selectedIndex + 1) + "/" + String(options.size())).c_str(), kCountW - 3, 1);
        }
        spr.pushSprite(area.x + area.w - kCountW + 2, area.y + 2);
        spr.deleteSprite();
    }

    // Description of the current option, word-wrapped onto up to two lines.
    void drawDescription(int y, int h) {
        uint16_t palette[16] = {TFT_BLACK, kDescColour};
        const int w = area.w - 2 * kPad;
        TFT_eSprite spr(scr);
        spr.setColorDepth(4);
        if (!spr.createSprite(w, h)) return;
        spr.createPalette(palette, 16);
        spr.fillSprite(0);
        if (selectedIndex < descriptions.size()) {
            spr.setFreeFont(&Orbitron_12);
            spr.setTextColor(1);
            spr.setTextDatum(TC_DATUM);
            String rest = descriptions[selectedIndex];
            for (int line = 0; line < 2 && rest.length(); line++) {
                int cut = rest.length();
                if (spr.textWidth(rest) > w) {
                    // Last space that keeps the line within w.
                    cut = 0;
                    for (int i = 1; i <= static_cast<int>(rest.length()); i++) {
                        if (i == static_cast<int>(rest.length()) || rest[i] == ' ') {
                            if (spr.textWidth(rest.substring(0, i)) > w) break;
                            cut = i;
                        }
                    }
                    if (cut == 0) cut = rest.length();
                }
                String text = rest.substring(0, cut);
                rest = rest.substring(cut);
                rest.trim();
                if (line == 1 && rest.length()) text = fit(spr, text + " " + rest, w);
                spr.drawString(text.c_str(), w / 2, line * 14 + 1);
            }
        }
        spr.pushSprite(area.x + kPad, y);
        spr.deleteSprite();
    }

    uint16_t accent_;
    std::vector<String> options;
    std::vector<String> descriptions;
    size_t selectedIndex = 0;
    NewSelectionCallback newSelCB;
    std::function<void()> focusLostCB;
};

#endif
