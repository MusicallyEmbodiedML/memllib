#include "View.hpp"

void ViewBase::Setup(TFT_eSPI* tft) {
    tft_ = tft;
    needRedraw_ = true;
    OnSetup();
}

bool ViewBase::NeedRedraw() {
    bool ret = needRedraw_;
    needRedraw_ = false;
    return ret;
}
