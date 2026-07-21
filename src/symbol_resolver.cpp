#include "symbol_resolver.h"
#include "logger.h"

#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <cstdlib>

// Forward declare dobby symbol resolver as a weak symbol so the link
// succeeds whether or not the Dobby symbol-resolver plugin is built.
// When Plugin.SymbolResolver is OFF, DobbySymbolResolver is not defined
// and the symbol resolves to nullptr at link time; the call below is
// then skipped via the null check.
extern "C" __attribute__((weak)) void* DobbySymbolResolver(const char* image_name, const char* symbol_name);

namespace breeze {

void* SymbolResolver::ResolveLibraryBase(const std::string& library) const {
    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) {
        Logger::Error("Failed to open /proc/self/maps");
        return nullptr;
    }

    std::string line;
    while (std::getline(maps, line)) {
        if (line.find(library) == std::string::npos) continue;

        // Parse: 7a1b2c3d4000-7a1b2c3d5000 r-xp ... /path/to/lib.so
        auto dash = line.find('-');
        if (dash == std::string::npos) continue;

        std::string addr_str = line.substr(0, dash);
        try {
            return reinterpret_cast<void*>(std::stoull(addr_str, nullptr, 16));
        } catch (...) {
            continue;
        }
    }

    return nullptr;
}

void* SymbolResolver::ResolveSymbol(const std::string& library, const std::string& symbol) const {
    // Strategy 1: dlsym with RTLD_DEFAULT (searches all loaded libs)
    void* result = dlsym(RTLD_DEFAULT, symbol.c_str());
    if (result) {
        Logger::Debug("Resolved symbol %s via dlsym: %p", symbol.c_str(), result);
        return result;
    }

    // Strategy 2: dlopen + dlsym with specific library
    void* handle = dlopen(library.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    if (handle) {
        result = dlsym(handle, symbol.c_str());
        dlclose(handle);
        if (result) {
            Logger::Debug("Resolved symbol %s via dlopen+dlsym: %p", symbol.c_str(), result);
            return result;
        }
    }

    // Strategy 3: DobbySymbolResolver (only available when the Dobby
    // symbol-resolver plugin is built; skip if the weak symbol is null).
    if (DobbySymbolResolver) {
        result = DobbySymbolResolver(library.c_str(), symbol.c_str());
        if (result) {
            Logger::Debug("Resolved symbol %s via DobbySymbolResolver: %p", symbol.c_str(), result);
            return result;
        }
    }

    Logger::Warn("Failed to resolve symbol %s in %s", symbol.c_str(), library.c_str());
    return nullptr;
}

} // namespace breeze
