#ifndef __BUTTON_VIEW_HPP__
#define __BUTTON_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "fonts/Orbitron_16.h"
#include "fonts/Orbitron_12.h"


// A rounded tile. Off: dark fill with an accent outline and light text. On: filled with
// the accent colour, dark text. Pressing inverts it until release. The label is centred
// in Orbitron (16px caps, falling back to 12px, then wrapped onto up to 3 lines). An
// "empty" tile (e.g. an unused save slot) is drawn dim with a dash; an optional corner
// label (e.g. the slot number) sits top-left.
// Drawn into a 4-bit palette sprite and pushed in one go (exact colours, no text
// corruption, a few KB only while drawing).
class ButtonView : public ViewBase {
public:

    using ButtonCallback = std::function<void(size_t)>;

    // fillColour is the accent colour; fontColour and fontNum are kept for compatibility
    // (the tile style picks its own text colour and font).
    ButtonView(String name, size_t _id_, int _fillcolour_ = TFT_BLUE, int _fontcolour_ = TFT_WHITE, uint8_t fontNum_ = 4)
        : ViewBase(name), accent(static_cast<uint16_t>(_fillcolour_)), id(_id_)
    {
        (void)_fontcolour_;
        (void)fontNum_;
    }

    void SetReleaseCallback(ButtonCallback __callback) {
        callback = __callback;
    }

    void OnSetup() override {
    }

    void setAccent(uint16_t c) { accent = c; redraw(); }
    void setFillColour(int newCol) { setAccent(static_cast<uint16_t>(newCol)); }  // compat
    void setFontColour(int) {}                                                   // compat
    void setBorderWidth(int) {}                                                  // compat

    void setOn(bool v)            { if (v != on) { on = v; redraw(); } }
    void setEmpty(bool v)         { if (v != empty) { empty = v; redraw(); } }
    void setCornerLabel(const String& s) { corner = s; redraw(); }
    void setLabel(const String& s) { name_ = s; redraw(); }

    void OnDraw() override {
        enum : uint8_t { kBlack = 0, kTile, kAccent, kTextLight, kTextDark, kDim, kCornerGrey };
        uint16_t palette[16] = {TFT_BLACK, kTileColour, accent, kTextLightColour,
                                kTextDarkColour, kDimColour, kCornerColour};
        TFT_eSprite spr(scr);
        spr.setColorDepth(4);
        if (!spr.createSprite(area.w, area.h)) return;
        spr.createPalette(palette, 16);
        spr.fillSprite(kBlack);

        const bool filled = on != pressed;
        const int r = std::min(kRadius, std::min(area.w, area.h) / 4);
        spr.fillRoundRect(0, 0, area.w, area.h, r, filled ? kAccent : kTile);
        if (!filled) spr.drawRoundRect(0, 0, area.w, area.h, r, empty ? kDim : kAccent);

        const uint8_t textCol = filled ? kTextDark : (empty ? kDim : kTextLight);
        int top = 0;
        if (corner.length()) {
            spr.setFreeFont(&Orbitron_12);
            spr.setTextColor(filled ? kTextDark : kCornerGrey);
            spr.setTextDatum(TL_DATUM);
            spr.drawString(corner.c_str(), 5, 3);
            top = 12;  // keep the label clear of the corner number
        }
        drawLabel(spr, empty ? String("-") : name_, textCol, top);

        spr.pushSprite(area.x, area.y);
        spr.deleteSprite();
    }


    void OnTouchDown(size_t x, size_t y) override {
        pressed = true;
        redraw();
    }

    void OnTouchUp(size_t x, size_t y) override {
        if (callback) {
            callback(id);
        }
        pressed = false;
        redraw();
    }


private:
    static constexpr int kRadius = 6;
    static constexpr int kPadX = 4;
    static constexpr uint16_t kTileColour = 0x18E3;       // matches the header bar
    static constexpr uint16_t kTextLightColour = 0xE71C;
    static constexpr uint16_t kTextDarkColour = 0x0841;
    static constexpr uint16_t kDimColour = 0x4208;
    static constexpr uint16_t kCornerColour = 0x8410;

    // Centred in the tile below `top`: Orbitron 16 caps, else 12 caps, else 12 wrapped
    // (at spaces where possible) onto up to 3 lines, the last cut short with "..".
    void drawLabel(TFT_eSprite& spr, const String& text, uint8_t colour, int top) {
        const int maxW = area.w - 2 * kPadX;
        const int cx = area.w / 2;
        const int cy = top + (area.h - top) / 2;
        spr.setTextColor(colour);
        spr.setTextDatum(MC_DATUM);
        String caps = text;
        caps.toUpperCase();

        spr.setFreeFont(&Orbitron_16);
        if (spr.textWidth(caps) <= maxW) { spr.drawString(caps.c_str(), cx, cy); return; }
        spr.setFreeFont(&Orbitron_12);
        if (spr.textWidth(caps) <= maxW) { spr.drawString(caps.c_str(), cx, cy); return; }

        constexpr int kMaxLines = 3;
        constexpr int kLineH = 13;
        String lines[kMaxLines];
        int n = 0;
        String rest = caps;
        while (rest.length() && n < kMaxLines) {
            // Longest prefix that fits, preferring to break at a space.
            int fit = 0;
            for (int i = 1; i <= static_cast<int>(rest.length()); i++) {
                if (spr.textWidth(rest.substring(0, i)) > maxW) break;
                fit = i;
            }
            if (fit == 0) fit = 1;
            if (fit < static_cast<int>(rest.length())) {
                const int sp = rest.lastIndexOf(' ', fit);
                if (sp > 0) fit = sp;
            }
            lines[n] = rest.substring(0, fit);
            rest = rest.substring(fit);
            rest.trim();
            n++;
        }
        if (rest.length() && n > 0) {  // didn't all fit: shorten the last line
            String& last = lines[n - 1];
            while (last.length() && spr.textWidth(last + "..") > maxW) last.remove(last.length() - 1);
            last += "..";
        }
        const int y0 = cy - ((n - 1) * kLineH) / 2;
        for (int i = 0; i < n; i++) spr.drawString(lines[i].c_str(), cx, y0 + i * kLineH);
    }

    uint16_t accent;
    size_t id;

    ButtonCallback callback = nullptr;

    bool pressed = false;
    bool on = false;
    bool empty = false;
    String corner;
};

#endif
