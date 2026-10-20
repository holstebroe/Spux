// The GLSL sources of src/ui/modern/shaders, embedded at configure time (cmake/SpuxEmbedShaders.cmake).
#pragma once

namespace spux::inline v1::ui::modern {

struct ShaderSource {
    const char* name;
    const char* text;
};
extern const ShaderSource kShaderSources[];
extern const unsigned long kShaderSourceCount;

} // namespace spux::inline v1::ui::modern
