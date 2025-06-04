#ifndef __VIEW_HPP__
#define __VIEW_HPP__


class ViewBase {

public:
    ViewBase() = delete; // Prevent instantiation of base class
    virtual void Setup() = 0; // Pure virtual function for setup
    virtual void Draw() = 0; // Pure virtual function for drawing
    virtual bool NeedRedraw() const = 0; // Check if redraw is needed
};


#endif // __VIEW_HPP__
