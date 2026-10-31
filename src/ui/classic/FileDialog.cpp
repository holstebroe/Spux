#include "spux/ui/classic/FileDialog.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

#if defined(__linux__) && !defined(__APPLE__)
#include <algorithm>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <commdlg.h> // after windows.h; the blank line keeps clang-format from sorting it before
#include <shlobj.h>
#endif

namespace spux::inline v1::ui::classic {

// ---- last folder per dialog kind ----------------------------------------------------------------------------

namespace {

std::string configFile() {
#if defined(_WIN32)
    const char* base = std::getenv("LOCALAPPDATA");
    if (!base)
        base = std::getenv("USERPROFILE");
    return base ? std::string(base) + "\\spux_dialog_folders.txt" : std::string();
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return std::string(xdg) + "/spux_dialog_folders.txt";
    const char* home = std::getenv("HOME");
    return home ? std::string(home) + "/.config/spux_dialog_folders.txt" : std::string();
#endif
}

// Dialog folders are user interface state: they never reach what an instance computes.
std::map<std::string, std::string>& folders() {
    static std::map<std::string, std::string> m;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::ifstream in(configFile());
        std::string line;
        while (in && std::getline(in, line)) {
            const size_t tab = line.find('\t');
            if (tab != std::string::npos)
                m[line.substr(0, tab)] = line.substr(tab + 1);
        }
    }
    return m;
}

std::mutex& foldersMutex() {
    static std::mutex m;
    return m;
}

} // namespace

std::string lastDirectory(const std::string& key, const std::string& fallback) {
    std::lock_guard<std::mutex> lock(foldersMutex());
    auto& m = folders();
    auto it = m.find(key);
    if (it != m.end() && !it->second.empty())
        return it->second;
    // another kind's folder is a better start than the home folder
    for (const auto& kv : m)
        if (!kv.second.empty())
            return kv.second;
    return fallback;
}

void rememberDirectory(const std::string& key, const std::string& chosenPath) {
    if (chosenPath.empty())
        return;
    std::string dir = chosenPath;
    const size_t cut = dir.find_last_of("/\\");
    if (cut == std::string::npos)
        return;
    dir.resize(cut == 0 ? 1 : cut);
    std::lock_guard<std::mutex> lock(foldersMutex());
    auto& m = folders();
    m[key] = dir;
    const std::string file = configFile();
    if (file.empty())
        return;
    std::ofstream out(file, std::ios::trunc);
    for (const auto& kv : m)
        out << kv.first << '\t' << kv.second << '\n';
}

std::string homeDirectory() {
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    return home ? home : "";
}

// "png;jpg" -> "*.png<sep>*.jpg"
[[maybe_unused]] static std::string patterns(const std::string& extensions, const char* sep) {
    std::string out;
    size_t start = 0;
    while (start <= extensions.size()) {
        const size_t end = std::min(extensions.find(';', start), extensions.size());
        out += (out.empty() ? "*." : sep + std::string("*.")) + extensions.substr(start, end - start);
        start = end + 1;
    }
    return out;
}

#if defined(__linux__) && !defined(__APPLE__)

static bool onPath(const char* program) {
    const char* path = std::getenv("PATH");
    if (!path)
        return false;
    std::string dirs(path);
    size_t start = 0;
    while (start <= dirs.size()) {
        const size_t end = std::min(dirs.find(':', start), dirs.size());
        const std::string file = dirs.substr(start, end - start) + "/" + program;
        if (end > start && access(file.c_str(), X_OK) == 0)
            return true;
        start = end + 1;
    }
    return false;
}

bool FileDialogProcess::start(const FileDialogOptions& options) {
    if (running())
        return true;
    const std::string pattern = patterns(options.extension, " ");
    const std::string described = options.filterName + " (" + pattern + ")";
    std::vector<std::string> args;
    if (onPath("zenity") && options.folder) {
        args = {"zenity", "--file-selection", "--directory", "--title=" + options.title,
                "--filename=" + options.startPath};
    } else if (onPath("kdialog") && options.folder) {
        args = {"kdialog", "--getexistingdirectory", options.startPath, "--title", options.title};
    } else if (onPath("zenity")) {
        args = {"zenity",
                "--file-selection",
                "--title=" + options.title,
                "--file-filter=" + described + " | " + pattern,
                "--file-filter=All files | *",
                "--filename=" + options.startPath};
        if (options.save) {
            args.push_back("--save");
            args.push_back("--confirm-overwrite");
        }
        if (options.multiple && !options.save) {
            args.push_back("--multiple");
            args.push_back("--separator=\n");
        }
    } else if (onPath("kdialog")) {
        args = {"kdialog",         options.save ? "--getsavefilename" : "--getopenfilename",
                options.startPath, pattern + "|" + described,
                "--title",         options.title};
        if (options.multiple && !options.save) {
            args.push_back("--multiple");
            args.push_back("--separate-output");
        }
    } else {
        return false;
    }
    int fds[2];
    if (pipe(fds) != 0)
        return false;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    std::vector<char*> argv;
    for (auto& a : args)
        argv.push_back(&a[0]);
    argv.push_back(nullptr);
    pid_t pid = 0;
    const int err = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (err != 0) {
        close(fds[0]);
        return false;
    }
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    pid_ = pid;
    fd_ = fds[0];
    output_.clear();
    save_ = options.save;
    multiple_ = options.multiple && !options.save;
    return true;
}

bool FileDialogProcess::poll(std::string& path, bool& save) {
    if (!running())
        return false;
    char buf[512];
    ssize_t n;
    while ((n = read(fd_, buf, sizeof(buf))) > 0)
        output_.append(buf, static_cast<size_t>(n));
    int status = 0;
    if (waitpid(pid_, &status, WNOHANG) != pid_)
        return false;
    while ((n = read(fd_, buf, sizeof(buf))) > 0)
        output_.append(buf, static_cast<size_t>(n));
    close(fd_);
    pid_ = 0;
    fd_ = -1;
    path.clear();
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        path = output_;
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r'))
            path.pop_back();
        if (!multiple_)
            path = path.substr(0, path.find('\n'));
    }
    save = save_;
    return true;
}

void FileDialogProcess::cancel() {
    if (!running())
        return;
    kill(pid_, SIGTERM);
    waitpid(pid_, nullptr, 0);
    close(fd_);
    pid_ = 0;
    fd_ = -1;
}

#elif defined(_WIN32)

static std::wstring widen(const std::string& s) {
    if (s.empty())
        return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::vector<std::filesystem::path> runFileDialogs(void* owner, const FileDialogOptions& options) {
    if (options.folder) {
        const std::wstring title = widen(options.title);
        BROWSEINFOW bi = {};
        bi.hwndOwner = static_cast<HWND>(owner);
        bi.lpszTitle = title.c_str();
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        // the new dialog style needs COM on this thread; a host that set it up differently is fine as it is
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        std::vector<std::filesystem::path> out;
        if (PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&bi)) {
            wchar_t path[MAX_PATH] = {};
            if (SHGetPathFromIDListW(item, path))
                out.emplace_back(path);
            CoTaskMemFree(item);
        }
        if (SUCCEEDED(com))
            CoUninitialize();
        return out;
    }
    const bool many = options.multiple && !options.save;
    const std::filesystem::path start = std::filesystem::path(
        std::u8string(reinterpret_cast<const char8_t*>(options.startPath.data()), options.startPath.size()));
    std::wstring dir, name;
    std::error_code ec;
    if (!options.startPath.empty() && std::filesystem::is_directory(start, ec)) {
        dir = start.wstring();
    } else {
        dir = start.parent_path().wstring();
        name = start.filename().wstring();
    }
    std::vector<wchar_t> file(many ? 32768 : MAX_PATH, L'\0');
    if (name.size() < file.size())
        std::copy(name.begin(), name.end(), file.begin());
    const std::wstring ext = widen(options.extension.substr(0, options.extension.find(';')));
    const std::wstring pattern = widen(patterns(options.extension, ";"));
    std::wstring filter = widen(options.filterName) + L" (" + pattern + L")";
    filter.push_back(L'\0');
    filter += pattern;
    filter.push_back(L'\0');
    filter += L"All files (*.*)";
    filter.push_back(L'\0');
    filter += L"*.*";
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    const std::wstring title = widen(options.title);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file.data();
    ofn.nMaxFile = static_cast<DWORD>(file.size());
    ofn.lpstrInitialDir = dir.empty() ? nullptr : dir.c_str();
    ofn.lpstrDefExt = ext.c_str();
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (options.save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST) |
                (many ? (OFN_ALLOWMULTISELECT | OFN_EXPLORER) : 0);
    const BOOL ok = options.save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    std::vector<std::filesystem::path> out;
    if (!ok)
        return out;
    // multi-select returns "folder\0file1\0file2\0\0"; a single file returns its full path
    const std::wstring first(file.data());
    const wchar_t* next = file.data() + first.size() + 1;
    if (!many || *next == L'\0') {
        out.emplace_back(first);
        return out;
    }
    while (*next) {
        const std::wstring item(next);
        out.emplace_back(std::filesystem::path(first) / item);
        next += item.size() + 1;
    }
    return out;
}

std::filesystem::path runFileDialog(void* owner, const FileDialogOptions& options) {
    FileDialogOptions single = options;
    single.multiple = false;
    const auto files = runFileDialogs(owner, single);
    return files.empty() ? std::filesystem::path() : files.front();
}

#endif

} // namespace spux::inline v1::ui::classic
