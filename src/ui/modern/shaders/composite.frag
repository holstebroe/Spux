#include "common.glsl"
// Scene + bloom, tone map, vignette, grain, dithering, sRGB.
uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform float u_bloomStrength;
uniform float u_grain;
uniform float u_vignette;
uniform float u_time;
uniform vec2 u_res;
in vec2 v_uv;
out vec4 o_color;
void main() {
    vec3 c = texture(u_scene, v_uv).rgb + texture(u_bloom, v_uv).rgb * u_bloomStrength;
    // soft shoulder above 0.85: highlights roll off towards 1 instead of clipping hard
    float l = max(c.r, max(c.g, c.b));
    if (l > 0.85) {
        float t = 0.85 + 0.15 * (1.0 - exp(-(l - 0.85) * 20.0));
        c *= t / l;
    }
    vec2 d = v_uv - 0.5;
    c *= 1.0 - dot(d, d) * u_vignette;
    vec3 s = linearToSrgb(c);
    float n = hash21(gl_FragCoord.xy + fract(u_time * 7.0) * 61.0) - 0.5;
    s += n * (u_grain + 1.0 / 255.0);
    o_color = vec4(s, 1.0);
}
