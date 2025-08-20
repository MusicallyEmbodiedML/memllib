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
        barSectionHeight = area.h - (2 * offsetY);
        rangeTotal = rangeHigh - rangeLow;
        rangeTotalInv = 1.f / rangeTotal;
    }  

    void OnDisplay() override {
    };

    void UpdateValues(const std::vector<float>& values, bool resetMinMax=false) {
        newValues = values;
        if (resetMinMax) {
            runningMax = values;
            runningMin = values;
        }
        redraw();
        // if (drawnValues) {
        //     if (values.size() != newValues.size()) {
        //         // Serial.println("Error: Mismatched size in BarGraphView::UpdateValues");
        //         return;
        //     }
        //     oldValues = newValues;
        //     newValues = values;
        //     drawnValues = false;
        //     redraw();
        // }
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
            //TODO: min/max markers
        }
        oldValues = newValues;  // Update old values after drawing
        drawnValues = true;
    }  



private:
    std::vector<float> newValues, oldValues;
    int colour = TFT_GREEN;
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
