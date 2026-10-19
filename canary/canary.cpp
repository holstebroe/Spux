// Entry point of the size canary: a CLAP plugin library that offers no plugins.
#include <clap/entry.h>

namespace {
bool init(const char*) { return true; }
void deinit() {}
const void* getFactory(const char*) { return nullptr; }
} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    init,
    deinit,
    getFactory,
};
