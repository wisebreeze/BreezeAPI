#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <unordered_map>

struct JSContext;
struct JSRuntime;

namespace breeze {

// ── JS value types ────────────────────────────────────────────
enum class JSType : int32_t {
    Undefined  = 0,
    Null       = 1,
    Bool       = 2,
    Number     = 3,
    String     = 4,
    Object     = 5,
    Function   = 6,
    Exception  = 7,
};

// ── JS evaluation result ──────────────────────────────────────
struct JSResult {
    bool        success = false;
    JSType      type    = JSType::Undefined;
    std::string value;              ///< Stringified result or error message
};

// ── Native callback for JS ────────────────────────────────────
using JSNativeCallback = std::function<std::string(const std::vector<std::string>&)>;

// ══════════════════════════════════════════════════════════════
//  JSEngine — QuickJS wrapper
// ══════════════════════════════════════════════════════════════

class JSEngine {
public:
    JSEngine();
    ~JSEngine();

    JSEngine(const JSEngine&) = delete;
    JSEngine& operator=(const JSEngine&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────

    /**
     * Initialize the QuickJS runtime and context.
     * Called automatically by the constructor.
     * Safe to call multiple times; subsequent calls are no-ops.
     */
    bool Init();

    /** Shut down and release all JS resources. */
    void Shutdown();

    bool IsInitialized() const;

    // ── Script evaluation ─────────────────────────────────────

    /**
     * Evaluate a JavaScript source string.
     *
     * @param source    JavaScript code to evaluate
     * @param filename  Optional filename for error reporting
     * @return JSResult with success status, type, and stringified value
     */
    JSResult Eval(const std::string& source, const std::string& filename = "<eval>");

    /**
     * Evaluate a JavaScript module from source.
     * The module can use import/export syntax.
     *
     * @param source    JavaScript module code
     * @param filename  Module filename (must end with .js or .mjs)
     * @return JSResult with success status and result
     */
    JSResult EvalModule(const std::string& source, const std::string& filename = "<module>");

    // ── Global object manipulation ────────────────────────────

    /**
     * Set a global property with a string value.
     *
     * @param name   Property name on the global object
     * @param value  String value to set
     * @return true on success
     */
    bool SetGlobalString(const std::string& name, const std::string& value);

    /**
     * Set a global property with a numeric value.
     *
     * @param name   Property name on the global object
     * @param value  Numeric value to set
     * @return true on success
     */
    bool SetGlobalNumber(const std::string& name, double value);

    /**
     * Set a global property with a boolean value.
     *
     * @param name   Property name on the global object
     * @param value  Boolean value to set
     * @return true on success
     */
    bool SetGlobalBool(const std::string& name, bool value);

    /**
     * Get a global property as a string.
     *
     * @param name  Property name on the global object
     * @return String value, or empty optional if not found / not convertible
     */
    std::optional<std::string> GetGlobalString(const std::string& name) const;

    /**
     * Get a global property as a number.
     *
     * @param name  Property name on the global object
     * @return Numeric value, or empty optional if not found / not convertible
     */
    std::optional<double> GetGlobalNumber(const std::string& name) const;

    // ── Native function registration ──────────────────────────

    /**
     * Register a native C++ callback as a global JavaScript function.
     *
     * The callback receives arguments as stringified values and must
     * return a string result. Arguments are automatically converted
     * to strings before being passed; the return string is converted
     * back to a JS string value.
     *
     * Example:
     *   engine.RegisterNativeFunction("add", [](args) -> std::string {
     *       double a = std::stod(args[0]);
     *       double b = std::stod(args[1]);
     *       return std::to_string(a + b);
     *   });
     *
     * Then in JS: add("1", "2") returns "3"
     *
     * @param name      Function name on the global object
     * @param callback  C++ callback to invoke when JS calls the function
     * @return true on success
     */
    bool RegisterNativeFunction(const std::string& name, JSNativeCallback callback);

    /**
     * Unregister a previously registered native function.
     *
     * @param name  Function name to remove
     * @return true if the function was found and removed
     */
    bool UnregisterNativeFunction(const std::string& name);

    // ── Module loading ────────────────────────────────────────

    /**
     * Register a JS source as a module that can be imported.
     *
     * After registering, JS code can import it:
     *   import { foo } from 'my_module';
     *
     * @param specifier  Module specifier (import path)
     * @param source     JavaScript module source code
     * @return true on success
     */
    bool RegisterModule(const std::string& specifier, const std::string& source);

    // ── Garbage collection ────────────────────────────────────

    /** Run a garbage collection cycle. */
    void GC();

    /** Get memory usage in bytes. */
    size_t GetMemoryUsage() const;

    /**
     * Set the memory limit for the JS runtime.
     *
     * @param limit  Maximum bytes the JS heap can use (0 = unlimited)
     */
    void SetMemoryLimit(size_t limit);

    /**
     * Set the stack size limit for the JS runtime.
     *
     * @param limit  Maximum stack size in bytes (0 = default 256KB)
     */
    void SetStackSize(size_t limit);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace breeze
