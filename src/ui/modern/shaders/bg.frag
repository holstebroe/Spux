#include "common.glsl"
// Animated backdrop: a near-black field with slow domain-warped ribbons tinted by the two accents.
uniform vec2 u_res;
uniform float u_time;
uniform vec3 u_colA;
uniform vec3 u_colB;
uniform float u_pools;   // strength of the accent pools
uniform float u_dust;    // strength of the twinkling points
in vec2 v_uv;
out vec4 o_color;
void main() {
    vec2 uv = v_uv;
    float aspect = u_res.x / u_res.y;
    vec2 p = vec2(uv.x * aspect, uv.y);
    float t = u_time * 0.03;
    vec2 q = vec2(fbm(p * 0.7 + vec2(t, -t * 0.6)), fbm(p * 0.7 + vec2(5.2 - t, 1.3 + t * 0.8)));
    float n = fbm(p * 0.6 + 1.6 * q + vec2(0.0, t * 0.5));
    // two broad, slowly drifting pools of light in the accent colors
    float poolA = smoothstep(0.25, 0.95, n) * smoothstep(1.1, 0.1, length((uv - vec2(0.18 + 0.08 * sin(t * 2.0), 0.85)) * vec2(aspect, 1.0)));
    float poolB = smoothstep(0.3, 1.0, 1.0 - n) * smoothstep(1.2, 0.2, length((uv - vec2(0.85 + 0.06 * cos(t * 1.7), 0.15)) * vec2(aspect, 1.0)));
    vec3 base = mix(vec3(0.003, 0.004, 0.010), vec3(0.008, 0.010, 0.022), uv.y);
    vec3 col = base + (u_colA * poolA * 0.16 + u_colB * poolB * 0.13) * u_pools;
    // fine drifting dust: a few faint points that twinkle slowly
    vec2 gp = p * 70.0;
    vec2 g = floor(gp);
    float h = hash21(g);
    vec2 center = vec2(hash21(g + 3.1), hash21(g + 7.7)) * 0.6 + 0.2;
    float star = step(0.985, h) * smoothstep(0.10, 0.0, length(fract(gp) - center)) * (0.5 + 0.5 * sin(u_time * (0.4 + h * 2.0) + h * 40.0));
    col += vec3(0.55, 0.65, 1.0) * star * 0.5 * u_dust;
    float v = length((uv - 0.5) * vec2(1.0, 0.85));
    col *= 1.0 - smoothstep(0.35, 0.95, v) * 0.6;
    o_color = vec4(col, 1.0);
}
