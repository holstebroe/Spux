// Acceptance gate (spux DESIGN_GUIDELINES 7): the Tannhauser panel on spux_ui_core, in place of
// Tannhauser's retained src/gui/GuiWindow.cpp. The page loops over Tannhauser's own layout table
// (PanelLayout, unchanged: "a layout table can stay as data that a page loops over") and draws with its
// own panel::drawControl. Sections, in the order the gate counts them:
//
//   binding    EditorHost over TannhauserClap's parameter calls (goes away with spux_params)
//   painters   panel::drawControl behind spux painter interfaces, one set per control
//   page       the widgets, the readout and the plugin's own views (tone buttons, ribbon, keyboard)
//   window     the GuiWindow calls TannhauserClap makes, the CLAP gui extension
#include "gui/GuiWindow.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "Graphics.hpp"
#include "GraphicsRenderer.hpp"
#include "PanelRenderer.hpp"
#include "clap/TannhauserClap.hpp"
#include "core/Params.hpp"
#include "presets/Presets.hpp"
#include "spux/ui/Editor.hpp"

namespace tannhauser {

using namespace spux::ui;
namespace params = spux::params;

namespace {

// The renderer: Tannhauser's Graphics at 2x, as its window draws. Text through IRenderer is not used.
struct NoFont {
    int getTextWidth(const char*, int) const { return 0; }
    uint32_t getHeight() const { return 0; }
};
class Renderer final : public IRenderer {
public:
    Renderer(int w, int h) : layers_(w, h, 2) {}
    Graphics graphics() {
        return Graphics(layers_.target(), static_cast<uint32_t>(layers_.width()),
                        static_cast<uint32_t>(layers_.height()), 2);
    }
    int width() const override { return layers_.width(); }
    int height() const override { return layers_.height(); }
    LayerCache* layers() override { return &layers_; }
    void clear(Color c) override { graphics().clear(c); }
    void pushClip(const Rect&) override {}
    void popClip() override {}
    void fillRect(const Rect& r, Color c) override { graphics().fillRect(r.x, r.y, r.w, r.h, c); }
    void strokeRect(const Rect& r, Color c, float) override { graphics().drawRect(r.x, r.y, r.w, r.h, c); }
    void fillRoundRect(const Rect& r, float, Color c) override { fillRect(r, c); }
    void fillGradientRect(const Rect& r, Color c, Color) override { fillRect(r, c); }
    void drawLine(Point a, Point b, Color c, float) override {
        graphics().drawLine(static_cast<int>(a.x), static_cast<int>(a.y), static_cast<int>(b.x), static_cast<int>(b.y),
                            c, 1);
    }
    void drawPolyline(const Point* p, int n, Color c, float t) override {
        for (int i = 0; i + 1 < n; ++i)
            drawLine(p[i], p[i + 1], c, t);
    }
    void fillCircle(Point p, float r, Color c) override {
        graphics().fillCircle(static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(r), c);
    }
    void strokeCircle(Point p, float r, Color c, float) override {
        graphics().drawCircle(static_cast<int>(p.x), static_cast<int>(p.y), static_cast<int>(r), c, 1);
    }
    void drawText(const char*, int, int, Color, int) override {}
    int textWidth(const char*, int) const override { return 0; }
    int textHeight(int) const override { return 0; }

private:
    CpuLayers layers_;
};

// --- binding -----------------------------------------------------------------------------------------------

class TannhauserHost final : public EditorHost {
public:
    explicit TannhauserHost(TannhauserClap* p) : plugin_(p) {
        for (uint32_t id = 0; id < PARAM_COUNT; ++id) {
            const ParamInfo& t = paramInfo(id);
            params::ParamInfo pi;
            pi.id = id;
            pi.key = t.key;
            pi.name = t.name;
            pi.kind = (t.flags & PF_STEPPED) ? params::ParamKind::Int : params::ParamKind::Float;
            pi.min = t.min;
            pi.max = t.max;
            pi.def = t.def;
            pi.flags = params::PF_HOST_PLAIN;
            infos_[id] = pi;
        }
        table_ = params::ParamTable{infos_};
    }
    const params::ParamTable& paramTable() const override { return table_; }
    double paramHost(std::size_t i) const override { return plugin_->paramValue(static_cast<clap_id>(i)); }
    void setParamHost(std::size_t i, double v) override { plugin_->onParamValueFromGui(static_cast<clap_id>(i), v); }
    std::uint32_t paramVersion() const override { // TannhauserClap has no version counter: hash the values
        std::uint64_t h = 1469598103934665603ull;
        for (uint32_t i = 0; i < PARAM_COUNT; ++i) {
            const double v = plugin_->paramValue(i);
            std::uint64_t b;
            std::memcpy(&b, &v, sizeof b);
            h = (h ^ b) * 1099511628211ull;
        }
        return static_cast<std::uint32_t>(h ^ (h >> 32));
    }
    bool pushEdit(const params::ParamEdit& e, std::size_t) override {
        if (e.kind == params::ParamEdit::Kind::Begin)
            plugin_->onBeginEditFromGui(e.index);
        else if (e.kind == params::ParamEdit::Kind::End)
            plugin_->onEndEditFromGui(e.index);
        return true; // values went through setParamHost
    }
    void requestFlush() override {}
    void formatParam(std::size_t i, double v, char* buf, std::size_t n) override {
        plugin_->paramsValueToText(static_cast<clap_id>(i), v, buf, static_cast<uint32_t>(n));
    }

private:
    TannhauserClap* plugin_;
    params::ParamInfo infos_[PARAM_COUNT];
    params::ParamTable table_;
};

// --- painters ----------------------------------------------------------------------------------------------

Margin marginOf(CtlType t) {
    switch (t) {
    case CtlType::Slider:
    case CtlType::Knob:
        return Margin::all(8);
    case CtlType::Paddle:
        return Margin::xy(4, 8);
    case CtlType::Lever:
        return Margin::all(3);
    case CtlType::ToneButton:
        return Margin::all(1);
    case CtlType::Keyboard:
        return {};
    default:
        return Margin::all(2);
    }
}

template <class Base> struct CtlPainter final : Base {
    const Ctl* c = nullptr;
    Margin margin() const override { return marginOf(c->type); }
    void paint(IRenderer& r, const Rect&, const typename Base::StateType& s) const override {
        Graphics g = static_cast<Renderer&>(r).graphics();
        panel::drawControl(g, *c, state(s));
    }
    static CtlState state(const SliderState& s) { return {s.value, false, s.hot, s.active}; }
    static CtlState state(const KnobState& s) { return {s.value, false, s.hot, s.active}; }
    static CtlState state(const ToggleState& s) { return {0.0, s.on, s.hot, false}; }
    static CtlState state(const SelectorState& s) {
        CtlState t{0.0, false, s.hot, false};
        t.step = s.index;
        return t;
    }
    static CtlState state(const ButtonState& s) {
        CtlState t{0.0, s.on, s.hot, s.active};
        t.hasMemory = s.variant != 0;
        return t;
    }
};

// A lever's step under a press, as the retained panel computes it: (y - top - 4) / ((h - 8) / steps).
struct LeverPainter final : SelectorPainter {
    const Ctl* c = nullptr;
    Margin margin() const override { return marginOf(c->type); }
    Rect cell(const Rect& b, const SelectorState& s, int i) const override {
        const double q = (b.h - 8.0) / std::max(1, s.count);
        const int y0 = i == 0 ? b.y : b.y + static_cast<int>(std::ceil(4.0 + i * q));
        const int y1 = i == s.count - 1 ? b.bottom() : b.y + static_cast<int>(std::ceil(4.0 + (i + 1) * q));
        return {b.x, y0, b.w, y1 - y0};
    }
    void paint(IRenderer& r, const Rect&, const SelectorState& s) const override {
        Graphics g = static_cast<Renderer&>(r).graphics();
        CtlState t{0.0, false, s.hot, false};
        t.step = s.index;
        panel::drawControl(g, *c, t);
    }
};

struct Painters {
    CtlPainter<SliderPainter> slider;
    CtlPainter<KnobPainter> knob;
    CtlPainter<TogglePainter> toggle;
    CtlPainter<ButtonPainter> button;
    LeverPainter lever;
};

struct Style {
    Theme theme;
    CtlPainter<KnobPainter> knob; // the style's defaults are never used: every control has its own
    CtlPainter<SliderPainter> slider;
    CtlPainter<TogglePainter> toggle;
    LeverPainter selector;
    CtlPainter<ButtonPainter> button;
};

// --- page --------------------------------------------------------------------------------------------------

using G = Gui<Style>;

Rect rectOf(const Ctl& c) { return {c.x, c.y, c.w, c.h}; }
bool isBlackKey(int k) {
    const int n = k % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}

class Panel final : public RootView<G> {
public:
    Panel(TannhauserClap* p, Renderer*& r) : plugin_(p), r_(r), L_(panelLayout()), painters_(L_.controls.size()) {
        for (std::size_t i = 0; i < L_.controls.size(); ++i) {
            const Ctl* c = &L_.controls[i];
            painters_[i].slider.c = painters_[i].knob.c = painters_[i].toggle.c = painters_[i].button.c =
                painters_[i].lever.c = c;
        }
    }

    void background(G&) override {
        Graphics g = r_->graphics();
        panel::drawStatic(g, L_);
    }

    void frame(G& g) override {
        active_ = -1;
        for (std::size_t i = 0; i < L_.controls.size(); ++i) {
            const Ctl& c = L_.controls[i];
            const Painters& p = painters_[i];
            g.pushId(static_cast<std::uint64_t>(i));
            switch (c.type) {
            case CtlType::Slider:
            case CtlType::Paddle:
                g.slider(c.param, rectOf(c),
                         {.vertical = true,
                          .inverted = c.type == CtlType::Paddle,
                          .dragPixels = c.h - 14.0f,
                          .hitPad = 3,
                          .painter = &p.slider});
                break;
            case CtlType::Knob:
                g.knob(c.param, rectOf(c), {.dragPixels = 150.0f, .painter = &p.knob});
                break;
            case CtlType::Rocker:
            case CtlType::Toggle:
                g.toggle(c.param, rectOf(c), {.painter = &p.toggle});
                break;
            case CtlType::Lever:
                g.selector(c.param, rectOf(c), {.vertical = true, .wheel = -1, .painter = &p.lever});
                break;
            case CtlType::ToneButton:
                toneButton(g, c);
                break;
            case CtlType::PresetPrev:
            case CtlType::PresetNext:
                if (g.button("step", rectOf(c), "", {.painter = &p.button})) {
                    plugin_->stepPreset(c.type == CtlType::PresetNext ? 1 : -1);
                    lit_[0] = lit_[1] = 13;
                }
                if (g.leftDown() && rectOf(c).contains(Point{g.mouseX(), g.mouseY()}))
                    active_ = static_cast<int>(i);
                break;
            case CtlType::PresetLcd:
                lcd(g, c);
                break;
            case CtlType::Ribbon:
                ribbon(g, c, static_cast<int>(i));
                break;
            case CtlType::Keyboard:
                keyboard(g, c, static_cast<int>(i));
                break;
            }
            if (c.param >= 0 && g.activeParam() == static_cast<std::uint32_t>(c.param))
                active_ = static_cast<int>(i);
            g.popId();
        }
        readout(g);
    }

    std::uint64_t contentVersion() override { // what the panel shows besides the parameters
        std::uint64_t h = 1469598103934665603ull;
        auto mixIn = [&h](std::uint64_t v) { h = (h ^ v) * 1099511628211ull; };
        for (int k = 0; k < 128; ++k)
            mixIn(plugin_->isKeyDown(k) ? static_cast<std::uint64_t>(k) + 1 : 0);
        for (int m = 0; m < 4; ++m)
            mixIn(plugin_->hasMemory(m));
        mixIn(static_cast<std::uint64_t>(plugin_->currentPresetIndex()));
        mixIn(plugin_->isPresetModified());
        mixIn(statusShown());
        return h;
    }

    void showStatus(const std::string& text) {
        status_ = text;
        statusUntil_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    }
    const std::string& readoutText() const { return readout_; }

private:
    bool statusShown() const { return !status_.empty() && std::chrono::steady_clock::now() < statusUntil_; }

    // Tone selector: factory tones load into a line, M1..M4 recall (shift: store), PANEL keeps the sliders.
    // A view keyed on what the button shows (its shadow reaches past its 1 px margin, so it redraws only
    // when it must).
    void toneButton(G& g, const Ctl& c) {
        const int row = c.aux / 16, b = c.aux % 16;
        const bool memory = b == 11 || b == 12;
        const int slot = (b - 11) * 2 + row;
        const Interaction in = g.interact(1, rectOf(c));
        if (in.pressed) {
            if (b < 11) {
                plugin_->loadFactoryTone(row, b, row);
                lit_[row] = b;
            } else if (memory) {
                if (in.shift) {
                    plugin_->storeMemory(slot);
                    showStatus("STORED MEMORY " + std::to_string(slot + 1));
                } else if (plugin_->hasMemory(slot)) {
                    plugin_->recallMemory(slot);
                    lit_[row] = b;
                } else {
                    showStatus("MEMORY EMPTY: SHIFT-CLICK TO STORE");
                }
            } else {
                lit_[row] = 13;
            }
        }
        CtlState s;
        s.on = lit_[row] == b;
        s.hover = in.hot;
        s.hasMemory = memory && plugin_->hasMemory(slot);
        if (g.view(2, rectOf(c).expand(marginOf(c.type)),
                   (s.on ? 1u : 0u) | (s.hover ? 2u : 0u) | (s.hasMemory ? 4u : 0u))) {
            Graphics gr = r_->graphics();
            panel::drawControl(gr, c, s);
        }
    }

    // The preset LCD: the name (or a status message); the wheel steps through the presets.
    void lcd(G& g, const Ctl& c) {
        const Interaction in = g.interact(1, rectOf(c));
        if (in.wheel != 0.0f)
            plugin_->stepPreset(in.wheel > 0 ? -1 : 1);
        CtlState s;
        s.hover = in.hot;
        s.text = statusShown() ? status_ : plugin_->currentPresetName();
        s.modified = !statusShown() && plugin_->isPresetModified();
        if (g.view(2, rectOf(c).expand(marginOf(c.type)),
                   std::hash<std::string>{}(s.text) ^ (s.modified ? 1u : 0u) ^ (s.hover ? 2u : 0u))) {
            Graphics gr = r_->graphics();
            panel::drawControl(gr, c, s);
        }
    }

    // Ribbon: bends relative to the first touch, the whole width is one octave each way.
    void ribbon(G& g, const Ctl& c, int index) {
        const Interaction in = g.interact(1, rectOf(c));
        const double width = c.w;
        if (in.pressed) {
            g.beginEdit(c.param);
            touch_ = (in.start.x - c.x) / width;
            g.setPlain(P_RIBBON_TOUCH, 1.0); // the engine bends the sounding notes relative to here
            g.editValue(c.param, 0.0);
        }
        if (in.active) {
            touch_ = std::clamp((static_cast<double>(in.pointer.x) - c.x) / width, 0.0, 1.0);
            g.editValue(c.param, std::clamp(2.0 * in.dx / width, -1.0, 1.0));
            active_ = index;
        }
        if (in.released) { // release the touch before the ribbon returns to 0, so held bends stay
            g.setPlain(P_RIBBON_TOUCH, 0.0);
            g.editValue(c.param, 0.0);
            g.endEdit(c.param);
            touch_ = -1.0;
        }
        CtlState s;
        s.hover = in.hot;
        s.active = in.active;
        s.ribbonTouch = touch_;
        s.ribbonValue = g.plain(c.param);
        std::uint64_t key;
        std::memcpy(&key, &s.ribbonValue, sizeof key);
        if (g.view(2, rectOf(c).expand(marginOf(c.type)),
                   key ^ std::hash<double>{}(touch_) ^ (s.hover ? 1u : 0u) ^ (s.active ? 2u : 0u))) {
            Graphics gr = r_->graphics();
            panel::drawControl(gr, c, s);
        }
    }

    int keyAt(const Ctl& kb, float x, float y) const {
        if (!rectOf(kb).contains(Point{x, y}))
            return -1;
        int whites = 0;
        for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k)
            whites += isBlackKey(k) ? 0 : 1;
        const double ww = static_cast<double>(kb.w) / whites;
        const int xi = static_cast<int>(x), yi = static_cast<int>(y);
        if (yi < kb.y + kb.h * 0.62) { // black keys first (they sit on top)
            double xx = kb.x;
            for (int k = PanelLayout::kKeyboardFirst; k <= PanelLayout::kKeyboardLast; ++k) {
                if (!isBlackKey(k)) {
                    xx += ww;
                    continue;
                }
                if (std::fabs(xi - xx) < ww * 0.31)
                    return k;
            }
        }
        const int wi = static_cast<int>((xi - kb.x) / ww);
        for (int k = PanelLayout::kKeyboardFirst, n = 0; k <= PanelLayout::kKeyboardLast; ++k) {
            if (isBlackKey(k))
                continue;
            if (n++ == wi)
                return k;
        }
        return -1;
    }

    // Keyboard: click to play, lower on the key is harder; dragging slides to the next key.
    void keyboard(G& g, const Ctl& c, int index) {
        const Interaction in = g.interact(1, rectOf(c));
        auto velocity = [&](float y) {
            return 0.25 + 0.75 * std::clamp((static_cast<int>(y) - c.y) / static_cast<double>(c.h), 0.0, 1.0);
        };
        if (in.pressed) {
            playing_ = keyAt(c, in.start.x, in.start.y);
            if (playing_ >= 0)
                plugin_->guiNote(playing_, velocity(in.start.y), true);
        } else if (in.active) {
            const int k = keyAt(c, in.pointer.x, in.pointer.y);
            if (k >= 0 && k != playing_) {
                if (playing_ >= 0)
                    plugin_->guiNote(playing_, 0.0, false);
                plugin_->guiNote(k, velocity(in.pointer.y), true);
                playing_ = k;
            }
        }
        if (in.released && playing_ >= 0) {
            plugin_->guiNote(playing_, 0.0, false);
            playing_ = -1;
        }
        if (in.active || in.pressed)
            active_ = index;
        bool keys[128];
        std::uint64_t key = 1469598103934665603ull;
        for (int k = 0; k < 128; ++k) {
            keys[k] = plugin_->isKeyDown(k);
            key = (key ^ (keys[k] ? static_cast<std::uint64_t>(k) + 1 : 0)) * 1099511628211ull;
        }
        if (g.view(2, rectOf(c), key)) {
            CtlState s;
            s.keys = keys;
            Graphics gr = r_->graphics();
            panel::drawControl(gr, c, s);
        }
    }

    // The header readout: the dragged or hovered control's name, value and description.
    void readout(G& g) {
        int shown = active_;
        if (shown < 0 && !g.leftDown())
            for (std::size_t i = 0; i < L_.controls.size() && shown < 0; ++i) {
                const Ctl& c = L_.controls[i];
                const int pad = (c.type == CtlType::Slider || c.type == CtlType::Paddle) ? 3 : 0;
                if (rectOf(c).inset(-pad).contains(Point{g.mouseX(), g.mouseY()}))
                    shown = static_cast<int>(i);
            }
        if (!g.leftDown())
            hover_ = shown;
        else if (active_ < 0)
            shown = hover_; // as before the press, while the button is down elsewhere
        readout_.clear();
        if (shown >= 0) {
            const Ctl& c = L_.controls[static_cast<std::size_t>(shown)];
            if (c.param >= 0) {
                readout_ = std::string(paramInfo(static_cast<uint32_t>(c.param)).name) + ":  " +
                           g.valueText(static_cast<uint32_t>(c.param));
                const char* desc = paramDescription(static_cast<uint32_t>(c.param));
                if (desc && *desc)
                    readout_ += std::string("\n") + desc;
            } else if (c.type == CtlType::ToneButton) {
                const int row = c.aux / 16, b = c.aux % 16;
                if (b < 11)
                    readout_ = std::string("LOAD FACTORY TONE INTO LINE ") + (row == 0 ? "I: " : "II: ") +
                               kFactoryToneNames[row][b];
                else if (b < 13)
                    readout_ = std::string("MEMORY ") + std::to_string((b - 11) * 2 + row + 1) +
                               "  (SHIFT-CLICK STORES LINE " + (row == 0 ? "I)" : "II)");
                else
                    readout_ = "PANEL: THE SLIDERS ARE ALWAYS LIVE";
            } else if (c.type == CtlType::PresetLcd) {
                readout_ = "CLICK FOR THE PRESET MENU, WHEEL TO STEP";
            } else if (c.type == CtlType::Keyboard) {
                readout_ = "CLICK TO PLAY (LOWER = HARDER)";
            }
            for (auto& ch : readout_)
                if (ch >= 'a' && ch <= 'z')
                    ch = static_cast<char>(ch - 'a' + 'A');
        }
        int x, y, w, h;
        panel::readoutBounds(x, y, w, h);
        if (g.view(3, {x, y, w, h}, std::hash<std::string>{}(readout_))) {
            Graphics gr = r_->graphics();
            panel::drawReadout(gr, readout_);
        }
    }

    TannhauserClap* plugin_;
    Renderer*& r_;
    const PanelLayout& L_;
    std::vector<Painters> painters_;
    int lit_[2] = {13, 13};
    int active_ = -1, hover_ = -1, playing_ = -1;
    double touch_ = -1.0;
    std::string readout_, status_;
    std::chrono::steady_clock::time_point statusUntil_{};
};

Theme tannhauserTheme() {
    Theme t;
    t.colors.background = 0xFF1B1C1E;
    t.feel.fine = 0.2f; // shift: a fifth of the travel
    t.feel.keyStep = 0.02;
    return t;
}

} // namespace

// --- window ------------------------------------------------------------------------------------------------

struct GuiWindow::Impl {
    explicit Impl(TannhauserClap* p)
        : plugin(p), host(p), panel(p, renderer), editor(host, style, panel, options(), [this](int w, int h) {
              auto r = std::make_unique<Renderer>(w, h);
              renderer = r.get();
              return r;
          }) {
        style.theme = tannhauserTheme();
    }
    static EditorOptions options() {
        EditorOptions o;
        o.width = static_cast<int>(kDefaultWidth);
        o.height = static_cast<int>(kDefaultHeight);
        o.caching = std::getenv("SPUX_GATE_NO_CACHE") == nullptr; // a full redraw every frame, to check the caches
        return o;
    }
    TannhauserClap* plugin;
    Renderer* renderer = nullptr;
    Style style;
    TannhauserHost host;
    Panel panel;
    Editor<Style> editor;
    mutable std::vector<uint32_t> pixels;
};

GuiWindow::GuiWindow(TannhauserClap* plugin) : impl_(std::make_unique<Impl>(plugin)) {}
GuiWindow::~GuiWindow() { destroy(); }

bool GuiWindow::setParent(const clap_window_t* window) {
    if (!window || !window->api)
        return false;
#if defined(__linux__) && !defined(__APPLE__)
    if (std::strcmp(window->api, CLAP_WINDOW_API_X11) == 0)
        return impl_->editor.setParent({spux::platform::WindowApi::X11, static_cast<std::uintptr_t>(window->x11)});
#elif defined(_WIN32)
    if (std::strcmp(window->api, CLAP_WINDOW_API_WIN32) == 0)
        return impl_->editor.setParent(
            {spux::platform::WindowApi::Win32, reinterpret_cast<std::uintptr_t>(window->win32)});
#endif
    return false;
}
bool GuiWindow::setSize(uint32_t w, uint32_t h) {
    return impl_->editor.setSize(static_cast<int>(w), static_cast<int>(h));
}
bool GuiWindow::show() {
    impl_->editor.show();
    return true;
}
bool GuiWindow::hide() {
    impl_->editor.hide();
    return true;
}
void GuiWindow::destroy() {
    if (impl_)
        impl_->editor.destroy();
}
void GuiWindow::renderFrame() { impl_->editor.frame(); }
const std::vector<uint32_t>& GuiWindow::getPixelBuffer() const {
    impl_->pixels.assign(impl_->editor.pixels(), impl_->editor.pixels() + kDefaultWidth * kDefaultHeight);
    return impl_->pixels;
}
void GuiWindow::handleMouseDown(int x, int y, bool shift) {
    impl_->editor.mouseDown(static_cast<float>(x), static_cast<float>(y), MouseButton::Left, shift);
}
void GuiWindow::handleMouseDrag(int x, int y, bool shift) {
    impl_->editor.mouseMove(static_cast<float>(x), static_cast<float>(y), shift);
}
void GuiWindow::handleMouseUp() {
    auto& g = impl_->editor.gui();
    impl_->editor.mouseUp(g.mouseX(), g.mouseY(), MouseButton::Left);
}
void GuiWindow::handleMouseMove(int x, int y) { impl_->editor.mouseMove(static_cast<float>(x), static_cast<float>(y)); }
void GuiWindow::handleWheel(int x, int y, int delta) {
    impl_->editor.mouseWheel(static_cast<float>(x), static_cast<float>(y), static_cast<float>(delta), false);
}
void GuiWindow::handleKey(int) {}
std::string GuiWindow::readoutText() const { return impl_->panel.readoutText(); }
void GuiWindow::showStatus(const std::string& text) { impl_->panel.showStatus(text); }

// CLAP GUI extension: as in Tannhauser's GuiWindow.cpp.
static TannhauserClap* selfOf(const clap_plugin_t* p) { return static_cast<TannhauserClap*>(p->plugin_data); }

const clap_plugin_gui_t g_tannhauserGuiExtension = {
    [](const clap_plugin_t*, const char* api, bool floating) -> bool {
#if defined(__linux__) && !defined(__APPLE__)
        return api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0 && !floating;
#elif defined(_WIN32)
        return api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0 && !floating;
#else
        (void)api;
        (void)floating;
        return false;
#endif
    },
    [](const clap_plugin_t*, const char** api, bool* floating) -> bool {
        if (!api || !floating)
            return false;
#if defined(__linux__) && !defined(__APPLE__)
        *api = CLAP_WINDOW_API_X11;
#elif defined(_WIN32)
        *api = CLAP_WINDOW_API_WIN32;
#else
        return false;
#endif
        *floating = false;
        return true;
    },
    [](const clap_plugin_t* p, const char*, bool) -> bool {
        try {
            selfOf(p)->createGuiWindow();
        } catch (...) {
            return false;
        }
        return true;
    },
    [](const clap_plugin_t* p) {
        try {
            selfOf(p)->destroyGuiWindow();
        } catch (...) {
        }
    },
    [](const clap_plugin_t*, double) -> bool { return false; },
    [](const clap_plugin_t*, uint32_t* w, uint32_t* h) -> bool {
        if (!w || !h)
            return false;
        *w = GuiWindow::kDefaultWidth;
        *h = GuiWindow::kDefaultHeight;
        return true;
    },
    [](const clap_plugin_t*) -> bool { return false; },
    [](const clap_plugin_t*, clap_gui_resize_hints_t*) -> bool { return false; },
    [](const clap_plugin_t*, uint32_t* w, uint32_t* h) -> bool {
        if (!w || !h)
            return false;
        *w = GuiWindow::kDefaultWidth;
        *h = GuiWindow::kDefaultHeight;
        return true;
    },
    [](const clap_plugin_t* p, uint32_t w, uint32_t h) -> bool {
        try {
            auto* g = selfOf(p)->getGuiWindow();
            return g ? g->setSize(w, h) : true;
        } catch (...) {
            return false;
        }
    },
    [](const clap_plugin_t* p, const clap_window_t* window) -> bool {
        if (!window || !window->api)
            return false;
        try {
            if (!selfOf(p)->getGuiWindow())
                selfOf(p)->createGuiWindow();
            return selfOf(p)->getGuiWindow()->setParent(window);
        } catch (...) {
            return false;
        }
    },
    [](const clap_plugin_t*, const clap_window_t*) -> bool { return false; },
    [](const clap_plugin_t*, const char*) {},
    [](const clap_plugin_t* p) -> bool {
        try {
            auto* g = selfOf(p)->getGuiWindow();
            return g ? g->show() : false;
        } catch (...) {
            return false;
        }
    },
    [](const clap_plugin_t* p) -> bool {
        try {
            auto* g = selfOf(p)->getGuiWindow();
            return g ? g->hide() : false;
        } catch (...) {
            return false;
        }
    },
};

} // namespace tannhauser
