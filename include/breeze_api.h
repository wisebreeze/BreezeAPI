#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <unordered_map>

#ifdef _WIN32
#   define BREEZE_EXPORT __declspec(dllexport)
#else
#   define BREEZE_EXPORT __attribute__((visibility("default")))
#endif

namespace breeze {

// ── Type aliases ──────────────────────────────────────────────
using Address  = void*;
using HookCallback = std::function<void()>;

// ── Hook result ───────────────────────────────────────────────
enum class HookStatus : int32_t {
    Ok              = 0,
    AlreadyHooked   = 1,
    NotHooked       = 2,
    SymbolNotFound  = 3,
    InvalidAddress  = 4,
    InternalError   = 5,
};

// ── Log level ─────────────────────────────────────────────────
enum class LogLevel : int32_t {
    Verbose = 0,
    Debug   = 1,
    Info    = 2,
    Warn    = 3,
    Error   = 4,
    Silent  = 5,
};

// ── Hook info ─────────────────────────────────────────────────
struct HookInfo {
    std::string  name;           ///< Human-readable identifier
    std::string  library;        ///< Library name (e.g. "libminecraftpe.so")
    std::string  symbol;         ///< Symbol name or offset expression
    Address      target_addr;    ///< Resolved target address
    Address      trampoline;     ///< Original function trampoline
    HookStatus   status;         ///< Current hook status
};

// ── Configuration ─────────────────────────────────────────────
struct BreezeConfig {
    LogLevel     log_level   = LogLevel::Info;
    bool         log_to_android = true;     ///< Use __android_log_print
    std::string  log_tag     = "BreezeAPI";
};

// ══════════════════════════════════════════════════════════════
//  Core API
// ══════════════════════════════════════════════════════════════

class BREEZE_EXPORT BreezeAPI {
public:
    // ── Lifecycle ─────────────────────────────────────────────
    static BreezeAPI& Instance();

    /**
     * Initialise the API. Must be called before any hook operations.
     * Safe to call multiple times; subsequent calls are no-ops.
     */
    bool Init(const BreezeConfig& config = {});

    /** Shut down and unhook everything. */
    void Shutdown();

    bool IsInitialized() const;

    // ── Hook operations ───────────────────────────────────────

    /**
     * Hook a function by its symbol name inside a loaded library.
     *
     * @param library   Library name (e.g. "libminecraftpe.so")
     * @param symbol    Exported symbol name
     * @param replace   Replacement function pointer
     * @param original  [out] Receives the trampoline to call the original
     * @param tag       Optional human-readable tag for debugging
     * @return HookStatus
     */
    HookStatus HookBySymbol(
        const std::string& library,
        const std::string& symbol,
        Address            replace,
        Address*           original = nullptr,
        const std::string& tag = ""
    );

    /**
     * Hook a function by address offset relative to a library base.
     *
     * @param library   Library name
     * @param offset    Offset from the library base address
     * @param replace   Replacement function pointer
     * @param original  [out] Trampoline for the original
     * @param tag       Optional tag
     * @return HookStatus
     */
    HookStatus HookByOffset(
        const std::string& library,
        uintptr_t          offset,
        Address            replace,
        Address*           original = nullptr,
        const std::string& tag = ""
    );

    /**
     * Hook a function by absolute address.
     *
     * @param target    Absolute address of the function
     * @param replace   Replacement function pointer
     * @param original  [out] Trampoline for the original
     * @param tag       Optional tag
     * @return HookStatus
     */
    HookStatus HookByAddress(
        Address            target,
        Address            replace,
        Address*           original = nullptr,
        const std::string& tag = ""
    );

    /**
     * Unhook a previously hooked function.
     *
     * @param target  The address that was hooked
     * @return HookStatus
     */
    HookStatus Unhook(Address target);

    /** Unhook all currently active hooks. */
    void UnhookAll();

    // ── Query ─────────────────────────────────────────────────

    /** Get info about a specific hook by target address. */
    std::optional<HookInfo> GetHookInfo(Address target) const;

    /** Get info about a specific hook by tag name. */
    std::optional<HookInfo> GetHookInfoByTag(const std::string& tag) const;

    /** List all active hooks. */
    std::vector<HookInfo> GetAllHooks() const;

    // ── Symbol resolution ─────────────────────────────────────

    /**
     * Resolve the base address of a loaded library.
     *
     * @param library  Library name
     * @return Base address, or nullptr if not found
     */
    Address ResolveLibraryBase(const std::string& library) const;

    /**
     * Resolve a symbol address inside a loaded library.
     *
     * @param library  Library name
     * @param symbol   Symbol name
     * @return Symbol address, or nullptr if not found
     */
    Address ResolveSymbol(const std::string& library, const std::string& symbol) const;

    // ── Configuration ─────────────────────────────────────────

    void SetLogLevel(LogLevel level);
    LogLevel GetLogLevel() const;

private:
    BreezeAPI();
    ~BreezeAPI();

    BreezeAPI(const BreezeAPI&) = delete;
    BreezeAPI& operator=(const BreezeAPI&) = delete;

    struct Impl;
    Impl* impl_;
};

} // namespace breeze

// ── C-compatible interface ────────────────────────────────────
extern "C" {

BREEZE_EXPORT bool        breeze_init();
BREEZE_EXPORT void        breeze_shutdown();

BREEZE_EXPORT int32_t     breeze_hook_symbol(const char* lib, const char* sym, void* replace, void** original);
BREEZE_EXPORT int32_t     breeze_hook_offset(const char* lib, uintptr_t offset, void* replace, void** original);
BREEZE_EXPORT int32_t     breeze_hook_address(void* target, void* replace, void** original);
BREEZE_EXPORT int32_t     breeze_unhook(void* target);
BREEZE_EXPORT void        breeze_unhook_all();

BREEZE_EXPORT void*       breeze_resolve_library(const char* lib);
BREEZE_EXPORT void*       breeze_resolve_symbol(const char* lib, const char* sym);

BREEZE_EXPORT void        breeze_set_log_level(int32_t level);

} // extern "C"
