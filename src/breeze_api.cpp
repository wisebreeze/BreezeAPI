#include "breeze_api.h"
#include "hook_manager.h"
#include "symbol_resolver.h"
#include "js_engine.h"
#include "logger.h"

#include <mutex>
#include <cstring>

namespace breeze {

// ── Implementation struct ─────────────────────────────────────
struct BreezeAPI::Impl {
    bool              initialized = false;
    BreezeConfig      config;
    HookManager       hook_mgr;
    SymbolResolver     resolver;
    JSEngine          js_engine;
    std::mutex        mutex;
};

// ── Singleton ─────────────────────────────────────────────────
BreezeAPI& BreezeAPI::Instance() {
    static BreezeAPI instance;
    return instance;
}

BreezeAPI::BreezeAPI() : impl_(new Impl()) {}
BreezeAPI::~BreezeAPI() { delete impl_; }

// ── Lifecycle ─────────────────────────────────────────────────
bool BreezeAPI::Init(const BreezeConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->initialized) return true;

    impl_->config = config;
    Logger::Init(config.log_level, config.log_to_android, config.log_tag);
    Logger::Info("BreezeAPI v1.1.0 initialized");

    impl_->initialized = true;
    return true;
}

void BreezeAPI::Shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return;

    impl_->hook_mgr.UnhookAll();
    impl_->js_engine.Shutdown();
    Logger::Info("BreezeAPI shut down");
    impl_->initialized = false;
}

bool BreezeAPI::IsInitialized() const {
    return impl_->initialized;
}

// ── Hook operations ───────────────────────────────────────────
HookStatus BreezeAPI::HookBySymbol(
    const std::string& library,
    const std::string& symbol,
    Address            replace,
    Address*           original,
    const std::string& tag)
{
    auto base = impl_->resolver.ResolveLibraryBase(library);
    if (!base) {
        Logger::Error("Library not found: %s", library.c_str());
        return HookStatus::SymbolNotFound;
    }

    auto target = impl_->resolver.ResolveSymbol(library, symbol);
    if (!target) {
        Logger::Error("Symbol not found: %s in %s", symbol.c_str(), library.c_str());
        return HookStatus::SymbolNotFound;
    }

    return impl_->hook_mgr.Hook(target, replace, original, tag.empty() ? symbol : tag);
}

HookStatus BreezeAPI::HookByOffset(
    const std::string& library,
    uintptr_t          offset,
    Address            replace,
    Address*           original,
    const std::string& tag)
{
    auto base = impl_->resolver.ResolveLibraryBase(library);
    if (!base) {
        Logger::Error("Library not found: %s", library.c_str());
        return HookStatus::SymbolNotFound;
    }

    auto target = static_cast<Address>(static_cast<uint8_t*>(base) + offset);
    std::string name = tag.empty() ? (library + "+0x" + std::to_string(offset)) : tag;

    return impl_->hook_mgr.Hook(target, replace, original, name);
}

HookStatus BreezeAPI::HookByAddress(
    Address            target,
    Address            replace,
    Address*           original,
    const std::string& tag)
{
    if (!target) return HookStatus::InvalidAddress;
    return impl_->hook_mgr.Hook(target, replace, original, tag);
}

HookStatus BreezeAPI::Unhook(Address target) {
    return impl_->hook_mgr.Unhook(target);
}

void BreezeAPI::UnhookAll() {
    impl_->hook_mgr.UnhookAll();
}

// ── Query ─────────────────────────────────────────────────────
std::optional<HookInfo> BreezeAPI::GetHookInfo(Address target) const {
    return impl_->hook_mgr.GetHookInfo(target);
}

std::optional<HookInfo> BreezeAPI::GetHookInfoByTag(const std::string& tag) const {
    return impl_->hook_mgr.GetHookInfoByTag(tag);
}

std::vector<HookInfo> BreezeAPI::GetAllHooks() const {
    return impl_->hook_mgr.GetAllHooks();
}

// ── Symbol resolution ─────────────────────────────────────────
Address BreezeAPI::ResolveLibraryBase(const std::string& library) const {
    return impl_->resolver.ResolveLibraryBase(library);
}

Address BreezeAPI::ResolveSymbol(const std::string& library, const std::string& symbol) const {
    return impl_->resolver.ResolveSymbol(library, symbol);
}

// ── JavaScript engine ─────────────────────────────────────────
JSResult BreezeAPI::EvalJS(const std::string& source, const std::string& filename) {
    return impl_->js_engine.Eval(source, filename);
}

JSResult BreezeAPI::EvalJSModule(const std::string& source, const std::string& filename) {
    return impl_->js_engine.EvalModule(source, filename);
}

bool BreezeAPI::SetJSGlobalString(const std::string& name, const std::string& value) {
    return impl_->js_engine.SetGlobalString(name, value);
}

bool BreezeAPI::SetJSGlobalNumber(const std::string& name, double value) {
    return impl_->js_engine.SetGlobalNumber(name, value);
}

bool BreezeAPI::SetJSGlobalBool(const std::string& name, bool value) {
    return impl_->js_engine.SetGlobalBool(name, value);
}

std::optional<std::string> BreezeAPI::GetJSGlobalString(const std::string& name) const {
    return impl_->js_engine.GetGlobalString(name);
}

std::optional<double> BreezeAPI::GetJSGlobalNumber(const std::string& name) const {
    return impl_->js_engine.GetGlobalNumber(name);
}

bool BreezeAPI::RegisterJSFunction(const std::string& name, JSNativeCallback callback) {
    return impl_->js_engine.RegisterNativeFunction(name, std::move(callback));
}

bool BreezeAPI::UnregisterJSFunction(const std::string& name) {
    return impl_->js_engine.UnregisterNativeFunction(name);
}

bool BreezeAPI::RegisterJSModule(const std::string& specifier, const std::string& source) {
    return impl_->js_engine.RegisterModule(specifier, source);
}

void BreezeAPI::JSGC() {
    impl_->js_engine.GC();
}

size_t BreezeAPI::GetJSMemoryUsage() const {
    return impl_->js_engine.GetMemoryUsage();
}

void BreezeAPI::SetJSMemoryLimit(size_t limit) {
    impl_->js_engine.SetMemoryLimit(limit);
}

void BreezeAPI::SetJSStackSize(size_t limit) {
    impl_->js_engine.SetStackSize(limit);
}

// ── Configuration ─────────────────────────────────────────────
void BreezeAPI::SetLogLevel(LogLevel level) {
    impl_->config.log_level = level;
    Logger::SetLevel(level);
}

LogLevel BreezeAPI::GetLogLevel() const {
    return impl_->config.log_level;
}

} // namespace breeze

// ══════════════════════════════════════════════════════════════
//  C-compatible interface
// ══════════════════════════════════════════════════════════════

extern "C" {

bool breeze_init() {
    return breeze::BreezeAPI::Instance().Init();
}

void breeze_shutdown() {
    breeze::BreezeAPI::Instance().Shutdown();
}

int32_t breeze_hook_symbol(const char* lib, const char* sym, void* replace, void** original) {
    return static_cast<int32_t>(
        breeze::BreezeAPI::Instance().HookBySymbol(lib, sym, replace, original, sym ?: "")
    );
}

int32_t breeze_hook_offset(const char* lib, uintptr_t offset, void* replace, void** original) {
    return static_cast<int32_t>(
        breeze::BreezeAPI::Instance().HookByOffset(lib, offset, replace, original)
    );
}

int32_t breeze_hook_address(void* target, void* replace, void** original) {
    return static_cast<int32_t>(
        breeze::BreezeAPI::Instance().HookByAddress(target, replace, original)
    );
}

int32_t breeze_unhook(void* target) {
    return static_cast<int32_t>(breeze::BreezeAPI::Instance().Unhook(target));
}

void breeze_unhook_all() {
    breeze::BreezeAPI::Instance().UnhookAll();
}

void* breeze_resolve_library(const char* lib) {
    return breeze::BreezeAPI::Instance().ResolveLibraryBase(lib);
}

void* breeze_resolve_symbol(const char* lib, const char* sym) {
    return breeze::BreezeAPI::Instance().ResolveSymbol(lib, sym);
}

void breeze_set_log_level(int32_t level) {
    breeze::BreezeAPI::Instance().SetLogLevel(static_cast<breeze::LogLevel>(level));
}

// ── JS engine C ABI ──────────────────────────────────────────

breeze_js_result breeze_js_eval(const char* source, const char* filename) {
    auto r = breeze::BreezeAPI::Instance().EvalJS(
        source, filename ? filename : "<eval>");
    // NOTE: the returned value pointer is only valid until the next JS call
    // Callers should copy the string immediately.
    return {r.success ? 1 : 0, r.type, r.value.c_str()};
}

breeze_js_result breeze_js_eval_module(const char* source, const char* filename) {
    auto r = breeze::BreezeAPI::Instance().EvalJSModule(
        source, filename ? filename : "<module>.mjs");
    return {r.success ? 1 : 0, r.type, r.value.c_str()};
}

bool breeze_js_set_global_string(const char* name, const char* value) {
    return breeze::BreezeAPI::Instance().SetJSGlobalString(name, value);
}

bool breeze_js_set_global_number(const char* name, double value) {
    return breeze::BreezeAPI::Instance().SetJSGlobalNumber(name, value);
}

bool breeze_js_set_global_bool(const char* name, int32_t value) {
    return breeze::BreezeAPI::Instance().SetJSGlobalBool(name, value != 0);
}

char* breeze_js_get_global_string(const char* name) {
    auto val = breeze::BreezeAPI::Instance().GetJSGlobalString(name);
    if (!val) return nullptr;
    // Allocate a copy that the caller must free with breeze_js_free_string
    char* result = static_cast<char*>(malloc(val->size() + 1));
    if (result) {
        memcpy(result, val->c_str(), val->size() + 1);
    }
    return result;
}

double breeze_js_get_global_number(const char* name) {
    auto val = breeze::BreezeAPI::Instance().GetJSGlobalNumber(name);
    return val.value_or(0.0);
}

void breeze_js_gc() {
    breeze::BreezeAPI::Instance().JSGC();
}

size_t breeze_js_memory_usage() {
    return breeze::BreezeAPI::Instance().GetJSMemoryUsage();
}

void breeze_js_set_memory_limit(size_t limit) {
    breeze::BreezeAPI::Instance().SetJSMemoryLimit(limit);
}

void breeze_js_set_stack_size(size_t limit) {
    breeze::BreezeAPI::Instance().SetJSStackSize(limit);
}

void breeze_js_free_string(char* str) {
    free(str);
}

} // extern "C"
