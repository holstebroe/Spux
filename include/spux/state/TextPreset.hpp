// Human-readable user preset files: a header line, then `key=value` lines, keyed by the
// stable parameter key (never the id), so a file can be read, diffed and edited by hand.
//
//   # MyReverb preset v1
//   name=Long Hall
//   program=HALL
//   decay=0.8073888172
//
// Reading: lines are trimmed; blank lines and lines starting with '#' are comments, except that the first
// line `# <format> v<version>` names the format and its version; a line without '=' is skipped; key and
// value are trimmed at the first '='. Later lines for the same key win (find() returns the last one).
// What a key means, which keys are required and what a missing key keeps is the plugin's business.
//
// Numbers are written and read without the C locale (std::to_chars, std::from_chars), so a file
// saved under a locale with a decimal comma loads everywhere. Not real-time.
#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "spux/Version.hpp"

namespace spux::inline v1::state {

struct TextPreset {
    std::string format;                                       // "MyReverb preset"; empty when the file has no header
    std::uint32_t version = 0;                                // from the header; 0 when there is none
    std::vector<std::pair<std::string, std::string>> entries; // in file order

    [[nodiscard]] const std::string* find(std::string_view key) const {
        for (auto it = entries.rbegin(); it != entries.rend(); ++it)
            if (it->first == key)
                return &it->second;
        return nullptr;
    }
    void set(std::string key, std::string value) { entries.emplace_back(std::move(key), std::move(value)); }
};

namespace detail {
inline std::string_view trim(std::string_view s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string_view::npos)
        return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
} // namespace detail

// A number as printf("%.*g") in the C locale; `digits` 17 keeps every double exactly.
inline std::string formatNumber(double v, int digits = 17) {
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::general, digits);
    return r.ec == std::errc() ? std::string(buf, r.ptr) : std::string("0");
}

// The number at the start of `text` (a leading '+' is accepted, trailing text such as a unit is
// ignored); false when there is none.
inline bool parseNumber(std::string_view text, double& out) {
    text = detail::trim(text);
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);
    double v = 0.0;
    const auto r = std::from_chars(text.data(), text.data() + text.size(), v);
    if (r.ec != std::errc() || r.ptr == text.data())
        return false;
    out = v;
    return true;
}

// False when the text holds no `key=value` line at all.
inline bool parseTextPreset(std::string_view text, TextPreset& out) {
    TextPreset p;
    bool first = true;
    while (!text.empty()) {
        const auto nl = text.find('\n');
        const std::string_view raw = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        const std::string_view line = detail::trim(raw);
        if (line.empty())
            continue;
        if (line.front() == '#') {
            if (first) { // "# <format> v<version>"
                const std::string_view h = detail::trim(line.substr(1));
                const auto sp = h.rfind(" v");
                std::uint32_t version = 0;
                if (sp != std::string_view::npos) {
                    const auto r = std::from_chars(h.data() + sp + 2, h.data() + h.size(), version);
                    if (r.ec == std::errc() && r.ptr == h.data() + h.size() && version > 0) {
                        p.format = std::string(detail::trim(h.substr(0, sp)));
                        p.version = version;
                    }
                }
            }
            first = false;
            continue;
        }
        first = false;
        const auto eq = line.find('=');
        if (eq == std::string_view::npos)
            continue;
        p.set(std::string(detail::trim(line.substr(0, eq))), std::string(detail::trim(line.substr(eq + 1))));
    }
    if (p.entries.empty())
        return false;
    out = std::move(p);
    return true;
}

// The header (when `format` is not empty), then one line per entry in order. Keys and values must not
// hold a line break; values are written as given (use formatNumber for numbers).
inline std::string writeTextPreset(const TextPreset& p) {
    std::string out;
    if (!p.format.empty())
        out += "# " + p.format + " v" + std::to_string(p.version) + "\n";
    for (const auto& [key, value] : p.entries)
        out += key + "=" + value + "\n";
    return out;
}

} // namespace spux::inline v1::state
