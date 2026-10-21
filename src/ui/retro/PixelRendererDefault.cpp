#include "spux/ui/retro/BitmapFont.hpp"
#include "spux/ui/retro/PixelRenderer.hpp"

// The constructor with the default face is its own object, so a program that passes its own font never links the
// 5x7 data.
namespace spux::inline v1::ui::retro {

PixelRenderer::PixelRenderer(int width, int height) : PixelRenderer(width, height, font5x7()) {}

} // namespace spux::inline v1::ui::retro
