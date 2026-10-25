// spux_presets: the preset library over a folder (numbered groups and files, save, create) and over a baked table.
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "spux/presets/PresetLibrary.hpp"

namespace spux_test {
extern const std::vector<spux::presets::Preset> kBakedTestPresets;
}

using namespace spux::presets;

namespace {

std::string slurp(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// A scratch copy of the test folder.
struct Scratch {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "spux_presets_test";
    Scratch() {
        std::filesystem::remove_all(dir);
        std::filesystem::copy(SPUX_TEST_PRESETS_DIR, dir, std::filesystem::copy_options::recursive);
    }
    ~Scratch() { std::filesystem::remove_all(dir); }
};

} // namespace

TEST_CASE("without a folder the library serves the baked table") {
    PresetLibrary lib(spux_test::kBakedTestPresets);
    CHECK(!lib.fromFiles());
    REQUIRE(lib.count() == 3);
    CHECK(lib.label(0) == "FIRST: ALPHA");
    CHECK(lib.at(1).name == "BETA");
    CHECK(lib.at(2).group == "SECOND");
    CHECK(lib.at(0).text == "# Test preset v1\nname=Alpha\nvalue=1");
    CHECK(lib.find("SECOND: GAMMA") == 2);
    CHECK(lib.find("SECOND: NOPE") == -1);
    CHECK(lib.save(0, "x") == "NOT A FILE PRESET");
    CHECK(lib.create("FIRST", "x") == -1);
}

TEST_CASE("a folder is read in order, without the number prefixes, and matches the baked table") {
    PresetLibrary lib(spux_test::kBakedTestPresets, SPUX_TEST_PRESETS_DIR);
    REQUIRE(lib.fromFiles());
    REQUIRE(lib.count() == 3);
    for (int i = 0; i < 3; ++i) {
        CHECK(lib.at(i).group == spux_test::kBakedTestPresets[i].group);
        CHECK(lib.at(i).name == spux_test::kBakedTestPresets[i].name);
        CHECK(lib.at(i).text == spux_test::kBakedTestPresets[i].text);
        CHECK(!lib.at(i).file.empty());
    }
}

TEST_CASE("a missing or empty folder falls back to the baked table") {
    PresetLibrary missing(spux_test::kBakedTestPresets, "/no/such/folder");
    CHECK(!missing.fromFiles());
    CHECK(missing.count() == 3);
}

TEST_CASE("save writes the file, and refuses a file that changed on disk") {
    Scratch s;
    PresetLibrary lib(spux_test::kBakedTestPresets, s.dir);
    REQUIRE(lib.fromFiles());
    CHECK(lib.save(1, "# Test preset v1\nname=Beta\nvalue=22") == "");
    CHECK(slurp(lib.at(1).file) == "# Test preset v1\nname=Beta\nvalue=22\n");
    CHECK(lib.at(1).text == "# Test preset v1\nname=Beta\nvalue=22");
    CHECK(lib.save(1, "# Test preset v1\nname=Beta\nvalue=23") == ""); // saving again is fine
    { std::ofstream(lib.at(1).file, std::ios::binary | std::ios::trunc) << "edited elsewhere\n"; }
    CHECK(lib.save(1, "mine") == "FILE CHANGED ON DISK: RESCAN FIRST");
    CHECK(slurp(lib.at(1).file) == "edited elsewhere\n");
    lib.rescan();
    CHECK(lib.at(1).text == "edited elsewhere");
    CHECK(lib.save(1, "mine") == "");
    CHECK(lib.save(9, "x") == "NOT A FILE PRESET");
}

TEST_CASE("create adds the next numbered file to a group") {
    Scratch s;
    PresetLibrary lib(spux_test::kBakedTestPresets, s.dir);
    const int a = lib.create("FIRST", "new one");
    REQUIRE(a >= 0);
    CHECK(lib.count() == 4);
    CHECK(lib.at(a).group == "FIRST");
    CHECK(lib.at(a).name == "NEW PRESET");
    CHECK(lib.at(a).file.filename().string() == "03 NEW PRESET.txt");
    CHECK(slurp(lib.at(a).file) == "new one\n");
    const int b = lib.create("FIRST", "another");
    CHECK(lib.at(b).file.filename().string() == "04 NEW PRESET.txt");
    CHECK(lib.at(a).file.filename().string() == "03 NEW PRESET.txt");
    const int c = lib.create("SECOND", "x", "MINE");
    CHECK(lib.at(c).file.filename().string() == "02 MINE.txt");
    CHECK(lib.create("NO SUCH GROUP", "x") == -1);
    lib.rescan();
    CHECK(lib.count() == 6);
}
