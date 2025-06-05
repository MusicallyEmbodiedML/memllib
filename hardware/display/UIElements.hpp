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
    virtual void Release() = 0; // Handle release events
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

    static constexpr size_t kBorder = 3;
    static constexpr size_t kTextSize = 1;

    Button(const char* label, uint16_t color, bool is_toggle = false)
        : UIElementBase(label), color_(color),
          is_toggle_(is_toggle), pressed_(false),
          toggleState_(false) {
        // Initialize the label buffer with the provided label
        strncpy(labelBuffer_, label, sizeof(labelBuffer_) - 1);
        labelBuffer_[sizeof(labelBuffer_) - 1] = '\0';
    }

    ~Button() {}

    void Draw() override {
        if (button_) {
            button_->drawSmoothButton(toggleState_, kBorder, TFT_BLACK);
        }
    }

    void OnSetup() override {
        current_x_ = grid_.widthStep * pos_x_;
        current_y_ = grid_.heightStep * pos_y_;
        current_w_ = grid_.widthStep;
        current_h_ = grid_.heightStep;

        button_ = std::make_unique<ButtonWidget>(tft_);
        button_->initButtonUL(current_x_, current_y_, current_w_, current_h_,
                             TFT_WHITE, color_, TFT_BLACK, labelBuffer_, kTextSize);
        button_->drawSmoothButton(pressed_, kBorder, TFT_BLACK);
    }

    void SetCallback(std::function<void(bool)> callback) {
        pressedCallback_ = callback;
    }

    void Interact(size_t x, size_t y) override {
        // Check that the button is initialised
        if (button_) {
            // Check if the interaction is within the button bounds
            if (button_->contains(x, y)) {
                // Sanitise button presses using pressed_ state
                if (!pressed_) {
                    pressed_ = true;
                    if (is_toggle_) {
                        // Toggle the state if it's a toggle button
                        toggleState_ = !toggleState_;
                    } else {
                        // For non-toggle buttons, just set the pressed state
                        toggleState_ = true;  // Set to true to indicate pressed
                    }
                    button_->drawSmoothButton(toggleState_, kBorder, TFT_BLACK);
                    // Call the callback with the toggle state
                    if (pressedCallback_) {
                        pressedCallback_(toggleState_);
                    }
                }
            }
            // Otherwise ignore the interaction
        }
    }

    void Release() override {
        // Reset pressed state and redraw button
        if (button_) {
            pressed_ = false;
            if (!is_toggle_) {
                toggleState_ = false;  // Reset toggle state for non-toggle buttons
            }
            button_->drawSmoothButton(toggleState_, kBorder, TFT_BLACK);
        }
    }

protected:
    uint16_t color_;
    bool is_toggle_;
    std::function<void(bool)> pressedCallback_;  // Changed from bool(bool) to void(bool)
    bool pressed_;
    bool toggleState_{false};  // Track toggle state
    std::unique_ptr<ButtonWidget> button_;
    char labelBuffer_[32];  // Fixed buffer for button label
    uint16_t current_x_{0};
    uint16_t current_y_{0};
    uint16_t current_w_{0};
    uint16_t current_h_{0};
};

#endif  // __UI_ELEMENTS_HPP__
