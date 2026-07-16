#pragma once

#include <cstdint>
#include <string>

namespace breeze {

class SymbolResolver {
public:
    SymbolResolver() = default;

    /**
     * Resolve the base address of a loaded ELF library.
     * On Android this reads /proc/self/maps.
     */
    void* ResolveLibraryBase(const std::string& library) const;

    /**
     * Resolve an exported symbol address from a loaded library.
     * Uses dlsym first, then falls back to DobbySymbolResolver.
     */
    void* ResolveSymbol(const std::string& library, const std::string& symbol) const;
};

} // namespace breeze
