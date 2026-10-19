// The allocation guard counts allocations inside an AudioThreadScope and ignores those outside it.
// (Moved from Lumeoid's alloc_guard_test, which merged with Plumeflow's.)
#include <doctest.h>

#include <atomic>
#include <cstddef>
#include <new>
#include <thread>
#include <vector>

#include "spux/testing/AllocGuard.hpp"

using namespace spux::testing;

namespace {
// The compiler may elide a new/delete pair; letting the pointer escape keeps the call.
void* volatile gSink = nullptr;
void allocateOnce(std::size_t n) {
    gSink = ::operator new(n);
    ::operator delete(gSink);
}
struct alignas(64) Wide {
    char c[64];
};
} // namespace

TEST_CASE("allocation inside an audio scope is counted, outside it is not") {
    setAbortOnViolation(false);
    resetViolations();
    allocateOnce(4);
    CHECK(violations() == 0);
    {
        AudioThreadScope scope;
        allocateOnce(4);
        allocateOnce(32);
    }
    CHECK(violations() == 2);
    allocateOnce(4);
    CHECK(violations() == 2);
    resetViolations();
    setAbortOnViolation(true);
}

TEST_CASE("scopes nest, and every kind of new is seen") {
    setAbortOnViolation(false);
    resetViolations();
    std::size_t afterVector = 0, afterNews = 0;
    {
        AudioThreadScope outer;
        { AudioThreadScope inner; }
        // Escaping through volatile pointers keeps the compiler from dropping the new-expressions. The
        // counts are read inside the scope and checked outside it: the test framework allocates too.
        std::vector<int> v;
        v.reserve(8);
        int* volatile data = v.data();
        afterVector = violations();
        int* volatile a = new int[4];
        Wide* volatile w = new Wide;
        int* volatile nt = new (std::nothrow) int(3);
        afterNews = violations();
        delete[] a;
        delete w;
        delete nt;
        (void)data;
    }
    CHECK(afterVector == 1);
    CHECK(afterNews == 4);
    resetViolations();
    allocateOnce(8);
    CHECK(violations() == 0);
    setAbortOnViolation(true);
}

TEST_CASE("the scope is per thread") {
    setAbortOnViolation(false);
    resetViolations();
    std::atomic<bool> go{false}, done{false};
    // The thread is created first: creating one allocates, and that would be this thread's violation.
    std::thread t([&] {
        while (!go.load())
            std::this_thread::yield();
        allocateOnce(16); // the other thread has no scope
        done.store(true);
    });
    {
        AudioThreadScope scope;
        go.store(true);
        while (!done.load())
            std::this_thread::yield();
        CHECK(violations() == 0);
    }
    t.join();
    setAbortOnViolation(true);
}
