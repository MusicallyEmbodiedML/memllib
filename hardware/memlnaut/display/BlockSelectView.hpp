#ifndef __BLOCK_SELECT_VIEW_HPP__
#define __BLOCK_SELECT_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"
#include "ButtonView.hpp"


// A grid of ButtonView tiles plus a message line. The grid is laid out to fit the view:
// up to 4 tiles in one row, up to 12 in two, else three; tiles numbered left to right,
// equal gutters, centred. buttonWidth/buttonHeight are only hints (the height caps the
// tile height so a single row doesn't become huge).
// "Alt" state = the tile is on (filled with the accent colour). The accent is the alt
// colour, or the button colour if they are the same; setAccent() overrides it.
// Slot style (save/load screens): each tile shows its number in the corner and a tile
// with an empty name is drawn as an empty slot.
class BlockSelectView : public ViewBase {
public:
    using OnSelectCallback = std::function<void(size_t)>;

    BlockSelectView(String name, int _buttonColour_ = TFT_BLUE, size_t nButtons_=8, size_t buttonWidth_=60, size_t buttonHeight_=60, int fontColour_=TFT_WHITE, std::vector<String> buttonNames_ = {}, int buttonAltColour_=TFT_BLUE, uint8_t buttonFontNum_=4, int altFontColour_=TFT_MAROON, int32_t altBorderWidth_=3)
                : ViewBase(name),
                accent(static_cast<uint16_t>(buttonAltColour_ != _buttonColour_ ? buttonAltColour_ : _buttonColour_)),
                nButtons(nButtons_), buttonWidth(buttonWidth_), buttonHeight(buttonHeight_)
    {
        (void)fontColour_;
        (void)buttonFontNum_;
        (void)altFontColour_;
        (void)altBorderWidth_;
        rows = (nButtons <= 4) ? 1 : (nButtons <= 12 ? 2 : 3);
        cols = (nButtons + rows - 1) / rows;
        if (!buttonNames_.empty() && buttonNames_.size() == nButtons_) {
            buttonNames = buttonNames_;
        } else {
            for(size_t i = 1; i <= nButtons; i++) {
                buttonNames.push_back(String(i));
            }
        }
        altColour.resize(nButtons, false);
    }

    void SetOnSelectCallback(OnSelectCallback _cb_) {
        cb = _cb_;
    }

    void setAccent(uint16_t c) {
        accent = c;
        for (auto& b : buttons) b->setAccent(c);
    }

    void setSlotStyle(bool on) {
        slotStyle = on;
        for (size_t i = 0; i < buttons.size(); i++) styleSlot(i);
    }

    // Safe before OnSetup (only records the state).
    void setAltColour(size_t index, bool on) {
        if (index < altColour.size()) altColour[index] = on;
        if (index < buttons.size()) buttons[index]->setOn(on);
    }

    void toggleAlt(size_t index) {
        if (index >= altColour.size()) return;
        setAltState(index, !altColour[index]);
    }

    void setAltState(size_t index, bool on) {
        if (index >= buttons.size()) return;
        altColour[index] = on;
        buttons[index]->setOn(on);
    }


    void OnSetup() override {
        // Grid sized to the area above the message line, centred.
        const int availH = area.h - kMsgH;
        const int tileW = (area.w - static_cast<int>(cols + 1) * kGap) / static_cast<int>(cols);
        int tileH = (availH - static_cast<int>(rows + 1) * kGap) / static_cast<int>(rows);
        tileH = std::min(tileH, std::max(static_cast<int>(buttonHeight), kMinTileH));
        const int gridW = static_cast<int>(cols) * tileW + static_cast<int>(cols - 1) * kGap;
        const int gridH = static_cast<int>(rows) * tileH + static_cast<int>(rows - 1) * kGap;
        const int x0 = area.x + (area.w - gridW) / 2;
        const int y0 = area.y + (availH - gridH) / 2;

        for (size_t idx = 0; idx < nButtons; idx++) {
            const int r = static_cast<int>(idx / cols), c = static_cast<int>(idx % cols);
            auto button = std::make_shared<ButtonView>(buttonNames[idx], idx + 1, accent);
            button->setOn(altColour[idx]);
            rect bounds = { x0 + c * (tileW + kGap), y0 + r * (tileH + kGap), tileW, tileH };
            AddSubView(button, bounds);
            button->SetReleaseCallback([this](size_t id) {
                if (cb) {
                    cb(id);
                }
            });
            buttons.push_back(button);
            styleSlot(idx);
        }
    }

    void OnScreenCleared() override {
        msgDirty = true;
    }

    void OnDraw() override {
        if (!msgDirty) return;
        msgDirty = false;
        drawMessage();
    }

    void updateButtonName(size_t idx, const String& newName) {
        if (idx < buttonNames.size()) buttonNames[idx] = newName;
        if (idx < buttons.size()) {
            buttons[idx]->setLabel(newName);
            styleSlot(idx);
        }
    }

    void SetMessage(const String &__msg) {
        msg = __msg;
        msgDirty = true;
        needRedraw_ = true;  // just the message line, not the tiles
    }



private:
    static constexpr int kGap = 8;
    static constexpr int kMsgH = 18;
    static constexpr int kMinTileH = 40;
    static constexpr uint16_t kMsgColour = 0xBDF7;

    void styleSlot(size_t idx) {
        if (idx >= buttons.size()) return;
        buttons[idx]->setCornerLabel(slotStyle ? String(idx + 1) : String(""));
        buttons[idx]->setEmpty(slotStyle && buttonNames[idx].length() == 0);
    }

    // Bottom line, Orbitron 12 if it fits, else the plain 16px font. Drawn into a 4-bit
    // sprite like the tiles.
    void drawMessage() {
        TFT_eSprite spr(scr);
        spr.setColorDepth(4);
        if (!spr.createSprite(area.w, kMsgH)) return;
        uint16_t palette[16] = {TFT_BLACK, kMsgColour};
        spr.createPalette(palette, 16);
        spr.fillSprite(0);
        if (msg.length()) {
            spr.setTextColor(1);
            spr.setTextDatum(ML_DATUM);
            spr.setFreeFont(&Orbitron_12);
            if (spr.textWidth(msg) > area.w - 2 * kGap) spr.setTextFont(2);
            spr.drawString(msg.c_str(), kGap, kMsgH / 2);
        }
        spr.pushSprite(area.x, area.y + area.h - kMsgH);
        spr.deleteSprite();
    }

    std::vector<std::shared_ptr<ButtonView>> buttons;
    uint16_t accent;
    OnSelectCallback cb = nullptr;
    String msg;
    bool msgDirty = true;
    bool slotStyle = false;

    size_t nButtons;
    size_t rows;
    size_t cols;
    size_t buttonWidth = 50;
    size_t buttonHeight = 50;
    std::vector<String> buttonNames;
    std::vector<bool> altColour;
};

#endif
