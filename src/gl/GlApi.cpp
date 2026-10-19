#include "spux/gl/GlApi.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace spux::inline v1::gl {

namespace {
bool isOptional(const char* n) {
    for (const char* o :
         {"GenQueries", "DeleteQueries", "BeginQuery", "EndQuery", "GetQueryObjectuiv", "GetQueryObjectui64v"})
        if (!std::strcmp(n, o))
            return true;
    return false;
}
} // namespace

bool GlApi::load(GetProc getProc, void* user) {
    bool ok = true;
#define SPUX_GL_LOAD(ret, name, ...)                                                                                   \
    name = reinterpret_cast<ret(SPUX_GLAPIENTRY*)(__VA_ARGS__)>(getProc(user, "gl" #name));                            \
    if (!name && !isOptional(#name))                                                                                   \
        ok = false;
    SPUX_GL_FUNCTIONS(SPUX_GL_LOAD)
#undef SPUX_GL_LOAD
    if (!ok)
        return false;
    if (!GenQueries || !DeleteQueries || !BeginQuery || !EndQuery || !GetQueryObjectuiv || !GetQueryObjectui64v) {
        GenQueries = nullptr; // timer queries all or nothing
        GetQueryObjectui64v = nullptr;
    }

    const char* v = reinterpret_cast<const char*>(GetString(VERSION));
    const char* r = reinterpret_cast<const char*>(GetString(RENDERER));
    std::snprintf(version, sizeof(version), "%s", v ? v : "");
    std::snprintf(renderer, sizeof(renderer), "%s", r ? r : "");
    GLint ma = 0, mi = 0;
    GetIntegerv(MAJOR_VERSION, &ma);
    GetIntegerv(MINOR_VERSION, &mi);
    if (GetError() != NO_ERROR_ || ma == 0)
        std::sscanf(version, "%d.%d", &ma, &mi); // pre-3.0 contexts have no such query
    major = ma;
    minor = mi;
    return true;
}

bool GlApi::softwareRenderer() const {
    std::string r = renderer;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return r.find("llvmpipe") != std::string::npos || r.find("softpipe") != std::string::npos ||
           r.find("swiftshader") != std::string::npos || r.find("basic render") != std::string::npos ||
           r.find("software") != std::string::npos || r.find("gdi generic") != std::string::npos;
}

} // namespace spux::inline v1::gl
