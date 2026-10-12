#pragma once
#include "common.hpp"

namespace lacz::mem {

// Resolve a module base by name. Uses the PEB loader list so we never touch
// the kernel32 import table directly (avoids the "client.dll" string landing in
// our import directory for a trivial static signature).
uptr module_base(const wchar_t* module_name);
uptr module_size(const wchar_t* module_name);

uptr find_module(const wchar_t* module);           // alias, cached
uptr find_export(const wchar_t* module, const char* proc);
uptr relative(uptr instruction, i32 offset, i32 size);

// Typed read/write. Never blind-dereference; each returns false on AV.
template <typename T>
bool read(uptr address, T& out);

template <typename T>
T read(uptr address, T fallback = T{});

template <typename T>
bool write(uptr address, const T& value);

// Raw copy helpers -- these tolerate spans that cross page boundaries.
bool read_raw(uptr address, void* buffer, size_t size);
bool write_raw(uptr address, const void* buffer, size_t size);

// Virtual method call: index into the object's vtable and CFG-validated call.
template <typename T, typename... Args>
T vfunc(void* this_ptr, u32 index, Args... args);

template <typename T, typename... Args>
T call(uptr address, Args... args);

bool is_valid_ptr(uptr address, size_t alignment = 1);
bool is_readable(uptr address, size_t size);

inline uptr base;   // client.dll base
inline uptr base_size;

} // namespace lacz::mem

#include "memory.inl"