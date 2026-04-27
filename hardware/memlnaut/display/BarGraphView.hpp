#ifndef __BARGRAPH_VIEW_HPP__
#define __BARGRAPH_VIEW_HPP__

#include "View.hpp"
#include "UIElements.hpp"


class BarGraphView : public ViewBase {
public:
    BarGraphView(String name, size_t nOutputs, int barwidth = 2, int colour = TFT_GREEN, float rangeLow=0.f, float rangeHigh=1.f)
        : ViewBase(name), colour(colour), barwidth(barwidth), oldValues(nOutputs, 0.0f), newValues(nOutputs, 0.0f),
          rangeLow(rangeLow), rangeHigh(rangeHigh), runningMax(nOutputs,0), runningMin(nOutputs,0)  
    {

    }

    void OnSetup() override {
        offsetX = 5;
        offsetY = 5;
        barSectionWidth = (area.w - (2 * offsetX)) / static_cast<float>(newValues.size());
        barSectionHeight = area.h - (2 * offsetY) - 18;
        rangeTotal = rangeHigh - rangeLow;
        rangeTotalInv = 1.f / rangeTotal;
    }

    void OnDisplay() override {
    };

    void setStatus(float loss, size_t memCount, float lr, float noise) {
        statusLoss_ = loss;
        statusMemCount_ = memCount;
        statusLR_ = lr;
        statusNoise_ = noise;
    }

    void flashCommand(const String& cmd) {
        statusCmd_ = cmd;
        cmdExpiry_ = millis() + 3000;
    }

    void UpdateValues(const std::vector<float>& values, bool resetMinMax=false) {
        for(size_t i=0; i < values.size(); i++) {
            newValues[i] = values[i];
        }
        if (resetMinMax) {
            runningMax = values;
            runningMin = values;
        }
        if (!statusCmd_.isEmpty() && millis() > cmdExpiry_) {
            statusCmd_ = "";
        }
        redraw();
    }

    
    void OnDraw() override {
        for(size_t i=0; i < newValues.size(); i++) {
            float newVal = newValues[i];
            // if (newVal < rangeLow) {
            //     newVal = rangeLow;
            // } else if (newVal > rangeHigh) {
            //     newVal = rangeHigh;
            // }
            float normalizedNewValue = (newVal - rangeLow) * rangeTotalInv;
            int newBarHeight = static_cast<int>(normalizedNewValue * barSectionHeight);

            float oldVal = oldValues[i];
            // if (newVal < rangeLow) {
            //     newVal = rangeLow;
            // } else if (newVal > rangeHigh) {
            //     newVal = rangeHigh;
            // }
            float normalizedOldValue = (oldVal - rangeLow) * rangeTotalInv;
            int oldBarHeight = static_cast<int>(normalizedOldValue * barSectionHeight);

            int x = area.x + offsetX + static_cast<int>(i * barSectionWidth);
            
            int newy = area.y + area.h - offsetY - newBarHeight;
            int oldy = area.y + area.h - offsetY - oldBarHeight;

            // if (newy < oldy) {
            //     scr->fillRect(x,oldy,barwidth,oldy-newy,TFT_BLACK); 
            // }else{
            //     scr->fillRect(x,newy,barwidth,newy-oldy,colour); 
            // }
            // scr->drawLine(x,oldy, x+barwidth, oldy, TFT_BLACK);
            // scr->drawLine(x,newy, x+barwidth, newy, colour);
            scr->fillRect(x,oldy, barwidth, 4, TFT_BLACK);
            scr->fillRect(x,newy, barwidth, 4, colour);

            // Update running max/min
            //TODO: what happens after reset?

            // if (newValues[i] > runningMax[i]) {

            //     float normalizedOldMaxValue = (runningMax[i] - rangeLow) * rangeTotalInv;
            //     int oldMaxBarHeight = static_cast<int>(normalizedOldMaxValue * barSectionHeight);
            //     int oldmaxy = area.y + area.h - offsetY - oldMaxBarHeight;
            //     scr->drawLine(x,oldmaxy, x+barwidth, oldmaxy, TFT_BLACK);

            //     runningMax[i] = newValues[i];

            //     normalizedOldMaxValue = (runningMax[i] - rangeLow) * rangeTotalInv;
            //     oldMaxBarHeight = static_cast<int>(normalizedOldMaxValue * barSectionHeight);
            //     oldmaxy = area.y + area.h - offsetY - oldMaxBarHeight;
            //     scr->drawLine(x,oldmaxy, x+barwidth, oldmaxy, TFT_PINK);
            // }
            // if (newValues[i] < runningMin[i]) {

            //     float normalizedOldMinValue = (runningMin[i] - rangeLow) * rangeTotalInv;
            //     int oldMinBarHeight = static_cast<int>(normalizedOldMinValue * barSectionHeight);
            //     int oldminy = area.y + area.h - offsetY - oldMinBarHeight;
            //     scr->drawLine(x,oldminy, x+barwidth, oldminy, TFT_BLACK);

            //     runningMin[i] = newValues[i];

            //     normalizedOldMinValue = (runningMin[i] - rangeLow) * rangeTotalInv;
            //     oldMinBarHeight = static_cast<int>(normalizedOldMinValue * barSectionHeight);
            //     oldminy = area.y + area.h - offsetY - oldMinBarHeight;
            //     scr->drawLine(x,oldminy, x+barwidth, oldminy, TFT_PINK);
            // }
        }
        oldValues = newValues;
        drawnValues = true;

        int statusY = area.y + area.h - 18;
        scr->fillRect(area.x, statusY, area.w, 18, TFT_BLACK);
        scr->setTextColor(TFT_DARKGREY, TFT_BLACK);
        scr->setTextFont(1);
        scr->setCursor(area.x + 2, statusY + 4);
        char buf[48];
        snprintf(buf, sizeof(buf), "L:%.3f M:%d LR:%.2f N:%.2f",
                 statusLoss_, (int)statusMemCount_, statusLR_, statusNoise_);
        scr->print(buf);
        if (!statusCmd_.isEmpty()) {
            scr->setTextColor(TFT_YELLOW, TFT_BLACK);
            scr->setCursor(area.x + area.w - 68, statusY + 4);
            scr->print(statusCmd_);
        }
    }



private:
    std::vector<float> newValues, oldValues;
    int colour = TFT_GREEN;

    float statusLoss_{0.f};
    size_t statusMemCount_{0};
    float statusLR_{0.f};
    float statusNoise_{0.f};
    String statusCmd_{};
    unsigned long cmdExpiry_{0};
    int barwidth = 2;
    float rangeLow = 0.0f;
    float rangeHigh = 1.0f;
    float rangeTotal=1.f;
    float rangeTotalInv=1.f;

    int offsetX, offsetY;
    float barSectionWidth, barSectionHeight;
    bool drawnValues = false;

    std::vector<float> runningMax, runningMin;

};

#endif 
