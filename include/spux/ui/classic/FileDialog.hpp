#ifndef SPUX_UI_CLASSIC_FILE_DIALOG_HPP
#define SPUX_UI_CLASSIC_FILE_DIALOG_HPP

// Open / save file dialogs for the plugin GUIs (data files, banks, presets).
//
// Linux: plain X11 has no file dialog, so zenity or kdialog runs as a child
// process whose output (the chosen path) the GUI's event loop polls; the GUI
// keeps running while the dialog is open. Windows: the common dialog, modal.

#include <filesystem>
#include <string>
#include <vector>

namespace spux::inline v1::ui::classic {

struct FileDialogOptions {
    bool save{false};
    std::string title;
    std::string filterName; // e.g. "Calibration profiles"
    // Without the dot, e.g. "json"; several separated by ';' ("png;jpg"), the first is the default.
    std::string extension;
    std::string startPath; // a directory, or a directory and file name
    bool multiple{false};  // open dialogs only: several files; the result lists one path per line
    bool folder{
        false};      // choose a folder, not a file: the result is the folder's path (filters and multiple are ignored)
    std::string key; // dialog kind, so each kind reopens in its own last folder (see lastDirectory)
};

// The folder the dialog of this kind was last used in (kept in a small file in the user's config folder,
// spux_dialog_folders.txt), or the folder of another kind, or `fallback` when there is none. Keys are the
// plugin's own names; prefix them with the plugin's name when plugins share the config folder.
std::string lastDirectory(const std::string& key, const std::string& fallback);
// Remembers the folder of `chosenPath` for this kind (a file path; its folder is stored).
void rememberDirectory(const std::string& key, const std::string& chosenPath);

// The user's home directory (HOME, or USERPROFILE on Windows), or "".
std::string homeDirectory();

#if defined(__linux__) && !defined(__APPLE__)
class FileDialogProcess {
public:
    FileDialogProcess() = default;
    FileDialogProcess(const FileDialogProcess&) = delete;
    FileDialogProcess& operator=(const FileDialogProcess&) = delete;
    ~FileDialogProcess() { cancel(); }

    bool running() const { return pid_ > 0; }
    // False if neither zenity nor kdialog is installed or it could not start.
    bool start(const FileDialogOptions& options);
    // True once the dialog has closed: `path` is the chosen file, or empty when it was canceled; with
    // FileDialogOptions::multiple one path per line. `save` tells which kind of dialog it was.
    bool poll(std::string& path, bool& save);
    void cancel();

private:
    int pid_{0};
    int fd_{-1};
    bool save_{false};
    bool multiple_{false};
    std::string output_;
};
#elif defined(_WIN32)
// Modal. Empty if canceled. `owner` is the parent HWND.
std::filesystem::path runFileDialog(void* owner, const FileDialogOptions& options);
// The same with FileDialogOptions::multiple: every chosen file, empty if canceled.
std::vector<std::filesystem::path> runFileDialogs(void* owner, const FileDialogOptions& options);
#endif

} // namespace spux::inline v1::ui::classic

#endif // SPUX_UI_CLASSIC_FILE_DIALOG_HPP
