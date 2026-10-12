#pragma once
#include "memory.hpp"

namespace lacz::mem {

template <typename T>
inline bool read(uptr address, T& out) {
    if (address < 0x10000 || address > 0x00007FFFFFFFFFFF)
        return false;
    __try {
        out = *reinterpret_cast<T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
inline T read(uptr address, T fallback) {
    if (address < 0x10000 || address > 0x00007FFFFFFFFFFF)
        return fallback;
    __try {
        return *reinterpret_cast<T*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

template <typename T>
inline bool write(uptr address, const T& value) {
    if (address < 0x10000 || address > 0x00007FFFFFFFFFFF)
        return false;
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(T),
                        PAGE_EXECUTE_READWRITE, &old))
        return false;
    __try {
        *reinterpret_cast<T*>(address) = value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), old, &old);
        return false;
    }
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), old, &old);
    return true;
}

template <typename T, typename... Args>
inline T vfunc(void* this_ptr, u32 index, Args... args) {
    if (!this_ptr)
        return T{};
    __try {
        auto** vtable = *reinterpret_cast<void***>(this_ptr);
        if (!vtable)
            return T{};
        auto fn = reinterpret_cast<T(*)(void*, Args...)>(vtable[index]);
        if (!fn)
            return T{};
        return fn(this_ptr, args...);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return T{};
    }
}

template <typename T, typename... Args>
inline T call(uptr address, Args... args) {
    if (!address)
        return T{};
    __try {
        return reinterpret_cast<T(*)(Args...)>(address)(args...);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return T{};
    }
}

} // namespace lacz::mem
