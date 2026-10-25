// Factory presets kept as a folder of text files and compiled into the plugin.
//
// The folder is the source of truth: a directory per group and a file per preset, named `NN NAME.txt` (the `NN `
// prefixes only give the order and are not part of the names), each file one preset's `text`, whatever format the
// plugin keeps in it (a key=value preset, see spux/state/TextPreset.hpp, or anything else). tools/bake_presets.py
// turns the folder into a table that is checked in and passed to the library; the plugin's build checks that the
// table still matches the folder (cmake/SpuxPresets.cmake).
//
// A development build gives the library the folder as well: it then reads the presets from the files, and can write
// one back or add one, so presets are edited by ear in the plugin and the files committed. Without a folder (every
// release build) the library only serves the table. There is no runtime switch.
//
// Not real-time; one thread (the editor's) at a time. `rescan()` and `create()` invalidate references to presets.
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "spux/Version.hpp"

namespace spux::inline v1::presets {

struct Preset {
    std::string group;
    std::string name;
    std::string text;
    std::filesystem::path file; // empty unless read from the folder
};

class PresetLibrary {
public:
    // `baked`: the compiled table (outlives the library). `folder`: the development folder, empty in a release
    // build; used when it exists and holds at least one preset. `extension`: the preset files' extension.
    explicit PresetLibrary(const std::vector<Preset>& baked, std::filesystem::path folder = {},
                           std::string extension = ".txt")
        : baked_(&baked), folder_(std::move(folder)), extension_(std::move(extension)) {
        rescan();
    }

    [[nodiscard]] int count() const { return static_cast<int>(presets().size()); }
    [[nodiscard]] const Preset& at(int index) const { return presets()[static_cast<std::size_t>(index)]; }
    // "GROUP: NAME".
    [[nodiscard]] std::string label(int index) const { return at(index).group + ": " + at(index).name; }
    // The preset with this label, or -1.
    [[nodiscard]] int find(const std::string& label) const {
        for (int i = 0; i < count(); ++i)
            if (this->label(i) == label)
                return i;
        return -1;
    }

    // True when the presets were read from the folder (so save and create work).
    [[nodiscard]] bool fromFiles() const { return !files_.empty(); }

    // Reads the folder again, if there is one.
    void rescan() {
        files_.clear();
        std::error_code ec;
        if (folder_.empty() || !std::filesystem::is_directory(folder_, ec))
            return;
        for (const auto& gdir : sorted(folder_, true))
            for (const auto& file : sorted(gdir, false))
                files_.push_back({withoutNumber(gdir.filename().string()), withoutNumber(file.stem().string()),
                                  trimNewlines(readFile(file)), file});
    }

    // Writes `text` to the preset's file. Returns "" or what went wrong. Nothing is written if the file no longer
    // holds what was read (it was edited elsewhere): rescan first.
    std::string save(int index, const std::string& text) {
        if (!fromFiles() || index < 0 || index >= count())
            return "NOT A FILE PRESET";
        Preset& p = files_[static_cast<std::size_t>(index)];
        if (trimNewlines(readFile(p.file)) != p.text)
            return "FILE CHANGED ON DISK: RESCAN FIRST";
        std::ofstream f(p.file, std::ios::binary | std::ios::trunc);
        f << text << '\n';
        f.close();
        if (!f)
            return "COULD NOT WRITE " + p.file.filename().string();
        p.text = trimNewlines(text);
        return "";
    }

    // Adds a preset with `text` to `group` as the file `NN <name><extension>` (NN: the next free number in the group; a
    // number is appended to `name` while that file name exists). Returns the new preset's index, or -1 (no folder, or
    // no such group).
    int create(const std::string& group, const std::string& text, const std::string& name = "NEW PRESET") {
        if (!fromFiles())
            return -1;
        for (const auto& gdir : sorted(folder_, true)) {
            if (withoutNumber(gdir.filename().string()) != group)
                continue;
            int next = 1;
            for (const auto& file : sorted(gdir, false))
                next = std::max(next, std::atoi(file.filename().string().c_str()) + 1);
            char prefix[16];
            std::snprintf(prefix, sizeof prefix, "%02d ", next);
            std::string unique = name;
            for (int k = 2; std::filesystem::exists(gdir / (prefix + unique + extension_)); ++k)
                unique = name + " " + std::to_string(k);
            const std::filesystem::path path = gdir / (prefix + unique + extension_);
            std::ofstream f(path, std::ios::binary);
            f << text << '\n';
            f.close();
            if (!f)
                return -1;
            rescan();
            for (int i = 0; i < count(); ++i)
                if (at(i).file == path)
                    return i;
            return -1;
        }
        return -1;
    }

private:
    [[nodiscard]] const std::vector<Preset>& presets() const { return files_.empty() ? *baked_ : files_; }

    // "NN NAME" -> "NAME".
    static std::string withoutNumber(const std::string& s) {
        std::size_t i = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9')
            ++i;
        return i > 0 && i < s.size() && s[i] == ' ' ? s.substr(i + 1) : s;
    }
    static std::string trimNewlines(const std::string& s) {
        const auto a = s.find_first_not_of('\n'), b = s.find_last_not_of('\n');
        return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
    }
    static std::string readFile(const std::filesystem::path& p) {
        std::ifstream f(p, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string s = ss.str();
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        return s;
    }
    // Directories, or files with the extension, of `dir` in name order.
    std::vector<std::filesystem::path> sorted(const std::filesystem::path& dir, bool directories) const {
        std::vector<std::filesystem::path> out;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            if (directories ? e.is_directory(ec) : (e.is_regular_file(ec) && e.path().extension() == extension_))
                out.push_back(e.path());
        }
        std::sort(out.begin(), out.end(),
                  [](const auto& a, const auto& b) { return a.filename().string() < b.filename().string(); });
        return out;
    }

    const std::vector<Preset>* baked_;
    std::filesystem::path folder_;
    std::string extension_;
    std::vector<Preset> files_;
};

} // namespace spux::inline v1::presets
