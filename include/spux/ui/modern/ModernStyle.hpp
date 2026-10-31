// ModernStyle: the Modern flavor's painters for the standard widgets, as one Style for Gui<Style> and
// Editor<Style> (Gui.hpp). Glass panels, glowing accents, pill-shaped controls, the Inter font at any
// size. Draws through IRenderer only: on the GPU renderer the glow, shadows and bloom show; on the software
// renderer the same layout appears with the effects plain.
//
//   spux::ui::modern::ModernStyle style;                           // the default look
//   spux::ui::modern::ModernStyle style(myConfig);                 // or a ModernConfig of the plugin's own
//   spux::ui::Gui<spux::ui::modern::ModernStyle> gui(host, style);
//
// Each painter is a public member and a class of its own, so a plugin can replace one for a single widget
// call (KnobOptions::painter ...) or derive from it. The style keeps its painters' view of the config:
// it must not be copied or moved while a Gui uses it.
#pragma once

#include <string>

#include "spux/ui/Painters.hpp"
#include "spux/ui/modern/ModernConfig.hpp"

namespace spux::inline v1::ui::modern {

// The variants of ButtonState::variant that ModernStyle draws.
enum ButtonVariant : int {
    kButtonPill = 0, // a pill with a hairline; the accent when on
    kButtonChip = 1  // a lit chip: a halo and a stronger outline when on
};

// `s` shortened with an ellipsis until it is at most maxWidth wide at the given size.
std::string fitText(const IRenderer& r, const std::string& s, float px, float maxWidth,
                    FontWeight weight = FontWeight::Regular);

namespace detail {
class PainterBase {
public:
    explicit PainterBase(const ModernConfig& c) : cfg_(&c) {}

protected:
    const ModernConfig& cfg() const { return *cfg_; }

private:
    const ModernConfig* cfg_;
};
} // namespace detail

class ModernKnob final : public KnobPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const KnobState& s) const override;
    [[nodiscard]] Rect hitArea(const Rect& bounds) const override;
    bool depthHandle(const Rect& bounds, double norm, Point& at) const override;
};

// Label and value beside a track with a round thumb (horizontal), or a vertical track.
class ModernSlider final : public SliderPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const SliderState& s) const override;
    [[nodiscard]] Rect track(const Rect& bounds, const SliderState& s) const override;
};

// A switch with its label to the right.
class ModernToggle final : public TogglePainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ToggleState& s) const override;
};

// Segments in a pill (horizontal) or a column (vertical).
class ModernSelector final : public SelectorPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const SelectorState& s) const override;
};

class ModernButton final : public ButtonPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ButtonState& s) const override;
};

// A lit dot with a halo, the label to its right.
class ModernLed final : public LedPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const LedState& s) const override;
    [[nodiscard]] Margin margin() const override { return Margin::all(10); }
};

// Rounded keys on a dark rail; held keys take the accent, sounding keys glow in it.
class ModernKeyboard final : public KeyboardPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const KeyboardState& s) const override;
};

// Pills in a row, the selected one in the accent.
class ModernTabs final : public TabsPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const TabsState& s) const override;
};

// The name and value of the parameter under the pointer, the hint to the right.
class ModernReadout final : public ReadoutPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const ReadoutState& s) const override;
};

class ModernDropdown final : public DropdownPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const DropdownState& s) const override;
};

class ModernStepper final : public StepperPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const StepperState& s) const override;
};

// A glass panel with a lit dot and a title. Its shadow reaches outside the bounds (margin()).
class ModernPanel final : public PanelPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const PanelState& s) const override;
    [[nodiscard]] Margin margin() const override;
};

class ModernTextField final : public TextFieldPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    void paint(IRenderer& r, const Rect& bounds, const TextFieldState& s) const override;
};

class ModernPopup final : public PopupPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    Rect layout(IRenderer& r, const PopupState& s, int viewW, int viewH, std::vector<Rect>& itemRects) const override;
    void paint(IRenderer& r, const Rect& bounds, const PopupState& s) const override;
    void paintScrollButton(IRenderer& r, const Rect& bounds, bool down, bool hot, const Theme& theme) const override;
    void paintScrollMarker(IRenderer& r, const Rect& track, const Rect& thumb, const Theme& theme) const override;
    [[nodiscard]] Margin margin() const override;
};

class ModernTooltip final : public TooltipPainter, detail::PainterBase {
public:
    using PainterBase::PainterBase;
    Rect layout(IRenderer& r, const TooltipState& s, int viewW, int viewH) const override;
    void paint(IRenderer& r, const Rect& bounds, const TooltipState& s) const override;
    [[nodiscard]] Margin margin() const override;
};

struct ModernStyle {
    explicit ModernStyle(const ModernConfig& c = {})
        : theme(c.theme), config_(c), knob(config_), slider(config_), toggle(config_), selector(config_),
          button(config_), led(config_), tabs(config_), readout(config_), keyboard(config_), dropdown(config_),
          stepper(config_), panel(config_), textField(config_), popup(config_), tooltip(config_) {}
    ModernStyle(const ModernStyle&) = delete;
    ModernStyle& operator=(const ModernStyle&) = delete;

    // Replaces the look. Call it between frames and have the Gui redraw everything afterwards
    // (GuiCore::invalidate, or Editor's next background key).
    void configure(const ModernConfig& c) {
        config_ = c;
        theme = c.theme;
    }
    [[nodiscard]] const ModernConfig& config() const { return config_; }

    Theme theme;

private:
    ModernConfig config_;

public:
    ModernKnob knob;
    ModernSlider slider;
    ModernToggle toggle;
    ModernSelector selector;
    ModernButton button;
    ModernLed led;
    ModernTabs tabs;
    ModernReadout readout;
    ModernKeyboard keyboard;
    ModernDropdown dropdown;
    ModernStepper stepper;
    ModernPanel panel;
    ModernTextField textField;
    ModernPopup popup;
    ModernTooltip tooltip;
};

} // namespace spux::inline v1::ui::modern
