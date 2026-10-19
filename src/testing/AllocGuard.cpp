#include "spux/testing/AllocGuard.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace spux::inline v1::testing {
namespace {
thread_local int tlsDepth = 0;
std::atomic<std::size_t> gViolations{0};
std::atomic<bool> gAbort{true};

void note() noexcept {
    if (tlsDepth == 0)
        return;
    gViolations.fetch_add(1, std::memory_order_relaxed);
    if (gAbort.load(std::memory_order_relaxed)) {
        std::fputs("spux: allocation on the audio thread\n", stderr);
        std::abort();
    }
}

void* allocate(std::size_t size) {
    note();
    if (void* p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}

void* allocateAligned(std::size_t size, std::size_t align) {
    note();
    if (size == 0)
        size = 1;
#if defined(_MSC_VER)
    if (void* p = _aligned_malloc(size, align))
        return p;
#else
    const std::size_t rounded = (size + align - 1) / align * align;
    if (void* p = std::aligned_alloc(align, rounded))
        return p;
#endif
    throw std::bad_alloc();
}

void release(void* p) noexcept { std::free(p); }
void releaseAligned(void* p) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
} // namespace

AudioThreadScope::AudioThreadScope() { ++tlsDepth; }
AudioThreadScope::~AudioThreadScope() { --tlsDepth; }
std::size_t violations() { return gViolations.load(); }
void resetViolations() { gViolations.store(0); }
void setAbortOnViolation(bool abortOnViolation) { gAbort.store(abortOnViolation); }

} // namespace spux::inline v1::testing

// Global replacements. They have to live at global scope, so they cannot carry the spux namespace.
namespace st = spux::testing;
void* operator new(std::size_t n) { return st::allocate(n); }
void* operator new[](std::size_t n) { return st::allocate(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try {
        return st::allocate(n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept {
    try {
        return st::allocate(n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new(std::size_t n, std::align_val_t a) { return st::allocateAligned(n, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return st::allocateAligned(n, static_cast<std::size_t>(a)); }
void operator delete(void* p) noexcept { st::release(p); }
void operator delete[](void* p) noexcept { st::release(p); }
void operator delete(void* p, std::size_t) noexcept { st::release(p); }
void operator delete[](void* p, std::size_t) noexcept { st::release(p); }
void operator delete(void* p, std::align_val_t) noexcept { st::releaseAligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { st::releaseAligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { st::releaseAligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { st::releaseAligned(p); }
