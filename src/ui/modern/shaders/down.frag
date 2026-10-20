// Dual-filter downsample.
uniform sampler2D u_tex;
uniform vec2 u_texel;   // texel size of the source
in vec2 v_uv;
out vec4 o_color;
void main() {
    vec3 s = texture(u_tex, v_uv).rgb * 4.0;
    s += texture(u_tex, v_uv + u_texel * vec2(-1, -1)).rgb;
    s += texture(u_tex, v_uv + u_texel * vec2(1, -1)).rgb;
    s += texture(u_tex, v_uv + u_texel * vec2(-1, 1)).rgb;
    s += texture(u_tex, v_uv + u_texel * vec2(1, 1)).rgb;
    o_color = vec4(s * 0.125, 1.0);
}
