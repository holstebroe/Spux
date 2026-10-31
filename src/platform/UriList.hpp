// A text/uri-list (the XDND file drop format) as local paths: "file:///a%20b.syx\r\nfile:///c.mid". Lines that are
// not file URIs are skipped, as are comments; "file://host/path" keeps the path.
#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace spux::inline v1::platform {

inline std::vector<std::filesystem::path> uriListPaths(const std::string& list) {
    std::vector<std::filesystem::path> out;
    std::size_t from = 0;
    while (from < list.size()) {
        std::size_t to = list.find('\n', from);
        if (to == std::string::npos)
            to = list.size();
        std::string line = list.substr(from, to - from);
        from = to + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == '\0'))
            line.pop_back();
        if (line.compare(0, 7, "file://") != 0)
            continue;
        line = line.substr(7);
        if (!line.empty() && line[0] != '/')
            line = line.substr(std::min(line.find('/'), line.size())); // file://host/path
        std::string path;
        for (std::size_t i = 0; i < line.size(); ++i) {
            auto hex = [](char c) {
                return c >= '0' && c <= '9'                     ? c - '0'
                       : (c | 0x20) >= 'a' && (c | 0x20) <= 'f' ? (c | 0x20) - 'a' + 10
                                                                : -1;
            };
            if (line[i] == '%' && i + 2 < line.size() + 0 && hex(line[i + 1]) >= 0 && hex(line[i + 2]) >= 0) {
                path += static_cast<char>(hex(line[i + 1]) * 16 + hex(line[i + 2]));
                i += 2;
            } else {
                path += line[i];
            }
        }
        if (!path.empty())
            out.emplace_back(path);
    }
    return out;
}

} // namespace spux::inline v1::platform
