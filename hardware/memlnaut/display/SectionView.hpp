#ifndef __SECTION_VIEW_HPP__
#define __SECTION_VIEW_HPP__

#include "View.hpp"
#include "../MEMLNaut.hpp"

class SectionView : public ViewBase {
public:
    SectionView(String name) : ViewBase(name) {}

    void addChild(const std::shared_ptr<ViewBase>& child) {
        children_.push_back(child);
        if (scr != nullptr) {
            child->SetGrid(grid_);
            child->Setup(scr, area);
        }
    }

    std::vector<std::shared_ptr<ViewBase>>* getSectionChildren() override {
        return &children_;
    }

    void OnSetup() override {
        Serial.println("DBG: SectionView::OnSetup " + name_);
        for (auto& child : children_) {
            Serial.println("DBG:   setting up child: " + child->GetName());
            child->SetGrid(grid_);
            child->Setup(scr, area);
            Serial.println("DBG:   child ok: " + child->GetName());
        }
        Serial.println("DBG: SectionView::OnSetup done " + name_);
    }

    void OnDraw() override {
        scr->fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);

        constexpr int itemH = 30;

        for (size_t i = 0; i < children_.size(); i++) {
            int y = area.y + (int)i * itemH;
            bool isSelected = isFocused() && (i == selectedIndex_);

            uint16_t bg = isSelected ? TFT_NAVY : TFT_BLACK;
            scr->fillRect(area.x, y, area.w, itemH, bg);
            scr->drawFastHLine(area.x, y + itemH - 1, area.w, TFT_DARKGREY);

            scr->setTextColor(isSelected ? TFT_YELLOW : TFT_WHITE, bg);
            scr->setTextDatum(ML_DATUM);
            scr->setTextFont(2);
            scr->drawString(children_[i]->GetName().c_str(), area.x + 10, y + itemH / 2);

            scr->setTextColor(TFT_DARKGREY, bg);
            scr->setTextDatum(MR_DATUM);
            scr->drawString(">", area.x + area.w - 10, y + itemH / 2);
        }

        scr->setTextDatum(TL_DATUM);
        scr->setTextFont(1);
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

    void HandleRotaryEncChange(int delta) override {
        int next = (int)selectedIndex_ + delta;
        next = constrain(next, 0, (int)children_.size() - 1);
        if ((size_t)next != selectedIndex_) {
            selectedIndex_ = (size_t)next;
            redraw();
        }
    }

    void HandleRotaryEncSwitch() override {
        if (!children_.empty()) {
            MEMLNaut::Instance()->disp->navigateInto(children_, name_, selectedIndex_);
        }
    }

    void OnTouchDown(size_t x, size_t y) override {
        constexpr int itemH = 30;
        int idx = ((int)y - area.y) / itemH;
        if (idx >= 0 && idx < (int)children_.size()) {
            selectedIndex_ = (size_t)idx;
            MEMLNaut::Instance()->disp->navigateInto(children_, name_, selectedIndex_);
        }
    }

private:
    std::vector<std::shared_ptr<ViewBase>> children_;
    size_t selectedIndex_ = 0;
};

#endif
