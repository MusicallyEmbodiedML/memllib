#ifndef __UI_ELEMENTS_HPP__
#define __UI_ELEMENTS_HPP__

#include <TFT_eSPI.h>
#include <TFT_eWidget.h>
#include <functional>
#include <string>


struct GridDef {
    size_t widthElements;
    size_t heightElements;
    size_t widthStep;
    size_t heightStep;
};

class UIElementBase {
public:
    UIElementBase() = delete; // Prevent instantiation of base class
    virtual ~UIElementBase() = default; // Virtual destructor
    void Setup(TFT_eSPI* tft) {  // Changed to non-virtual like ViewBase
        tft_ = tft;
        OnSetup();  // Call virtual setup hook
    }
    virtual void OnSetup() = 0;  // New virtual setup hook
    virtual void Draw() = 0; // No longer needs TFT parameter
    virtual void Interact(size_t x, size_t y) = 0; // Handle interaction events
    void SetGrid(const GridDef &grid, size_t pos_x, size_t pos_y) {
        grid_ = grid;
        pos_x_ = pos_x;
        pos_y_ = pos_y;
    }
protected:
    explicit UIElementBase(const char* label) : label_(label), tft_(nullptr) {}  // Changed to const char*
    GridDef grid_; // Grid definition for layout
    size_t pos_x_{0}; // Position in grid X
    size_t pos_y_{0}; // Position in grid Y
    const char* label_;  // Changed to const char*
    TFT_eSPI* tft_;  // Added tft pointer
};


class Button : public UIElementBase {
public:
    Button(const char* label, uint16_t color, uint16_t alt_color = TFT_BLACK, bool is_toggle = false)
        : UIElementBase(label), color_(color), alt_color_(alt_color),
          is_toggle_(is_toggle), pressed_(false) {
        strncpy(labelBuffer_, label, sizeof(labelBuffer_) - 1);
        labelBuffer_[sizeof(labelBuffer_) - 1] = '\0';
    }

    ~Button() {
        if (button_) {
            activeButtons_.erase(button_.get());
        }
    }

    static void staticPressAction() {
        Serial.println("Button pressed");
        for (const auto& pair : activeButtons_) {
            ButtonWidget* widget = pair.first;
            Button* btn = pair.second;
            if (widget->justPressed()) {
                if (btn->is_toggle_) {
                    btn->pressed_ = !btn->pressed_;
                    btn->button_->initButtonUL(btn->current_x_, btn->current_y_,
                                             btn->current_w_, btn->current_h_,
                                             TFT_WHITE,
                                             btn->pressed_ ? btn->alt_color_ : btn->color_,
                                             TFT_BLACK, btn->labelBuffer_, 1);
                    btn->button_->drawSmoothButton(btn->pressed_, 3, TFT_BLACK);
                    if (btn->pressedCallback_) {
                        btn->pressedCallback_(btn->pressed_);
                    }
                } else {
                    btn->pressed_ = true;
                    btn->button_->initButtonUL(btn->current_x_, btn->current_y_,
                                             btn->current_w_, btn->current_h_,
                                             TFT_WHITE, btn->alt_color_, TFT_BLACK,
                                             btn->labelBuffer_, 1);
                    btn->button_->drawSmoothButton(true, 3, TFT_BLACK);
                    if (btn->pressedCallback_) {
                        btn->pressedCallback_(true);
                    }
                }
                break;
            }
        }
    }

    static void staticReleaseAction() {
        Serial.println("Button released");
        for (const auto& pair : activeButtons_) {
            ButtonWidget* widget = pair.first;
            Button* btn = pair.second;
            if (!btn->is_toggle_ && widget->justReleased()) {
                btn->pressed_ = false;
                btn->button_->initButtonUL(btn->current_x_, btn->current_y_,
                                         btn->current_w_, btn->current_h_,
                                         TFT_WHITE, btn->color_, TFT_BLACK,
                                         btn->labelBuffer_, 1);
                btn->button_->drawSmoothButton(false, 3, TFT_BLACK);
                if (btn->pressedCallback_) {
                    btn->pressedCallback_(false);
                }
            }
        }
    }

    void Draw() override {  // Remove TFT parameter
        if (button_) {
            button_->drawSmoothButton(pressed_);
        }
    }

    void OnSetup() override {
        current_x_ = grid_.widthStep * pos_x_;
        current_y_ = grid_.heightStep * pos_y_;
        current_w_ = grid_.widthStep;
        current_h_ = grid_.heightStep;

        button_ = std::make_unique<ButtonWidget>(tft_);
        button_->initButtonUL(current_x_, current_y_, current_w_, current_h_,
                             TFT_WHITE, color_, TFT_BLACK, labelBuffer_, 1);

        activeButtons_[button_.get()] = this;
        button_->setPressAction(staticPressAction);
        button_->setReleaseAction(staticReleaseAction);
        button_->drawSmoothButton(pressed_, 3, TFT_BLACK);
    }

    void SetCallback(std::function<void(bool)> callback) {
        pressedCallback_ = callback;
    }

    void Interact(size_t x, size_t y) override {
        if (button_) {
            if (button_->contains(x, y)) {
                button_->press(true);
                button_->pressAction();
            } else {
                button_->press(false);
                button_->releaseAction();
            }
        }
    }

protected:
    uint16_t color_;
    uint16_t alt_color_;
    bool is_toggle_;
    std::function<void(bool)> pressedCallback_;  // Changed from bool(bool) to void(bool)
    bool pressed_;
    std::unique_ptr<ButtonWidget> button_;
    char labelBuffer_[32];  // Fixed buffer for button label
    static std::map<ButtonWidget*, Button*> activeButtons_;
    uint16_t current_x_{0};
    uint16_t current_y_{0};
    uint16_t current_w_{0};
    uint16_t current_h_{0};
};

#endif  // __UI_ELEMENTS_HPP__
