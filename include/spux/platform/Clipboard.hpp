// Reading the system clipboard: an image (a screen capture) or files copied in the file manager.
// Windows only for now; elsewhere both report nothing. Call from the UI thread, never the audio thread.
#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace spux::inline v1::platform {

struct ClipboardImage {
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba; // straight RGBA, top row first, opaque
};

// A bitmap on the clipboard (24 or 32 bits per pixel, as screen captures leave it). False when there is none.
bool readClipboardImage(ClipboardImage& out);

// Files copied in the file manager; empty when there are none.
std::vector<std::filesystem::path> readClipboardFiles();

} // namespace spux::inline v1::platform
