// Acceptance gate (spux DESIGN_GUIDELINES 7): the Acidus panel on spux_ui_core, in place of Acidus's
// retained src/gui/GuiWindow.cpp. The sections, in the order the gate counts them:
//
//   binding    EditorHost over AcidusClap's parameter calls (goes away when Acidus uses spux_params)
//   painters   the widgets' look: Acidus's own skins (IGuiSkin) behind spux painter interfaces
//   page       the layout and the widgets: what the gate compares with the retained version
//   window     the GuiWindow calls AcidusClap makes, the file dialog, the CLAP gui extension
#include "gui/GuiWindow.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "FileDialog.hpp"
#include "Font.hpp"
#include "Graphics.hpp"
#include "GraphicsRenderer.hpp"
#include "IGuiSkin.hpp"
#include "LogoPlate.hpp"
#include "Utf8Path.hpp"
#include "clap/AcidusClap.hpp"
#include "core/CalibrationPresets.hpp"
#include "spux/ui/Editor.hpp"
#include "spux/ui/Layout.hpp"
#if defined(ACIDUS_GUI_STYLE_MODERN)
#include "modern/LabelFont.hpp"
#include "modern/ModernDraw.hpp"
#endif

namespace acidus {

using namespace spux::ui;
namespace params = spux::params;
using Renderer = gate::GraphicsRenderer<Graphics, Font>;

namespace {

// --- binding -----------------------------------------------------------------------------------------------

class AcidusHost final : public EditorHost {
public:
    explicit AcidusHost(AcidusClap* p) : plugin_(p) {
        names_.reserve(plugin_ ? plugin_->paramsCount() : 0); // c_str() must stay put
        for (uint32_t i = 0; plugin_ && i < plugin_->paramsCount(); ++i) {
            clap_param_info_t ci{};
            if (!plugin_->paramsInfo(i, &ci))
                continue;
            params::ParamInfo pi;
            pi.id = ci.id;
            pi.key = names_.emplace_back(ci.name).c_str();
            pi.name = pi.key;
            pi.kind = (ci.flags & CLAP_PARAM_IS_STEPPED) ? params::ParamKind::Int : params::ParamKind::Float;
            pi.min = ci.min_value;
            pi.max = ci.max_value;
            pi.def = ci.default_value;
            pi.flags = params::PF_HOST_PLAIN;
            infos_.push_back(pi);
        }
        std::sort(infos_.begin(), infos_.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        table_ = params::ParamTable{infos_};
    }
    const params::ParamTable& paramTable() const override { return table_; }
    double paramHost(std::size_t i) const override {
        double v = 0.0;
        plugin_->paramsValue(infos_[i].id, &v);
        return v;
    }
    void setParamHost(std::size_t i, double v) override { plugin_->onParamValueFromGui(infos_[i].id, v); }
    std::uint32_t paramVersion() const override { // AcidusClap has no version counter: hash the values
        std::uint64_t h = 1469598103934665603ull;
        for (std::size_t i = 0; i < infos_.size(); ++i) {
            const double v = paramHost(i);
            std::uint64_t b;
            std::memcpy(&b, &v, sizeof b);
            h = (h ^ b) * 1099511628211ull;
        }
        return static_cast<std::uint32_t>(h ^ (h >> 32));
    }
    bool pushEdit(const params::ParamEdit& e, std::size_t) override {
        if (e.kind == params::ParamEdit::Kind::Begin)
            plugin_->onBeginEditFromGui(infos_[e.index].id);
        else if (e.kind == params::ParamEdit::Kind::End)
            plugin_->onEndEditFromGui(infos_[e.index].id);
        return true; // values went through setParamHost
    }
    void requestFlush() override {}

private:
    AcidusClap* plugin_;
    std::vector<std::string> names_;
    std::vector<params::ParamInfo> infos_;
    params::ParamTable table_;
};

// --- painters ----------------------------------------------------------------------------------------------

struct SkinKnob final : KnobPainter {
    IGuiSkin* skin = nullptr;
    const Font* font = nullptr;
    void paint(IRenderer& r, const Rect& b, const KnobState& s) const override {
        Graphics g = static_cast<Renderer&>(r).graphics();
        const Control c{static_cast<int>(s.paramId),
                        "",
                        ControlType::Knob,
                        b.x + b.w / 2,
                        b.y + b.h / 2,
                        20,
                        s.param->min,
                        s.param->max,
                        s.plain,
                        false,
                        s.accent};
        skin->drawKnob(g, c, *font);
    }
    Margin margin() const override { return {2, 6, 2, 6}; }
};

struct SkinSwitch final : TogglePainter {
    IGuiSkin* skin = nullptr;
    const Font* font = nullptr;
    void paint(IRenderer& r, const Rect& b, const ToggleState& s) const override {
        Graphics g = static_cast<Renderer&>(r).graphics();
        const Control c{static_cast<int>(s.paramId),
                        "",
                        ControlType::ToggleSwitch,
                        b.x + b.w / 2,
                        b.y + b.h / 2,
                        17,
                        0.0,
                        1.0,
                        s.on ? 1.0 : 0.0,
                        true};
        skin->drawToggleSwitch(g, c, *font);
    }
    Margin margin() const override { return {10, 0, 10, 0}; }
};

// The calibration menu as Acidus draws it: 13 px rows, 5 px separators, opened at the pointer.
struct Menu final : PopupPainter {
    const Font* font = nullptr;
    static constexpr int kItemH = 13, kSeparatorH = 5;
    Rect layout(IRenderer&, const PopupState& s, int viewW, int viewH, std::vector<Rect>& rects) const override {
        int textW = 0, h = 6;
        for (const PopupItem& it : *s.items) {
            textW = std::max(textW, font->getTextWidth(it.label.c_str(), 1));
            h += it.separator ? kSeparatorH : kItemH;
        }
        const int w = textW + 16;
        const int x = std::max(2, std::min(s.anchor.x, viewW - w - 3)),
                  y = std::max(2, std::min(s.anchor.y, viewH - h - 3));
        rects.clear();
        int top = y + 3;
        for (const PopupItem& it : *s.items) {
            const int ih = it.separator ? kSeparatorH : kItemH;
            rects.push_back({x, top, w, ih});
            top += ih;
        }
        return {x, y, w, h};
    }
    // --- drawing (not counted: Acidus's drawMenu, as it was) ---
    void paint(IRenderer& r, const Rect& b, const PopupState& s) const override {
        Graphics g = static_cast<Renderer&>(r).graphics();
        g.fillRect(b.x + 3, b.y + 4, b.w, b.h, 0x60000000);
        g.fillRect(b.x, b.y, b.w, b.h, 0xFF131517);
        g.drawRect(b.x, b.y, b.w, b.h, 0xFF4A4E52);
        for (std::size_t i = 0; i < s.items->size(); ++i) {
            const Rect& c = (*s.itemRects)[i];
            if ((*s.items)[i].separator) {
                g.drawLine(b.x + 4, c.y + 2, b.x + b.w - 5, c.y + 2, 0xFF2C3030, 1);
                continue;
            }
            const bool hover = static_cast<int>(i) == s.hot;
            if (hover)
                g.fillRect(b.x + 2, c.y, b.w - 4, kItemH, 0xFF1B8224);
            g.drawText(*font, (*s.items)[i].label.c_str(), b.x + 8, c.y + 3, hover ? 0xFF000000 : 0xFF39FF14, 1);
        }
    }
    // --- painters (counted again) ---
    Margin margin() const override { return {0, 0, 3, 4}; }
};

struct Style {
    Theme theme;
    SkinKnob knob;
    SkinSwitch toggle;
    Menu popup;
};

// --- drawing (not counted) ---------------------------------------------------------------------------------

// A printed control label, as the skin prints it (the skins draw it with the control; here it is part
// of the background).
void printLabel(Graphics& g, const Font& font, const char* text, int cx, int top) {
#if defined(ACIDUS_GUI_STYLE_MODERN)
    (void)font;
    modern::drawTextCentered(g, modern::kLabelFont, text, cx + 0.5f, static_cast<float>(top), 7.f, modern::kInk, 0.18f);
#else
    g.drawText(font, text, cx - font.getTextWidth(text, 1) / 2, top, 0xFF16181A, 1);
#endif
}

// --- page --------------------------------------------------------------------------------------------------

using G = Gui<Style>;

struct Knob {
    clap_id id;
    const char* label;
    int x;
    uint32_t cap; // centre cap colour, 0: none
};
constexpr Knob kKnobs[] = {
    {PARAM_CUTOFF, "CUT OFF FREQ", 120, 0},    {PARAM_RESONANCE, "RESONANCE", 186, 0},
    {PARAM_ENV_MOD, "ENV MOD", 252, 0},        {PARAM_DECAY, "DECAY", 318, 0},
    {PARAM_ACCENT, "ACCENT", 384, 0},          {PARAM_TUNE, "TUNING", 456, 0},
    {PARAM_VOLUME, "VOLUME", 528, 0xFF6B4A22}, {PARAM_DRIVE, "DRIVE", 594, 0xFFF5C21B},
};
constexpr int kRowY = 100, kSwitchX = 50, kLabelTop = kRowY - 46;
constexpr int kDivider = static_cast<int>(GuiWindow::kDefaultWidth) - 180;

class Panel final : public RootView<G> {
public:
    Panel(AcidusClap* p, IGuiSkin& skin, const Font& font, Renderer*& r)
        : plugin_(p), skin_(skin), font_(font), r_(r), logo_(p, font, kDivider + 26, 42) {}

    void background(G&) override {
        Graphics g = r_->graphics();
        skin_.drawPanel(g, r_->width(), r_->height(), kDivider);
        printLabel(g, font_, "WAVEFORM", kSwitchX, kLabelTop);
        for (const Knob& k : kKnobs)
            printLabel(g, font_, k.label, k.x, kLabelTop);
        logo_.draw(g, LogoPlate::kPlate);
    }

    void frame(G& g) override {
        g.toggle(PARAM_WAVEFORM, around(kSwitchX, kRowY, 45, 81), {.centreBand = 10, .wheel = false});
        for (const Knob& k : kKnobs)
            g.knob(k.id, around(k.x, kRowY, 60, 60), {.accent = k.cap, .wheelStep = 0.0, .hitRadius = 30.0f});

        // the logo plate: a click on the calibration label loads the next one, a right click opens the menu
        int x, y, w, h;
        logo_.labelRect(x, y, w, h);
        if (g.interact(1, {x, y, w, h}).pressed && plugin_)
            plugin_->cycleCalibrationPresetFromGui();
        logo_.plateRect(x, y, w, h);
        if (g.interact(2, {x, y, w, h}).rightPressed)
            openMenu(g);
        if (plugin_ && plugin_->accentCount() != accents_) {
            accents_ = plugin_->accentCount();
            logo_.flashEyes();
        }
        if (!frozen_)
            logo_.advance(g.dt(), g.norm(PARAM_CUTOFF));
        if (g.view(3, {x - 4, y - 4, w + 10, h + 10}, logo_.key())) {
            Graphics gr = r_->graphics();
            logo_.drawAnimated(gr, r_->width(), r_->height());
        }
    }

    void openMenu(G& g) {
        std::vector<PopupItem> items{{"EXPORT CALIBRATION..."}, {"IMPORT CALIBRATION..."}, {"", true}};
        const int current = plugin_ ? plugin_->calibrationPresetIndex() : -1;
        for (int i = 0; plugin_ && i < plugin_->calibrationPresetCount(); ++i)
            items.push_back({(i == current ? "> " : "  ") + (i == AcidusClap::kCustomCalibration
                                                                 ? plugin_->customCalibrationName()
                                                                 : std::string(calibrationPresets()[i].name))});
        const Rect at{static_cast<int>(g.mouseX()), static_cast<int>(g.mouseY()), 0, 0};
        g.popup("calibration", at, std::move(items), -1, [this](int i) {
            if (i == 0 || i == 1)
                fileRequest_ = i == 0 ? 1 : 2;
            else if (i >= 3 && plugin_)
                plugin_->selectCalibrationPreset(i - 3, true);
        });
    }

    bool animating() override { return true; } // the bubbles never stop
    void tick(double) override { pollFileDialog(); }

    LogoPlate& logo() { return logo_; }
    void setFrozen(bool f) { frozen_ = f; }

private:
    // --- window (not counted): export / import through the file dialog (FileDialog.hpp) ---
    void pollFileDialog() {
#if defined(__linux__) && !defined(__APPLE__)
        if (fileRequest_ != 0 && !dialog_.running()) {
            if (!dialog_.start(options(fileRequest_ == 1)))
                logo_.showStatus("NO FILE DIALOG");
            fileRequest_ = 0;
        }
        std::string chosen;
        bool save = false;
        if (dialog_.poll(chosen, save) && !chosen.empty() && plugin_) {
            lastDir_ = utf8String(utf8Path(chosen).parent_path());
            std::string error;
            if (save)
                logo_.showStatus(plugin_->exportCalibration(chosen, error) ? "EXPORTED" : "EXPORT FAILED");
            else
                logo_.showStatus(!plugin_->importCalibration(chosen, error)
                                     ? "IMPORT FAILED"
                                     : (error.empty() ? "IMPORTED" : "IMPORTED, CLAMPED"));
        }
#endif
    }
    FileDialogOptions options(bool save) const {
        FileDialogOptions o;
        o.save = save;
        o.title = save ? "Export Acidus calibration" : "Import Acidus calibration";
        o.filterName = "Calibration profiles";
        o.extension = "json";
        std::string dir = lastDir_.empty() ? homeDirectory() : lastDir_;
        if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
            dir += '/';
        o.startPath = dir;
        if (save && plugin_) {
            std::string file;
            for (char c : plugin_->calibrationPresetName()) {
                if (c >= 'A' && c <= 'Z')
                    file += static_cast<char>(c - 'A' + 'a');
                else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
                    file += c;
                else if (c == ' ' && !file.empty() && file.back() != '-')
                    file += '-';
            }
            o.startPath += (file.empty() ? "acidus-calibration" : file) + ".json";
        }
        return o;
    }

    AcidusClap* plugin_;
    IGuiSkin& skin_;
    const Font& font_;
    Renderer*& r_;
    LogoPlate logo_;
    uint32_t accents_ = 0;
    bool frozen_ = false;
    int fileRequest_ = 0; // 1 export, 2 import
    std::string lastDir_;
#if defined(__linux__) && !defined(__APPLE__)
    FileDialogProcess dialog_;
#endif
};

// --- page (the feel of the panel's controls) ------------------------------------------------------------

Theme acidusTheme() {
    Theme t;
    t.colors.accent = 0; // a knob without a cap colour
    t.colors.background = 0xFFDBDFE1;
    t.feel.dragPixels = 80.0f; // 0.0125 of the range per pixel
    t.feel.fine = 0.2f;        // shift: 0.0025
    t.feel.doubleClick = 0.4;
    t.feel.wheelStep = 0.0; // no wheel on the panel
    return t;
}

} // namespace

// --- window ------------------------------------------------------------------------------------------------

struct GuiWindow::Impl {
    explicit Impl(AcidusClap* p)
        : plugin(p), skin(createGuiSkin()), host(p), panel(p, *skin, font, renderer),
          editor(host, style, panel, options(), [this](int w, int h) {
              auto r = std::make_unique<Renderer>(w, h, 2, font);
              renderer = r.get();
              return r;
          }) {
        style.theme = acidusTheme();
        style.knob.skin = style.toggle.skin = skin.get();
        style.knob.font = style.toggle.font = style.popup.font = &font;
    }
    static EditorOptions options() {
        EditorOptions o;
        o.width = static_cast<int>(kDefaultWidth);
        o.height = static_cast<int>(kDefaultHeight);
        o.caching = std::getenv("SPUX_GATE_NO_CACHE") == nullptr; // a full redraw every frame, to check the caches
        o.activeHz = o.idleHz = 30;                               // the logo animates at 30 fps, as before
        return o;
    }

    AcidusClap* plugin;
    Font font{Font::classic5x7()};
    std::unique_ptr<IGuiSkin> skin;
    Renderer* renderer = nullptr;
    Style style;
    AcidusHost host;
    Panel panel;
    Editor<Style> editor;
    mutable std::vector<uint32_t> pixels;
};

GuiWindow::GuiWindow(AcidusClap* plugin) : impl_(std::make_unique<Impl>(plugin)) {}
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
bool GuiWindow::setSize(uint32_t width, uint32_t height) {
    return impl_->editor.setSize(static_cast<int>(width), static_cast<int>(height));
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
void GuiWindow::advanceAnimation(double dt) { impl_->panel.logo().advance(dt, impl_->editor.gui().norm(PARAM_CUTOFF)); }
void GuiWindow::setAnimationFrozen(bool frozen) { impl_->panel.setFrozen(frozen); }
size_t GuiWindow::getBubbleCount() const { return impl_->panel.logo().bubbleCount(); }

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
void GuiWindow::handleRightClick(int x, int y) {
    impl_->editor.mouseDown(static_cast<float>(x), static_cast<float>(y), MouseButton::Right, false);
    impl_->editor.mouseUp(static_cast<float>(x), static_cast<float>(y), MouseButton::Right);
}
void GuiWindow::handleMouseMove(int x, int y) { impl_->editor.mouseMove(static_cast<float>(x), static_cast<float>(y)); }
bool GuiWindow::isMenuOpen() const { return impl_->editor.gui().popupOpen(); }

// CLAP GUI extension: as in Acidus's GuiWindow.cpp.
const clap_plugin_gui_t g_acidusGuiExtension = {
    [](const clap_plugin_t*, const char* api, bool is_floating) -> bool {
#if defined(__linux__) && !defined(__APPLE__)
        return api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0 && !is_floating;
#elif defined(_WIN32)
        return api && std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0 && !is_floating;
#else
        return false;
#endif
    },
    [](const clap_plugin_t*, const char** api, bool* is_floating) -> bool {
        if (!api || !is_floating)
            return false;
#if defined(__linux__) && !defined(__APPLE__)
        *api = CLAP_WINDOW_API_X11;
#elif defined(_WIN32)
        *api = CLAP_WINDOW_API_WIN32;
#endif
        *is_floating = false;
        return true;
    },
    [](const clap_plugin_t* plugin, const char*, bool) -> bool {
        auto* self = static_cast<AcidusClap*>(plugin->plugin_data);
        try {
            self->createGuiWindow();
        } catch (...) {
            return false;
        }
        return true;
    },
    [](const clap_plugin_t* plugin) {
        auto* self = static_cast<AcidusClap*>(plugin->plugin_data);
        try {
            self->destroyGuiWindow();
        } catch (...) {
        }
    },
    [](const clap_plugin_t*, double) -> bool { return false; },
    [](const clap_plugin_t*, uint32_t* width, uint32_t* height) -> bool {
        if (!width || !height)
            return false;
        *width = GuiWindow::kDefaultWidth;
        *height = GuiWindow::kDefaultHeight;
        return true;
    },
    [](const clap_plugin_t*) -> bool { return false; },
    [](const clap_plugin_t*, clap_gui_resize_hints_t*) -> bool { return false; },
    [](const clap_plugin_t*, uint32_t* width, uint32_t* height) -> bool {
        if (!width || !height)
            return false;
        *width = GuiWindow::kDefaultWidth;
        *height = GuiWindow::kDefaultHeight;
        return true;
    },
    [](const clap_plugin_t* plugin, uint32_t width, uint32_t height) -> bool {
        auto* self = static_cast<AcidusClap*>(plugin->plugin_data);
        try {
            if (self->getGuiWindow())
                return self->getGuiWindow()->setSize(width, height);
        } catch (...) {
            return false;
        }
        return true;
    },
    [](const clap_plugin_t* plugin, const clap_window_t* window) -> bool {
        auto* self = static_cast<AcidusClap*>(plugin->plugin_data);
        if (!window || !window->api)
            return false;
        try {
            if (!self->getGuiWindow())
                self->createGuiWindow();
            return self->getGuiWindow() && self->getGuiWindow()->setParent(window);
        } catch (...) {
            return false;
        }
    },
    [](const clap_plugin_t*, const clap_window_t*) -> bool { return false; },
    [](const clap_plugin_t*, const char*) {},
    [](const clap_plugin_t* plugin) -> bool {
        auto* self = static_cast<AcidusClap*>(plugin->plugin_data);
        try {
            if (self->getGuiWindow())
                return self->getGuiWindow()->show();
        } catch (...) {
        }
        return false;
    },
    [](const clap_plugin_t* plugin) -> bool {
        auto* self = static_cast<AcidusClap*>(plugin->plugin_data);
        try {
            if (self->getGuiWindow())
                return self->getGuiWindow()->hide();
        } catch (...) {
        }
        return false;
    },
};

} // namespace acidus
