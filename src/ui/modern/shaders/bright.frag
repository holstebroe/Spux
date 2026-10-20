// Bright pass and first downsample: keeps what is above the threshold, soft knee.
uniform sampler2D u_tex;
uniform vec2 u_texel;
uniform float u_threshold;
in vec2 v_uv;
out vec4 o_color;
vec3 pick(vec2 uv) {
    vec3 c = texture(u_tex, uv).rgb;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - u_threshold, 0.0);
    k = k * k / (k + 0.25);
    return c * (k / max(l, 1e-4));
}
void main() {
    vec3 s = pick(v_uv + u_texel * vec2(-1, -1)) + pick(v_uv + u_texel * vec2(1, -1)) +
             pick(v_uv + u_texel * vec2(-1, 1)) + pick(v_uv + u_texel * vec2(1, 1));
    o_color = vec4(s * 0.25, 1.0);
}
