// Acceptance gate (spux DESIGN_GUIDELINES 7): the Tannhauser panel on spux_ui_core. A drop-in for
// Tannhauser's retained `src/gui/GuiWindow.hpp` with the calls TannhauserClap and the gate's bench use,
// so spux's gate build links the whole plugin with this editor in place of the retained one.
// Not ported: the preset menu (a two-level cascading menu; spux's popup has one level) and Save As.
#ifndef TANNHAUSER_GUI_WINDOW_HPP
#define TANNHAUSER_GUI_WINDOW_HPP

#include <clap/clap.h>
#include <clap/ext/gui.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "PanelLayout.hpp"

namespace tannhauser {

class TannhauserClap;

extern const clap_plugin_gui_t g_tannhauserGuiExtension;

class GuiWindow {
public:
    static constexpr uint32_t kDefaultWidth = PanelLayout::kWidth;
    static constexpr uint32_t kDefaultHeight = PanelLayout::kHeight;

    explicit GuiWindow(TannhauserClap* plugin);
    ~GuiWindow();

    bool setParent(const clap_window_t* window);
    bool setSize(uint32_t width, uint32_t height);
    bool show();
    bool hide();
    void destroy();

    // One tick: draws a frame when something changed.
    void renderFrame();
    const std::vector<uint32_t>& getPixelBuffer() const;
    uint32_t getWidth() const { return kDefaultWidth; }
    uint32_t getHeight() const { return kDefaultHeight; }

    void handleMouseDown(int x, int y, bool shift);
    void handleMouseDrag(int x, int y, bool shift);
    void handleMouseUp();
    void handleMouseMove(int x, int y);
    void handleWheel(int x, int y, int delta);
    void handleKey(int key);
    bool isMenuOpen() const { return false; }
    std::string readoutText() const;
    void showStatus(const std::string& text);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tannhauser

#endif
