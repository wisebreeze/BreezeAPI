#pragma once

#include "breeze_api.h"
#include <unordered_map>
#include <mutex>
#include <optional>

// Forward declare dobby header
extern "C" int DobbyHook(void* address, void* replace_func, void** origin_func);
extern "C" int DobbyDestroy(void* address);
extern "C" void* DobbySymbolResolver(const char* image_name, const char* symbol_name);

namespace breeze {

class HookManager {
public:
    HookManager() = default;
    ~HookManager() { UnhookAll(); }

    HookStatus Hook(Address target, Address replace, Address* original, const std::string& tag);
    HookStatus Unhook(Address target);
    void UnhookAll();

    std::optional<HookInfo> GetHookInfo(Address target) const;
    std::optional<HookInfo> GetHookInfoByTag(const std::string& tag) const;
    std::vector<HookInfo> GetAllHooks() const;

private:
    struct HookEntry {
        HookInfo info;
    };

    mutable std::mutex mutex_;
    std::unordered_map<Address, HookEntry> hooks_;
    std::unordered_map<std::string, Address> tag_index_;
};

} // namespace breeze
