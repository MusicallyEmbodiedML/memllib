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

    Button(const char* label, uint16_t color, bool is_toggle = false);
    ~Button() = default;

    void Draw() override;
    void OnSetup() override;
    void SetCallback(std::function<void(bool)> callback);
    void Interact(size_t x, size_t y) override;
    void Release() override;

protected:
    uint16_t color_;
    bool is_toggle_;
    std::function<void(bool)> pressedCallback_;
    bool pressed_;
    bool toggleState_{false};  // Track toggle state
    std::unique_ptr<ButtonWidget> button_;
    char labelBuffer_[32];  // Fixed buffer for button label
    uint16_t current_x_{0};
    uint16_t current_y_{0};
    uint16_t current_w_{0};
    uint16_t current_h_{0};
};

#if 0
/**
 * @brief Class showing a value (name and value) on the display.
 * It can be incremented or decremented by a step value, e.g. by an
 * encoder or a button.
 *
 * @tparam T
 */
template <typename T>
class Value : public UIElementBase {
public:
    Value(const char* label, T min, T max, T step)
        : UIElementBase(label), min_(min), max_(max), step_(step) {
        snprintf(labelBuffer_, sizeof(labelBuffer_), "%s: %d", label, value_);
    }
    ~Value() = default;

    void Draw() override;
    void OnSetup() override;
    void SetValue(T value);
    T GetValue() const { return value_; }
    void Increment(bool up = true);
    void SetCallback(std::function<void(T)> callback);
    void Interact(size_t x, size_t y) override;
    void Release() override;

protected:

    T value_{0};
    T min_;
    T max_;
    T step_;
    std::function<void(T)> valueChangedCallback_;
    char labelBuffer_[32];  // Fixed buffer for value label
    uint16_t current_x_{0};
    uint16_t current_y_{0};
    uint16_t current_w_{0};
    uint16_t current_h_{0};
};
#endif

#endif  // __UI_ELEMENTS_HPP__
