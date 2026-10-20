// A plain Style for the spux_ui_core tests: every painter draws boxes inside its bounds, with colours
// that tell its state apart (hot, active, focused, value). Translucent fills make a double draw visible.
#pragma once

#include <algorithm>
#include <cmath>

#include "spux/ui/Painters.hpp"

namespace uitest {

using namespace spux::ui;

inline Color stateColor(const WidgetState& s, Color base) {
    return s.active ? rgb(250, 60, 60) : s.hot ? rgb(60, 250, 60) : s.focused ? rgb(60, 60, 250) : base;
}

struct TestKnob final : KnobPainter {
    void paint(IRenderer& r, const Rect& b, const KnobState& s) const override {
        r.pushClip(b);
        r.fillRect(b, withAlpha(stateColor(s, rgb(90, 90, 90)), 0.5f));
        r.fillRect({b.x, b.y, static_cast<int>(std::lround(b.w * s.value)), 4}, rgb(255, 200, 0));
        r.drawText(s.editing ? s.editText : s.valueText, b.x, b.bottom() - 8, rgb(250, 250, 250), 1);
        Point h;
        if (depthHandle(b, s.mod.base + (s.mod.armedRoute ? s.mod.armedDepth : 0.0), h))
            r.fillCircle(h, 3.0f, s.mod.overHandle ? rgb(255, 255, 255) : rgb(200, 100, 255));
        r.popClip();
    }
    bool depthHandle(const Rect& b, double norm, Point& at) const override {
        at = {static_cast<float>(b.x) + 6.0f +
                  static_cast<float>(b.w - 12) * static_cast<float>(std::clamp(norm, 0.0, 1.0)),
              static_cast<float>(b.y) + 10.0f};
        return true;
    }
};

struct TestSlider final : SliderPainter {
    void paint(IRenderer& r, const Rect& b, const SliderState& s) const override {
        r.fillRect(b, withAlpha(stateColor(s, rgb(80, 80, 120)), 0.5f));
        const int len = static_cast<int>(std::lround((s.vertical ? b.h : b.w) * s.value));
        r.fillRect(s.vertical ? Rect{b.x, b.bottom() - len, b.w, len} : Rect{b.x, b.y, len, b.h}, rgb(0, 200, 255));
    }
};

struct TestToggle final : TogglePainter {
    void paint(IRenderer& r, const Rect& b, const ToggleState& s) const override {
        r.fillRect(b, withAlpha(stateColor(s, s.on ? rgb(250, 250, 0) : rgb(40, 40, 40)), 0.5f));
    }
};

struct TestSelector final : SelectorPainter {
    void paint(IRenderer& r, const Rect& b, const SelectorState& s) const override {
        for (int i = 0; i < s.count; ++i)
            r.fillRect(cell(b, s, i).inset(1), i == s.index      ? rgb(255, 120, 0)
                                               : i == s.hotIndex ? rgb(120, 120, 0)
                                                                 : rgb(30, 30, 30));
    }
};

struct TestButton final : ButtonPainter {
    void paint(IRenderer& r, const Rect& b, const ButtonState& s) const override {
        r.fillRect(b, withAlpha(stateColor(s, s.on ? rgb(0, 160, 0) : rgb(70, 70, 70)), 0.5f));
        r.pushClip(b);
        r.drawText(s.label, b.x + 2, b.y + 2, rgb(255, 255, 255), 1);
        r.popClip();
    }
};

struct TestDropdown final : DropdownPainter {
    void paint(IRenderer& r, const Rect& b, const DropdownState& s) const override {
        r.fillRect(b, withAlpha(stateColor(s, s.open ? rgb(0, 0, 200) : rgb(60, 60, 60)), 0.5f));
        r.pushClip(b);
        r.drawText(s.valueText, b.x + 2, b.y + 2, rgb(255, 255, 255), 1);
        r.popClip();
    }
};

struct TestStepper final : StepperPainter {
    void paint(IRenderer& r, const Rect& b, const StepperState& s) const override {
        r.fillRect(b, withAlpha(stateColor(s, rgb(50, 50, 80)), 0.5f));
        r.fillRect(minusPart(b).inset(2), s.hotPart < 0 ? rgb(200, 200, 200) : rgb(100, 100, 100));
        r.fillRect(plusPart(b).inset(2), s.hotPart > 0 ? rgb(200, 200, 200) : rgb(100, 100, 100));
    }
};

struct TestPanel final : PanelPainter {
    void paint(IRenderer& r, const Rect& b, const PanelState&) const override { r.fillRect(b, rgb(20, 30, 40)); }
};

struct TestTextField final : TextFieldPainter {
    void paint(IRenderer& r, const Rect& b, const TextFieldState& s) const override {
        r.fillRect(b, rgb(10, 10, 10));
        r.pushClip(b);
        r.drawText(s.text, b.x + 2, b.y + 2, rgb(255, 255, 255), 1);
        r.popClip();
    }
};

struct TestPopup final : PopupPainter {
    static constexpr int kRow = 12;
    Rect layout(IRenderer&, const PopupState& s, int viewW, int viewH, std::vector<Rect>& rects) const override {
        const int n = static_cast<int>(s.items->size());
        const int w = std::max(s.anchor.w, 60), h = n * kRow + 4;
        int x = s.anchor.x, y = s.anchor.bottom() + 2;
        if (y + h > viewH)
            y = std::max(0, s.anchor.y - h - 2);
        if (x + w > viewW)
            x = std::max(0, viewW - w);
        rects.clear();
        for (int i = 0; i < n; ++i)
            rects.push_back({x + 2, y + 2 + i * kRow, w - 4, kRow});
        return {x, y, w, h};
    }
    void paint(IRenderer& r, const Rect& b, const PopupState& s) const override {
        r.fillRect(b, rgb(0, 0, 0));
        for (std::size_t i = 0; i < s.itemRects->size(); ++i)
            r.fillRect((*s.itemRects)[i].inset(1), static_cast<int>(i) == s.hot ? rgb(0, 255, 255) : rgb(0, 80, 80));
    }
};

struct TestTooltip final : TooltipPainter {
    Rect layout(IRenderer&, const TooltipState& s, int viewW, int viewH) const override {
        return Rect{static_cast<int>(s.pointer.x) + 8, static_cast<int>(s.pointer.y) + 8, 80, 20}.intersect(
            {0, 0, viewW, viewH});
    }
    void paint(IRenderer& r, const Rect& b, const TooltipState&) const override { r.fillRect(b, rgb(255, 255, 200)); }
};

struct TestStyle {
    Theme theme;
    TestKnob knob;
    TestSlider slider;
    TestToggle toggle;
    TestSelector selector;
    TestButton button;
    TestDropdown dropdown;
    TestStepper stepper;
    TestPanel panel;
    TestTextField textField;
    TestPopup popup;
    TestTooltip tooltip;
};

// A style with knobs only: no popups or tooltips (Gui<KnobsOnly>::end compiles without them).
struct KnobsOnly {
    Theme theme;
    TestKnob knob;
};

} // namespace uitest
