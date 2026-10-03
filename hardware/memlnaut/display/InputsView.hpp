#ifndef __INPUTS_VIEW_HPP__
#define __INPUTS_VIEW_HPP__

#include "View.hpp"
#include "BarGraphView.hpp"
#include <numeric>

// NN Inputs screen. Every source gets one labelled bar per input; joystick sources get a
// map instead: X/Y as a cursor over the positions of the liked (green) and disliked (red)
// memories, so you can see which parts of the joystick range have been taught. The
// cursor is a triangle: in 3D Z sets its rotation (fixed size); in 4D the second stick
// sets its size (Z) and rotation (W). Memories are small triangles in the same code, and
// the nearest liked memory (over all the stick dims) is outlined at full size as a target
// to steer to. The source name sits in a status line at the bottom.
// Like the RL screen, only what changed is pushed to the display: per frame that is the
// bars that moved and, in map mode, the cursor (plus the few pixels it uncovers).
class InputsView : public ViewBase {
public:
    struct MemPoint {
        uint8_t x, y, z, w;  // input position, 0..255 (w: 4D joystick only)
        bool liked;
        bool operator==(const MemPoint& o) const {
            return x == o.x && y == o.y && z == o.z && w == o.w && liked == o.liked;
        }
    };

    static constexpr int kStatusBarHeight = 20;

    InputsView(String name, size_t maxInputs, uint16_t colour = TFT_YELLOW)
        : ViewBase(name)
        , bars_(std::make_shared<BarGraphView>(name, maxInputs, 10, colour))
    {
        bars_->setStyle(BarGraphView::Style::BARS);
        bars_->setGridEnabled(true);
        bars_->setMaxBarWidth(24);
    }

    void OnSetup() override {
        layoutMap();
        attachSubviews();
    }

    void OnScreenCleared() override {
        fullRepaint_ = true;
    }

    // The input source changed. labels: one per active input. map: show the X/Y map
    // (joystick sources: 3 inputs = X/Y + rotation, 4 = X/Y + size and rotation).
    void setSource(const String& name, const std::vector<String>& labels, bool map) {
        sourceName_ = name;
        mapMode_ = map && labels.size() > 2;
        has4_ = mapMode_ && labels.size() >= 4;
        if (!labels.empty()) {
            std::vector<size_t> starts(labels.size());
            std::iota(starts.begin(), starts.end(), 0);
            bars_->setNumDisplayBars(labels.size());
            bars_->setGroups(starts, labels);  // one group per bar: gap + label each
        }
        if (scr) {
            layoutMap();
            attachSubviews();
        }
        clearArea_ = true;
        needRedraw_ = true;
    }

    bool isMapMode() const { return mapMode_; }

    void UpdateValues(const std::vector<float>& values) {
        if (!mapMode_) {
            bars_->UpdateValues(values);
            return;
        }
        if (values.size() < 2) return;
        x_ = values[0];
        y_ = values[1];
        if (values.size() >= 3) z_ = values[2];
        if (has4_ && values.size() >= 4) w_ = values[3];
        if (memDirty_ || !cursorDrawn_ || !(markerFor(x_, y_, z_, w_) == drawn_)) needRedraw_ = true;
    }

    // Input positions of the replay memories. A no-op unless they changed, since a change
    // means re-plotting the whole map.
    void setMemoryPoints(std::vector<MemPoint>& pts) {
        if (pts == mem_) return;
        mem_.swap(pts);
        memDirty_ = true;
        needRedraw_ = true;
    }

    void OnDraw() override {
        if (clearArea_) {
            scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);
            for (auto& sv : subviews) sv->invalidate();
            clearArea_ = false;
            fullRepaint_ = true;
        }
        if (fullRepaint_) {
            drawStatus();
            if (mapMode_) memDirty_ = true;
            fullRepaint_ = false;
        }
        if (!mapMode_) return;

        if (memDirty_) {
            scr->fillRect(mapX_ + 1, mapY_ + 1, mapS_ - 2, mapS_ - 2, TFT_BLACK);
            scr->drawRect(mapX_, mapY_, mapS_, mapS_, kFrameColour);
            for (int k = 0; k < 3; k++) {
                scr->drawFastVLine(gridX_[k], mapY_ + 1, mapS_ - 2, kGridColour);
                scr->drawFastHLine(mapX_ + 1, gridY_[k], mapS_ - 2, kGridColour);
            }
            for (const auto& p : mem_) drawMemPoint(p);
            cursorDrawn_ = false;
            ghostShown_ = false;
            memDirty_ = false;
        }

        const Marker m = markerFor(x_, y_, z_, w_);
        Marker g{};
        const bool wantGhost = ghostFor(g);
        const bool ghostChanged = wantGhost != ghostShown_ || (wantGhost && !(g == ghost_));
        if (cursorDrawn_ && m == drawn_ && !ghostChanged) return;

        // Layers, bottom to top: grid, memories, target outline, cursor.
        if (cursorDrawn_) {
            paintMarker(drawn_, TFT_BLACK, TFT_BLACK);
            repair(bounds(drawn_));
        }
        if (ghostChanged && ghostShown_) {
            ghostShown_ = false;
            paintMarker(ghost_, TFT_BLACK, TFT_BLACK);
            repair(bounds(ghost_));
        }
        if (ghostChanged && wantGhost) {
            ghost_ = g;
            ghostShown_ = true;
            paintMarker(ghost_, kGhostColour, kGhostColour);
        }
        paintMarker(m, kStick2Colour, TFT_WHITE);
        drawn_ = m;
        cursorDrawn_ = true;
    }

private:
    static constexpr int kMapInset = 6;  // keeps a small triangle off the frame
    static constexpr uint16_t kGridColour = 0x2104;
    static constexpr uint16_t kFrameColour = 0x4208;
    static constexpr uint16_t kLikedColour = TFT_GREEN;
    static constexpr uint16_t kDislikedColour = TFT_RED;
    static constexpr uint16_t kStick2Colour = TFT_CYAN;
    static constexpr uint16_t kGhostColour = 0x0340;  // dim green: nearest liked memory

    static float clamp01(float v) {
        return v < 0.f ? 0.f : (v > 1.f ? 1.f : (std::isfinite(v) ? v : 0.f));
    }

    // A triangle around (cx, cy); vertex 0 is the tip.
    struct Marker {
        int cx, cy;
        int vx[3], vy[3];
        bool operator==(const Marker& o) const {
            if (cx != o.cx || cy != o.cy) return false;
            for (int i = 0; i < 3; i++)
                if (vx[i] != o.vx[i] || vy[i] != o.vy[i]) return false;
            return true;
        }
    };

    static constexpr float kTriMinR = 5.f;
    static constexpr float kMemTriMinR = 2.f;   // memory triangles: small, so many fit
    static constexpr float kMemTriMaxR = 7.f;
    static constexpr float kTri3DSize = 0.6f;   // 3D: fixed size, as a fraction of the range
    static constexpr float kTriBaseAngle = 2.44f;  // base vertices at +-140 deg from the tip

    float triMaxR() const { return mapS_ / 6.f; }

    // Triangle around (m.cx, m.cy): radius r, tip pointing at ang (0 = right, CCW). Shrunk
    // (keeping the rotation) so every vertex, and the 3px tip mark, stays inside the frame.
    void makeTriangle(Marker& m, float r, float ang) const {
        const float offs[3] = {0.f, kTriBaseAngle, -kTriBaseAngle};
        float dx[3], dy[3];
        for (int i = 0; i < 3; i++) {
            dx[i] = cosf(ang + offs[i]);
            dy[i] = -sinf(ang + offs[i]);  // screen y points down
        }
        const float lo_x = mapX_ + 2 - m.cx, hi_x = mapX_ + mapS_ - 3 - m.cx;
        const float lo_y = mapY_ + 2 - m.cy, hi_y = mapY_ + mapS_ - 3 - m.cy;
        for (int i = 0; i < 3; i++) {
            if (dx[i] * r > hi_x) r = hi_x / dx[i];
            if (dx[i] * r < lo_x) r = lo_x / dx[i];
            if (dy[i] * r > hi_y) r = hi_y / dy[i];
            if (dy[i] * r < lo_y) r = lo_y / dy[i];
        }
        for (int i = 0; i < 3; i++) {
            m.vx[i] = m.cx + static_cast<int>(lroundf(dx[i] * r));
            m.vy[i] = m.cy + static_cast<int>(lroundf(dy[i] * r));
        }
    }

    // 4D: Z sets the size (minR..maxR), W the rotation. 3D: fixed size, Z the rotation.
    // Rotation is one full turn over the input range, 0 = pointing right.
    Marker triangleFor(float x, float y, float z, float w, float minR, float maxR) const {
        Marker m{};
        toMap(x, y, m.cx, m.cy);
        const float size = has4_ ? clamp01(z) : kTri3DSize;
        const float turn = has4_ ? clamp01(w) : clamp01(z);
        makeTriangle(m, minR + size * (maxR - minR), turn * 6.2831853f);
        return m;
    }

    Marker markerFor(float x, float y, float z, float w) const {
        return triangleFor(x, y, z, w, kTriMinR, triMaxR());
    }

    Marker memMarker(const MemPoint& p) const {
        return triangleFor(p.x / 255.f, p.y / 255.f, p.z / 255.f, p.w / 255.f,
                           kMemTriMinR, kMemTriMaxR);
    }

    // The liked memory nearest the current input over all four dims (raw input space, as
    // the network sees it), as a full-size outline. False if there are no likes.
    bool ghostFor(Marker& g) const {
        auto q = [](float v) { return static_cast<int>((v < 0.f ? 0.f : (v > 1.f ? 1.f : v)) * 255.f + 0.5f); };
        const int x = q(x_), y = q(y_), z = q(z_), w = q(w_);
        int best = -1;
        int bestD = 0;
        for (size_t i = 0; i < mem_.size(); i++) {
            const auto& p = mem_[i];
            if (!p.liked) continue;
            const int dx = p.x - x, dy = p.y - y, dz = p.z - z, dw = has4_ ? p.w - w : 0;
            const int d = dx * dx + dy * dy + dz * dz + dw * dw;
            if (best < 0 || d < bestD) { best = static_cast<int>(i); bestD = d; }
        }
        if (best < 0) return false;
        const auto& p = mem_[best];
        g = triangleFor(p.x / 255.f, p.y / 255.f, p.z / 255.f, p.w / 255.f, kTriMinR, triMaxR());
        return true;
    }

    // Marker drawing uses only fast H/V lines and fillRect (no drawPixel-based primitives),
    // and erasing repeats exactly the same calls in black.

    void paintMarker(const Marker& m, uint16_t triColour, uint16_t cursorColour) {
        for (int i = 0; i < 3; i++)
            plotLine(m.vx[i], m.vy[i], m.vx[(i + 1) % 3], m.vy[(i + 1) % 3], triColour);
        scr->fillRect(m.vx[0] - 1, m.vy[0] - 1, 3, 3, triColour);  // mark the tip
        scr->fillRect(m.cx, m.cy, 1, 1, cursorColour);
    }

    // Bresenham, emitted as runs along the major axis so each run is one fast line.
    void plotLine(int x0, int y0, int x1, int y1, uint16_t c) {
        const bool steep = abs(y1 - y0) > abs(x1 - x0);
        if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
        if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
        const int dx = x1 - x0, dy = abs(y1 - y0), ystep = (y0 < y1) ? 1 : -1;
        int err = dx / 2, runStart = x0, y = y0;
        for (int x = x0; x <= x1; x++) {
            err -= dy;
            if (err < 0 || x == x1) {
                const int len = x - runStart + 1;
                if (steep) scr->drawFastVLine(y, runStart, len, c);
                else       scr->drawFastHLine(runStart, y, len, c);
                runStart = x + 1;
                if (err < 0) { y += ystep; err += dx; }
            }
        }
    }

    void layoutMap() {
        mapS_ = area.h - kStatusBarHeight - 10;
        mapX_ = area.x + (area.w - mapS_) / 2;
        mapY_ = area.y + 5;
        for (int k = 0; k < 3; k++) {
            gridX_[k] = mapX_ + (mapS_ * (k + 1)) / 4;
            gridY_[k] = mapY_ + (mapS_ * (k + 1)) / 4;
        }
    }

    // Swap in the subview for the current mode (it draws itself after OnDraw).
    void attachSubviews() {
        subviews.clear();
        if (!mapMode_) AddSubView(bars_, {area.x, area.y, area.w, area.h - kStatusBarHeight});
    }

    // Input value (0..1) to map pixel; y = 1 at the top.
    void toMap(float x, float y, int& px, int& py) const {
        x = clamp01(x);
        y = clamp01(y);
        const int span = mapS_ - 1 - 2 * kMapInset;
        px = mapX_ + kMapInset + static_cast<int>(x * span + 0.5f);
        py = mapY_ + kMapInset + static_cast<int>((1.f - y) * span + 0.5f);
    }

    void drawMemPoint(const MemPoint& p) {
        const uint16_t c = p.liked ? kLikedColour : kDislikedColour;
        const Marker m = memMarker(p);
        for (int i = 0; i < 3; i++)
            plotLine(m.vx[i], m.vy[i], m.vx[(i + 1) % 3], m.vy[(i + 1) % 3], c);
    }

    struct Box { int x0, y0, x1, y1; };

    static bool overlaps(const Box& a, const Box& b) {
        return a.x0 <= b.x1 && b.x0 <= a.x1 && a.y0 <= b.y1 && b.y0 <= a.y1;
    }

    Box bounds(const Marker& m) const {
        Box b{m.cx, m.cy, m.cx, m.cy};
        for (int i = 0; i < 3; i++) {  // +-1 covers the tip mark
            b.x0 = std::min(b.x0, m.vx[i] - 1); b.x1 = std::max(b.x1, m.vx[i] + 1);
            b.y0 = std::min(b.y0, m.vy[i] - 1); b.y1 = std::max(b.y1, m.vy[i] + 1);
        }
        return b;
    }

    Box memBounds(const MemPoint& p) const {
        int px, py;
        toMap(p.x / 255.f, p.y / 255.f, px, py);
        const int r = static_cast<int>(kMemTriMaxR) + 1;
        return {px - r, py - r, px + r, py + r};
    }

    // After erasing something, restore what lies under it inside box b: gridlines, then
    // any memory glyph or target outline touching b (redrawn whole, so exact).
    void repair(Box b) {
        b.x0 = std::max(b.x0, mapX_ + 1); b.x1 = std::min(b.x1, mapX_ + mapS_ - 2);
        b.y0 = std::max(b.y0, mapY_ + 1); b.y1 = std::min(b.y1, mapY_ + mapS_ - 2);
        if (b.x0 > b.x1 || b.y0 > b.y1) return;
        for (int k = 0; k < 3; k++) {
            if (gridX_[k] >= b.x0 && gridX_[k] <= b.x1) scr->drawFastVLine(gridX_[k], b.y0, b.y1 - b.y0 + 1, kGridColour);
            if (gridY_[k] >= b.y0 && gridY_[k] <= b.y1) scr->drawFastHLine(b.x0, gridY_[k], b.x1 - b.x0 + 1, kGridColour);
        }
        for (const auto& p : mem_)
            if (overlaps(memBounds(p), b)) drawMemPoint(p);
        if (ghostShown_ && overlaps(bounds(ghost_), b)) paintMarker(ghost_, kGhostColour, kGhostColour);
    }

    void drawStatus() {
        const int y = area.y + area.h - kStatusBarHeight;
        scr->fillRect(area.x, y, area.w, kStatusBarHeight, TFT_BLACK);
        scr->setTextFont(1);
        scr->setTextDatum(TL_DATUM);
        scr->setTextColor(TFT_DARKGREY, TFT_BLACK);
        scr->drawString("Source:", area.x + 2, y + 6);
        scr->setTextColor(TFT_WHITE, TFT_BLACK);
        scr->drawString(sourceName_.c_str(), area.x + 50, y + 6);
        if (mapMode_) {
            scr->setTextColor(kLikedColour, TFT_BLACK);
            scr->drawString("liked", area.x + 200, y + 6);
            scr->setTextColor(kDislikedColour, TFT_BLACK);
            scr->drawString("disliked", area.x + 240, y + 6);
        }
    }

    std::shared_ptr<BarGraphView> bars_;      // all inputs (non-map sources)
    String sourceName_;
    bool mapMode_{false};

    std::vector<MemPoint> mem_;
    float x_{0.f}, y_{0.f}, z_{0.f}, w_{0.f};
    Marker drawn_{};
    bool cursorDrawn_{false};
    Marker ghost_{};          // nearest-liked-memory outline (4D)
    bool ghostShown_{false};
    bool has4_{false};  // 4D joystick: second stick on the map

    int mapX_{0}, mapY_{0}, mapS_{1};
    int gridX_[3]{}, gridY_[3]{};

    bool clearArea_{true};
    bool fullRepaint_{true};
    bool memDirty_{true};
};

#endif // __INPUTS_VIEW_HPP__
