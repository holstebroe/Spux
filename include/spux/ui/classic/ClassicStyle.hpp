// ClassicStyle: the Classic flavor's painters for the standard widgets, as one Style for Gui<Style> and
// Editor<Style> (Gui.hpp). Controls are shaded per pixel at the supersampled resolution of the
// ShadedRenderer (lit from the upper left, soft drop shadows, anti-aliased printed lettering); the look is a
// ClassicConfig (ClassicConfig.hpp), slate() or silver() or a plugin's own.
//
//   spux::ui::classic::ClassicStyle style(spux::ui::classic::ClassicConfig::silver());
//   spux::ui::Gui<spux::ui::classic::ClassicStyle> gui(host, style);
//   Editor<ClassicStyle> editor(host, style, root, options, spux::ui::classic::classicRenderer());
//
// The painters draw on a ShadedRenderer and draw nothing on another renderer. Each paints inside its bounds
// plus margin(): the bounds of a knob must include the room for the printed scale around the dial (10
// pixels beyond the dial's radius, 15 for the knurled knob), so a page lays a knob out as a square a little
// larger than the cap.
//
// Conventions where a painter state has no field for it:
//   - a widget whose accent is the theme's default accent has no color of its own: a knob is bare metal, a
//     slider cap is dark; any other accent colors the knurled knob's cap, the slider cap, the rocker key,
//     the lever's handle and a cap button;
//   - a Panel without a title is the surface (the slate gradient or the worn paint) and takes the whole of
//     its bounds; a titled panel is a frame, an outline with the title cut into its top line;
//   - a button's label may hold a newline for a cap button's two lines; an icon (Icons.hpp) is drawn in
//     the ink of the label, beside it or alone, and replaces the label of a selector's step.
#pragma once

#include <functional>
#include <memory>

#include "spux/ui/Painters.hpp"
#include "spux/ui/classic/ClassicConfig.hpp"
#include "spux/ui/classic/ShadedRenderer.hpp"

namespace spux::inline v1::ui::classic {

// A round lamp, centered on `center`: dark glass with a bezel when unlit, a glowing lens with a halo when lit
// (`level` 0..1 scales the glow).
void drawLed(ShadedRenderer& r, Point center, float radius, Argb color, bool lit, float level = 1.0f);

// A renderer factory for Editor<ClassicStyle>: a ShadedRenderer at the given supersampling.
inline std::function<std::unique_ptr<IRenderer>(int, int)> classicRenderer(int supersample = 2) {
    return [supersample](int w, int h) -> std::unique_ptr<IRenderer> {
        return std::make_unique<ShadedRenderer>(w, h, supersample);
    };
}

namespace detail {
class PainterBase {
public:
    explicit PainterBase(const ClassicConfig& c) : cfg_(&c) {}

protected:
    const ClassicConfig& cfg() const { return *cfg_; }

private:
    const ClassicConfig* cfg_;
};
} // namespace detail

// A disc or knurled knob with a printed scale, a value above or below and a label. The dial is the largest
// circle in the bounds less the rows for the texts and the room for the scale.
class ClassicKnob final : public KnobPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const KnobState& s) const override;
    [[nodiscard]] Rect hitArea(const Rect& bounds) const override;
};

// A fader: a recessed slot with a printed scale and a cap that slides in it (horizontal: the slot lies down).
class ClassicSlider final : public SliderPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const SliderState& s) const override;
    [[nodiscard]] Rect track(const Rect& bounds, const SliderState& s) const override;
    [[nodiscard]] Margin margin() const override { return Margin::all(4); }
};

// A rocker key with a lamp when on, or a lever in a slot (ClassicConfig::toggle). The label goes above the
// control when the bounds have room for it.
class ClassicToggle final : public TogglePainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ToggleState& s) const override;
};

// Vertical: a lever in a slot with the step names beside it. Horizontal: a row of buttons.
class ClassicSelector final : public SelectorPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const SelectorState& s) const override;
    [[nodiscard]] Rect cell(const Rect& b, const SelectorState& s, int i) const override;
};

// See ButtonVariant and StdButtonVariant (arrows, the display button). The shadow reaches outside the bounds
// (margin()).
class ClassicButton final : public ButtonPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ButtonState& s) const override;
    [[nodiscard]] Margin margin() const override { return {3, 3, 5, 6}; }
};

// The surface (no title) or a frame (a title).
class ClassicPanel final : public PanelPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const PanelState& s) const override;
};

// A lamp with its label to the right.
class ClassicLed final : public LedPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const LedState& s) const override;
    [[nodiscard]] Margin margin() const override { return Margin::all(6); }
};

// A piano keyboard: a felt strip, ivory keys and black keys with shadows; held keys are pressed, sounding keys
// take the accent.
class ClassicKeyboard final : public KeyboardPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const KeyboardState& s) const override;
    [[nodiscard]] Margin margin() const override { return {0, 0, 2, 3}; }
};

// A recessed display with the label, the value and a chevron.
class ClassicDropdown final : public DropdownPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const DropdownState& s) const override;
};

// [-] and [+] buttons at the ends of a recessed display.
class ClassicStepper final : public StepperPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const StepperState& s) const override;
};

class ClassicTextField final : public TextFieldPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const TextFieldState& s) const override;
};

// A dark list panel with a soft shadow; the shadow reaches outside the popup (margin()).
class ClassicPopup final : public PopupPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    Rect layout(IRenderer& r, const PopupState& s, int viewW, int viewH, std::vector<Rect>& itemRects) const override;
    void paint(IRenderer& r, const Rect& bounds, const PopupState& s) const override;
    void paintScrollButton(IRenderer& r, const Rect& bounds, bool down, bool hot, const Theme& theme) const override;
    void paintScrollMarker(IRenderer& r, const Rect& track, const Rect& thumb, const Theme& theme) const override;
    [[nodiscard]] Margin margin() const override { return {4, 4, 8, 9}; }
};

class ClassicTooltip final : public TooltipPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    Rect layout(IRenderer& r, const TooltipState& s, int viewW, int viewH) const override;
    void paint(IRenderer& r, const Rect& bounds, const TooltipState& s) const override;
    [[nodiscard]] Margin margin() const override { return {4, 4, 8, 9}; }
};

// A row of tabs: button faces, the selected one lit in the on edge and ink.
class ClassicTabs final : public TabsPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const TabsState& s) const override;
};

// The name and value of the parameter under the pointer in a recessed display, the hint right of it.
class ClassicReadout final : public ReadoutPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ReadoutState& s) const override;
};

struct ClassicStyle {
    explicit ClassicStyle(const ClassicConfig& c = ClassicConfig::slate())
        : theme(c.theme), config_(c), knob(config_), slider(config_), toggle(config_), selector(config_),
          button(config_), led(config_), tabs(config_), readout(config_), keyboard(config_), dropdown(config_),
          stepper(config_), panel(config_), textField(config_), popup(config_), tooltip(config_) {}
    ClassicStyle(const ClassicStyle&) = delete;
    ClassicStyle& operator=(const ClassicStyle&) = delete;

    // Replaces the look. Call it between frames and have the Gui redraw everything afterwards.
    void configure(const ClassicConfig& c) {
        config_ = c;
        theme = c.theme;
    }
    [[nodiscard]] const ClassicConfig& config() const { return config_; }

    Theme theme;

private:
    ClassicConfig config_;

public:
    ClassicKnob knob;
    ClassicSlider slider;
    ClassicToggle toggle;
    ClassicSelector selector;
    ClassicButton button;
    ClassicLed led;
    ClassicTabs tabs;
    ClassicReadout readout;
    ClassicKeyboard keyboard;
    ClassicDropdown dropdown;
    ClassicStepper stepper;
    ClassicPanel panel;
    ClassicTextField textField;
    ClassicPopup popup;
    ClassicTooltip tooltip;
};

} // namespace spux::inline v1::ui::classic
