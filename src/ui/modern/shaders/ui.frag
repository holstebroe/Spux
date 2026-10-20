#include "common.glsl"
uniform sampler2D u_tex;
in vec2 v_p;
in vec4 v_shape;
in vec4 v_c0;
in vec4 v_c1;
flat in float v_kind;
flat in float v_emit;
out vec4 o_color;

float sdRoundBox(vec2 p, vec2 b, float r) {
    r = min(r, min(b.x, b.y));
    vec2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
float aa(float d) { return clamp(0.5 - d, 0.0, 1.0); }

void main() {
    int k = int(v_kind + 0.5);
    vec4 col = v_c0;
    float cov = 1.0;
    if (k == 0) {            // rounded box: shape = half size, radius, stroke (0 = fill)
        float d = sdRoundBox(v_p, v_shape.xy, v_shape.z);
        if (v_shape.w > 0.0) d = abs(d + v_shape.w * 0.5) - v_shape.w * 0.5;
        cov = aa(d);
    } else if (k == 1) {     // circle: radius, stroke
        float d = length(v_p) - v_shape.x;
        if (v_shape.y > 0.0) d = abs(d + v_shape.y * 0.5) - v_shape.y * 0.5;
        cov = aa(d);
    } else if (k == 2) {     // capsule along x: half length, radius
        vec2 q = vec2(max(abs(v_p.x) - v_shape.x, 0.0), v_p.y);
        cov = aa(length(q) - v_shape.y);
    } else if (k == 3) {     // arc: radius, half thickness, angle0, angle1
        float r = length(v_p);
        float a0 = v_shape.z, a1 = v_shape.w;
        float a = mod(atan(v_p.y, v_p.x) - a0, 6.28318530718);
        float d;
        if (a <= a1 - a0) {
            d = abs(r - v_shape.x) - v_shape.y;
        } else {
            vec2 e0 = v_shape.x * vec2(cos(a0), sin(a0));
            vec2 e1 = v_shape.x * vec2(cos(a1), sin(a1));
            d = min(length(v_p - e0), length(v_p - e1)) - v_shape.y;
        }
        cov = aa(d);
    } else if (k == 4) {     // soft glow blob
        float t = length(v_p) / v_shape.x;
        cov = exp(-t * t * 3.2) * (1.0 - smoothstep(0.85, 1.0, t));
    } else if (k == 5) {     // radial gradient disc, inner color to outer color
        float t = length(v_p) / v_shape.x;
        col = mix(v_c0, v_c1, clamp(t, 0.0, 1.0));
        cov = aa((t - 1.0) * v_shape.x);
    } else if (k == 6) {     // SDF text: v_p is the atlas uv, shape.x the weight bias
        float s = texture(u_tex, v_p).r;
        float w = max(fwidth(s) * 0.7, 0.012);
        float edge = 0.5 - v_shape.x;
        cov = smoothstep(edge - w, edge + w, s);
    } else if (k == 7) {     // drop shadow of a rounded box: half size, radius, sigma
        float d = sdRoundBox(v_p, v_shape.xy, v_shape.z);
        cov = 1.0 - smoothstep(-v_shape.w * 1.2, v_shape.w * 1.6, d);
        cov *= cov;
    } else if (k == 8) {     // polyline strip: p.y is the distance across the line, shape.x the half width
        cov = clamp(v_shape.x + 0.5 - abs(v_p.y), 0.0, 1.0);
    } else if (k == 9) {     // unfiltered vertex colors (fills under traces)
        cov = 1.0;
    }
    o_color = vec4(col.rgb * v_emit, col.a * cov);
}
