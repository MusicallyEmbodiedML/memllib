#ifndef __MIXER_VIEW_HPP__
#define __MIXER_VIEW_HPP__

#include "View.hpp"
#include "fonts/Orbitron_12.h"
#include <functional>
#include <vector>

// Touch mixer: one vertical fader per channel. Touch or drag in a channel's column to set
// its value; the value line shows the level, with a mark at unity. Each fader is drawn
// into its own small 4-bit sprite and only redrawn when its value changes.
// onChange fires on every move (apply the gain); onCommit fires on release (e.g. save).
class MixerView : public ViewBase {
public:
    struct Channel {
        String name;
        float value;  // kMin..kMax
    };
    static constexpr float kMin = 0.f;
    static constexpr float kMax = 2.f;   // a trim: x0 .. x2
    static constexpr float kUnity = 1.f;

    using ChangeFn = std::function<void(size_t channel, float value)>;
    using CommitFn = std::function<void()>;

    MixerView(String name, std::vector<String> channelNames, uint16_t accent = TFT_CYAN)
        : ViewBase(name), accent_(accent)
    {
        for (auto& n : channelNames) channels_.push_back({n, kUnity});
        dirty_.assign(channels_.size(), true);
    }

    void setOnChange(ChangeFn fn) { onChange_ = fn; }
    void setOnCommit(CommitFn fn) { onCommit_ = fn; }

    // Set without firing onChange (e.g. restoring saved values).
    void setValue(size_t ch, float v) {
        if (ch >= channels_.size()) return;
        channels_[ch].value = clamp(v);
        dirty_[ch] = true;
        needRedraw_ = true;
    }
    float getValue(size_t ch) const { return ch < channels_.size() ? channels_[ch].value : kUnity; }
    size_t numChannels() const { return channels_.size(); }

    void OnSetup() override {}

    void OnScreenCleared() override {
        dirty_.assign(channels_.size(), true);
    }

    void OnDraw() override {
        for (size_t i = 0; i < channels_.size(); i++) {
            if (dirty_[i]) {
                drawChannel(i);
                dirty_[i] = false;
            }
        }
    }

    void OnTouchDown(size_t x, size_t y) override { touch(x, y); }
    void OnTouchDrag(size_t x, size_t y) override { touch(x, y); }
    void OnTouchUp(size_t, size_t) override {
        activeCh_ = -1;
        if (onCommit_) onCommit_();
    }

private:
    static constexpr int kGap = 6;
    static constexpr int kLabelH = 16;   // channel name, bottom
    static constexpr int kValueH = 14;   // value readout, top
    static constexpr int kTrackW = 10;
    static constexpr uint16_t kTrackColour = 0x18E3;
    static constexpr uint16_t kUnityColour = 0x7BEF;
    static constexpr uint16_t kTextColour = 0xBDF7;

    static float clamp(float v) { return v < kMin ? kMin : (v > kMax ? kMax : v); }

    int colW() const { return (area.w - (static_cast<int>(channels_.size()) + 1) * kGap) / static_cast<int>(channels_.size()); }
    int colX(size_t i) const { return area.x + kGap + static_cast<int>(i) * (colW() + kGap); }
    int trackTop() const { return area.y + kValueH + 4; }
    int trackBottom() const { return area.y + area.h - kLabelH - 4; }
    int valueToY(float v) const {
        const float t = (v - kMin) / (kMax - kMin);
        return trackBottom() - static_cast<int>(t * (trackBottom() - trackTop()));
    }

    void touch(size_t x, size_t y) {
        // The channel is chosen at the press and kept for the drag, so a drag that drifts
        // sideways doesn't jump to the neighbouring fader.
        if (activeCh_ < 0) {
            for (size_t i = 0; i < channels_.size(); i++) {
                if (static_cast<int>(x) >= colX(i) - kGap / 2 && static_cast<int>(x) < colX(i) + colW() + kGap / 2) {
                    activeCh_ = static_cast<int>(i);
                    break;
                }
            }
            if (activeCh_ < 0) return;
        }
        const int top = trackTop(), bottom = trackBottom();
        int yy = static_cast<int>(y);
        yy = yy < top ? top : (yy > bottom ? bottom : yy);
        float v = kMin + (kMax - kMin) * static_cast<float>(bottom - yy) / static_cast<float>(bottom - top);
        if (fabsf(v - kUnity) < 0.04f) v = kUnity;  // easy to land on unity
        auto& ch = channels_[activeCh_];
        if (v == ch.value) return;
        ch.value = v;
        dirty_[activeCh_] = true;
        needRedraw_ = true;
        if (onChange_) onChange_(static_cast<size_t>(activeCh_), v);
    }

    void drawChannel(size_t i) {
        enum : uint8_t { kBg = 0, kTrack, kAccent, kUnity, kText };
        uint16_t palette[16] = {TFT_BLACK, kTrackColour, accent_, kUnityColour, kTextColour};
        const int w = colW(), h = area.h;
        TFT_eSprite spr(scr);
        spr.setColorDepth(4);
        if (!spr.createSprite(w, h)) return;
        spr.createPalette(palette, 16);
        spr.fillSprite(kBg);

        const int cx = w / 2;
        const int top = trackTop() - area.y, bottom = trackBottom() - area.y;
        const int vy = valueToY(channels_[i].value) - area.y;
        spr.fillRoundRect(cx - kTrackW / 2, top, kTrackW, bottom - top, 3, kTrack);
        if (bottom > vy) spr.fillRoundRect(cx - kTrackW / 2, vy, kTrackW, bottom - vy, 3, kAccent);
        const int uy = valueToY(kUnity) - area.y;
        spr.drawFastHLine(cx - kTrackW / 2 - 4, uy, kTrackW + 8, kUnity);  // unity mark
        spr.fillRect(cx - kTrackW / 2 - 3, vy - 2, kTrackW + 6, 4, kText);  // handle

        spr.setFreeFont(&Orbitron_12);
        spr.setTextColor(kText);
        spr.setTextDatum(TC_DATUM);
        spr.drawString(String(channels_[i].value, 2).c_str(), cx, 2);
        spr.setTextDatum(BC_DATUM);
        String label = channels_[i].name;
        label.toUpperCase();
        spr.drawString(label.c_str(), cx, h - 2);

        spr.pushSprite(colX(i), area.y);
        spr.deleteSprite();
    }

    std::vector<Channel> channels_;
    std::vector<bool> dirty_;
    uint16_t accent_;
    int activeCh_ = -1;
    ChangeFn onChange_;
    CommitFn onCommit_;
};

#endif // __MIXER_VIEW_HPP__
