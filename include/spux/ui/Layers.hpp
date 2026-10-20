// Retained-style caches for CPU renderers (guidelines 7, "Immediate mode", caches 2 and 3).
//
// A CPU renderer keeps three pixel layers:
//   background  the static panel (printed labels, frames): drawn once, again only when its key changes;
//   widgets     background plus the widgets; a widget that changes is redrawn over a restored copy of
//               the background under its area, an unchanged one is not touched;
//   display     widgets plus the overlays (popups, tooltips): what the window shows while an overlay is
//               up. In a frame without overlays the window is fed from `widgets` and this layer is idle.
// Everything written to `widgets` or `display` is recorded as dirty; the window blits only that. Gui
// drives the layers (restore, composite, targets); a renderer draws into whatever target is set.
#pragma once

#include <cstdint>
#include <vector>

#include "spux/ui/Geometry.hpp"

namespace spux::inline v1::ui {

enum class Layer { Background, Widgets, Display };

class LayerCache {
public:
    virtual ~LayerCache() = default;

    // Where drawing goes from now on.
    virtual void setTarget(Layer layer) = 0;
    [[nodiscard]] virtual Layer target() const = 0;
    // widgets := background inside r (logical pixels, clipped to the frame); r becomes dirty.
    virtual void restore(const Rect& r) = 0;
    // Marks r as changed in widgets and display.
    virtual void markDirty(const Rect& r) = 0;
    // This frame has overlays: display := widgets inside every dirty rectangle (all of it when the
    // display layer was idle), before the overlays are drawn on it.
    virtual void composite() = 0;
    // Bounding box of what changed since clearDirty, logical pixels; empty when nothing did.
    [[nodiscard]] virtual Rect dirtyBounds() const = 0;
    virtual void clearDirty() = 0;
    // Writes the dirty part of the frame into `out` (the logical size, opaque ARGB): the display layer
    // after a composite, the widgets layer otherwise. Call before clearDirty.
    virtual void resolve(std::uint32_t* out) const = 0;
};

// The three layers as ARGB buffers at a supersampling factor (1: Retro's pixels, 2: Classic's box
// filter), and the step from the display layer to the window's pixels. Used by spux's CPU renderers and
// by a plugin's own: draw into target() (scale() pixels per logical pixel, rowPixels() per row).
class CpuLayers final : public LayerCache {
public:
    CpuLayers() = default;
    CpuLayers(int width, int height, int scale) { resize(width, height, scale); }

    // Logical size and supersampling factor; clears all layers to opaque black and marks the frame dirty.
    void resize(int width, int height, int scale);
    [[nodiscard]] int width() const { return w_; }
    [[nodiscard]] int height() const { return h_; }
    [[nodiscard]] int scale() const { return s_; }
    [[nodiscard]] int rowPixels() const { return w_ * s_; }

    [[nodiscard]] std::uint32_t* target() { return buffer(target_); }
    [[nodiscard]] std::uint32_t* buffer(Layer layer);
    [[nodiscard]] const std::uint32_t* buffer(Layer layer) const;

    void setTarget(Layer layer) override { target_ = layer; }
    [[nodiscard]] Layer target() const override { return target_; }
    void restore(const Rect& r) override;
    void markDirty(const Rect& r) override;
    void composite() override;
    [[nodiscard]] Rect dirtyBounds() const override { return bounds_; }
    void clearDirty() override;

    // Writes the dirty part of the frame into `out` (width() x height() opaque ARGB): copied at scale 1,
    // box-filtered (rounded mean of each s x s block) above. Does not clear the dirty region.
    void resolve(std::uint32_t* out) const override { resolve(out, bounds_); }
    // The same for any part of the frame.
    void resolve(std::uint32_t* out, const Rect& r) const;

private:
    void copy(Layer to, Layer from, const Rect& r);
    [[nodiscard]] Rect clip(const Rect& r) const { return r.intersect({0, 0, w_, h_}); }

    int w_ = 0, h_ = 0, s_ = 1;
    std::vector<std::uint32_t> layers_[3];
    Layer target_ = Layer::Widgets;
    std::vector<Rect> dirty_; // since the last composite
    Rect bounds_;
    bool composited_ = false;   // this frame has overlays: resolve from the display layer
    bool displayValid_ = false; // the display layer matches widgets outside the dirty rectangles
};

} // namespace spux::inline v1::ui
