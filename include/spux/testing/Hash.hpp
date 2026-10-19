// Golden hash helpers (S5, section 3 of the guidelines): FNV-1a 64 over sample, pixel or byte data, and a
// golden file that keeps one hash per case for each platform and compiler. A changed hash is a change
// detector, not a failure: regenerate the file in the same commit as the change and note it in the
// changelog.
//
// Floats and words are hashed by their bit pattern, least significant byte first, so the value does not
// depend on the machine's byte order (a -0.0 or a NaN payload is a difference, on purpose).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace spux::inline v1::testing {

inline constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
// The Classic drawing golden (moved from Tearwash, Tannhauser and Acidus) was made with an offset basis that
// lost its last digit. It is still a deterministic hash, so the golden stays valid with this basis; new
// goldens use the standard one.
inline constexpr std::uint64_t kFnvLegacyBasis = 1469598103934665603ull;

class Fnv1a64 {
public:
    explicit Fnv1a64(std::uint64_t basis = kFnvOffsetBasis) : h_(basis) {}
    void bytes(const void* data, std::size_t n) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            h_ ^= p[i];
            h_ *= 1099511628211ull;
        }
    }
    void word(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            h_ ^= (v >> (8 * i)) & 0xFF;
            h_ *= 1099511628211ull;
        }
    }
    void real(float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, 4);
        word(bits);
    }
    void reals(const float* p, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            real(p[i]);
    }
    void words(const std::uint32_t* p, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            word(p[i]);
    }
    std::uint64_t value() const { return h_; }

private:
    std::uint64_t h_;
};

inline std::uint64_t hashBytes(const void* p, std::size_t n, std::uint64_t basis = kFnvOffsetBasis) {
    Fnv1a64 h(basis);
    h.bytes(p, n);
    return h.value();
}
inline std::uint64_t hashFloats(const std::vector<float>& x, std::uint64_t basis = kFnvOffsetBasis) {
    Fnv1a64 h(basis);
    h.reals(x.data(), x.size());
    return h.value();
}
// Pixels (0xAARRGGBB words) or any other 32-bit data.
inline std::uint64_t hashWords(const std::vector<std::uint32_t>& x, std::uint64_t basis = kFnvOffsetBasis) {
    Fnv1a64 h(basis);
    h.words(x.data(), x.size());
    return h.value();
}

// 16 lower-case hex digits.
inline std::string hashToHex(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

// "linux-x86_64-gcc13": the key of a golden file. One file per platform and compiler family, because
// the libm and the vector code of another toolchain may round differently (R3 asks for bit-identical
// output within one build only).
inline std::string platformTag() {
#if defined(_WIN32)
    std::string tag = "windows";
#elif defined(__APPLE__)
    std::string tag = "macos";
#else
    std::string tag = "linux";
#endif
#if defined(__x86_64__) || defined(_M_X64)
    tag += "-x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    tag += "-arm64";
#else
    tag += "-other";
#endif
#if defined(__clang__)
    tag += "-clang" + std::to_string(__clang_major__);
#elif defined(__GNUC__)
    tag += "-gcc" + std::to_string(__GNUC__);
#elif defined(_MSC_VER)
    tag += "-msvc" + std::to_string(_MSC_VER / 10);
#else
    tag += "-unknown";
#endif
    return tag;
}

// True when the environment variable (default SPUX_UPDATE_GOLDEN) is set to something but "0".
inline bool updateRequested(const char* variable = "SPUX_UPDATE_GOLDEN") {
    const char* v = std::getenv(variable);
    return v && *v && std::strcmp(v, "0") != 0;
}

// A text file of "name hash" lines (blank lines and lines starting with # are skipped), kept sorted by
// name so it diffs well.
class GoldenFile {
public:
    enum class Result { Match, Mismatch, Missing, Updated };

    // `update` defaults to the environment: with SPUX_UPDATE_GOLDEN=1 verify() records instead of comparing.
    explicit GoldenFile(std::string path, bool update = updateRequested()) : path_(std::move(path)), update_(update) {
        std::ifstream in(path_);
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            std::string name, hash;
            if (ls >> name >> hash && name[0] != '#')
                entries_[name] = hash;
        }
    }

    // Compares `hash` with the stored one. Missing: no entry for this name (a new case, or a platform that
    // has no file yet); a test reports it and fails unless it is updating.
    Result verify(const std::string& name, std::uint64_t hash) {
        const std::string hex = hashToHex(hash);
        if (update_) {
            if (entries_[name] != hex)
                dirty_ = true;
            entries_[name] = hex;
            return Result::Updated;
        }
        const auto it = entries_.find(name);
        if (it == entries_.end())
            return Result::Missing;
        return it->second == hex ? Result::Match : Result::Mismatch;
    }

    const std::string* find(const std::string& name) const {
        const auto it = entries_.find(name);
        return it == entries_.end() ? nullptr : &it->second;
    }

    bool updating() const { return update_; }
    const std::string& path() const { return path_; }

    // Writes the file when verify() recorded a change. Returns false when it could not write.
    bool save() {
        if (!dirty_)
            return true;
        std::ofstream out(path_, std::ios::trunc);
        for (const auto& [name, hash] : entries_)
            out << name << ' ' << hash << '\n';
        dirty_ = false;
        return out.good();
    }

    // A line for a failed check: what was stored, what the render gave, and how to accept it.
    std::string describe(const std::string& name, std::uint64_t hash, Result r) const {
        const std::string* stored = find(name);
        std::string s = "golden '" + name + "' (" + path_ + "): ";
        if (r == Result::Missing)
            s += "no reference; run with SPUX_UPDATE_GOLDEN=1 to record " + hashToHex(hash);
        else if (r == Result::Mismatch)
            s += "expected " + *stored + ", got " + hashToHex(hash) + "; SPUX_UPDATE_GOLDEN=1 accepts the change";
        else
            s += "ok";
        return s;
    }

private:
    std::string path_;
    bool update_;
    bool dirty_ = false;
    std::map<std::string, std::string> entries_;
};

// "<dir>/<platformTag>.txt"
inline std::string goldenPath(const std::string& dir) { return dir + "/" + platformTag() + ".txt"; }

} // namespace spux::inline v1::testing
