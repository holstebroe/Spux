// OpenGL 3.3 core function table, loaded at run time through the platform's getProcAddress (spec 11).
// No GL headers or libraries are needed to build, and nothing is process-global: every device owns its
// own table, so several editors in one host (each with its own context) never share state. Only the
// functions the renderer uses are listed.
#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define SPUX_GLAPIENTRY __stdcall
#else
#define SPUX_GLAPIENTRY
#endif

namespace spux::inline v1::gl {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLfloat = float;
using GLchar = char;
using GLubyte = unsigned char;
using GLintptr = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;
using GLuint64 = std::uint64_t;

// Constants of the GL enums the renderer uses (spux::gl::TRIANGLES ...). Trailing underscores avoid
// the macros of the Windows headers.
inline constexpr GLenum FALSE_ = 0, TRUE_ = 1;
inline constexpr GLenum DEPTH_BUFFER_BIT = 0x100, COLOR_BUFFER_BIT = 0x4000;
inline constexpr GLenum TRIANGLES = 4, TRIANGLE_STRIP = 5;
inline constexpr GLenum ZERO = 0, ONE = 1, SRC_ALPHA = 0x302, ONE_MINUS_SRC_ALPHA = 0x303;
inline constexpr GLenum BLEND = 0xBE2, SCISSOR_TEST = 0xC11, DEPTH_TEST = 0xB71, CULL_FACE = 0xB44;
inline constexpr GLenum UNPACK_ALIGNMENT = 0xCF5, PACK_ALIGNMENT = 0xD05;
inline constexpr GLenum UNSIGNED_BYTE = 0x1401, UNSIGNED_SHORT = 0x1403, UNSIGNED_INT = 0x1405, FLOAT = 0x1406;
inline constexpr GLenum RED = 0x1903, RGB = 0x1907, RGBA = 0x1908;
inline constexpr GLenum R8 = 0x8229, RGBA8 = 0x8058, RGBA16F = 0x881A, SRGB8_ALPHA8 = 0x8C43, HALF_FLOAT = 0x140B;
inline constexpr GLenum TEXTURE_2D = 0xDE1, TEXTURE0 = 0x84C0;
inline constexpr GLenum TEXTURE_MAG_FILTER = 0x2800, TEXTURE_MIN_FILTER = 0x2801, TEXTURE_WRAP_S = 0x2802,
                        TEXTURE_WRAP_T = 0x2803;
inline constexpr GLenum NEAREST = 0x2600, LINEAR = 0x2601, CLAMP_TO_EDGE = 0x812F, REPEAT = 0x2901;
inline constexpr GLenum ARRAY_BUFFER = 0x8892, ELEMENT_ARRAY_BUFFER = 0x8893, STREAM_DRAW = 0x88E0,
                        STATIC_DRAW = 0x88E4;
inline constexpr GLenum FRAGMENT_SHADER = 0x8B30, VERTEX_SHADER = 0x8B31, COMPILE_STATUS = 0x8B81, LINK_STATUS = 0x8B82;
inline constexpr GLenum INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum FRAMEBUFFER = 0x8D40, COLOR_ATTACHMENT0 = 0x8CE0, FRAMEBUFFER_COMPLETE = 0x8CD5;
inline constexpr GLenum READ_FRAMEBUFFER = 0x8CA8, DRAW_FRAMEBUFFER = 0x8CA9;
inline constexpr GLenum VENDOR = 0x1F00, RENDERER = 0x1F01, VERSION = 0x1F02, SHADING_LANGUAGE_VERSION = 0x8B8C;
inline constexpr GLenum MAJOR_VERSION = 0x821B, MINOR_VERSION = 0x821C, MAX_TEXTURE_SIZE = 0xD33;
inline constexpr GLenum TIME_ELAPSED = 0x88BF, QUERY_RESULT = 0x8866, QUERY_RESULT_AVAILABLE = 0x8867;
inline constexpr GLenum NO_ERROR_ = 0;

#define SPUX_GL_FUNCTIONS(X)                                                                                           \
    X(void, GetIntegerv, GLenum, GLint*)                                                                               \
    X(const GLubyte*, GetString, GLenum)                                                                               \
    X(GLenum, GetError)                                                                                                \
    X(void, Enable, GLenum)                                                                                            \
    X(void, Disable, GLenum)                                                                                           \
    X(void, BlendFunc, GLenum, GLenum)                                                                                 \
    X(void, BlendFuncSeparate, GLenum, GLenum, GLenum, GLenum)                                                         \
    X(void, Viewport, GLint, GLint, GLsizei, GLsizei)                                                                  \
    X(void, Scissor, GLint, GLint, GLsizei, GLsizei)                                                                   \
    X(void, ClearColor, GLfloat, GLfloat, GLfloat, GLfloat)                                                            \
    X(void, Clear, GLbitfield)                                                                                         \
    X(void, DrawArrays, GLenum, GLint, GLsizei)                                                                        \
    X(void, Finish)                                                                                                    \
    X(void, Flush)                                                                                                     \
    X(void, ReadPixels, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)                                         \
    X(void, PixelStorei, GLenum, GLint)                                                                                \
    X(void, GenTextures, GLsizei, GLuint*)                                                                             \
    X(void, DeleteTextures, GLsizei, const GLuint*)                                                                    \
    X(void, BindTexture, GLenum, GLuint)                                                                               \
    X(void, TexImage2D, GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)                    \
    X(void, TexSubImage2D, GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)                 \
    X(void, TexParameteri, GLenum, GLenum, GLint)                                                                      \
    X(void, ActiveTexture, GLenum)                                                                                     \
    X(void, GenBuffers, GLsizei, GLuint*)                                                                              \
    X(void, DeleteBuffers, GLsizei, const GLuint*)                                                                     \
    X(void, BindBuffer, GLenum, GLuint)                                                                                \
    X(void, BufferData, GLenum, GLsizeiptr, const void*, GLenum)                                                       \
    X(void, BufferSubData, GLenum, GLintptr, GLsizeiptr, const void*)                                                  \
    X(void, GenVertexArrays, GLsizei, GLuint*)                                                                         \
    X(void, DeleteVertexArrays, GLsizei, const GLuint*)                                                                \
    X(void, BindVertexArray, GLuint)                                                                                   \
    X(void, EnableVertexAttribArray, GLuint)                                                                           \
    X(void, VertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)                               \
    X(GLuint, CreateShader, GLenum)                                                                                    \
    X(void, ShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*)                                         \
    X(void, CompileShader, GLuint)                                                                                     \
    X(void, GetShaderiv, GLuint, GLenum, GLint*)                                                                       \
    X(void, GetShaderInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                                                      \
    X(void, DeleteShader, GLuint)                                                                                      \
    X(GLuint, CreateProgram)                                                                                           \
    X(void, AttachShader, GLuint, GLuint)                                                                              \
    X(void, BindAttribLocation, GLuint, GLuint, const GLchar*)                                                         \
    X(void, LinkProgram, GLuint)                                                                                       \
    X(void, GetProgramiv, GLuint, GLenum, GLint*)                                                                      \
    X(void, GetProgramInfoLog, GLuint, GLsizei, GLsizei*, GLchar*)                                                     \
    X(void, UseProgram, GLuint)                                                                                        \
    X(void, DeleteProgram, GLuint)                                                                                     \
    X(GLint, GetUniformLocation, GLuint, const GLchar*)                                                                \
    X(void, Uniform1i, GLint, GLint)                                                                                   \
    X(void, Uniform1f, GLint, GLfloat)                                                                                 \
    X(void, Uniform2f, GLint, GLfloat, GLfloat)                                                                        \
    X(void, Uniform3f, GLint, GLfloat, GLfloat, GLfloat)                                                               \
    X(void, Uniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)                                                      \
    X(void, GenFramebuffers, GLsizei, GLuint*)                                                                         \
    X(void, DeleteFramebuffers, GLsizei, const GLuint*)                                                                \
    X(void, BindFramebuffer, GLenum, GLuint)                                                                           \
    X(void, FramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint)                                               \
    X(GLenum, CheckFramebufferStatus, GLenum)                                                                          \
    X(void, GenQueries, GLsizei, GLuint*)                                                                              \
    X(void, DeleteQueries, GLsizei, const GLuint*)                                                                     \
    X(void, BeginQuery, GLenum, GLuint)                                                                                \
    X(void, EndQuery, GLenum)                                                                                          \
    X(void, GetQueryObjectuiv, GLuint, GLenum, GLuint*)                                                                \
    X(void, GetQueryObjectui64v, GLuint, GLenum, GLuint64*)

struct GlApi {
#define SPUX_GL_DECL(ret, name, ...) ret(SPUX_GLAPIENTRY* name)(__VA_ARGS__) = nullptr;
    SPUX_GL_FUNCTIONS(SPUX_GL_DECL)
#undef SPUX_GL_DECL

    using GetProc = void* (*)(void* user, const char* name);
    // Loads every function; false when a required one is missing (the queries are optional).
    bool load(GetProc getProc, void* user);
    bool hasTimerQuery() const { return GenQueries && GetQueryObjectui64v; }

    int major = 0, minor = 0;
    char renderer[128] = {};
    char version[64] = {};
    // true for a software rasteriser (llvmpipe, Microsoft Basic Render Driver, SwiftShader...)
    bool softwareRenderer() const;
};

} // namespace spux::inline v1::gl
