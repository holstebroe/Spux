// Painter interfaces, one per widget type (guidelines 7 "Painters and themes").
//
// Gui does the interaction; a painter only draws. It receives the renderer, the widget's bounds and a
// state struct (value, hot, active, focused, modulation, label, ...) and draws inside the bounds plus
// its declared margin(), so a widget can be redrawn alone over the cached background. A painter may
// also answer geometry questions the interaction needs (where a knob's depth handle sits, which cell of
// a selector a point is in); the defaults use the bounds.
//
// A flavor implements the painters it supports and lists them in its Style (Gui.hpp); a plugin can
// supply its own painter per widget call or for a whole widget type.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "spux/params/ParamInfo.hpp"
#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Theme.hpp"

namespace spux::inline v1::ui {

// What every widget state has.
struct WidgetState {
    bool hot = false;      // the pointer is over it (and nothing blocks it)
    bool active = false;   // being dragged or pressed
    bool focused = false;  // has the keyboard focus (draw a focus ring)
    bool disabled = false; // shown, but takes no input
    const Theme* theme = nullptr;
};

// The live modulation of a parameter, from the editor host's modulation extension (EditorHost.hpp).
// Values are normalized positions.
struct ModulationView {
    bool any = false;      // some route reaches this parameter
    double base = 0.0;     // the parameter's own value
    double shared = 0.0;   // after the shared (not per-voice) sources
    double voices[4] = {}; // per sounding voice, up to four
    int voiceCount = 0;
    bool armed = false;        // a source is armed: the knob shows its depth handle
    bool armedRoute = false;   // the armed source already reaches this parameter
    double armedDepth = 0.0;   // extent of the armed source's route (signed, normalized)
    bool overHandle = false;   // the pointer is over the depth handle
    bool handleActive = false; // the depth handle is being dragged
};

struct KnobState : WidgetState {
    const params::ParamInfo* param = nullptr;
    std::uint32_t paramId = 0;
    double value = 0.0; // normalized position
    double plain = 0.0;
    const char* label = "";     // short label (the parameter's name unless the page gives one)
    const char* valueText = ""; // the value with its unit, or the page's text
    bool editing = false;       // typed value entry: show editText instead of the value
    const char* editText = "";
    bool caret = false;   // the entry caret is visible this frame
    bool bipolar = false; // the arc starts at the center
    bool large = false;
    bool dimmed = false; // shown but overridden (a tempo-synced rate)
    bool showValue = true;
    Color accent = 0;
    ModulationView mod;
};

struct SliderState : WidgetState {
    const params::ParamInfo* param = nullptr;
    std::uint32_t paramId = 0;
    double value = 0.0; // normalized position
    double plain = 0.0;
    const char* label = "";
    const char* valueText = "";
    bool vertical = false;
    Color accent = 0;
};

struct ToggleState : WidgetState {
    const params::ParamInfo* param = nullptr;
    std::uint32_t paramId = 0;
    bool on = false;
    const char* label = "";
    Color accent = 0;
};

// A choice among a few steps: segments in a row, a lever's positions, a rotary switch.
struct SelectorState : WidgetState {
    const params::ParamInfo* param = nullptr;
    std::uint32_t paramId = 0;
    int count = 0;
    int index = 0;                       // the current step
    int hotIndex = -1;                   // the step under the pointer
    const char* const* labels = nullptr; // count labels, or null: the parameter's enum labels
    bool vertical = false;
    Color accent = 0;
};

struct ButtonState : WidgetState {
    const char* label = "";
    bool on = false; // selected (a tab, a lit button)
    Color accent = 0;
    int variant = 0; // a painter's own variants (chip, arrow, ...)
};

struct DropdownState : WidgetState {
    const params::ParamInfo* param = nullptr;
    const char* label = "";
    const char* valueText = "";
    bool open = false;
};

struct StepperState : WidgetState {
    const params::ParamInfo* param = nullptr;
    const char* label = "";
    const char* valueText = "";
    int hotPart = 0; // -1 over the minus part, +1 over the plus part
};

struct PanelState {
    const char* title = "";
    Color accent = 0;
    const Theme* theme = nullptr;
};

struct TextFieldState : WidgetState {
    const char* text = "";
    bool caret = false;
};

struct PopupItem {
    std::string label;
    bool separator = false;
    bool enabled = true;
};

struct PopupState {
    const std::vector<PopupItem>* items = nullptr;
    Rect anchor;       // the widget it belongs to, or a point (w = h = 0)
    int selected = -1; // marked as the current choice
    int hot = -1;      // under the pointer
    int columns = 1;
    const std::vector<Rect>* itemRects = nullptr; // from layout()
    const Theme* theme = nullptr;
};

struct TooltipState {
    const char* title = "";
    const char* text = "";
    Point pointer;
    const Theme* theme = nullptr;
};

template <class State> class Painter {
public:
    using StateType = State;
    virtual ~Painter() = default;
    virtual void paint(IRenderer& r, const Rect& bounds, const State& s) const = 0;
    // How far outside the bounds this painter draws (shadows, glow, focus ring).
    [[nodiscard]] virtual Margin margin() const { return {}; }
};

class KnobPainter : public Painter<KnobState> {
public:
    // The part of the bounds that takes a press (the dial).
    [[nodiscard]] virtual Rect hitArea(const Rect& bounds) const { return bounds; }
    // Where the modulation depth handle is drawn for a normalized position; false: no depth handle.
    virtual bool depthHandle(const Rect& /*bounds*/, double /*norm*/, Point& /*at*/) const { return false; }
};

class SliderPainter : public Painter<SliderState> {
public:
    // The track a press maps onto (horizontal or vertical); the default is the bounds.
    [[nodiscard]] virtual Rect track(const Rect& bounds, const SliderState&) const { return bounds; }
};

class TogglePainter : public Painter<ToggleState> {};

class SelectorPainter : public Painter<SelectorState> {
public:
    // The part of the bounds that picks step i; the default divides the bounds evenly along the axis.
    [[nodiscard]] virtual Rect cell(const Rect& b, const SelectorState& s, int i) const {
        const int n = s.count > 0 ? s.count : 1;
        if (s.vertical) {
            const int y0 = b.y + b.h * i / n, y1 = b.y + b.h * (i + 1) / n;
            return {b.x, y0, b.w, y1 - y0};
        }
        const int x0 = b.x + b.w * i / n, x1 = b.x + b.w * (i + 1) / n;
        return {x0, b.y, x1 - x0, b.h};
    }
};

class ButtonPainter : public Painter<ButtonState> {};
class DropdownPainter : public Painter<DropdownState> {};

class StepperPainter : public Painter<StepperState> {
public:
    // Square parts at the ends, at most half the width each.
    [[nodiscard]] virtual Rect minusPart(const Rect& b) const {
        const int s = b.h < b.w / 2 ? b.h : b.w / 2;
        return {b.x, b.y, s, b.h};
    }
    [[nodiscard]] virtual Rect plusPart(const Rect& b) const {
        const int s = b.h < b.w / 2 ? b.h : b.w / 2;
        return {b.right() - s, b.y, s, b.h};
    }
};

class PanelPainter : public Painter<PanelState> {};
class TextFieldPainter : public Painter<TextFieldState> {};

class PopupPainter {
public:
    virtual ~PopupPainter() = default;
    // Places the items (one rectangle each, separators included) inside a view of viewW x viewH and
    // returns the popup's rectangle.
    virtual Rect layout(IRenderer& r, const PopupState& s, int viewW, int viewH,
                        std::vector<Rect>& itemRects) const = 0;
    virtual void paint(IRenderer& r, const Rect& bounds, const PopupState& s) const = 0;
    // Room outside the popup's rectangle (its shadow).
    [[nodiscard]] virtual Margin margin() const { return {}; }
};

class TooltipPainter {
public:
    virtual ~TooltipPainter() = default;
    virtual Rect layout(IRenderer& r, const TooltipState& s, int viewW, int viewH) const = 0;
    virtual void paint(IRenderer& r, const Rect& bounds, const TooltipState& s) const = 0;
    [[nodiscard]] virtual Margin margin() const { return {}; }
};

} // namespace spux::inline v1::ui
