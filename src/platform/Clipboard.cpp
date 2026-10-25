#include "spux/platform/Clipboard.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>

#include <cstring>
#include <string>
#endif

namespace spux::inline v1::platform {

#if defined(_WIN32)

namespace {

// A packed DIB (CF_DIB: header, optional masks, pixels). Only uncompressed 24 and 32 bit, the forms screen captures
// use; 32-bit pixels are taken as BGRX, and alpha is dropped because captures often leave it 0.
bool fromDib(const std::uint8_t* data, SIZE_T size, ClipboardImage& out) {
    if (size < sizeof(BITMAPINFOHEADER))
        return false;
    BITMAPINFOHEADER h;
    std::memcpy(&h, data, sizeof h);
    const int bpp = h.biBitCount;
    const DWORD rgb = BI_RGB, fields = BI_BITFIELDS;
    const bool bitfields = h.biCompression == fields && h.biSize == sizeof(BITMAPINFOHEADER);
    if ((bpp != 24 && bpp != 32) || (h.biCompression != rgb && h.biCompression != fields))
        return false;
    const int w = h.biWidth, rows = h.biHeight < 0 ? -h.biHeight : h.biHeight;
    if (w <= 0 || rows <= 0 || w > 32768 || rows > 32768)
        return false;
    const SIZE_T stride = (static_cast<SIZE_T>(w) * static_cast<SIZE_T>(bpp) + 31) / 32 * 4;
    const SIZE_T offset = h.biSize + (bitfields ? 12 : 0) + static_cast<SIZE_T>(h.biClrUsed) * 4;
    if (offset + stride * static_cast<SIZE_T>(rows) > size)
        return false;
    out.width = w;
    out.height = rows;
    out.rgba.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(rows) * 4);
    for (int y = 0; y < rows; ++y) {
        const int src = h.biHeight < 0 ? y : rows - 1 - y; // positive height: bottom row first
        const std::uint8_t* p = data + offset + stride * static_cast<SIZE_T>(src);
        std::uint8_t* q = &out.rgba[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 4];
        for (int x = 0; x < w; ++x, p += bpp / 8, q += 4) {
            q[0] = p[2];
            q[1] = p[1];
            q[2] = p[0];
            q[3] = 255;
        }
    }
    return true;
}

} // namespace

bool readClipboardImage(ClipboardImage& out) {
    if (!IsClipboardFormatAvailable(CF_DIB) || !OpenClipboard(nullptr))
        return false;
    bool ok = false;
    if (HANDLE mem = GetClipboardData(CF_DIB)) {
        if (const void* data = GlobalLock(mem)) {
            ok = fromDib(static_cast<const std::uint8_t*>(data), GlobalSize(mem), out);
            GlobalUnlock(mem);
        }
    }
    CloseClipboard();
    return ok;
}

std::vector<std::filesystem::path> readClipboardFiles() {
    std::vector<std::filesystem::path> files;
    if (!IsClipboardFormatAvailable(CF_HDROP) || !OpenClipboard(nullptr))
        return files;
    if (auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP))) {
        const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i) {
            std::wstring name(DragQueryFileW(drop, i, nullptr, 0), L'\0');
            DragQueryFileW(drop, i, name.data(), static_cast<UINT>(name.size() + 1));
            files.emplace_back(name);
        }
    }
    CloseClipboard();
    return files;
}

#else

bool readClipboardImage(ClipboardImage&) { return false; }
std::vector<std::filesystem::path> readClipboardFiles() { return {}; }

#endif

} // namespace spux::inline v1::platform
