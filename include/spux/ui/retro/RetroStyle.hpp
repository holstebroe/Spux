// RetroStyle: the Retro flavor's painters for the standard widgets, as one Style for Gui<Style> and Editor<Style>
// (Gui.hpp). Whole pixels, bitmap text, flat colors and one-pixel bevels; the look is a RetroConfig
// (RetroConfig.hpp), silver(), tracker(), forge() or a plugin's own.
//
//   spux::ui::retro::RetroStyle style(spux::ui::retro::RetroConfig::silver());
//   spux::ui::Gui<spux::ui::retro::RetroStyle> gui(host, style);
//   Editor<RetroStyle> editor(host, style, root, options, spux::ui::retro::retroRenderer(style.config()));
//
// The painters draw on a PixelRenderer and draw nothing on another renderer. Each paints inside its bounds plus
// margin(). Layout follows the other flavors, so a page moves between them: a knob is a square with a row above
// and below it (RetroConfig::Knob) and 9 pixels around the dial for the printed scale, a vertical fader has a row
// for its value above and its label below, a lever wants about 18 x 42 pixels and a label row.
//
// Conventions where a painter state has no field for it:
//   - a widget whose accent is the theme's default accent has no color of its own: a knob has no colored cap, a
//     fader cap and a selector's lever are the button face; any other accent colors them;
//   - a Panel without a title is the surface and takes the whole of its bounds; a titled one is a frame;
//   - a button's label may hold a newline for a cap button's two lines; an icon (Icons.hpp) is drawn in the ink of
//     the label, beside it or alone, and replaces the label of a selector's step;
//   - on is the lever down, as in the other flavors.
#pragma once

#include <functional>
#include <memory>

#include "spux/ui/Painters.hpp"
#include "spux/ui/retro/PixelRenderer.hpp"
#include "spux/ui/retro/RetroConfig.hpp"

namespace spux::inline v1::ui::retro {

// Values of ButtonState::variant that RetroStyle draws (the arrows and the display button are core variants,
// StdButtonVariant in Painters.hpp).
enum ButtonVariant : int {
    kButtonPlain = 0, // a bevelled key; sunken while pressed, the on face when on
    kButtonCap = 1    // a colored cap (the accent), dim when off, lit when on; a label with a newline is two lines
};

// A renderer factory for Editor<RetroStyle>: a PixelRenderer with the config's fonts (the bold font is the title
// font). It reads the config each time it makes a renderer, so after RetroStyle::configure() the renderers made
// next (a resize, a new editor) have the new fonts. The config and its fonts must outlive the factory.
inline std::function<std::unique_ptr<IRenderer>(int, int)> retroRenderer(const RetroConfig& c) {
    const RetroConfig* cfg = &c;
    return [cfg](int w, int h) -> std::unique_ptr<IRenderer> {
        return cfg->font ? std::make_unique<PixelRenderer>(w, h, *cfg->font, cfg->titleFont)
                         : std::make_unique<PixelRenderer>(w, h);
    };
}

namespace detail {
class PainterBase {
public:
    explicit PainterBase(const RetroConfig& c) : cfg_(&c) {}

protected:
    const RetroConfig& cfg() const { return *cfg_; }

private:
    const RetroConfig* cfg_;
};
} // namespace detail

// A fluted knob with a printed scale (or a flat disc), the label in one row and the value in the other.
class RetroKnob final : public KnobPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const KnobState& s) const override;
    [[nodiscard]] Rect hitArea(const Rect& bounds) const override;
};

// A fader: a recessed slot with a printed scale and a cap that slides in it (horizontal: the slot lies down).
class RetroSlider final : public SliderPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const SliderState& s) const override;
    [[nodiscard]] Rect track(const Rect& bounds, const SliderState& s) const override;
};

// A lever in a slot, or a bevelled key with a lamp (RetroConfig::toggle). The label goes above the lever when the
// bounds have room for it, inside the key.
class RetroToggle final : public TogglePainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ToggleState& s) const override;
};

// Vertical: a lever in a slot with the step names beside it. Horizontal: a row of keys.
class RetroSelector final : public SelectorPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const SelectorState& s) const override;
    [[nodiscard]] Rect cell(const Rect& b, const SelectorState& s, int i) const override;
};

// See ButtonVariant and StdButtonVariant.
class RetroButton final : public ButtonPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ButtonState& s) const override;
};

// The surface (no title) or a frame (a title).
class RetroPanel final : public PanelPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const PanelState& s) const override;
};

// A lamp with its label to the right.
class RetroLed final : public LedPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const LedState& s) const override;
};

// A piano keyboard: a felt strip, white keys and black keys; held keys are dark, sounding keys take the accent.
class RetroKeyboard final : public KeyboardPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const KeyboardState& s) const override;
};

// A recessed display with the label, the value and a chevron.
class RetroDropdown final : public DropdownPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const DropdownState& s) const override;
};

// [-] and [+] keys at the ends of a recessed display.
class RetroStepper final : public StepperPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const StepperState& s) const override;
};

class RetroTextField final : public TextFieldPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const TextFieldState& s) const override;
};

// A list with a one-pixel edge and a two-pixel shadow (margin()).
class RetroPopup final : public PopupPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    Rect layout(IRenderer& r, const PopupState& s, int viewW, int viewH, std::vector<Rect>& itemRects) const override;
    void paint(IRenderer& r, const Rect& bounds, const PopupState& s) const override;
    void paintScrollButton(IRenderer& r, const Rect& bounds, bool down, bool hot, const Theme& theme) const override;
    void paintScrollMarker(IRenderer& r, const Rect& track, const Rect& thumb, const Theme& theme) const override;
    [[nodiscard]] Margin margin() const override { return {0, 0, 2, 2}; }
};

class RetroTooltip final : public TooltipPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    Rect layout(IRenderer& r, const TooltipState& s, int viewW, int viewH) const override;
    void paint(IRenderer& r, const Rect& bounds, const TooltipState& s) const override;
    [[nodiscard]] Margin margin() const override { return {0, 0, 2, 2}; }
};

// A row of tabs: key faces, the selected one on.
class RetroTabs final : public TabsPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const TabsState& s) const override;
};

// The name and value of the parameter under the pointer in a recessed display, the hint right of it.
class RetroReadout final : public ReadoutPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ReadoutState& s) const override;
};

struct RetroStyle {
    explicit RetroStyle(const RetroConfig& c = RetroConfig::silver())
        : theme(c.theme), config_(c), knob(config_), slider(config_), toggle(config_), selector(config_),
          button(config_), led(config_), tabs(config_), readout(config_), keyboard(config_), dropdown(config_),
          stepper(config_), panel(config_), textField(config_), popup(config_), tooltip(config_) {}
    RetroStyle(const RetroStyle&) = delete;
    RetroStyle& operator=(const RetroStyle&) = delete;

    // Replaces the look. Call it between frames, have the Gui redraw everything afterwards, and recreate the editor's
    // renderer (retroRenderer) so its fonts match; a renderer made before keeps the fonts it was made with.
    void configure(const RetroConfig& c) {
        config_ = c;
        theme = c.theme;
    }
    [[nodiscard]] const RetroConfig& config() const { return config_; }

    Theme theme;

private:
    RetroConfig config_;

public:
    RetroKnob knob;
    RetroSlider slider;
    RetroToggle toggle;
    RetroSelector selector;
    RetroButton button;
    RetroLed led;
    RetroTabs tabs;
    RetroReadout readout;
    RetroKeyboard keyboard;
    RetroDropdown dropdown;
    RetroStepper stepper;
    RetroPanel panel;
    RetroTextField textField;
    RetroPopup popup;
    RetroTooltip tooltip;
};

} // namespace spux::inline v1::ui::retro
