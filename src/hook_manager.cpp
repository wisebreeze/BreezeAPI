#include "hook_manager.h"
#include "logger.h"

namespace breeze {

HookStatus HookManager::Hook(Address target, Address replace, Address* original, const std::string& tag) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!target || !replace) {
        Logger::Error("Hook: invalid target or replace address");
        return HookStatus::InvalidAddress;
    }

    // Check if already hooked
    if (hooks_.find(target) != hooks_.end()) {
        Logger::Warn("Hook: address %p already hooked (tag: %s)", target, hooks_[target].info.name.c_str());
        return HookStatus::AlreadyHooked;
    }

    // Call Dobby to install the hook
    void* trampoline = nullptr;
    int ret = DobbyHook(target, replace, &trampoline);

    if (ret != 0) {
        Logger::Error("Hook: DobbyHook failed for %p (tag: %s), ret=%d", target, tag.c_str(), ret);
        return HookStatus::InternalError;
    }

    if (original) {
        *original = trampoline;
    }

    // Record the hook
    HookEntry entry;
    entry.info.name        = tag;
    entry.info.target_addr = target;
    entry.info.trampoline  = trampoline;
    entry.info.status      = HookStatus::Ok;

    hooks_[target] = entry;
    if (!tag.empty()) {
        tag_index_[tag] = target;
    }

    Logger::Info("Hook: installed hook at %p (tag: %s, trampoline: %p)", target, tag.c_str(), trampoline);
    return HookStatus::Ok;
}

HookStatus HookManager::Unhook(Address target) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = hooks_.find(target);
    if (it == hooks_.end()) {
        return HookStatus::NotHooked;
    }

    int ret = DobbyDestroy(target);
    if (ret != 0) {
        Logger::Error("Unhook: DobbyDestroy failed for %p, ret=%d", target, ret);
        return HookStatus::InternalError;
    }

    std::string tag = it->second.info.name;
    if (!tag.empty()) {
        tag_index_.erase(tag);
    }
    hooks_.erase(it);

    Logger::Info("Unhook: removed hook at %p (tag: %s)", target, tag.c_str());
    return HookStatus::Ok;
}

void HookManager::UnhookAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [addr, entry] : hooks_) {
        DobbyDestroy(addr);
        Logger::Info("UnhookAll: removed hook at %p (tag: %s)", addr, entry.info.name.c_str());
    }
    hooks_.clear();
    tag_index_.clear();
}

std::optional<HookInfo> HookManager::GetHookInfo(Address target) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = hooks_.find(target);
    if (it != hooks_.end()) return it->second.info;
    return std::nullopt;
}

std::optional<HookInfo> HookManager::GetHookInfoByTag(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tag_index_.find(tag);
    if (it != tag_index_.end()) {
        auto hit = hooks_.find(it->second);
        if (hit != hooks_.end()) return hit->second.info;
    }
    return std::nullopt;
}

std::vector<HookInfo> HookManager::GetAllHooks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HookInfo> result;
    result.reserve(hooks_.size());
    for (auto& [_, entry] : hooks_) {
        result.push_back(entry.info);
    }
    return result;
}

} // namespace breeze
