// Dual-filter upsample, drawn additively onto the larger level.
uniform sampler2D u_tex;
uniform vec2 u_texel;   // texel size of the source
uniform float u_weight;
in vec2 v_uv;
out vec4 o_color;
void main() {
    vec3 s = texture(u_tex, v_uv + u_texel * vec2(-2, 0)).rgb + texture(u_tex, v_uv + u_texel * vec2(2, 0)).rgb +
             texture(u_tex, v_uv + u_texel * vec2(0, -2)).rgb + texture(u_tex, v_uv + u_texel * vec2(0, 2)).rgb;
    s += 2.0 * (texture(u_tex, v_uv + u_texel * vec2(-1, -1)).rgb + texture(u_tex, v_uv + u_texel * vec2(1, -1)).rgb +
                texture(u_tex, v_uv + u_texel * vec2(-1, 1)).rgb + texture(u_tex, v_uv + u_texel * vec2(1, 1)).rgb);
    o_color = vec4(s * (1.0 / 12.0) * u_weight, 1.0);
}
