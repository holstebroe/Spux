#include "spux/ui/modern/GpuRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string_view>

#include "ShaderSources.hpp"

namespace spux::inline v1::ui::modern {

using gl::GLenum;
using gl::GLint;
using gl::GLsizei;
using gl::GLsizeiptr;
using gl::GLuint;
using gl::GLuint64;

namespace {

constexpr std::size_t kMaxBatchVertices = 60000; // a multiple of 6

const char* shaderText(const std::string& name) {
    for (unsigned long i = 0; i < kShaderSourceCount; ++i)
        if (name == kShaderSources[i].name)
            return kShaderSources[i].text;
    return nullptr;
}

// Resolves #include "file" lines against the embedded sources and puts the version header on top.
std::string preprocess(const std::string& name, int depth = 0) {
    const char* text = shaderText(name);
    if (!text || depth > 4)
        return {};
    std::string out;
    std::string_view src(text);
    std::size_t pos = 0;
    while (pos < src.size()) {
        std::size_t eol = src.find('\n', pos);
        if (eol == std::string_view::npos)
            eol = src.size();
        const std::string_view line = src.substr(pos, eol - pos);
        if (line.rfind("#include", 0) == 0) {
            const std::size_t a = line.find('"'), b = line.rfind('"');
            if (a != std::string_view::npos && b > a)
                out += preprocess(std::string(line.substr(a + 1, b - a - 1)), depth + 1);
        } else {
            out.append(line);
            out += '\n';
        }
        pos = eol + 1;
    }
    return out;
}

void unpack(Color c, std::uint8_t (&out)[4]) {
    out[0] = static_cast<std::uint8_t>(c >> 16);
    out[1] = static_cast<std::uint8_t>(c >> 8);
    out[2] = static_cast<std::uint8_t>(c);
    out[3] = static_cast<std::uint8_t>(c >> 24);
}

float srgbToLinear(float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); }

void linearColor(Color c, float (&rgb)[3]) {
    rgb[0] = srgbToLinear(static_cast<float>((c >> 16) & 255u) / 255.0f);
    rgb[1] = srgbToLinear(static_cast<float>((c >> 8) & 255u) / 255.0f);
    rgb[2] = srgbToLinear(static_cast<float>(c & 255u) / 255.0f);
}

constexpr float kPad = 1.5f; // anti-aliasing margin around shapes, framebuffer pixels

} // namespace

std::unique_ptr<GpuRenderer> GpuRenderer::create(const gl::GlApi& api, int w, int h, float scale, bool offscreen,
                                                 std::string* error) {
    std::unique_ptr<GpuRenderer> r(new GpuRenderer(api));
    r->w_ = w;
    r->h_ = h;
    r->scale_ = scale;
    r->fbW_ = std::max(1, static_cast<int>(std::lround(static_cast<float>(w) * scale)));
    r->fbH_ = std::max(1, static_cast<int>(std::lround(static_cast<float>(h) * scale)));
    if (!r->init(offscreen, error))
        return nullptr;
    return r;
}

GpuRenderer::~GpuRenderer() {
    destroyTargets();
    for (Program* p : {&ui_, &bg_, &bright_, &down_, &up_, &composite_})
        if (p->id)
            gl_.DeleteProgram(p->id);
    if (vbo_)
        gl_.DeleteBuffers(1, &vbo_);
    if (vao_)
        gl_.DeleteVertexArrays(1, &vao_);
    if (atlasTex_)
        gl_.DeleteTextures(1, &atlasTex_);
    if (queries_[0] && gl_.DeleteQueries)
        gl_.DeleteQueries(2, queries_);
}

bool GpuRenderer::buildProgram(Program& p, const char* vert, const char* frag, std::string* error) {
    auto compile = [&](GLenum type, const std::string& src, const char* name) -> GLuint {
        if (src.empty()) {
            if (error)
                *error = std::string("missing shader ") + name;
            return 0;
        }
        const std::string full = "#version 330 core\n" + src;
        const GLuint s = gl_.CreateShader(type);
        const char* ptr = full.c_str();
        gl_.ShaderSource(s, 1, &ptr, nullptr);
        gl_.CompileShader(s);
        GLint ok = 0;
        gl_.GetShaderiv(s, gl::COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048] = {};
            gl_.GetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
            if (error)
                *error = std::string(name) + ": " + log;
            gl_.DeleteShader(s);
            return 0;
        }
        return s;
    };
    const GLuint vs = compile(gl::VERTEX_SHADER, preprocess(vert), vert);
    const GLuint fs = vs ? compile(gl::FRAGMENT_SHADER, preprocess(frag), frag) : 0;
    if (!vs || !fs) {
        if (vs)
            gl_.DeleteShader(vs);
        return false;
    }
    p.id = gl_.CreateProgram();
    gl_.AttachShader(p.id, vs);
    gl_.AttachShader(p.id, fs);
    gl_.LinkProgram(p.id);
    gl_.DeleteShader(vs);
    gl_.DeleteShader(fs);
    GLint ok = 0;
    gl_.GetProgramiv(p.id, gl::LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        gl_.GetProgramInfoLog(p.id, sizeof(log) - 1, nullptr, log);
        if (error)
            *error = std::string("link ") + frag + ": " + log;
        gl_.DeleteProgram(p.id);
        p.id = 0;
        return false;
    }
    auto loc = [&](const char* n) { return gl_.GetUniformLocation(p.id, n); };
    p.res = loc("u_res");
    p.tex = loc("u_tex");
    p.texel = loc("u_texel");
    p.time = loc("u_time");
    p.threshold = loc("u_threshold");
    p.weight = loc("u_weight");
    p.colA = loc("u_colA");
    p.colB = loc("u_colB");
    p.scene = loc("u_scene");
    p.bloom = loc("u_bloom");
    p.bloomStrength = loc("u_bloomStrength");
    p.grain = loc("u_grain");
    p.vignette = loc("u_vignette");
    p.pools = loc("u_pools");
    p.dust = loc("u_dust");
    return true;
}

bool GpuRenderer::makeTarget(Target& t, int w, int h, GLenum internalFormat, GLenum format, GLenum type) {
    t.w = w;
    t.h = h;
    gl_.GenTextures(1, &t.tex);
    gl_.BindTexture(gl::TEXTURE_2D, t.tex);
    gl_.TexImage2D(gl::TEXTURE_2D, 0, static_cast<GLint>(internalFormat), w, h, 0, format, type, nullptr);
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, static_cast<GLint>(gl::LINEAR));
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, static_cast<GLint>(gl::LINEAR));
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, static_cast<GLint>(gl::CLAMP_TO_EDGE));
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, static_cast<GLint>(gl::CLAMP_TO_EDGE));
    gl_.GenFramebuffers(1, &t.fbo);
    gl_.BindFramebuffer(gl::FRAMEBUFFER, t.fbo);
    gl_.FramebufferTexture2D(gl::FRAMEBUFFER, gl::COLOR_ATTACHMENT0, gl::TEXTURE_2D, t.tex, 0);
    const bool ok = gl_.CheckFramebufferStatus(gl::FRAMEBUFFER) == gl::FRAMEBUFFER_COMPLETE;
    gl_.BindFramebuffer(gl::FRAMEBUFFER, 0);
    return ok;
}

void GpuRenderer::destroyTarget(Target& t) {
    if (t.fbo)
        gl_.DeleteFramebuffers(1, &t.fbo);
    if (t.tex)
        gl_.DeleteTextures(1, &t.tex);
    t = {};
}

bool GpuRenderer::createTargets() {
    if (!makeTarget(scene_, fbW_, fbH_, gl::RGBA16F, gl::RGBA, gl::HALF_FLOAT))
        return false;
    for (int i = 0; i < kMaxBloom; ++i)
        if (!makeTarget(bloom_[i], std::max(1, fbW_ >> (i + 1)), std::max(1, fbH_ >> (i + 1)), gl::RGBA16F, gl::RGBA,
                        gl::HALF_FLOAT))
            return false;
    if (offscreen_ && !makeTarget(out_, fbW_, fbH_, gl::RGBA8, gl::RGBA, gl::UNSIGNED_BYTE))
        return false;
    return true;
}

void GpuRenderer::destroyTargets() {
    destroyTarget(scene_);
    destroyTarget(out_);
    for (Target& t : bloom_)
        destroyTarget(t);
}

bool GpuRenderer::init(bool offscreen, std::string* error) {
    offscreen_ = offscreen;
    if (gl_.major < 3 || (gl_.major == 3 && gl_.minor < 3)) {
        if (error)
            *error = std::string("OpenGL 3.3 is required, the context is ") + gl_.version;
        return false;
    }
    if (!font_.build()) {
        if (error)
            *error = "font atlas";
        return false;
    }
    if (!buildProgram(ui_, "ui.vert", "ui.frag", error) || !buildProgram(bg_, "fullscreen.vert", "bg.frag", error) ||
        !buildProgram(bright_, "fullscreen.vert", "bright.frag", error) ||
        !buildProgram(down_, "fullscreen.vert", "down.frag", error) ||
        !buildProgram(up_, "fullscreen.vert", "up.frag", error) ||
        !buildProgram(composite_, "fullscreen.vert", "composite.frag", error))
        return false;

    gl_.GenVertexArrays(1, &vao_);
    gl_.BindVertexArray(vao_);
    gl_.GenBuffers(1, &vbo_);
    gl_.BindBuffer(gl::ARRAY_BUFFER, vbo_);
    gl_.BufferData(gl::ARRAY_BUFFER, static_cast<GLsizeiptr>(kMaxBatchVertices * sizeof(Vertex)), nullptr,
                   gl::STREAM_DRAW);
    const GLsizei stride = sizeof(Vertex);
    auto attr = [&](GLuint index, GLint size, GLenum type, bool normalized, std::size_t offset) {
        gl_.EnableVertexAttribArray(index);
        gl_.VertexAttribPointer(index, size, type, normalized ? gl::TRUE_ : gl::FALSE_, stride,
                                reinterpret_cast<const void*>(offset));
    };
    attr(0, 2, gl::FLOAT, false, offsetof(Vertex, x));
    attr(1, 2, gl::FLOAT, false, offsetof(Vertex, px));
    attr(2, 4, gl::FLOAT, false, offsetof(Vertex, shape));
    attr(3, 4, gl::UNSIGNED_BYTE, true, offsetof(Vertex, c0));
    attr(4, 4, gl::UNSIGNED_BYTE, true, offsetof(Vertex, c1));
    attr(5, 2, gl::FLOAT, false, offsetof(Vertex, kind));
    verts_.reserve(kMaxBatchVertices);

    gl_.GenTextures(1, &atlasTex_);
    gl_.BindTexture(gl::TEXTURE_2D, atlasTex_);
    gl_.PixelStorei(gl::UNPACK_ALIGNMENT, 1);
    gl_.TexImage2D(gl::TEXTURE_2D, 0, static_cast<GLint>(gl::R8), FontAtlas::kSize, FontAtlas::kSize, 0, gl::RED,
                   gl::UNSIGNED_BYTE, font_.pixels().data());
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, static_cast<GLint>(gl::LINEAR));
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, static_cast<GLint>(gl::LINEAR));
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, static_cast<GLint>(gl::CLAMP_TO_EDGE));
    gl_.TexParameteri(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, static_cast<GLint>(gl::CLAMP_TO_EDGE));

    if (!createTargets()) {
        if (error)
            *error = "cannot create the render targets (RGBA16F framebuffers)";
        return false;
    }
    if (gl_.hasTimerQuery())
        gl_.GenQueries(2, queries_);
    return true;
}

bool GpuRenderer::resize(int w, int h, float scale) {
    const int fw = std::max(1, static_cast<int>(std::lround(static_cast<float>(w) * scale)));
    const int fh = std::max(1, static_cast<int>(std::lround(static_cast<float>(h) * scale)));
    if (w == w_ && h == h_ && fw == fbW_ && fh == fbH_) {
        scale_ = scale;
        return true;
    }
    w_ = w;
    h_ = h;
    scale_ = scale;
    fbW_ = fw;
    fbH_ = fh;
    destroyTargets();
    return createTargets();
}

// --- frame ---------------------------------------------------------------------------------------

void GpuRenderer::bindTarget(const Target* t, int w, int h) {
    gl_.BindFramebuffer(gl::FRAMEBUFFER, t ? t->fbo : 0);
    gl_.Viewport(0, 0, w, h);
}

void GpuRenderer::beginFrame(double timeSeconds) {
    time_ = timeSeconds;
    verts_.clear();
    clips_.clear();
    blend_ = Blend::Alpha;
    emission_ = 1.0f;
    drawCalls_ = 0;
    vertexCountFrame_ = 0;
    clearedThisFrame_ = false;
    if (queries_[0]) {
        // Poll every pending query, not only the one issued last frame: a result that is late by
        // more than one frame must still be collected, or both slots stay pending and timing stops.
        for (int i = 0; i < 2; ++i) {
            if (!queryPending_[i])
                continue;
            GLuint ready = 0;
            gl_.GetQueryObjectuiv(queries_[i], gl::QUERY_RESULT_AVAILABLE, &ready);
            if (ready) {
                GLuint64 ns = 0;
                gl_.GetQueryObjectui64v(queries_[i], gl::QUERY_RESULT, &ns);
                gpuMs_ = gpuMs_ * 0.8f + static_cast<float>(ns) * 1e-6f * 0.2f;
                queryPending_[i] = false;
            }
        }
        if (queryPending_[queryIndex_])
            queryIndex_ ^= 1;
        if (!queryPending_[queryIndex_]) {
            gl_.BeginQuery(gl::TIME_ELAPSED, queries_[queryIndex_]);
            queryPending_[queryIndex_] = true;
            queryRunning_ = true;
        }
    }
    bindTarget(&scene_, fbW_, fbH_);
    gl_.Disable(gl::SCISSOR_TEST);
    scissorOn_ = false;
    gl_.Disable(gl::DEPTH_TEST);
    gl_.Disable(gl::CULL_FACE);
    gl_.Enable(gl::BLEND);
    gl_.BlendFuncSeparate(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
}

void GpuRenderer::applyScissor() {
    if (clips_.empty()) {
        if (scissorOn_)
            gl_.Disable(gl::SCISSOR_TEST);
        scissorOn_ = false;
        return;
    }
    const Rect& r = clips_.back();
    const int x = static_cast<int>(std::floor(static_cast<float>(r.x) * scale_));
    const int y1 = static_cast<int>(std::ceil(static_cast<float>(r.bottom()) * scale_));
    const int x1 = static_cast<int>(std::ceil(static_cast<float>(r.right()) * scale_));
    const int y = static_cast<int>(std::floor(static_cast<float>(r.y) * scale_));
    gl_.Enable(gl::SCISSOR_TEST);
    gl_.Scissor(x, fbH_ - y1, std::max(0, x1 - x), std::max(0, y1 - y));
    scissorOn_ = true;
}

void GpuRenderer::flush() {
    if (verts_.empty())
        return;
    gl_.UseProgram(ui_.id);
    gl_.Uniform2f(ui_.res, static_cast<float>(fbW_), static_cast<float>(fbH_));
    gl_.Uniform1i(ui_.tex, 0);
    gl_.ActiveTexture(gl::TEXTURE0);
    gl_.BindTexture(gl::TEXTURE_2D, atlasTex_);
    gl_.BindVertexArray(vao_);
    gl_.BindBuffer(gl::ARRAY_BUFFER, vbo_);
    gl_.BufferData(gl::ARRAY_BUFFER, static_cast<GLsizeiptr>(kMaxBatchVertices * sizeof(Vertex)), nullptr,
                   gl::STREAM_DRAW); // orphan
    gl_.BufferSubData(gl::ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(verts_.size() * sizeof(Vertex)), verts_.data());
    if (blend_ == Blend::Alpha)
        gl_.BlendFuncSeparate(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
    else
        gl_.BlendFuncSeparate(gl::SRC_ALPHA, gl::ONE, gl::ONE, gl::ONE);
    applyScissor();
    gl_.DrawArrays(gl::TRIANGLES, 0, static_cast<GLsizei>(verts_.size()));
    ++drawCalls_;
    vertexCountFrame_ += static_cast<int>(verts_.size());
    verts_.clear();
}

void GpuRenderer::drawFullscreen() { gl_.DrawArrays(gl::TRIANGLES, 0, 3); }

void GpuRenderer::endFrame() {
    flush();
    if (scissorOn_)
        gl_.Disable(gl::SCISSOR_TEST);
    scissorOn_ = false;
    gl_.BindVertexArray(vao_); // attribute-less draws still need a VAO bound in core profile
    gl_.Disable(gl::BLEND);
    gl_.ActiveTexture(gl::TEXTURE0);

    const int levels = quality_.bloom ? std::clamp(quality_.bloomLevels, 1, kMaxBloom) : 0;
    const int first = levels > 1 && quality_.bloomDownscale ? 1 : 0; // the level the bright pass writes to
    if (levels > 0) {
        // bright pass into the first level, then the downsample chain
        bindTarget(&bloom_[first], bloom_[first].w, bloom_[first].h);
        gl_.UseProgram(bright_.id);
        gl_.BindTexture(gl::TEXTURE_2D, scene_.tex);
        gl_.Uniform1i(bright_.tex, 0);
        const float spread = static_cast<float>(1 << first); // a smaller target samples wider
        gl_.Uniform2f(bright_.texel, spread / static_cast<float>(scene_.w), spread / static_cast<float>(scene_.h));
        gl_.Uniform1f(bright_.threshold, look_.bloomThreshold);
        drawFullscreen();
        gl_.UseProgram(down_.id);
        gl_.Uniform1i(down_.tex, 0);
        for (int i = first + 1; i < levels; ++i) {
            bindTarget(&bloom_[i], bloom_[i].w, bloom_[i].h);
            gl_.BindTexture(gl::TEXTURE_2D, bloom_[i - 1].tex);
            gl_.Uniform2f(down_.texel, 1.0f / static_cast<float>(bloom_[i - 1].w),
                          1.0f / static_cast<float>(bloom_[i - 1].h));
            drawFullscreen();
        }
        // upsample chain, added onto the larger levels
        gl_.Enable(gl::BLEND);
        gl_.BlendFunc(gl::ONE, gl::ONE);
        gl_.UseProgram(up_.id);
        gl_.Uniform1i(up_.tex, 0);
        for (int i = levels - 1; i > first; --i) {
            bindTarget(&bloom_[i - 1], bloom_[i - 1].w, bloom_[i - 1].h);
            gl_.BindTexture(gl::TEXTURE_2D, bloom_[i].tex);
            gl_.Uniform2f(up_.texel, 1.0f / static_cast<float>(bloom_[i].w), 1.0f / static_cast<float>(bloom_[i].h));
            gl_.Uniform1f(up_.weight, 0.9f);
            drawFullscreen();
        }
        gl_.Disable(gl::BLEND);
    }

    // composite
    bindTarget(offscreen_ ? &out_ : nullptr, fbW_, fbH_);
    gl_.UseProgram(composite_.id);
    gl_.ActiveTexture(gl::TEXTURE0);
    gl_.BindTexture(gl::TEXTURE_2D, scene_.tex);
    gl_.Uniform1i(composite_.scene, 0);
    gl_.ActiveTexture(gl::TEXTURE0 + 1);
    gl_.BindTexture(gl::TEXTURE_2D, levels > 0 ? bloom_[first].tex : scene_.tex);
    gl_.Uniform1i(composite_.bloom, 1);
    gl_.Uniform1f(composite_.bloomStrength, levels > 0 ? look_.bloomStrength * quality_.bloomStrength : 0.0f);
    gl_.Uniform1f(composite_.grain, quality_.grain ? look_.grain : 0.0f);
    gl_.Uniform1f(composite_.vignette, look_.vignette);
    gl_.Uniform1f(composite_.time, static_cast<float>(std::fmod(time_, 100.0)));
    gl_.Uniform2f(composite_.res, static_cast<float>(fbW_), static_cast<float>(fbH_));
    drawFullscreen();
    gl_.ActiveTexture(gl::TEXTURE0 + 1);
    gl_.BindTexture(gl::TEXTURE_2D, 0);
    gl_.ActiveTexture(gl::TEXTURE0);

    if (queryRunning_) {
        gl_.EndQuery(gl::TIME_ELAPSED);
        queryRunning_ = false;
        queryIndex_ ^= 1;
    }
}

bool GpuRenderer::readPixels(std::vector<std::uint32_t>& out) {
    if (!offscreen_)
        return false;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(fbW_) * static_cast<std::size_t>(fbH_) * 4);
    gl_.BindFramebuffer(gl::FRAMEBUFFER, out_.fbo);
    gl_.PixelStorei(gl::PACK_ALIGNMENT, 1);
    gl_.ReadPixels(0, 0, fbW_, fbH_, gl::RGBA, gl::UNSIGNED_BYTE, raw.data());
    gl_.BindFramebuffer(gl::FRAMEBUFFER, 0);
    out.resize(static_cast<std::size_t>(fbW_) * static_cast<std::size_t>(fbH_));
    for (int y = 0; y < fbH_; ++y) {
        const std::uint8_t* row = &raw[static_cast<std::size_t>(fbH_ - 1 - y) * static_cast<std::size_t>(fbW_) * 4];
        for (int x = 0; x < fbW_; ++x)
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(fbW_) + static_cast<std::size_t>(x)] =
                0xFF000000u | (static_cast<std::uint32_t>(row[x * 4]) << 16) |
                (static_cast<std::uint32_t>(row[x * 4 + 1]) << 8) | row[x * 4 + 2];
    }
    return true;
}

// --- batching ------------------------------------------------------------------------------------

void GpuRenderer::emitVertex(float x, float y, float px, float py, const float (&shape)[4], Color c0, Color c1,
                             Kind kind) {
    if (verts_.size() >= kMaxBatchVertices)
        flush();
    Vertex v;
    v.x = x;
    v.y = y;
    v.px = px;
    v.py = py;
    std::memcpy(v.shape, shape, sizeof(shape));
    unpack(c0, v.c0);
    unpack(c1, v.c1);
    v.kind = static_cast<float>(kind);
    v.emit = emission_;
    verts_.push_back(v);
}

void GpuRenderer::emitQuad(const Corner (&c)[4], const float (&shape)[4], Color c1, Kind kind) {
    // corners: 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right
    static constexpr int order[6] = {0, 1, 2, 1, 3, 2};
    if (verts_.size() + 6 > kMaxBatchVertices)
        flush();
    for (int i : order)
        emitVertex(c[i].x, c[i].y, c[i].px, c[i].py, shape, c[i].c0, c1, kind);
}

void GpuRenderer::setBlend(Blend b) {
    if (b == blend_)
        return;
    flush();
    blend_ = b;
}

void GpuRenderer::pushClip(const Rect& r) {
    flush();
    clips_.push_back(clips_.empty() ? r : clips_.back().intersect(r));
}
void GpuRenderer::popClip() {
    flush();
    if (!clips_.empty())
        clips_.pop_back();
}

void GpuRenderer::clear(Color c) {
    flush();
    float rgb[3];
    linearColor(c, rgb);
    gl_.ClearColor(rgb[0], rgb[1], rgb[2], 1.0f);
    gl_.Clear(gl::COLOR_BUFFER_BIT);
    clearedThisFrame_ = true;
}

void GpuRenderer::drawBackground(Color a, Color b, float time) {
    flush();
    gl_.Disable(gl::BLEND);
    if (scissorOn_)
        gl_.Disable(gl::SCISSOR_TEST);
    scissorOn_ = false;
    gl_.UseProgram(bg_.id);
    gl_.Uniform2f(bg_.res, static_cast<float>(fbW_), static_cast<float>(fbH_));
    gl_.Uniform1f(bg_.time, quality_.animatedBackground ? time : 12.0f);
    float ca[3], cb[3];
    linearColor(a, ca);
    linearColor(b, cb);
    gl_.Uniform3f(bg_.colA, ca[0], ca[1], ca[2]);
    gl_.Uniform3f(bg_.colB, cb[0], cb[1], cb[2]);
    gl_.Uniform1f(bg_.pools, look_.backgroundPools);
    gl_.Uniform1f(bg_.dust, look_.backgroundDust);
    gl_.BindVertexArray(vao_);
    drawFullscreen();
    gl_.Enable(gl::BLEND);
    clearedThisFrame_ = true;
}

// --- shapes --------------------------------------------------------------------------------------

void GpuRenderer::box(float x, float y, float w, float h, float radius, float stroke, Color top, Color bottom) {
    if (w <= 0 || h <= 0)
        return;
    const float s = scale_;
    const float X = x * s, Y = y * s, W = w * s, H = h * s;
    const float hx = W * 0.5f, hy = H * 0.5f;
    const float shape[4] = {hx, hy, std::min(radius * s, std::min(hx, hy)), stroke * s};
    const float cx = X + hx, cy = Y + hy;
    const float ex = hx + kPad, ey = hy + kPad;
    const Corner c[4] = {{cx - ex, cy - ey, -ex, -ey, top},
                         {cx + ex, cy - ey, ex, -ey, top},
                         {cx - ex, cy + ey, -ex, ey, bottom},
                         {cx + ex, cy + ey, ex, ey, bottom}};
    emitQuad(c, shape, 0, kBox);
}

void GpuRenderer::fillRect(const Rect& r, Color c) {
    box(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), 0, 0, c, c);
}
void GpuRenderer::strokeRect(const Rect& r, Color c, float t) {
    box(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), 0, t, c, c);
}
void GpuRenderer::fillRoundRect(const Rect& r, float radius, Color c) {
    box(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), radius, 0,
        c, c);
}
void GpuRenderer::strokeRoundRect(const Rect& r, float radius, Color c, float t) {
    box(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), radius, t,
        c, c);
}
void GpuRenderer::fillGradientRect(const Rect& r, Color top, Color bottom) {
    box(static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h), 0, 0, top,
        bottom);
}

void GpuRenderer::drawLine(Point a, Point b, Color c, float thickness) {
    const float s = scale_;
    const float ax = a.x * s, ay = a.y * s, bx = b.x * s, by = b.y * s;
    const float dx = bx - ax, dy = by - ay;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) {
        fillCircle(a, std::max(0.5f, thickness * 0.5f), c);
        return;
    }
    const float ux = dx / len, uy = dy / len; // along
    const float vx = -uy, vy = ux;            // across
    const float r = std::max(0.5f, thickness * 0.5f * s);
    const float half = len * 0.5f, ext = half + r + kPad, wid = r + kPad;
    const float cx = (ax + bx) * 0.5f, cy = (ay + by) * 0.5f;
    const float shape[4] = {half, r, 0, 0};
    const Corner q[4] = {{cx - ux * ext - vx * wid, cy - uy * ext - vy * wid, -ext, -wid, c},
                         {cx + ux * ext - vx * wid, cy + uy * ext - vy * wid, ext, -wid, c},
                         {cx - ux * ext + vx * wid, cy - uy * ext + vy * wid, -ext, wid, c},
                         {cx + ux * ext + vx * wid, cy + uy * ext + vy * wid, ext, wid, c}};
    emitQuad(q, shape, 0, kCapsule);
}

void GpuRenderer::drawPolyline(const Point* pts, int count, Color c, float thickness) {
    if (count < 2)
        return;
    std::vector<Color> cols(static_cast<std::size_t>(count), c);
    std::vector<float> ws(static_cast<std::size_t>(count), thickness);
    drawStrip(pts, cols.data(), ws.data(), count);
}

void GpuRenderer::drawStrip(const Point* pts, const Color* colors, const float* widths, int count) {
    if (count < 2)
        return;
    const float s = scale_;
    struct P {
        float lx, ly, rx, ry, half, ext;
    };
    std::vector<P> ps(static_cast<std::size_t>(count));
    auto dir = [&](int i, int j, float& x, float& y) {
        x = (pts[j].x - pts[i].x) * s;
        y = (pts[j].y - pts[i].y) * s;
        const float l = std::sqrt(x * x + y * y);
        if (l > 1e-6f) {
            x /= l;
            y /= l;
        } else {
            x = 1;
            y = 0;
        }
    };
    for (int i = 0; i < count; ++i) {
        float x0, y0, x1, y1;
        dir(std::max(0, i - 1), i, x0, y0);
        dir(i, std::min(count - 1, i + 1), x1, y1);
        if (i == 0) {
            x0 = x1;
            y0 = y1;
        }
        if (i == count - 1) {
            x1 = x0;
            y1 = y0;
        }
        float tx = x0 + x1, ty = y0 + y1;
        const float tl = std::sqrt(tx * tx + ty * ty);
        if (tl < 1e-4f) {
            tx = x1;
            ty = y1;
        } else {
            tx /= tl;
            ty /= tl;
        }
        const float nx = -ty, ny = tx;
        const float n0x = -y0, n0y = x0;
        const float miter = 1.0f / std::max(0.5f, nx * n0x + ny * n0y);
        const float half = std::max(0.5f, widths[i] * 0.5f * s);
        const float e = (half + kPad) * miter;
        const float px = pts[i].x * s, py = pts[i].y * s;
        ps[static_cast<std::size_t>(i)] = {px + nx * e, py + ny * e, px - nx * e, py - ny * e, half, half + kPad};
    }
    for (int i = 0; i + 1 < count; ++i) {
        const P& a = ps[static_cast<std::size_t>(i)];
        const P& b = ps[static_cast<std::size_t>(i + 1)];
        if (verts_.size() + 6 > kMaxBatchVertices)
            flush();
        const float sa[4] = {a.half, 0, 0, 0}, sb[4] = {b.half, 0, 0, 0};
        const Color ca = colors[i], cb = colors[i + 1];
        emitVertex(a.lx, a.ly, 0, -a.ext, sa, ca, 0, kStrip);
        emitVertex(a.rx, a.ry, 0, a.ext, sa, ca, 0, kStrip);
        emitVertex(b.lx, b.ly, 0, -b.ext, sb, cb, 0, kStrip);
        emitVertex(a.rx, a.ry, 0, a.ext, sa, ca, 0, kStrip);
        emitVertex(b.rx, b.ry, 0, b.ext, sb, cb, 0, kStrip);
        emitVertex(b.lx, b.ly, 0, -b.ext, sb, cb, 0, kStrip);
    }
}

void GpuRenderer::fillUnder(const Point* pts, int count, float baselineY, Color top, Color bottom) {
    const float s = scale_, by = baselineY * s;
    const float none[4] = {};
    for (int i = 0; i + 1 < count; ++i) {
        if (verts_.size() + 6 > kMaxBatchVertices)
            flush();
        const float x0 = pts[i].x * s, y0 = pts[i].y * s, x1 = pts[i + 1].x * s, y1 = pts[i + 1].y * s;
        emitVertex(x0, y0, 0, 0, none, top, 0, kSolid);
        emitVertex(x1, y1, 0, 0, none, top, 0, kSolid);
        emitVertex(x0, by, 0, 0, none, bottom, 0, kSolid);
        emitVertex(x1, y1, 0, 0, none, top, 0, kSolid);
        emitVertex(x1, by, 0, 0, none, bottom, 0, kSolid);
        emitVertex(x0, by, 0, 0, none, bottom, 0, kSolid);
    }
}

void GpuRenderer::fillCircle(Point center, float radius, Color c) {
    const float s = scale_;
    const float r = radius * s, cx = center.x * s, cy = center.y * s, e = r + kPad;
    const float shape[4] = {r, 0, 0, 0};
    const Corner q[4] = {
        {cx - e, cy - e, -e, -e, c}, {cx + e, cy - e, e, -e, c}, {cx - e, cy + e, -e, e, c}, {cx + e, cy + e, e, e, c}};
    emitQuad(q, shape, 0, kCircle);
}

void GpuRenderer::strokeCircle(Point center, float radius, Color c, float thickness) {
    const float s = scale_;
    const float r = radius * s, cx = center.x * s, cy = center.y * s, e = r + kPad;
    const float shape[4] = {r + thickness * s * 0.5f, thickness * s, 0, 0}; // stroke centerd on the radius
    const float ee = e + thickness * s * 0.5f;
    const Corner q[4] = {{cx - ee, cy - ee, -ee, -ee, c},
                         {cx + ee, cy - ee, ee, -ee, c},
                         {cx - ee, cy + ee, -ee, ee, c},
                         {cx + ee, cy + ee, ee, ee, c}};
    emitQuad(q, shape, 0, kCircle);
}

void GpuRenderer::drawArc(Point center, float radius, float a0, float a1, Color c, float thickness) {
    if (a1 <= a0)
        return;
    const float s = scale_;
    const float r = radius * s, ht = std::max(0.5f, thickness * 0.5f * s);
    const float cx = center.x * s, cy = center.y * s, e = r + ht + kPad;
    const float shape[4] = {r, ht, a0, std::min(a1, a0 + 6.2831853f)};
    const Corner q[4] = {
        {cx - e, cy - e, -e, -e, c}, {cx + e, cy - e, e, -e, c}, {cx - e, cy + e, -e, e, c}, {cx + e, cy + e, e, e, c}};
    emitQuad(q, shape, 0, kArc);
}

void GpuRenderer::drawGlow(Point center, float radius, Color c) {
    const float s = scale_;
    const float r = radius * s, cx = center.x * s, cy = center.y * s;
    const float shape[4] = {r, 0, 0, 0};
    const Corner q[4] = {
        {cx - r, cy - r, -r, -r, c}, {cx + r, cy - r, r, -r, c}, {cx - r, cy + r, -r, r, c}, {cx + r, cy + r, r, r, c}};
    emitQuad(q, shape, 0, kGlow);
}

void GpuRenderer::fillRadialGradient(Point center, float radius, Color inner, Color outer) {
    const float s = scale_;
    const float r = radius * s, cx = center.x * s, cy = center.y * s, e = r + kPad;
    const float shape[4] = {r, 0, 0, 0};
    const Corner q[4] = {{cx - e, cy - e, -e, -e, inner},
                         {cx + e, cy - e, e, -e, inner},
                         {cx - e, cy + e, -e, e, inner},
                         {cx + e, cy + e, e, e, inner}};
    emitQuad(q, shape, outer, kRadial);
}

void GpuRenderer::drawShadow(const Rect& r, float radius, float blur, Color c) {
    const float s = scale_;
    const float sigma = std::max(1.0f, blur * s * 0.5f);
    const float hx = static_cast<float>(r.w) * s * 0.5f, hy = static_cast<float>(r.h) * s * 0.5f;
    const float cx = static_cast<float>(r.x) * s + hx, cy = static_cast<float>(r.y) * s + hy;
    const float ex = hx + sigma * 2.5f, ey = hy + sigma * 2.5f;
    const float shape[4] = {hx, hy, radius * s, sigma};
    const Corner q[4] = {{cx - ex, cy - ey, -ex, -ey, c},
                         {cx + ex, cy - ey, ex, -ey, c},
                         {cx - ex, cy + ey, -ex, ey, c},
                         {cx + ex, cy + ey, ex, ey, c}};
    emitQuad(q, shape, 0, kShadow);
}

void GpuRenderer::drawPanel(const Rect& r, float radius, Color tint) {
    const float x = static_cast<float>(r.x), y = static_cast<float>(r.y), w = static_cast<float>(r.w),
                h = static_cast<float>(r.h);
    drawShadow({r.x, r.y + 4, r.w, r.h}, radius, 18.0f, 0xB0000000u);
    // body: tint, brighter at the top like light through glass
    box(x, y, w, h, radius, 0, mix(tint, 0xFFFFFFFFu, 0.07f), tint);
    // rim: a hairline that is lit from the top
    box(x, y, w, h, radius, 1.0f, withAlpha(0xFFFFFFFFu, 0.22f), withAlpha(0xFFFFFFFFu, 0.05f));
}

void GpuRenderer::drawTrace(const float* values, int n, const Rect& area, float lo, float hi, Color c) {
    if (n < 2 || area.empty() || hi == lo)
        return;
    std::vector<Point> pts(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float t = std::clamp((values[i] - lo) / (hi - lo), 0.0f, 1.0f);
        pts[static_cast<std::size_t>(i)] = {
            static_cast<float>(area.x) + static_cast<float>(area.w) * static_cast<float>(i) / static_cast<float>(n - 1),
            static_cast<float>(area.bottom()) - t * static_cast<float>(area.h)};
    }
    const Blend prev = blend_;
    const float prevEmit = emission_;
    setBlend(Blend::Add);
    fillUnder(pts.data(), n, static_cast<float>(area.bottom()), withAlpha(c, 0.28f), withAlpha(c, 0.0f));
    emission_ = 0.9f;
    drawPolyline(pts.data(), n, withAlpha(c, 0.30f), 5.0f);
    emission_ = 1.6f;
    drawPolyline(pts.data(), n, c, 1.6f);
    emission_ = prevEmit;
    setBlend(prev);
}

void GpuRenderer::drawMeter(const Rect& area, float level, Color c) {
    if (area.empty())
        return;
    const float l = std::clamp(level, 0.0f, 1.0f);
    const float r = std::min(static_cast<float>(area.w), static_cast<float>(area.h)) * 0.5f;
    box(static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.w), static_cast<float>(area.h),
        r, 0, withAlpha(0xFFFFFFFFu, 0.06f), withAlpha(0xFFFFFFFFu, 0.04f));
    const float fh = static_cast<float>(area.h) * l;
    if (fh < 1.0f)
        return;
    const float prevEmit = emission_;
    emission_ = 1.4f;
    box(static_cast<float>(area.x), static_cast<float>(area.bottom()) - fh, static_cast<float>(area.w), fh, r, 0, c,
        mix(c, 0xFF000000u, 0.45f));
    emission_ = prevEmit;
}

// --- text ----------------------------------------------------------------------------------------

float GpuRenderer::textWidthPx(const char* text, float px, FontWeight weight) const {
    return text ? font_.width(weight, text) * px : 0.0f;
}
int GpuRenderer::textWidth(const char* text, int scale) const {
    return static_cast<int>(std::ceil(textWidthPx(text, 9.0f * static_cast<float>(scale))));
}
int GpuRenderer::textHeight(int scale) const {
    return static_cast<int>(std::ceil(FontAtlas::kCapHeight * 9.0f * static_cast<float>(scale)));
}
void GpuRenderer::drawText(const char* text, int x, int y, Color c, int scale) {
    drawTextPx(text, static_cast<float>(x), static_cast<float>(y), c, 9.0f * static_cast<float>(scale));
}

void GpuRenderer::drawTextPx(const char* text, float x, float y, Color c, float px, Align align, FontWeight weight) {
    if (!text || !*text)
        return;
    float w = textWidthPx(text, px, weight);
    if (align == Align::Center)
        x -= w * 0.5f;
    else if (align == Align::Right)
        x -= w;
    const float s = scale_;
    const float baseline = y + FontAtlas::kCapHeight * px;
    // small text is thickened a little: a hairline stem at 10 px would drop out of the distance field
    const float small = std::max(0.0f, 14.0f - px);
    const float shape[4] = {(weight == FontWeight::Bold ? 0.0f : 0.02f) + small * 0.012f, 0, 0, 0};
    float pen = x;
    const std::string_view sv(text);
    for (std::size_t i = 0; i < sv.size();) {
        const FontAtlas::Glyph& g = font_.glyph(weight, FontAtlas::nextCodePoint(sv, i));
        if (g.valid) {
            const float x0 = (pen + g.x0 * px) * s, x1 = (pen + g.x1 * px) * s;
            const float y0 = (baseline + g.y0 * px) * s, y1 = (baseline + g.y1 * px) * s;
            const Corner q[4] = {
                {x0, y0, g.u0, g.v0, c}, {x1, y0, g.u1, g.v0, c}, {x0, y1, g.u0, g.v1, c}, {x1, y1, g.u1, g.v1, c}};
            emitQuad(q, shape, 0, kText);
        }
        pen += g.advance * px;
    }
}

} // namespace spux::inline v1::ui::modern
