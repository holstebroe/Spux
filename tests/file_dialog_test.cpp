// Last folder per dialog kind (spux/ui/classic/FileDialog.hpp).
#include <doctest.h>

#include <cstdlib>
#include <filesystem>

#include "spux/ui/classic/FileDialog.hpp"

using namespace spux::ui::classic;

TEST_CASE("a dialog kind reopens in the folder of its last file, and kinds share the last one as a start") {
#if defined(__linux__)
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "spux_file_dialog_test";
    std::filesystem::create_directories(dir);
    setenv("XDG_CONFIG_HOME", dir.c_str(), 1);

    CHECK(lastDirectory("banks", "fallback") == "fallback");
    rememberDirectory("banks", "/music/banks/a.syx");
    CHECK(lastDirectory("banks", "fallback") == "/music/banks");
    CHECK(lastDirectory("waves", "fallback") == "/music/banks"); // another kind's folder beats the fallback
    rememberDirectory("waves", "/music/waves/w.zip");
    CHECK(lastDirectory("waves", "fallback") == "/music/waves");
    CHECK(lastDirectory("banks", "fallback") == "/music/banks");
    CHECK(std::filesystem::exists(dir / "spux_dialog_folders.txt"));
    std::filesystem::remove_all(dir);
#endif
    FileDialogOptions o;
    CHECK_FALSE(o.multiple);
    CHECK_FALSE(o.folder);
    CHECK(o.key.empty());
}
