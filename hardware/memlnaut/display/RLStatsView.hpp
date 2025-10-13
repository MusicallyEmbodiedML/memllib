#ifndef __RLSTATSVIEW_VIEW_HPP__
#define __RLSTATSVIEW_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class RLStatsView : public ViewBase {
public:
    RLStatsView(String name)
        : ViewBase(name)
    {

    }

    void OnSetup() override {
    }  

    void OnDisplay() override {

    };
    
    void OnDraw() override {
        TFT_eSprite textSprite(scr);
        textSprite.createSprite(120, 20);
        textSprite.setTextFont(2);
        auto drawText = [&textSprite, this](size_t x, size_t y, const String& item, const int colour) {
            scr->fillRect(area.x + x,area.y+y,120,20,TFT_BLACK);
            textSprite.fillSprite(TFT_BLACK);
            textSprite.setTextColor(colour, TFT_BLACK);
            textSprite.drawString(item.c_str(), 0, 0);
            textSprite.pushSprite(area.x + x,area.y + y);
        };
        constexpr int32_t lineheight = 20;
        drawText(10, 1 * lineheight, "Actor gnorm:", itemColour);
        drawText(150, 1 * lineheight,  String(actorGradNorm).c_str(), valueColour);

        drawText(10, 2 * lineheight, "Critic Loss:", itemColour);

        drawText(150, 2 * lineheight,  String(criticLoss).c_str(), valueColour);
    }  

    void setActorGradNorm(float v) {
        actorGradNorm = v;
        redraw();
    }

    void setCriticLoss(float loss) {
        criticLoss = loss;
        redraw();
    }

private:
    const int itemColour = TFT_WHITE;
    const int valueColour = TFT_YELLOW;
    float actorGradNorm = 0.f;
    float criticLoss = 0.f;
    #define GRAPHSIZE 100
    std::array<float, GRAPHSIZE> histGnorm, histCriticLoss;
    size_t indexGnorm=0, indexCriticLoss=0;
    
};

#endif 
