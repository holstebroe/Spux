// Immediate-mode widgets (guidelines 7 "Immediate mode").
//
// The plugin writes page functions that run every frame:
//
//   void filterPage(MyGui& g, const Layout& l) {
//       g.knob(P_cutoff, l.cutoff);
//       if (g.plain(P_mode) == ModeLadder) g.knob(P_drive, l.drive);
//   }
//
// Each widget call reads its value through the EditorHost (spux_params), handles input inside its
// rectangle, asks the style's painter to draw it and on a drag writes the value back with gestures.
// Gui keeps only the hot, active and focused widget ids, drag state and, for the caches, one record
// per widget (area and a hash of its painter state).
//
// Two layers:
//   GuiCore      the interaction, parameter binding, gestures, overlays and caches (compiled once);
//   Gui<Style>   one line per widget: picks the style's painter and calls GuiCore. A Style is any struct
//                with a `theme` and one painter member per widget it supports (`knob`, `slider`,
//                `toggle`, `selector`, `button`, `dropdown`, `stepper`, `panel`, `textField`, and
//                optionally `popup` and `tooltip`). A widget whose painter the style lacks fails to
//                compile where a page calls it, and nowhere else.
//
// Caches (with a renderer that has layers() and caching on): a widget whose painter state hashes the
// same as last frame, in the same place, is not drawn; one that changed is drawn over a restored copy
// of the background under its bounds plus its painter's margin; a widget that disappears is erased.
// So widget areas must not overlap each other: panels and printed labels belong to the background
// (Editor's RootView::background), which is drawn once. overlapCount() reports violations.
//
// Widget ids come from the parameter id (and the widget kind), or from a label inside an id scope
// (pushId) in loops; idCollisions() counts ids used twice in one frame.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "spux/platform/NativeWindow.hpp"
#include "spux/ui/EditorHost.hpp"
#include "spux/ui/IRenderer.hpp"
#include "spux/ui/Layers.hpp"
#include "spux/ui/Painters.hpp"
#include "spux/ui/Theme.hpp"

namespace spux::inline v1::ui {

using platform::KeyEvent;
using platform::MouseButton;

// "No parameter" where a parameter id is expected (0 is a valid id).
inline constexpr std::uint32_t kNoParamId = 0xFFFFFFFFu;

// --- per-widget options --------------------------------------------------------------------------------

struct KnobOptions {
    const char* label = nullptr;     // null: the parameter's name
    const char* valueText = nullptr; // null: the formatted value
    Color accent = 0;                // 0: the current accent (Gui::setAccent)
    bool bipolar = false;
    bool large = false;
    bool showValue = true;
    bool dimmed = false;
    bool valueEntry = true;               // right-click or Enter types a value
    float dragPixels = 0.0f;              // drag for the full range; 0: the theme's
    double wheelStep = -1.0;              // normalized step per notch; < 0: the theme's; 0: no wheel
    float hitRadius = 0.0f;               // > 0: a press must land this close to the center of the bounds
    const KnobPainter* painter = nullptr; // null: the style's
};

struct SliderOptions {
    const char* label = nullptr;
    const char* valueText = nullptr;
    Color accent = 0;
    bool vertical = false;
    bool inverted = false;   // drag and wheel move the other way (a CS-80 paddle grows downwards)
    bool jump = false;       // a press sets the value at the pointer (along the painter's track)
    float dragPixels = 0.0f; // relative drag for the full range; 0: the theme's
    double wheelStep = -1.0;
    int hitPad = 0; // the press area reaches this far outside the bounds
    const SliderPainter* painter = nullptr;
};

struct ToggleOptions {
    const char* label = nullptr;
    Color accent = 0;
    // > 0: a two-position switch. A press more than this many pixels above the center sets off, below
    // the center sets on, inside the band toggles. 0: any press toggles.
    int centerBand = 0;
    bool wheel = true; // a notch up sets on, down sets off
    const TogglePainter* painter = nullptr;
};

struct SelectorOptions {
    const char* const* labels = nullptr; // one per step; null: the parameter's enum labels
    Color accent = 0;
    bool vertical = false;
    int wheel = 0; // steps per notch up (+1, -1), 0: no wheel
    const SelectorPainter* painter = nullptr;
};

struct ButtonOptions {
    bool on = false; // selected (a tab, a lit button)
    Color accent = 0;
    int variant = 0;
    const ButtonPainter* painter = nullptr;
};

// What happened to a custom interactive area this frame (views, handles, keyboards, ribbons).
struct Interaction {
    bool hot = false;      // the pointer is over it
    bool pressed = false;  // the left button went down on it this frame
    bool active = false;   // held since a press on it
    bool released = false; // the button went up after a press on it (this frame)
    bool rightPressed = false;
    bool doubleClick = false; // pressed as the second click of a double click
    bool shift = false;
    Point pointer;              // now
    Point start;                // where the press was
    float dx = 0.0f, dy = 0.0f; // since the press
    float wheel = 0.0f;         // wheel notches over it this frame (positive: away from the user)
};

class GuiCore {
public:
    GuiCore(EditorHost& host, const Theme& theme);
    GuiCore(const GuiCore&) = delete;
    GuiCore& operator=(const GuiCore&) = delete;
    ~GuiCore() = default;

    // --- frame ---------------------------------------------------------------------------------------------
    // begin, the page's widgets, then Gui<Style>::end (overlays, focus, drag end, edit flush).
    void begin(IRenderer& r, double time, float dt, int width, int height);
    // Background mode: draws panels and printed text straight into the renderer's current target (the
    // Editor sets the background layer). Widgets are not allowed in it.
    void beginBackground(IRenderer& r, int width, int height);
    void endBackground();
    [[nodiscard]] IRenderer& r() { return *r_; }
    [[nodiscard]] EditorHost& host() { return host_; }
    [[nodiscard]] const Theme& theme() const { return *theme_; }
    void setTheme(const Theme& t);
    [[nodiscard]] double time() const { return time_; }
    [[nodiscard]] float dt() const { return dt_; }
    [[nodiscard]] int width() const { return w_; }
    [[nodiscard]] int height() const { return h_; }
    // Caches on (the default) use the renderer's layers when it has them.
    void setCaching(bool on) { caching_ = on; }
    [[nodiscard]] bool caching() const { return cache_ != nullptr; }
    // Every widget draws on the next frame (the background was redrawn, the theme changed).
    void invalidate() { forceAll_ = true; }
    // Something the next frame must show without new input (tooltip timer, caret, an open drag).
    [[nodiscard]] bool wantsFrame(double now) const;

    // --- input (logical coordinates), delivered between frames ----------------------------------------------
    void onMouseMove(float x, float y);
    void setShift(bool shift) { shift_ = shift; }
    void onMouseDown(float x, float y, MouseButton b, bool shift);
    void onMouseUp(float x, float y, MouseButton b);
    void onWheel(float x, float y, float steps, bool shift);
    void onLeave();
    void onKey(const KeyEvent& k);
    [[nodiscard]] float mouseX() const { return mx_; }
    [[nodiscard]] float mouseY() const { return my_; }
    [[nodiscard]] bool shift() const { return shift_; }
    [[nodiscard]] bool leftDown() const { return leftDown_; }
    [[nodiscard]] bool hovered(const Rect& r) const;
    // Keys delivered this frame that no widget took (page shortcuts).
    [[nodiscard]] const std::vector<KeyEvent>& keys() const { return keys_; }

    // --- ids ---------------------------------------------------------------------------------------------
    static std::uint64_t hashId(const char* s, std::uint64_t seed = 1469598103934665603ull);
    static std::uint64_t hashId(std::uint64_t v, std::uint64_t seed);
    void pushId(const char* s);
    void pushId(std::uint64_t v);
    void popId();
    [[nodiscard]] std::uint64_t id(const char* s) const { return hashId(s, scope()); }

    // --- shared page state ---------------------------------------------------------------------------------
    void setAccent(Color c) { accent_ = c; }
    [[nodiscard]] Color accent() const { return accent_ ? accent_ : theme_->colors.accent; }
    // The parameter under the pointer this frame and the one being dragged (for readouts); kNoParamId
    // when none.
    [[nodiscard]] std::uint32_t hoveredParam() const { return hoveredParam_; }
    [[nodiscard]] std::uint32_t activeParam() const { return activeParam_; }

    // --- plain widgets -------------------------------------------------------------------------------------
    // A custom view: true when the caller must draw `area` now (its key changed, or no caches); the
    // background under it is restored. Draw through r(), inside `area`.
    bool view(std::uint64_t id, const Rect& area, std::uint64_t key);
    // A custom interactive area; `radius` > 0 makes the hit a circle around the area's center.
    Interaction interact(std::uint64_t id, const Rect& hit, float radius = 0.0f);
    // A draggable round handle whose drag is one gesture of `gestureParam` (kNoParamId: none).
    Interaction handle(const char* id, Point center, float radius, std::uint32_t gestureParam = kNoParamId);
    // Text inside `area` (cached by content). For printed labels use the background.
    void text(const char* s, const Rect& area, Color c, float px, Align a = Align::Left,
              FontWeight w = FontWeight::Regular);
    // Text at a point, drawn every time: background mode or uncached frames only.
    void label(const char* s, float x, float y, Color c, float px = 11.0f, Align a = Align::Left,
               FontWeight w = FontWeight::Regular);
    // Opens a list popup anchored below `anchor` (or at a point when it is empty); `choose` runs with the
    // picked index in the next frame's begin.
    void popup(const char* id, const Rect& anchor, std::vector<PopupItem> items, int selected,
               std::function<void(int)> choose, int columns = 1);
    void popup(const char* id, const Rect& anchor, const std::vector<std::string>& items, int selected,
               std::function<void(int)> choose, int columns = 1);
    void closePopup() { popupOpen_ = false; }
    [[nodiscard]] bool popupOpen() const { return popupOpen_; }

    // --- parameter access (ids) -------------------------------------------------------------------------
    [[nodiscard]] const params::ParamInfo* info(std::uint32_t paramId) const;
    [[nodiscard]] double plain(std::uint32_t paramId) const;
    [[nodiscard]] double norm(std::uint32_t paramId) const;
    void setPlain(std::uint32_t paramId, double plain); // one gesture: begin, value, end
    void setNorm(std::uint32_t paramId, double norm);
    void beginEdit(std::uint32_t paramId);
    void editValue(std::uint32_t paramId, double plain);
    void endEdit(std::uint32_t paramId);
    // The parameter's value as text (EditorHost::formatParam).
    std::string valueText(std::uint32_t paramId);

    // --- checks ------------------------------------------------------------------------------------------
    // Pairs of cached widget areas that overlapped in the last frame, and ids used twice in it.
    [[nodiscard]] int overlapCount() const { return overlaps_; }
    [[nodiscard]] int idCollisions() const { return collisions_; }
    void setOverlapCheck(bool on) { checkOverlaps_ = on; }
    // Widgets drawn and skipped in the last frame.
    [[nodiscard]] int paintedCount() const { return painted_; }
    [[nodiscard]] int skippedCount() const { return skipped_; }

protected:
    enum Kind : std::uint64_t {
        kKnob = 1,
        kSlider,
        kToggle,
        kSelector,
        kDropdown,
        kStepper,
        kButton,
        kText,
        kPanel,
        kTextField
    };

    void knobImpl(std::uint32_t paramId, const Rect& b, const KnobOptions& o, const KnobPainter& p);
    void sliderImpl(std::uint32_t paramId, const Rect& b, const SliderOptions& o, const SliderPainter& p);
    bool toggleImpl(std::uint32_t paramId, const Rect& b, const ToggleOptions& o, const TogglePainter& p);
    void selectorImpl(std::uint32_t paramId, const Rect& b, const SelectorOptions& o, const SelectorPainter& p);
    void dropdownImpl(std::uint32_t paramId, const Rect& b, const char* label, const DropdownPainter& p);
    void stepperImpl(std::uint32_t paramId, const Rect& b, const char* label, const StepperPainter& p);
    bool buttonImpl(const char* id, const Rect& b, const char* label, const ButtonOptions& o, const ButtonPainter& p);
    void panelImpl(const Rect& b, const char* title, Color accent, const PanelPainter& p);
    enum class TextResult { Editing, Commit, Cancel };
    TextResult textFieldImpl(const char* id, const Rect& b, std::string& text, std::size_t maxBytes,
                             const TextFieldPainter& p);
    void endImpl(const PopupPainter* popup, const TooltipPainter* tooltip);

private:
    struct Record {
        std::uint64_t id = 0;
        Rect area;
        std::uint64_t key = 0;
        std::uint32_t frame = 0;
    };
    struct Popup {
        std::uint64_t id = 0;
        Rect anchor, rect;
        std::vector<PopupItem> items;
        std::vector<Rect> itemRects;
        int selected = -1, columns = 1, hot = -1;
        std::function<void(int)> choose;
    };

    template <class P, class S>
    bool paintWidget(std::uint64_t id, const Rect& b, const P& painter, const S& state, std::uint64_t key);
    bool beginPaint(std::uint64_t id, const Rect& area, std::uint64_t key);
    [[nodiscard]] std::uint64_t scope() const { return ids_.empty() ? 1469598103934665603ull : ids_.back(); }
    [[nodiscard]] std::uint64_t widgetId(Kind k, std::uint32_t paramId) const {
        return hashId(static_cast<std::uint64_t>(paramId) << 4 | k, scope());
    }
    [[nodiscard]] std::size_t indexOf(std::uint32_t paramId) const;
    [[nodiscard]] double plainAt(std::size_t index) const;
    [[nodiscard]] double normAt(std::size_t index) const;
    void setPlainAt(std::size_t index, double plain);
    void beginAt(std::size_t index);
    void valueAt(std::size_t index, double plain);
    void endAt(std::size_t index);
    void flushEdits();
    [[nodiscard]] bool pressedIn(const Rect& r, MouseButton b = MouseButton::Left) const;
    [[nodiscard]] bool pressedInCircle(Point c, float radius) const;
    bool takeDoubleClick(std::uint64_t id);
    void registerFocus(std::uint64_t id) { focusables_.push_back(id); }
    [[nodiscard]] bool focused(std::uint64_t id) const { return focus_ == id; }
    [[nodiscard]] bool keyPressed(KeyEvent::Key k) const;
    void tooltipFor(const Rect& r, std::uint32_t paramId);
    const char* formatAt(std::size_t index, double plain);
    double wheelNormStep(double optionStep) const;
    void markSeen(std::uint64_t id);

    EditorHost& host_;
    const Theme* theme_;
    IRenderer* r_ = nullptr;
    LayerCache* cache_ = nullptr;
    bool caching_ = true, forceAll_ = true, background_ = false;
    double time_ = 0.0;
    float dt_ = 0.0f;
    int w_ = 0, h_ = 0;
    Color accent_ = 0;
    std::vector<std::uint64_t> ids_;

    // input
    float mx_ = -1, my_ = -1;
    bool leftDown_ = false, rightDown_ = false, shift_ = false;
    bool downEvent_ = false, upEvent_ = false, rightDownEvent_ = false, rightUpEvent_ = false;
    float downX_ = 0, downY_ = 0;
    float wheel_ = 0.0f;
    std::vector<KeyEvent> keys_;
    double lastClickTime_ = -10.0;
    std::uint64_t lastClickId_ = 0;
    bool blocked_ = false; // a popup is open: widgets below ignore the mouse

    // widget state
    std::uint64_t active_ = 0, focus_ = 0, editing_ = 0;
    std::uint64_t hoverId_ = 0;
    double hoverSince_ = 0.0;
    double dragStart_ = 0.0;
    float dragStartX_ = 0.0f, dragStartY_ = 0.0f;
    bool dragShift_ = false;
    std::uint32_t activeParam_ = kNoParamId, hoveredParam_ = kNoParamId;
    std::vector<std::uint64_t> focusables_;
    std::string editText_;
    Popup popup_;
    bool popupOpen_ = false;
    std::uint32_t tooltipParam_ = kNoParamId;
    bool tooltipShown_ = false, hasTooltips_ = false;
    char text_[96] = {};

    // edits
    bool editsQueued_ = false;
    std::vector<bool> gestureOpen_;          // by table index: the begin reached the queue
    std::vector<std::uint8_t> gestureNest_;  // begins nested in an open gesture, whose ends are not sent
    std::vector<std::uint32_t> pendingEnds_; // gesture ends that did not fit, retried at every flush

    // caches
    // One per widget drawn last frame, in drawing order: a page draws in the same order every frame, so
    // the next record is almost always the one wanted (found at `cursor_`).
    std::vector<Record> records_;
    std::size_t cursor_ = 0;
    std::vector<Rect> overlayRects_, frameAreas_;
    std::vector<std::uint64_t> frameIds_;
    std::uint32_t frame_ = 0;
    bool checkOverlaps_ = false;
    int overlaps_ = 0, collisions_ = 0, painted_ = 0, skipped_ = 0;
};

template <class Style> class Gui : public GuiCore {
public:
    Gui(EditorHost& host, const Style& style) : GuiCore(host, style.theme), style_(style) {}
    [[nodiscard]] const Style& style() const { return style_; }

    // --- widgets bound to parameters ---
    void knob(std::uint32_t paramId, const Rect& b, const KnobOptions& o = {}) {
        knobImpl(paramId, b, o, o.painter ? *o.painter : static_cast<const KnobPainter&>(style_.knob));
    }
    void slider(std::uint32_t paramId, const Rect& b, const SliderOptions& o = {}) {
        sliderImpl(paramId, b, o, o.painter ? *o.painter : static_cast<const SliderPainter&>(style_.slider));
    }
    // True when the press changed it.
    bool toggle(std::uint32_t paramId, const Rect& b, const ToggleOptions& o = {}) {
        return toggleImpl(paramId, b, o, o.painter ? *o.painter : static_cast<const TogglePainter&>(style_.toggle));
    }
    void selector(std::uint32_t paramId, const Rect& b, const SelectorOptions& o = {}) {
        selectorImpl(paramId, b, o, o.painter ? *o.painter : static_cast<const SelectorPainter&>(style_.selector));
    }
    // A button that opens a list of the values.
    void dropdown(std::uint32_t paramId, const Rect& b, const char* label = nullptr) {
        dropdownImpl(paramId, b, label, static_cast<const DropdownPainter&>(style_.dropdown));
    }
    // [-] value [+].
    void stepper(std::uint32_t paramId, const Rect& b, const char* label = nullptr) {
        stepperImpl(paramId, b, label, static_cast<const StepperPainter&>(style_.stepper));
    }
    // The widget that fits the parameter's kind: toggle, dropdown or knob.
    void param(std::uint32_t paramId, const Rect& b, const char* label = nullptr) {
        const params::ParamInfo* p = info(paramId);
        if (!p)
            return;
        if (p->kind == params::ParamKind::Bool)
            toggle(paramId, b, {.label = label});
        else if (p->kind == params::ParamKind::Enum)
            dropdown(paramId, b, label);
        else
            knob(paramId, b, {.label = label});
    }

    // --- plain widgets ---
    // True when pressed this frame (or Enter while focused).
    bool button(const char* id, const Rect& b, const char* label, const ButtonOptions& o = {}) {
        return buttonImpl(id, b, label, o, o.painter ? *o.painter : static_cast<const ButtonPainter&>(style_.button));
    }
    void panel(const Rect& b, const char* title, Color accent = 0) {
        panelImpl(b, title, accent, static_cast<const PanelPainter&>(style_.panel));
    }
    // A single-line text field that owns the keyboard while it is drawn (modal name entry).
    using GuiCore::TextResult;
    TextResult textField(const char* id, const Rect& b, std::string& text, std::size_t maxBytes = 60) {
        return textFieldImpl(id, b, text, maxBytes, static_cast<const TextFieldPainter&>(style_.textField));
    }

    // Overlays (popups and tooltips, when the style has their painters), focus, drag end, edit flush.
    void end() { endImpl(popupPainter(), tooltipPainter()); }

private:
    const PopupPainter* popupPainter() const {
        if constexpr (requires { style_.popup; })
            return &static_cast<const PopupPainter&>(style_.popup);
        else
            return nullptr;
    }
    const TooltipPainter* tooltipPainter() const {
        if constexpr (requires { style_.tooltip; })
            return &static_cast<const TooltipPainter&>(style_.tooltip);
        else
            return nullptr;
    }

    const Style& style_;
};

} // namespace spux::inline v1::ui
