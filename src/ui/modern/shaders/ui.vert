#include "common.glsl"
uniform vec2 u_res;
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_p;
layout(location = 2) in vec4 a_shape;
layout(location = 3) in vec4 a_c0;
layout(location = 4) in vec4 a_c1;
layout(location = 5) in vec2 a_misc;   // kind, emission
out vec2 v_p;
out vec4 v_shape;
out vec4 v_c0;
out vec4 v_c1;
flat out float v_kind;
flat out float v_emit;
void main() {
    vec2 ndc = a_pos / u_res * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    v_p = a_p;
    v_shape = a_shape;
    v_c0 = vec4(srgbToLinear(a_c0.rgb), a_c0.a);
    v_c1 = vec4(srgbToLinear(a_c1.rgb), a_c1.a);
    v_kind = a_misc.x;
    v_emit = a_misc.y;
}
