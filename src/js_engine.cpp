#include "js_engine.h"
#include "logger.h"

#include <quickjs.h>
#include <cstring>
#include <mutex>
#include <cassert>

namespace breeze {

// ── Implementation struct (standalone, not nested) ────────────
struct JSEngineImpl {
    JSRuntime*     runtime  = nullptr;
    JSContext*     context  = nullptr;
    bool           initialized = false;

    std::unordered_map<std::string, JSNativeCallback> native_callbacks;
    std::unordered_map<std::string, std::string>      module_registry;
    mutable std::mutex mutex;

    ~JSEngineImpl() {
        if (context) JS_FreeContext(context);
        if (runtime) JS_FreeRuntime(runtime);
    }
};

// Helper to get the impl pointer from the opaque void*
static inline JSEngineImpl* GetImpl(void* p) {
    return static_cast<JSEngineImpl*>(p);
}
static inline const JSEngineImpl* GetImplConst(const void* p) {
    return static_cast<const JSEngineImpl*>(p);
}

// Forward declaration — defined after all JSEngine member functions
static JSResult ConvertResult(JSContext* ctx, JSValue val);

// ── Native function trampoline ────────────────────────────────
// Uses a unique magic number per function to dispatch correctly.
static JSValue js_native_callback(JSContext* ctx, JSValueConst this_val,
                                   int argc, JSValueConst* argv, int magic) {
    auto* runtime = JS_GetRuntime(ctx);
    auto* impl = static_cast<JSEngineImpl*>(JS_GetRuntimeOpaque(runtime));
    if (!impl) {
        return JS_ThrowInternalError(ctx, "JSEngine not initialized");
    }

    // Use magic number as index into a static name table
    // We maintain a global vector of registered names
    static std::vector<std::string> s_func_names;
    static std::mutex s_func_mutex;

    std::string func_name;
    {
        std::lock_guard<std::mutex> lock(s_func_mutex);
        if (magic >= 0 && magic < static_cast<int>(s_func_names.size())) {
            func_name = s_func_names[magic];
        }
    }

    if (func_name.empty()) {
        return JS_ThrowReferenceError(ctx, "Native function not found (magic=%d)", magic);
    }

    std::lock_guard<std::mutex> lock(impl->mutex);

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

// Helper: get the next magic number and register the name
static int AllocateMagicNumber(const std::string& name) {
    static std::vector<std::string> s_func_names;
    static std::mutex s_func_mutex;

    std::lock_guard<std::mutex> lock(s_func_mutex);
    int magic = static_cast<int>(s_func_names.size());
    s_func_names.push_back(name);
    return magic;
}

// ── Module loader ─────────────────────────────────────────────
static JSModuleDef* js_module_loader(JSContext* ctx, const char* module_name, void* opaque) {
    auto* impl = static_cast<JSEngineImpl*>(opaque);

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
    JSModuleDef* module = JS_VALUE_GET_PTR(val) ?
        reinterpret_cast<JSModuleDef*>(JS_VALUE_GET_PTR(val)) : nullptr;
    JS_FreeValue(ctx, val);

    return module;
}

// ── Constructor / Destructor ──────────────────────────────────
JSEngine::JSEngine() : impl_(new JSEngineImpl()) {
    Init();
}

JSEngine::~JSEngine() {
    Shutdown();
    delete GetImpl(impl_);
}

// ── Lifecycle ─────────────────────────────────────────────────
bool JSEngine::Init() {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->initialized) return true;

    impl->runtime = JS_NewRuntime();
    if (!impl->runtime) {
        Logger::Error("JSEngine: Failed to create QuickJS runtime");
        return false;
    }

    impl->context = JS_NewContext(impl->runtime);
    if (!impl->context) {
        Logger::Error("JSEngine: Failed to create QuickJS context");
        JS_FreeRuntime(impl->runtime);
        impl->runtime = nullptr;
        return false;
    }

    // Set runtime opaque pointer for callback dispatch
    JS_SetRuntimeOpaque(impl->runtime, impl);

    // Register the module loader
    JS_SetModuleLoaderFunc(impl->runtime, nullptr, js_module_loader, impl);

    impl->initialized = true;
    Logger::Info("JSEngine: QuickJS initialized");
    return true;
}

void JSEngine::Shutdown() {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return;

    impl->native_callbacks.clear();
    impl->module_registry.clear();

    if (impl->context) {
        JS_FreeContext(impl->context);
        impl->context = nullptr;
    }
    if (impl->runtime) {
        JS_FreeRuntime(impl->runtime);
        impl->runtime = nullptr;
    }

    impl->initialized = false;
    Logger::Info("JSEngine: shut down");
}

bool JSEngine::IsInitialized() const {
    return GetImplConst(impl_)->initialized;
}

// ── Script evaluation ─────────────────────────────────────────
JSResult JSEngine::Eval(const std::string& source, const std::string& filename) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) {
        return {false, JSType::Exception, "JSEngine not initialized"};
    }

    JSValue val = JS_Eval(impl->context, source.c_str(), source.size(),
                           filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    JSResult result = ConvertResult(impl->context, val);
    JS_FreeValue(impl->context, val);
    return result;
}

JSResult JSEngine::EvalModule(const std::string& source, const std::string& filename) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) {
        return {false, JSType::Exception, "JSEngine not initialized"};
    }

    JSValue val = JS_Eval(impl->context, source.c_str(), source.size(),
                           filename.c_str(), JS_EVAL_TYPE_MODULE);
    JSResult result = ConvertResult(impl->context, val);
    JS_FreeValue(impl->context, val);
    return result;
}

// ── Global object manipulation ────────────────────────────────
bool JSEngine::SetGlobalString(const std::string& name, const std::string& value) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return false;

    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue val = JS_NewStringLen(impl->context, value.c_str(), value.size());
    int ret = JS_SetPropertyStr(impl->context, global, name.c_str(), val);
    JS_FreeValue(impl->context, global);
    return ret >= 0;
}

bool JSEngine::SetGlobalNumber(const std::string& name, double value) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return false;

    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue val = JS_NewFloat64(impl->context, value);
    int ret = JS_SetPropertyStr(impl->context, global, name.c_str(), val);
    JS_FreeValue(impl->context, global);
    return ret >= 0;
}

bool JSEngine::SetGlobalBool(const std::string& name, bool value) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return false;

    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue val = JS_NewBool(impl->context, value ? 1 : 0);
    int ret = JS_SetPropertyStr(impl->context, global, name.c_str(), val);
    JS_FreeValue(impl->context, global);
    return ret >= 0;
}

std::optional<std::string> JSEngine::GetGlobalString(const std::string& name) const {
    auto* impl = GetImplConst(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return std::nullopt;

    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue val = JS_GetPropertyStr(impl->context, global, name.c_str());
    JS_FreeValue(impl->context, global);

    if (JS_IsUndefined(val) || JS_IsException(val)) {
        JS_FreeValue(impl->context, val);
        return std::nullopt;
    }

    const char* cstr = JS_ToCString(impl->context, val);
    std::optional<std::string> result;
    if (cstr) {
        result = std::string(cstr);
    }
    JS_FreeCString(impl->context, cstr);
    JS_FreeValue(impl->context, val);
    return result;
}

std::optional<double> JSEngine::GetGlobalNumber(const std::string& name) const {
    auto* impl = GetImplConst(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return std::nullopt;

    JSValue global = JS_GetGlobalObject(impl->context);
    JSValue val = JS_GetPropertyStr(impl->context, global, name.c_str());
    JS_FreeValue(impl->context, global);

    if (JS_IsUndefined(val) || JS_IsException(val)) {
        JS_FreeValue(impl->context, val);
        return std::nullopt;
    }

    double d;
    int ret = JS_ToFloat64(impl->context, &d, val);
    JS_FreeValue(impl->context, val);

    if (ret < 0) return std::nullopt;
    return d;
}

// ── Native function registration ──────────────────────────────
bool JSEngine::RegisterNativeFunction(const std::string& name, JSNativeCallback callback) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return false;

    // Allocate a unique magic number for this function
    int magic = AllocateMagicNumber(name);

    // Create a C function with magic dispatch
    JSValue global = JS_GetGlobalObject(impl->context);
    JSCFunctionType ft;
    ft.generic_magic = js_native_callback;
    JSValue func = JS_NewCFunction2(impl->context, ft.generic, name.c_str(), 0,
                                     JS_CFUNC_generic_magic, magic);
    int ret = JS_SetPropertyStr(impl->context, global, name.c_str(), func);
    JS_FreeValue(impl->context, global);

    if (ret < 0) return false;

    impl->native_callbacks[name] = std::move(callback);
    Logger::Debug("JSEngine: Registered native function '%s'", name.c_str());
    return true;
}

bool JSEngine::UnregisterNativeFunction(const std::string& name) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->initialized) return false;

    auto it = impl->native_callbacks.find(name);
    if (it == impl->native_callbacks.end()) return false;

    // Set the global property to undefined
    JSValue global = JS_GetGlobalObject(impl->context);
    JS_SetPropertyStr(impl->context, global, name.c_str(), JS_UNDEFINED);
    JS_FreeValue(impl->context, global);

    impl->native_callbacks.erase(it);
    Logger::Debug("JSEngine: Unregistered native function '%s'", name.c_str());
    return true;
}

// ── Module loading ────────────────────────────────────────────
bool JSEngine::RegisterModule(const std::string& specifier, const std::string& source) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->module_registry[specifier] = source;
    Logger::Debug("JSEngine: Registered module '%s'", specifier.c_str());
    return true;
}

// ── Garbage collection ────────────────────────────────────────
void JSEngine::GC() {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->runtime) {
        JS_RunGC(impl->runtime);
    }
}

size_t JSEngine::GetMemoryUsage() const {
    auto* impl = GetImplConst(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (!impl->runtime) return 0;

    JSMemoryUsage stats;
    JS_ComputeMemoryUsage(impl->runtime, &stats);
    return static_cast<size_t>(stats.memory_used_size);
}

void JSEngine::SetMemoryLimit(size_t limit) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->runtime) {
        JS_SetMemoryLimit(impl->runtime, limit);
    }
}

void JSEngine::SetStackSize(size_t limit) {
    auto* impl = GetImpl(impl_);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (impl->runtime) {
        JS_SetMaxStackSize(impl->runtime, limit);
    }
}

// ── Internal helpers ──────────────────────────────────────────
static JSResult ConvertResult(JSContext* ctx, JSValue val) {
    if (JS_IsException(val)) {
        JSValue exception = JS_GetException(ctx);
        const char* err = JS_ToCString(ctx, exception);

        // Try to get stack trace
        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
        const char* stack_str = JS_ToCString(ctx, stack);

        std::string error_msg;
        if (err) {
            error_msg = err;
        }
        if (stack_str && stack_str[0]) {
            error_msg += "\n";
            error_msg += stack_str;
        }

        JS_FreeCString(ctx, stack_str);
        JS_FreeValue(ctx, stack);
        JS_FreeCString(ctx, err);
        JS_FreeValue(ctx, exception);

        return {false, JSType::Exception, error_msg};
    }

    JSType type = JSType::Undefined;
    if (JS_IsUndefined(val))            type = JSType::Undefined;
    else if (JS_IsNull(val))            type = JSType::Null;
    else if (JS_IsBool(val))            type = JSType::Bool;
    else if (JS_IsNumber(val))          type = JSType::Number;
    else if (JS_IsString(val))          type = JSType::String;
    else if (JS_IsObject(val))          type = JSType::Object;
    else if (JS_IsFunction(ctx, val))   type = JSType::Function;

    const char* cstr = JS_ToCString(ctx, val);
    std::string value = cstr ? cstr : "";
    JS_FreeCString(ctx, cstr);

    return {true, type, value};
}

} // namespace breeze
