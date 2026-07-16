#include "breeze_api.h"
#include "hook_manager.h"
#include "symbol_resolver.h"
#include "logger.h"

#include <mutex>

namespace breeze {

// ── Implementation struct ─────────────────────────────────────
struct BreezeAPI::Impl {
    bool              initialized = false;
    BreezeConfig      config;
    HookManager       hook_mgr;
    SymbolResolver     resolver;
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
    Logger::Info("BreezeAPI v1.0.0 initialized");

    impl_->initialized = true;
    return true;
}

void BreezeAPI::Shutdown() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->initialized) return;

    impl_->hook_mgr.UnhookAll();
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

} // extern "C"
