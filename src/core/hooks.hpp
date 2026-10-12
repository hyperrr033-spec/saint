#pragma once
#include "common.hpp"

namespace lacz::hooks {

// Thin wrapper over MinHook that owns the hook list and guarantees a clean
// shutdown order (important: the render hook must die before the device does).
class manager {
public:
    static manager& get();

    bool initialize();
    void shutdown();

    // Create + enable in one call. Returns true on success.
    bool attach(void* target, void* detour, void** original, const char* name);
    bool detach(void* target);

    void enable_all(bool state);

private:
    struct entry {
        void* target;
        void* detour;
        void** original;
        std::string name;
        bool enabled;
    };

    std::mutex mutex_;
    std::vector<entry> entries_;
    bool initialized_ = false;
};

template <typename T>
inline void* as_ptr(T fn) {
    return reinterpret_cast<void*>(fn);
}

} // namespace lacz::hooks