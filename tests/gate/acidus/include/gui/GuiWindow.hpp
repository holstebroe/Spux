// Acceptance gate (spux DESIGN_GUIDELINES 7): the Acidus panel on spux_ui_core. A drop-in for Acidus's
// retained `src/gui/GuiWindow.hpp`: the same class name and the calls AcidusClap and the tests use, so
// spux's gate build links the whole plugin with this editor in place of the retained one. Control and
// ControlType are kept because the skins (IGuiSkin) draw from them.
#ifndef ACIDUS_GUI_WINDOW_HPP
#define ACIDUS_GUI_WINDOW_HPP

#include <clap/clap.h>
#include <clap/ext/gui.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace acidus {

class AcidusClap;

extern const clap_plugin_gui_t g_acidusGuiExtension;

enum class ControlType { Knob, ToggleSwitch };

struct Control {
    int id;
    const char* label;
    ControlType type;
    int x, y;
    int radius;
    double minVal, maxVal, currentVal;
    bool isStepped;
    uint32_t accentColor{0};
};

class GuiWindow {
public:
    static constexpr uint32_t kDefaultWidth = 854;
    static constexpr uint32_t kDefaultHeight = 180;

    explicit GuiWindow(AcidusClap* plugin);
    ~GuiWindow();

    bool setParent(const clap_window_t* window);
    bool setSize(uint32_t width, uint32_t height);
    bool show();
    bool hide();
    void destroy();

    uint32_t getWidth() const { return kDefaultWidth; }
    uint32_t getHeight() const { return kDefaultHeight; }
    const std::vector<uint32_t>& getPixelBuffer() const;

    // One tick: draws a frame when something changed.
    void renderFrame();
    void advanceAnimation(double dt);
    void setAnimationFrozen(bool frozen);

    void handleMouseDown(int x, int y, bool isShift = false);
    void handleMouseDrag(int x, int y, bool isShift = false);
    void handleMouseUp();
    void handleRightClick(int x, int y);
    void handleMouseMove(int x, int y);
    bool isMenuOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace acidus

#endif // ACIDUS_GUI_WINDOW_HPP
