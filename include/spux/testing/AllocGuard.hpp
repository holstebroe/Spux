// Allocation guard (R1): the test executable replaces the global operator new and delete. While an
// AudioThreadScope is alive on a thread, any allocation on that thread is a violation.
//
// Link `spux_testing_allocguard` into test executables only. Replacing operator new is process-wide by
// nature, so it is never linked into a plugin binary (rule 8). Violations are counted in atomics; nothing
// else is shared.
#pragma once

#include <cstddef>

namespace spux::inline v1::testing {

class AudioThreadScope {
public:
    AudioThreadScope();
    ~AudioThreadScope();
    AudioThreadScope(const AudioThreadScope&) = delete;
    AudioThreadScope& operator=(const AudioThreadScope&) = delete;
};

// Violations recorded since start (or since resetViolations()).
std::size_t violations();
void resetViolations();
// Default: abort on a violation, so a stray allocation fails any test loudly. A test that wants to count
// instead (the guard's own test, or a check of "exactly one allocation here") turns this off.
void setAbortOnViolation(bool abortOnViolation);

} // namespace spux::inline v1::testing
