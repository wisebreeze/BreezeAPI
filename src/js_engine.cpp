#include "js_engine.h"
#include "logger.h"

#include <quickjs.h>
#include <cstring>
#include <mutex>
#include <cassert>

namespace breeze {

// ── Implementation struct ─────────────────────────────────────
struct JSEngine::Impl {
    JSRuntime*     runtime  = nullptr;
    JSContext*     context  = nullptr;
    bool           initialized = false;

    // Map from function name to callback.
    // The C trampoline looks up the name via JS_GetPropertyStr("name").
    std::unordered_map<std::string, JSNativeCallback> native_callbacks;
    std::unordered_map<std::string, std::string>      module_registry;
    mutable std::mutex mutex;

    ~Impl() {
        if (context) JS_FreeContext(context);
        if (runtime) JS_FreeRuntime(runtime);
    }
};

// ── Native function trampoline ────────────────────────────────
// QuickJS C function callback. We store the function name as the
// "name" property on the JSFunction object, then recover it here.
static JSValue js_native_callback(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv, int magic) {
    auto* runtime = JS_GetRuntime(ctx);
    auto* impl = static_cast<JSEngine::Impl*>(JS_GetRuntimeOpaque(runtime));
    if (!impl) {
        return JS_ThrowInternalError(ctx, "JSEngine not initialized");
    }

    // Retrieve the function object and its name property.
    // Unfortunately there is no public API to get the active function in QuickJS.
    // Instead, we use the magic number as an index hint and search through
    // registered callbacks. For a small number of native functions this is fine.
    //
    // Better approach: iterate registered callbacks and check which one matches.
    // We do this by storing a name→callback map and checking each name against
    // a special ".__breeze_func_id" property we set on the function object.

    // Since we set magic=0 for all functions, we need another way.
    // Solution: iterate all registered callbacks — but we need the name.
    // We use JS_GetPropertyStr on the caller's arguments object to find the
    // callee name. However, QuickJS doesn't expose arguments.callee easily.
    //
    // Simplest reliable approach: store a global lookup table in JS.
    // We maintain a hidden JS global object __breeze_native_map that maps
    // function objects to their names.

    // Actually the simplest approach: just iterate all registered names.
    // With typically < 20 native functions, this is negligible overhead.

    std::lock_guard<std::mutex> lock(impl->mutex);

    // Try each registered callback by testing the function's name property
    // We store the name as a hidden "__breeze_name" property on each function
    JSValue callee = JS_UNDEFINED; // We can't easily get the callee

    // Alternative: since we control registration, we store a parallel map
    // keyed by a unique integer ID, and use magic as that ID.
    // But since we used magic=0 for all, we iterate instead.

    // Final approach: check each registered callback by trying to call it
    // if the name matches. We use a simple strategy: convert all args to
    // strings, find the matching callback by checking which one is callable.
    // Since we can't identify which function was called, we use a trick:
    // store the function name in a thread-local before the call.
    // This won't work since JS calls are from the same thread.

    // BEST APPROACH: Register each function with a unique magic number
    // that maps to the callback name. We'll use a static lookup table.
    // For now, we use a simpler design: wrap each callback in a JS closure
    // that captures the name.

    // SIMPLEST: just iterate all callbacks. For < 20 functions this is O(20).
    // But we still can't tell WHICH one was called...

    // The real solution: use a per-function magic number as an index.
    // Let's restructure: we'll register with a magic number that equals
    // the callback's index in a static vector.
    // Since we can't change the registration after the fact, we use a
    // different strategy: we look up a global map.

    // Check if we have a global __breeze_func_map
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue func_map = JS_GetPropertyStr(ctx, global, "__breeze_func_map");
    std::string func_name;

    if (JS_IsObject(func_map)) {
        // The function should have stored its name in the map
        // We try each registered name
        for (auto& [name, cb] : impl->native_callbacks) {
            JSValue entry = JS_GetPropertyStr(ctx, func_map, name.c_str());
            if (!JS_IsUndefined(entry)) {
                func_name = name;
                JS_FreeValue(ctx, entry);
                break;
            }
        }
    }
    JS_FreeValue(ctx, func_map);
    JS_FreeValue(ctx, global);

    if (func_name.empty()) {
        // Fallback: use the first registered callback (should not happen normally)
        if (!impl->native_callbacks.empty()) {
            func_name = impl->native_callbacks.begin()->first;
        } else {
            return JS_ThrowReferenceError(ctx, "No native functions registered");
        }
    }

    auto it = impl->native_callbacks.find(func_name);
    if (it == impl->native_callbacks.end()) {
        return JS_ThrowReferenceError(ctx, "Native function not found: %s", func_name.c_str());
    }

    // Convert JS arguments to strings
    std::vector<std::string> args;
    args.reserve(argc);
    for (int i = 0; i < argc; i++) {
        const char* cstr = JS_ToCString(ctx, argv[i]);
        args.emplace_back(cstr ? cstr : "");
        JS_FreeCString(ctx, cstr);
    }

    // Invoke the C++ callback
    std::string result;
    try {
        result = it->second(args);
    } catch (const std::exception& e) {
        return JS_ThrowInternalError(ctx, "Native function exception: %s", e.what());
    } catch (...) {
        return JS_ThrowInternalError(ctx, "Native function threw unknown exception");
    }

    return JS_NewStringLen(ctx, result.c_str(), result.size());
}

// ── Module loader ─────────────────────────────────────────────
static JSModuleDef* js_module_loader(JSContext* ctx, const char* module_name, void* opaque) {
    auto* impl = static_cast<JSEngine::Impl*>(opaque);

    std::lock_guard<std::mutex> lock(impl->mutex);
    auto it = impl->module_registry.find(module_name);
    if (it == impl->module_registry.end()) {
        JS_ThrowReferenceError(ctx, "Module not found: '%s'", module_name);
        return nullptr;
    }

    // Evaluate the module source directly
    JSValue val = JS_Eval(ctx, it->second.c_str(), it->second.size(),
                          module_name, JS_EVAL_TYPE_MODULE);
    if (JS_IsException(val)) {
        return nullptr;
    }

    // The module is now loaded in the runtime; retrieve its definition
    // JS_Eval with JS_EVAL_TYPE_MODULE returns the module namespace object
    // The module is automatically registered in the runtime's module list
    JSModuleDef* module = JS_VALUE_GET_PTR(val) ?
        reinterpret_cast<JSModuleDef*>(JS_VALUE_GET_PTR(val)) : nullptr;
    JS_FreeValue(ctx, val);

    return module;
}

// ── Constructor / Destructor ──────────────────────────────────
JSEngine::JSEngine() : impl_(new Impl()) {
    Init();
}

JSEngine::~JSEngine() {
    Shutdown();
    delete impl_;
}

// ── Lifecycle ─────────────────────────────────────────────────
bool JSEngine::Init() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->initialized) return true;

    impl_->runtime = JS_NewRuntime();
    if (!impl_->runtime) {
        Logger::Error("JSEngine: Failed to create QuickJS runtime");
        return false;
    }

    impl_->context = JS_NewContext(impl_->runtime);
    if (!impl_->context) {
        Logger::Error("JSEngine: Failed to create QuickJS context");
        JS_FreeRuntime(impl_->runtime);
        impl_->runtime = nullptr;
        return false;
    }

    // Set runtime opaque pointer for callback dispatch
    JS_SetRuntimeOpaque(impl_->runtime, impl_);

    // Register the module loader
    JS_SetModuleLoaderFunc(impl_->runtime, nullptr, js_module_loader, impl_);

    impl_->initialized = true;
    Logger::Info("JSEngine: QuickJS initialized");
    return true;
}

void JSEngine::Shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return;

    impl_->native_callbacks.clear();
    impl_->module_registry.clear();

    if (impl_->context) {
        JS_FreeContext(impl_->context);
        impl_->context = nullptr;
    }
    if (impl_->runtime) {
        JS_FreeRuntime(impl_->runtime);
        impl_->runtime = nullptr;
    }

    impl_->initialized = false;
    Logger::Info("JSEngine: shut down");
}

bool JSEngine::IsInitialized() const {
    return impl_->initialized;
}

// ── Script evaluation ─────────────────────────────────────────
JSResult JSEngine::Eval(const std::string& source, const std::string& filename) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) {
        return {false, JSType::Exception, "JSEngine not initialized"};
    }

    JSValue val = JS_Eval(impl_->context, source.c_str(), source.size(),
                           filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    JSResult result = ConvertResult(val);
    JS_FreeValue(impl_->context, val);
    return result;
}

JSResult JSEngine::EvalModule(const std::string& source, const std::string& filename) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) {
        return {false, JSType::Exception, "JSEngine not initialized"};
    }

    JSValue val = JS_Eval(impl_->context, source.c_str(), source.size(),
                           filename.c_str(), JS_EVAL_TYPE_MODULE);
    JSResult result = ConvertResult(val);
    JS_FreeValue(impl_->context, val);
    return result;
}

// ── Global object manipulation ────────────────────────────────
bool JSEngine::SetGlobalString(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return false;

    JSValue global = JS_GetGlobalObject(impl_->context);
    JSValue val = JS_NewStringLen(impl_->context, value.c_str(), value.size());
    int ret = JS_SetPropertyStr(impl_->context, global, name.c_str(), val);
    JS_FreeValue(impl_->context, global);
    return ret >= 0;
}

bool JSEngine::SetGlobalNumber(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return false;

    JSValue global = JS_GetGlobalObject(impl_->context);
    JSValue val = JS_NewFloat64(impl_->context, value);
    int ret = JS_SetPropertyStr(impl_->context, global, name.c_str(), val);
    JS_FreeValue(impl_->context, global);
    return ret >= 0;
}

bool JSEngine::SetGlobalBool(const std::string& name, bool value) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return false;

    JSValue global = JS_GetGlobalObject(impl_->context);
    JSValue val = JS_NewBool(impl_->context, value ? 1 : 0);
    int ret = JS_SetPropertyStr(impl_->context, global, name.c_str(), val);
    JS_FreeValue(impl_->context, global);
    return ret >= 0;
}

std::optional<std::string> JSEngine::GetGlobalString(const std::string& name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return std::nullopt;

    JSValue global = JS_GetGlobalObject(impl_->context);
    JSValue val = JS_GetPropertyStr(impl_->context, global, name.c_str());
    JS_FreeValue(impl_->context, global);

    if (JS_IsUndefined(val) || JS_IsException(val)) {
        JS_FreeValue(impl_->context, val);
        return std::nullopt;
    }

    const char* cstr = JS_ToCString(impl_->context, val);
    std::optional<std::string> result;
    if (cstr) {
        result = std::string(cstr);
    }
    JS_FreeCString(impl_->context, cstr);
    JS_FreeValue(impl_->context, val);
    return result;
}

std::optional<double> JSEngine::GetGlobalNumber(const std::string& name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return std::nullopt;

    JSValue global = JS_GetGlobalObject(impl_->context);
    JSValue val = JS_GetPropertyStr(impl_->context, global, name.c_str());
    JS_FreeValue(impl_->context, global);

    if (JS_IsUndefined(val) || JS_IsException(val)) {
        JS_FreeValue(impl_->context, val);
        return std::nullopt;
    }

    double d;
    int ret = JS_ToFloat64(impl_->context, &d, val);
    JS_FreeValue(impl_->context, val);

    if (ret < 0) return std::nullopt;
    return d;
}

// ── Native function registration ──────────────────────────────
bool JSEngine::RegisterNativeFunction(const std::string& name, JSNativeCallback callback) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return false;

    // Create a C function and set it as a global property
    JSValue global = JS_GetGlobalObject(impl_->context);
    JSValue func = JS_NewCFunction(impl_->context, js_native_callback, name.c_str(), 0);
    int ret = JS_SetPropertyStr(impl_->context, global, name.c_str(), func);
    JS_FreeValue(impl_->context, global);

    if (ret < 0) return false;

    // Also store the function name in the global __breeze_func_map
    // so the callback trampoline can identify which function was called
    JSValue global2 = JS_GetGlobalObject(impl_->context);
    JSValue func_map = JS_GetPropertyStr(impl_->context, global2, "__breeze_func_map");
    if (!JS_IsObject(func_map)) {
        func_map = JS_NewObject(impl_->context);
        JS_SetPropertyStr(impl_->context, global2, "__breeze_func_map", func_map);
    }
    JS_SetPropertyStr(impl_->context, func_map, name.c_str(), JS_NewBool(impl_->context, 1));
    JS_FreeValue(impl_->context, func_map);
    JS_FreeValue(impl_->context, global2);

    impl_->native_callbacks[name] = std::move(callback);
    Logger::Debug("JSEngine: Registered native function '%s'", name.c_str());
    return true;
}

bool JSEngine::UnregisterNativeFunction(const std::string& name) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return false;

    auto it = impl_->native_callbacks.find(name);
    if (it == impl_->native_callbacks.end()) return false;

    // Set the global property to undefined
    JSValue global = JS_GetGlobalObject(impl_->context);
    JS_SetPropertyStr(impl_->context, global, name.c_str(), JS_UNDEFINED);
    JS_FreeValue(impl_->context, global);

    // Remove from the func_map
    JSValue global2 = JS_GetGlobalObject(impl_->context);
    JSValue func_map = JS_GetPropertyStr(impl_->context, global2, "__breeze_func_map");
    if (JS_IsObject(func_map)) {
        JS_DeletePropertyStr(impl_->context, func_map, name.c_str());
    }
    JS_FreeValue(impl_->context, func_map);
    JS_FreeValue(impl_->context, global2);

    impl_->native_callbacks.erase(it);
    Logger::Debug("JSEngine: Unregistered native function '%s'", name.c_str());
    return true;
}

// ── Module loading ────────────────────────────────────────────
bool JSEngine::RegisterModule(const std::string& specifier, const std::string& source) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->module_registry[specifier] = source;
    Logger::Debug("JSEngine: Registered module '%s'", specifier.c_str());
    return true;
}

// ── Garbage collection ────────────────────────────────────────
void JSEngine::GC() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->runtime) {
        JS_RunGC(impl_->runtime);
    }
}

size_t JSEngine::GetMemoryUsage() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->runtime) return 0;

    JSMemoryUsage stats;
    JS_ComputeMemoryUsage(impl_->runtime, &stats);
    return static_cast<size_t>(stats.memory_used_size);
}

void JSEngine::SetMemoryLimit(size_t limit) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->runtime) {
        JS_SetMemoryLimit(impl_->runtime, limit);
    }
}

void JSEngine::SetStackSize(size_t limit) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->runtime) {
        JS_SetMaxStackSize(impl_->runtime, limit);
    }
}

// ── Internal helpers ──────────────────────────────────────────
JSResult JSEngine::ConvertResult(JSValue val) const {
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(impl_->context);
        const char* err = JS_ToCString(impl_->context, exception);

        // Try to get stack trace
        JSValue stack = JS_GetPropertyStr(impl_->context, exception, "stack");
        const char* stack_str = JS_ToCString(impl_->context, stack);

        std::string error_msg;
        if (err) {
            error_msg = err;
        }
        if (stack_str && stack_str[0]) {
            error_msg += "\n";
            error_msg += stack_str;
        }

        JS_FreeCString(impl_->context, stack_str);
        JS_FreeValue(impl_->context, stack);
        JS_FreeCString(impl_->context, err);
        JS_FreeValue(impl_->context, exception);

        return {false, JSType::Exception, error_msg};
    }

    JSType type = JSType::Undefined;
    if (JS_IsUndefined(val))            type = JSType::Undefined;
    else if (JS_IsNull(val))            type = JSType::Null;
    else if (JS_IsBool(val))            type = JSType::Bool;
    else if (JS_IsNumber(val))          type = JSType::Number;
    else if (JS_IsString(val))          type = JSType::String;
    else if (JS_IsObject(val))          type = JSType::Object;
    else if (JS_IsFunction(impl_->context, val)) type = JSType::Function;

    const char* cstr = JS_ToCString(impl_->context, val);
    std::string value = cstr ? cstr : "";
    JS_FreeCString(impl_->context, cstr);

    return {true, type, value};
}

} // namespace breeze
