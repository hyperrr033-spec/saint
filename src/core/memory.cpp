#include "memory.hpp"
#include <TlHelp32.h>

namespace lacz::mem {

// Minimal self-contained loader structures. Avoids pulling in winternl.h, whose
// LDR_DATA_TABLE_ENTRY member names differ between SDK versions.
struct ldr_unicode_string {
    u16 length;
    u16 maximum_length;
    wchar_t* buffer;
};

struct peb_ldr_entry {
    LIST_ENTRY in_load_order;
    LIST_ENTRY in_memory_order;
    LIST_ENTRY in_initialization_order;
    void*       dll_base;
    void*       entry_point;
    u32         size_of_image;
    ldr_unicode_string full_dll_name;
    ldr_unicode_string base_dll_name;
};

struct peb_ldr_data {
    u32 length;
    u8  initialized;
    void* ss_handle;
    LIST_ENTRY in_load_order_module_list;
};

struct peb_t {
    u8  inherited_address_space;
    u8  read_image_file_exec_options;
    u8  being_debugged;
    u8  bit_field;
    void* mutant;
    void* image_base_address;
    peb_ldr_data* ldr;
};

static uptr walk_modules(const wchar_t* wanted, u32* out_size) {
#if defined(_M_X64) || defined(__x86_64__)
    auto* peb = reinterpret_cast<peb_t*>(__readgsqword(0x60));
#else
    auto* peb = reinterpret_cast<peb_t*>(__readfsdword(0x30));
#endif
    if (!peb || !peb->ldr)
        return 0;

    auto* head = &peb->ldr->in_load_order_module_list;
    for (auto* it = head->Flink; it && it != head; it = it->Flink) {
        auto* entry = CONTAINING_RECORD(it, peb_ldr_entry, in_load_order);
        if (!entry->base_dll_name.buffer)
            continue;
        if (_wcsicmp(entry->base_dll_name.buffer, wanted) == 0) {
            if (out_size)
                *out_size = entry->size_of_image;
            return reinterpret_cast<uptr>(entry->dll_base);
        }
    }
    return 0;
}

uptr module_base(const wchar_t* module_name) {
    return walk_modules(module_name, nullptr);
}

uptr module_size(const wchar_t* module_name) {
    u32 size = 0;
    walk_modules(module_name, &size);
    return size;
}

uptr find_module(const wchar_t* module) {
    static std::unordered_map<std::wstring, uptr> cache;
    std::wstring key = module;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    uptr result = module_base(module);
    cache[key] = result;
    return result;
}

uptr find_export(const wchar_t* module, const char* proc) {
    uptr base_addr = module_base(module);
    if (!base_addr)
        return 0;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base_addr);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base_addr + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return 0;

    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress)
        return 0;

    auto* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base_addr + dir.VirtualAddress);
    auto* names = reinterpret_cast<u32*>(base_addr + exports->AddressOfNames);
    auto* funcs = reinterpret_cast<u32*>(base_addr + exports->AddressOfFunctions);
    auto* ords  = reinterpret_cast<u16*>(base_addr + exports->AddressOfNameOrdinals);

    for (u32 i = 0; i < exports->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(base_addr + names[i]);
        if (strcmp(name, proc) == 0)
            return base_addr + funcs[ords[i]];
    }
    return 0;
}

uptr relative(uptr instruction, i32 offset, i32 size) {
    return instruction + size + *reinterpret_cast<i32*>(instruction + offset);
}

// ---------------------------------------------------------------------------
// Address validation.
//
// The previous implementation called VirtualQuery (NtQueryVirtualMemory) for
// every check. This cheat walks the entity list hundreds of times per frame, so
// that turned into tens of thousands of kernel transitions + address-space lock
// acquisitions *per rendered frame* -- which is what stalled the game thread and
// made the menu/UI hitch. An SEH probe of the actual bytes is a plain user-mode
// load: a valid mapping touches no kernel, and an invalid one takes the page
// fault trap (typically mapped once, then the fault is fast).
//
// The trap is caught by the __except filter, so an unmapped/copy-on-write/etc.
// page simply yields false instead of terminating the process.
// ---------------------------------------------------------------------------
static bool probe_readable(uptr address, size_t size) {
    volatile u8 sink = 0;
    __try {
        const volatile u8* p = reinterpret_cast<const volatile u8*>(address);
        // Touch first and last byte so a span straddling into a guard page fails.
        sink ^= p[0];
        sink ^= p[size - 1];
        (void)sink;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool is_readable(uptr address, size_t size) {
    if (!address || address < 0x10000 || address > 0x00007FFFFFFFFFFF)
        return false;
    if (size == 0)
        return true;
    if (address + size < address)    // overflow
        return false;
    // Span checks are resolved on the fault path below; the pre-check here only
    // rejects the null/near-null page and kernel range.
    return probe_readable(address, size);
}

bool is_valid_ptr(uptr address, size_t alignment) {
    return address >= 0x10000 && address <= 0x00007FFFFFFFFFFF &&
           (address % alignment) == 0;
}

bool read_raw(uptr address, void* buffer, size_t size) {
    if (!address || address < 0x10000 || address > 0x00007FFFFFFFFFFF)
        return false;
    // Whole-copy under one guard so a mid-copy fault does not leave a partial
    // buffer silently accepted (matches the try/except contract callers expect).
    __try {
        std::memcpy(buffer, reinterpret_cast<const void*>(address), size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool write_raw(uptr address, const void* buffer, size_t size) {
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size,
                        PAGE_EXECUTE_READWRITE, &old))
        return false;
    std::memcpy(reinterpret_cast<void*>(address), buffer, size);
    VirtualProtect(reinterpret_cast<void*>(address), size, old, &old);
    return true;
}

} // namespace lacz::mem