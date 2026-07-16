#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <unordered_map>

#include "js_engine.h"  // JSResult, JSNativeCallback, JSType

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

    // ── JavaScript engine ─────────────────────────────────────

    /**
     * Evaluate a JavaScript source string.
     *
     * @param source    JavaScript code to evaluate
     * @param filename  Optional filename for error reporting
     * @return JSResult with success status, type, and stringified value
     */
    JSResult EvalJS(const std::string& source, const std::string& filename = "<eval>");

    /**
     * Evaluate a JavaScript ES module from source.
     *
     * @param source    JavaScript module code (can use import/export)
     * @param filename  Module filename (should end with .mjs)
     * @return JSResult with success status and result
     */
    JSResult EvalJSModule(const std::string& source, const std::string& filename = "<module>.mjs");

    /**
     * Set a global JavaScript property (string value).
     *
     * @param name   Property name on the global object
     * @param value  String value to set
     * @return true on success
     */
    bool SetJSGlobalString(const std::string& name, const std::string& value);

    /**
     * Set a global JavaScript property (number value).
     *
     * @param name   Property name on the global object
     * @param value  Numeric value to set
     * @return true on success
     */
    bool SetJSGlobalNumber(const std::string& name, double value);

    /**
     * Set a global JavaScript property (boolean value).
     *
     * @param name   Property name on the global object
     * @param value  Boolean value to set
     * @return true on success
     */
    bool SetJSGlobalBool(const std::string& name, bool value);

    /**
     * Get a global JavaScript property as a string.
     *
     * @param name  Property name on the global object
     * @return String value, or empty optional if not found
     */
    std::optional<std::string> GetJSGlobalString(const std::string& name) const;

    /**
     * Get a global JavaScript property as a number.
     *
     * @param name  Property name on the global object
     * @return Numeric value, or empty optional if not found
     */
    std::optional<double> GetJSGlobalNumber(const std::string& name) const;

    /**
     * Register a native C++ callback as a global JavaScript function.
     *
     * The callback receives arguments as stringified values and must
     * return a string result.
     *
     * @param name      Function name on the global object
     * @param callback  C++ callback to invoke when JS calls the function
     * @return true on success
     */
    bool RegisterJSFunction(const std::string& name, JSNativeCallback callback);

    /**
     * Unregister a previously registered native JS function.
     *
     * @param name  Function name to remove
     * @return true if the function was found and removed
     */
    bool UnregisterJSFunction(const std::string& name);

    /**
     * Register a JS source as a module that can be imported.
     *
     * @param specifier  Module specifier (import path)
     * @param source     JavaScript module source code
     * @return true on success
     */
    bool RegisterJSModule(const std::string& specifier, const std::string& source);

    /**
     * Run a garbage collection cycle on the JS engine.
     */
    void JSGC();

    /**
     * Get current JS heap memory usage in bytes.
     */
    size_t GetJSMemoryUsage() const;

    /**
     * Set the JS heap memory limit.
     *
     * @param limit  Maximum bytes the JS heap can use (0 = unlimited)
     */
    void SetJSMemoryLimit(size_t limit);

    /**
     * Set the JS stack size limit.
     *
     * @param limit  Maximum stack size in bytes (0 = default 256KB)
     */
    void SetJSStackSize(size_t limit);

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

// ── JS engine C ABI ──────────────────────────────────────────
typedef struct breeze_js_result {
    int32_t     success;
    int32_t     type;
    const char* value;
} breeze_js_result;

BREEZE_EXPORT breeze_js_result breeze_js_eval(const char* source, const char* filename);
BREEZE_EXPORT breeze_js_result breeze_js_eval_module(const char* source, const char* filename);
BREEZE_EXPORT bool             breeze_js_set_global_string(const char* name, const char* value);
BREEZE_EXPORT bool             breeze_js_set_global_number(const char* name, double value);
BREEZE_EXPORT bool             breeze_js_set_global_bool(const char* name, int32_t value);
BREEZE_EXPORT char*            breeze_js_get_global_string(const char* name);
BREEZE_EXPORT double           breeze_js_get_global_number(const char* name);
BREEZE_EXPORT void             breeze_js_gc();
BREEZE_EXPORT size_t           breeze_js_memory_usage();
BREEZE_EXPORT void             breeze_js_set_memory_limit(size_t limit);
BREEZE_EXPORT void             breeze_js_set_stack_size(size_t limit);
BREEZE_EXPORT void             breeze_js_free_string(char* str);

} // extern "C"
