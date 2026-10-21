// Renders one small image of every control in every flavor that has it, for docs/WIDGETS.md.
//
//   spux_widget_gallery <output directory>
//
// The names are <look>_<control>.png: classic (slate), silver (the Classic silver look), forge (the Classic forge
// look), modern (the Modern style on the software renderer), retro_silver, retro_tracker and retro_forge (the three
// Retro looks) and core for what every flavor draws the same (the waveform icons). Controls are drawn through their
// painters on the look's own background, at twice their size; <look>_page.png of a Retro look is the whole showcase
// page. The test helpers use doctest's CHECK; the gallery is not a test, so they compile to nothing.
#define DOCTEST_CONFIG_DISABLE

#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ui/ClassicSupport.hpp"
#include "ui/ModernSupport.hpp"
#include "ui/PngWriter.hpp"
#include "ui/RetroSupport.hpp"

namespace {

using namespace spux;
using namespace spux::ui;

constexpr int kZoom = 2;

int g_written = 0;

void save(const std::string& dir, const std::string& name, const std::vector<std::uint32_t>& px, int w, int h) {
    if (ui_test::writePng(dir + "/" + name + ".png", px, w, h, kZoom))
        ++g_written;
    else
        std::fprintf(stderr, "cannot write %s\n", name.c_str());
}

template <class Style> void drawControl(const std::string& name, Style& style, IRenderer& r, const Rect& b) {
    const Theme* th = &style.theme;
    const Color accent = rgb(232, 150, 40);
    const Color own = th->colors.accent;
    (void)own;
    static const char* const labels[3] = {"ECO", "NORM", "HIGH"};
    static const Icon waves[3] = {waveIcon(Wave::Sine), waveIcon(Wave::Saw), waveIcon(Wave::Square)};
    static std::uint8_t lit[128] = {};
    lit[52] = lit[56] = lit[59] = 1;
    if (name == "knob") {
        KnobState s;
        s.theme = th;
        s.value = 0.62;
        s.label = "CUTOFF";
        s.valueText = "1.2 kHz";
        s.accent = accent;
        style.knob.paint(r, b, s);
    } else if (name == "knob_modulated") {
        KnobState s;
        s.theme = th;
        s.value = 0.35;
        s.label = "CUTOFF";
        s.valueText = "1.2 kHz";
        s.accent = accent;
        s.pickable = true;
        s.mod.any = true;
        s.mod.base = 0.35;
        s.mod.shared = 0.75;
        style.knob.paint(r, b, s);
    } else if (name == "tabs") {
        static const char* const tabLabels[3] = {"LFO 1", "LFO 2", "MOD"};
        TabsState s;
        s.theme = th;
        s.count = 3;
        s.index = 1;
        s.labels = tabLabels;
        s.accent = accent;
        style.tabs.paint(r, b, s);
    } else if (name == "readout") {
        ReadoutState s;
        s.theme = th;
        s.name = "CUTOFF";
        s.value = "1.20 kHz";
        s.hint = "Drag up or down";
        style.readout.paint(r, b, s);
    } else if (name == "button_display") {
        ButtonState s;
        s.theme = th;
        s.label = "Hellfire Bass *";
        s.variant = kButtonDisplay;
        style.button.paint(r, b, s);
    } else if (name == "button_arrows") {
        ButtonState s;
        s.theme = th;
        s.variant = kButtonArrowLeft;
        style.button.paint(r, {b.x, b.y, b.w / 2 - 2, b.h}, s);
        s.variant = kButtonArrowRight;
        style.button.paint(r, {b.x + b.w / 2 + 2, b.y, b.w / 2 - 2, b.h}, s);
    } else if (name == "slider_vertical" || name == "slider_horizontal") {
        SliderState s;
        s.theme = th;
        s.vertical = name == "slider_vertical";
        s.value = 0.65;
        s.label = "LEVEL";
        s.valueText = "-6 dB";
        s.accent = accent;
        style.slider.paint(r, b, s);
    } else if (name == "toggle") {
        ToggleState s;
        s.theme = th;
        s.on = true;
        s.label = "MONO";
        s.accent = accent;
        style.toggle.paint(r, b, s);
    } else if (name == "selector") {
        SelectorState s;
        s.theme = th;
        s.count = 3;
        s.index = 1;
        s.labels = labels;
        s.accent = accent;
        style.selector.paint(r, b, s);
    } else if (name == "selector_vertical") {
        SelectorState s;
        s.theme = th;
        s.count = 3;
        s.index = 1;
        s.labels = labels;
        s.vertical = true;
        s.accent = accent;
        style.selector.paint(r, b, s);
    } else if (name == "selector_icons") {
        SelectorState s;
        s.theme = th;
        s.count = 3;
        s.index = 1;
        s.icons = waves;
        s.accent = accent;
        style.selector.paint(r, b, s);
    } else if (name == "button") {
        ButtonState s;
        s.theme = th;
        s.label = "PLAIN";
        s.on = true;
        s.accent = accent;
        style.button.paint(r, b, s);
    } else if (name == "button_icon") {
        ButtonState s;
        s.theme = th;
        s.label = "SAW";
        s.icon = waveIcon(Wave::Saw);
        s.accent = accent;
        style.button.paint(r, b, s);
    } else if (name == "led") {
        LedState s;
        s.theme = th;
        s.label = "CLIP";
        s.on = true;
        s.accent = rgb(214, 58, 46);
        style.led.paint(r, b, s);
    } else if (name == "keyboard") {
        KeyboardState s;
        s.theme = th;
        s.first = 48;
        s.last = 71;
        s.lit = lit;
        s.accent = accent;
        style.keyboard.paint(r, b, s);
    } else if (name == "dropdown") {
        DropdownState s;
        s.theme = th;
        s.label = "TYPE";
        s.valueText = "Normal";
        style.dropdown.paint(r, b, s);
    } else if (name == "stepper") {
        StepperState s;
        s.theme = th;
        s.label = "SEMI";
        s.valueText = "0";
        style.stepper.paint(r, b, s);
    } else if (name == "text_field") {
        TextFieldState s;
        s.theme = th;
        s.text = "Preset name";
        s.caret = true;
        style.textField.paint(r, b, s);
    } else if (name == "panel_frame") {
        PanelState s;
        s.theme = th;
        s.title = "FILTER";
        s.accent = accent;
        style.panel.paint(r, b, s);
    }
}

template <class Style> void drawOverlay(const std::string& name, Style& style, IRenderer& r, int w, int h) {
    if (name == "popup") {
        std::vector<PopupItem> items = {{"Sine"}, {"Triangle"}, {"", true, true}, {"Saw"}, {"Disabled", false, false}};
        PopupState s;
        s.theme = &style.theme;
        s.items = &items;
        s.anchor = {8, 4, 80, 18};
        s.selected = 1;
        std::vector<Rect> rects;
        const Rect box = style.popup.layout(r, s, w, h, rects);
        s.itemRects = &rects;
        s.hot = 3;
        style.popup.paint(r, box, s);
    } else if (name == "tooltip") {
        TooltipState s;
        s.theme = &style.theme;
        s.title = "Cutoff";
        s.text = "1.20 kHz   modulatable";
        s.pointer = {8, 4};
        const Rect box = style.tooltip.layout(r, s, w, h);
        style.tooltip.paint(r, box, s);
    }
}

struct Entry {
    const char* name;
    int w, h;
    bool overlay = false;
};

// Sizes that fit the controls of both flavors.
const Entry kEntries[] = {
    {"knob", 84, 96},
    {"knob_modulated", 84, 96},
    {"tabs", 200, 32},
    {"readout", 260, 32},
    {"button_display", 170, 30},
    {"button_arrows", 80, 30},
    {"slider_vertical", 52, 120},
    {"slider_horizontal", 150, 30},
    {"toggle", 110, 86},
    {"selector", 150, 30},
    {"selector_vertical", 90, 80},
    {"selector_icons", 150, 30},
    {"button", 90, 28},
    {"button_icon", 90, 28},
    {"led", 90, 24},
    {"keyboard", 300, 64},
    {"dropdown", 150, 28},
    {"stepper", 150, 28},
    {"text_field", 150, 28},
    {"panel_frame", 150, 70},
    {"popup", 150, 150, true},
    {"tooltip", 190, 50, true},
};

void classicGallery(const std::string& dir, const char* look, classic::ClassicConfig cfg) {
    cfg.trims = false; // the trims of a worn panel would cut through these small canvases
    classic::ClassicStyle style(cfg);
    for (const Entry& e : kEntries) {
        classic::ShadedRenderer r(e.w, e.h, 2);
        r.clear(cfg.theme.colors.background);
        // the surface under the control, as a page would have it
        PanelState surface;
        surface.theme = &style.theme;
        style.panel.paint(r, {0, 0, e.w, e.h}, surface);
        if (e.overlay)
            drawOverlay(e.name, style, r, e.w, e.h);
        else
            drawControl(e.name, style, r, {4, 4, e.w - 8, e.h - 8});
        save(dir, std::string(look) + "_" + e.name, r.frame(), e.w, e.h);
    }
}

void retroGallery(const std::string& dir, const std::string& look, retro::RetroConfig cfg) {
    const bool trims = cfg.trims.on;
    cfg.trims.on = false; // the trims of a page would cut through these small canvases
    retro::RetroStyle style(cfg);
    for (const Entry& e : kEntries) {
        retro::PixelRenderer r(e.w, e.h, *cfg.font, cfg.titleFont);
        r.clear(cfg.theme.colors.background);
        // the surface under the control, as a page would have it
        PanelState surface;
        surface.theme = &style.theme;
        style.panel.paint(r, {0, 0, e.w, e.h}, surface);
        if (e.overlay)
            drawOverlay(e.name, style, r, e.w, e.h);
        else
            drawControl(e.name, style, r, {4, 4, e.w - 8, e.h - 8});
        save(dir, look + "_" + e.name, r.frame(), e.w, e.h);
    }
    // the whole showcase page of the look
    retro_test::Host host;
    retro::PixelRenderer page(retro_test::kPageW, retro_test::kPageH, *cfg.font, cfg.titleFont);
    cfg.trims.on = trims;
    retro::RetroStyle pageStyle(cfg);
    retro_test::showcaseFrame(page, host, pageStyle);
    save(dir, look + "_page", page.frame(), retro_test::kPageW, retro_test::kPageH);
}

void modernGallery(const std::string& dir) {
    modern::ModernStyle style;
    auto atlas = modern::SoftRenderer::makeFontAtlas();
    for (const Entry& e : kEntries) {
        modern::SoftRenderer r(e.w, e.h, 2, atlas);
        r.clear(style.theme.colors.background);
        r.drawBackground(rgb(30, 38, 70), rgb(52, 36, 78), 6.0f);
        if (e.overlay)
            drawOverlay(e.name, style, r, e.w, e.h);
        else
            drawControl(e.name, style, r, {4, 4, e.w - 8, e.h - 8});
        save(dir, std::string("modern_") + e.name, r.frame(), e.w, e.h);
    }
    // the generic views, through a Gui (they are Gui views)
    modern_test::Host host;
    modern_test::ShowData d;
    struct View {
        const char* name;
        int w, h;
    };
    for (const View& v : {View{"spectrum", 200, 90}, View{"traces", 200, 80}, View{"stereo_meter", 200, 34},
                          View{"curve_editor", 200, 130}}) {
        modern::SoftRenderer r(v.w, v.h, 2, atlas);
        r.clear(style.theme.colors.background);
        r.drawBackground(rgb(30, 38, 70), rgb(52, 36, 78), 6.0f);
        modern_test::MGui g(host, style);
        g.setCaching(false);
        g.begin(r, 1.0, 0.016f, v.w, v.h);
        const Rect b{4, 4, v.w - 8, v.h - 8};
        const std::string n = v.name;
        if (n == "spectrum")
            drawSpectrum(style, g, "spec", b, d.bands.data(), static_cast<int>(d.bands.size()), modern_test::kCyan);
        else if (n == "traces") {
            const modern::TraceRow rows[2] = {
                {"Saw", modern_test::kGreen, d.saw.data(), static_cast<int>(d.saw.size()), false, 0.42f},
                {"Sine", modern_test::kViolet, d.sine.data(), static_cast<int>(d.sine.size()), true, -0.37f}};
            drawTraces(style, g, "traces", b, rows, 2);
        } else if (n == "stereo_meter")
            drawStereoMeter(style, g, "meter", b, 0.4f, 0.9f);
        else
            drawCurveEditor(style, g, "curve", b, "Velocity", d.curve, 5, {}, modern_test::kViolet, d.curveState);
        g.end();
        save(dir, std::string("modern_") + v.name, r.frame(), v.w, v.h);
    }
}

void coreGallery(const std::string& dir) {
    // the waveform icons, in a row, on a neutral ground
    constexpr int cell = 44, h = 30;
    classic::ShadedRenderer r(cell * kWaveCount, h, 2);
    r.clear(rgb(36, 40, 46));
    for (int i = 0; i < kWaveCount; ++i)
        drawWave(r, static_cast<Wave>(i), {i * cell + 6, 5, cell - 12, h - 10}, rgb(235, 238, 244), 1.5f);
    save(dir, "core_waves", r.frame(), cell * kWaveCount, h);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <output directory>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    std::filesystem::create_directories(dir);
    classicGallery(dir, "classic", classic::ClassicConfig::slate());
    classicGallery(dir, "silver", classic::ClassicConfig::silver());
    classicGallery(dir, "forge", classic::ClassicConfig::forge());
    retroGallery(dir, "retro_silver", retro::RetroConfig::silver());
    retroGallery(dir, "retro_tracker", retro::RetroConfig::tracker());
    retroGallery(dir, "retro_forge", retro::RetroConfig::forge());
    modernGallery(dir);
    coreGallery(dir);
    std::printf("%d images in %s\n", g_written, dir.c_str());
    return 0;
}
