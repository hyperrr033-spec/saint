#include "hooks.hpp"
#include <MinHook.h>

namespace lacz::hooks {

manager& manager::get() {
    static manager instance;
    return instance;
}

bool manager::initialize() {
    std::lock_guard lock(mutex_);
    if (initialized_)
        return true;
    if (MH_Initialize() != MH_OK)
        return false;
    initialized_ = true;
    return true;
}

bool manager::attach(void* target, void* detour, void** original, const char* name) {
    std::lock_guard lock(mutex_);
    if (!initialized_ || !target || !detour)
        return false;

    const MH_STATUS create_status = MH_CreateHook(target, detour, original);
    if (create_status != MH_OK)
        return false;

    const MH_STATUS enable_status = MH_EnableHook(target);
    if (enable_status != MH_OK) {
        // Creation succeeded but activation failed. Remove the inactive hook so
        // a later retry does not fail with MH_ERROR_ALREADY_CREATED.
        MH_RemoveHook(target);
        if (original)
            *original = nullptr;
        return false;
    }

    entries_.push_back({target, detour, original, name ? name : "hook", true});
    return true;
}

bool manager::detach(void* target) {
    std::lock_guard lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->target != target)
            continue;
        MH_DisableHook(target);
        MH_RemoveHook(target);
        entries_.erase(it);
        return true;
    }
    return false;
}

void manager::enable_all(bool state) {
    std::lock_guard lock(mutex_);
    for (auto& e : entries_) {
        if (e.enabled == state)
            continue;
        if (state)
            MH_EnableHook(e.target);
        else
            MH_DisableHook(e.target);
        e.enabled = state;
    }
}

void manager::shutdown() {
    std::lock_guard lock(mutex_);
    if (!initialized_)
        return;
    for (auto& e : entries_) {
        MH_DisableHook(e.target);
        MH_RemoveHook(e.target);
    }
    entries_.clear();
    MH_Uninitialize();
    initialized_ = false;
}

} // namespace lacz::hooks