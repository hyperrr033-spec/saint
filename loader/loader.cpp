// ---------------------------------------------------------------------------
// LaczDLC manual-map loader.
//
// Maps LaczDLC.dll into a target process without LoadLibrary, so the image
// never appears in the PEB module list and its imports/exports are resolved by
// hand.
//
// Pipeline:
//   1. locate cs2.exe
//   2. read the DLL from disk (with auto-discovery across build/exe dirs)
//   3. allocate a remote image base (preferred base when free, else anywhere)
//   4. map headers + sections, apply DIR64 relocations
//   5. resolve imports against the target's already-loaded modules
//   6. run a code-cave stub that calls DllMain(DLL_PROCESS_ATTACH)
//   7. call LaczMarkManualMapped so the cheat never tries to unload itself
//
// Build (x64, from the repo root):
//   cl /nologo /std:c++20 /O2 /EHsc loader\loader.cpp /Fe:loader\LaczLoader.exe
// ---------------------------------------------------------------------------

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using uptr = std::uintptr_t;

static int pause_exit(int code) {
    printf("\nPress ENTER to exit...");
    fflush(stdout);
    getchar();
    return code;
}

// ===========================================================================
// Process helpers
// ===========================================================================
static DWORD find_process(const wchar_t* exe_name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, exe_name) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pid;
}

static std::vector<u8> read_file(const char* path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    auto size = in.tellg();
    in.seekg(0);
    std::vector<u8> data(static_cast<size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static std::string resolve_dll_path(int argc, char** argv) {
    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        std::ifstream test(argv[1]);
        if (test.good()) return argv[1];
    }

    wchar_t exe_path_buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path_buf, MAX_PATH);
    std::wstring exe_path(exe_path_buf);
    size_t last_slash = exe_path.find_last_of(L"\\/");
    std::wstring exe_dir = (last_slash != std::wstring::npos) ? exe_path.substr(0, last_slash) : L".";

    std::vector<std::wstring> candidates = {
        exe_dir + L"\\LaczDLC.dll",
        exe_dir + L"\\build\\Release\\LaczDLC.dll",
        exe_dir + L"\\..\\build\\Release\\LaczDLC.dll",
        exe_dir + L"\\..\\LaczDLC.dll",
        L".\\LaczDLC.dll",
        L".\\build\\Release\\LaczDLC.dll",
        L"..\\build\\Release\\LaczDLC.dll"
    };

    for (const auto& cand : candidates) {
        std::ifstream test(cand);
        if (test.good()) {
            char mb_path[MAX_PATH]{};
            WideCharToMultiByte(CP_UTF8, 0, cand.c_str(), -1, mb_path, MAX_PATH, nullptr, nullptr);
            return std::string(mb_path);
        }
    }

    return "LaczDLC.dll";
}

// ===========================================================================
// Remote memory access helpers
// ===========================================================================
static bool rpm(HANDLE proc, uptr address, void* out, size_t size) {
    return ReadProcessMemory(proc, reinterpret_cast<void*>(address), out, size,
                             nullptr) != 0;
}

template <typename T>
static T rpm_t(HANDLE proc, uptr address) {
    T value{};
    rpm(proc, address, &value, sizeof(T));
    return value;
}

// ===========================================================================
// Manual mapper
// ===========================================================================
class mapper {
public:
    mapper(HANDLE process, std::vector<u8> image)
        : proc_(process), image_(std::move(image)) {}

    bool map() {
        if (!parse_headers())
            return false;
        if (!allocate_image())
            return false;
        if (!copy_headers())
            return false;
        if (!copy_sections())
            return false;
        if (!resolve_imports())
            return false;
        if (!protect_image())
            printf("[*] warning: final protect failed; continuing\n");
        if (!register_exception_tables())
            printf("[*] warning: function table registration failed; SEH may not work\n");
        printf("[+] mapped image at %p (entry rva 0x%X)\n", base_, entry_rva_);
        return true;
    }

    bool run_entry() {
        auto stub = build_entry_stub(reinterpret_cast<u64>(base_),
                                     reinterpret_cast<u64>(base_) + entry_rva_);
        u8* remote = reinterpret_cast<u8*>(VirtualAllocEx(
            proc_, nullptr, stub.size(), MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (!remote) {
            printf("[!] stub alloc failed: %lu\n", GetLastError());
            return false;
        }
        if (!wpm(remote, stub.data(), stub.size())) {
            printf("[!] stub write failed\n");
            return false;
        }

        HANDLE thread = CreateRemoteThread(
            proc_, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(remote), nullptr, 0, nullptr);
        if (!thread) {
            printf("[!] CreateRemoteThread failed: %lu\n", GetLastError());
            return false;
        }
        WaitForSingleObject(thread, 15000);
        DWORD exit_code = 0;
        GetExitCodeThread(thread, &exit_code);
        CloseHandle(thread);
        printf("[+] DllMain ran (thread exit %lu)\n", exit_code);
        return true;
    }

    bool call_export(const char* name) {
        uptr addr = find_export(name);
        if (!addr) {
            printf("[!] export '%s' not found\n", name);
            return false;
        }
        HANDLE thread = CreateRemoteThread(
            proc_, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(addr), nullptr, 0, nullptr);
        if (!thread) {
            printf("[!] calling '%s' failed: %lu\n", name, GetLastError());
            return false;
        }
        WaitForSingleObject(thread, 5000);
        CloseHandle(thread);
        printf("[+] %s() called\n", name);
        return true;
    }

private:
    bool parse_headers() {
        if (image_.size() < sizeof(IMAGE_DOS_HEADER))
            return false;
        dos_ = reinterpret_cast<IMAGE_DOS_HEADER*>(image_.data());
        if (dos_->e_magic != IMAGE_DOS_SIGNATURE)
            return false;
        nt_ = reinterpret_cast<IMAGE_NT_HEADERS*>(image_.data() + dos_->e_lfanew);
        if (nt_->Signature != IMAGE_NT_SIGNATURE)
            return false;
        if (nt_->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
            printf("[!] not an x64 image\n");
            return false;
        }
        image_size_ = nt_->OptionalHeader.SizeOfImage;
        entry_rva_ = nt_->OptionalHeader.AddressOfEntryPoint;
        return true;
    }

    bool allocate_image() {
        // NOTE: allocate READWRITE first, flip to EXECUTE after the image
        // is fully written. Reserving EXECUTE_READWRITE up-front at the
        // preferred base is what hitches CS2 on some builds (guard pages
        // + CFG checks fire while the image is still half-copied).
        u64 pref = nt_->OptionalHeader.ImageBase;
        base_ = reinterpret_cast<u8*>(VirtualAllocEx(
            proc_, reinterpret_cast<void*>(pref), image_size_,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!base_) {
            base_ = reinterpret_cast<u8*>(VirtualAllocEx(
                proc_, nullptr, image_size_, MEM_COMMIT | MEM_RESERVE,
                PAGE_READWRITE));
        }
        if (!base_) {
            printf("[!] VirtualAllocEx failed: %lu\n", GetLastError());
            return false;
        }
        delta_ = reinterpret_cast<u64>(base_) - pref;
        printf("[+] image base %p (preferred %llX, delta %lld)\n", base_, pref,
               delta_);
        return true;
    }

    bool copy_headers() {
        return wpm(base_, image_.data(), nt_->OptionalHeader.SizeOfHeaders);
    }

    bool copy_sections() {
        auto* section = IMAGE_FIRST_SECTION(nt_);
        for (int i = 0; i < nt_->FileHeader.NumberOfSections; ++i, ++section) {
            if (!section->SizeOfRawData)
                continue;
            std::vector<u8> buffer(section->SizeOfRawData);
            std::memcpy(buffer.data(), image_.data() + section->PointerToRawData,
                        section->SizeOfRawData);
            if (delta_ && !(section->Characteristics & IMAGE_SCN_MEM_DISCARDABLE))
                relocate_slice(buffer.data(), section->VirtualAddress,
                               static_cast<u32>(buffer.size()));
            if (!wpm(base_ + section->VirtualAddress, buffer.data(), buffer.size())) {
                printf("[!] section %d copy failed\n", i);
                return false;
            }
        }
        return true;
    }

    void relocate_slice(u8* slice, u32 virt_addr, u32 len) {
        auto& reloc =
            nt_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (!reloc.Size || !reloc.VirtualAddress)
            return;

        u32 offset = rva_to_offset(reloc.VirtualAddress);
        u8* begin = image_.data() + offset;
        u8* end = begin + reloc.Size;

        for (auto* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(begin);
             reinterpret_cast<u8*>(block) < end && block->SizeOfBlock;
             block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                 reinterpret_cast<u8*>(block) + block->SizeOfBlock)) {
            u32 block_rva = block->VirtualAddress;
            u32 count =
                (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(u16);
            auto* entries = reinterpret_cast<u16*>(
                reinterpret_cast<u8*>(block) + sizeof(IMAGE_BASE_RELOCATION));

            for (u32 i = 0; i < count; ++i) {
                u16 type = entries[i] >> 12;
                u16 off = entries[i] & 0x0FFF;
                if (type != IMAGE_REL_BASED_DIR64)
                    continue;
                u32 target_rva = block_rva + off;
                if (target_rva < virt_addr || target_rva >= virt_addr + len)
                    continue;
                u32 local = target_rva - virt_addr;
                if (local + sizeof(u64) > len)
                    continue;
                auto* ptr = reinterpret_cast<u64*>(slice + local);
                *ptr += delta_;
            }
        }
    }

    u32 rva_to_offset(u32 rva) {
        auto* section = IMAGE_FIRST_SECTION(nt_);
        for (int i = 0; i < nt_->FileHeader.NumberOfSections; ++i, ++section) {
            if (rva >= section->VirtualAddress &&
                rva < section->VirtualAddress + section->SizeOfRawData)
                return rva - section->VirtualAddress + section->PointerToRawData;
        }
        return rva;
    }

    bool wpm(void* dest, const void* src, size_t size) {
        return WriteProcessMemory(proc_, dest, src, size, nullptr) != 0;
    }

    bool protect_image() {
        // Cheat image has writable globals (.data/.bss, settings, ctx).
        // PAGE_EXECUTE_READ would AV on first write — keep WRITE.
        DWORD old = 0;
        return VirtualProtectEx(proc_, base_, image_size_, PAGE_EXECUTE_READWRITE, &old) != 0;
    }

    static bool is_api_set(const char* name) {
        return std::strncmp(name, "api-ms-", 7) == 0 ||
               std::strncmp(name, "ext-ms-", 7) == 0 ||
               std::strncmp(name, "api-ms-win", 10) == 0;
    }

    bool register_exception_tables() {
        auto& dir = nt_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (!dir.Size || !dir.VirtualAddress)
            return true;

        uptr ntdll = remote_module_base("ntdll.dll");
        if (!ntdll)
            return false;
        uptr add_table = export_by_name(ntdll, "RtlAddFunctionTable");
        if (!add_table)
            return false;

        u64 table_va = reinterpret_cast<u64>(base_) + dir.VirtualAddress;
        u32 entries = dir.Size / 12;

        std::vector<u8> code;
        auto emit = [&](std::initializer_list<u8> b) { for (u8 x : b) code.push_back(x); };
        auto emit64 = [&](u64 v) { for (int i = 0; i < 8; ++i) code.push_back((u8)(v >> (i * 8))); };

        emit({0x48, 0x83, 0xEC, 0x28});
        emit({0x48, 0xB9}); emit64(table_va);
        emit({0xBA}); for (int i=0;i<4;++i) code.push_back((u8)(entries >> (i*8)));
        emit({0x49, 0xB8}); emit64(reinterpret_cast<u64>(base_));
        emit({0x48, 0xB8}); emit64(add_table);
        emit({0xFF, 0xD0});
        emit({0x48, 0x83, 0xC4, 0x28});
        emit({0xC3});

        u8* remote = reinterpret_cast<u8*>(VirtualAllocEx(
            proc_, nullptr, code.size(), MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (!remote)
            return false;
        if (!wpm(remote, code.data(), code.size())) {
            VirtualFreeEx(proc_, remote, 0, MEM_RELEASE);
            return false;
        }
        HANDLE thread = CreateRemoteThread(proc_, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(remote), nullptr, 0, nullptr);
        if (!thread) {
            VirtualFreeEx(proc_, remote, 0, MEM_RELEASE);
            return false;
        }
        WaitForSingleObject(thread, 5000);
        CloseHandle(thread);
        VirtualFreeEx(proc_, remote, 0, MEM_RELEASE);
        printf("[+] registered %u exception table entries\n", entries);
        return true;
    }

    bool resolve_imports() {
        auto& dir = nt_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.Size || !dir.VirtualAddress)
            return true;

        auto* descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            image_.data() + rva_to_offset(dir.VirtualAddress));

        for (; descriptor->Name; ++descriptor) {
            const char* dll_name = reinterpret_cast<const char*>(
                image_.data() + rva_to_offset(descriptor->Name));

            uptr remote_mod = remote_module_base(dll_name);
            if (!remote_mod && is_api_set(dll_name)) {
                remote_mod = remote_module_base("kernelbase.dll");
                if (!remote_mod)
                    remote_mod = remote_module_base("kernel32.dll");
            }
            if (!remote_mod) {
                printf("[*] '%s' not loaded; mapping it into the target\n", dll_name);
                remote_mod = load_remote_module(dll_name);
            }
            if (!remote_mod) {
                printf("[!] target has no module '%s' (and loading it failed)\n",
                       dll_name);
                return false;
            }

            u32 thunk_rva = descriptor->OriginalFirstThunk
                                ? descriptor->OriginalFirstThunk
                                : descriptor->FirstThunk;
            auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                image_.data() + rva_to_offset(thunk_rva));

            std::vector<u64> iat;
            for (; thunk->u1.AddressOfData; ++thunk) {
                u64 addr = resolve_symbol(remote_mod, thunk);
                if (!addr) {
                    if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                        printf("[!] unresolved ordinal %llu in %s\n",
                               static_cast<unsigned long long>(thunk->u1.Ordinal & 0xFFFF),
                               dll_name);
                    } else {
                        auto* imp = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                            image_.data() + rva_to_offset(
                                static_cast<u32>(thunk->u1.AddressOfData)));
                        printf("[!] unresolved import '%s' from %s\n",
                               reinterpret_cast<const char*>(imp->Name), dll_name);
                    }
                }
                iat.push_back(addr);
            }
            iat.push_back(0);

            if (!wpm(base_ + descriptor->FirstThunk, iat.data(),
                     iat.size() * sizeof(u64))) {
                printf("[!] IAT write failed for '%s'\n", dll_name);
                return false;
            }
        }
        return true;
    }

    u64 resolve_symbol(uptr module, IMAGE_THUNK_DATA* thunk) {
        if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
            return export_by_ordinal(module,
                                     static_cast<u16>(thunk->u1.Ordinal & 0xFFFF));
        auto* import = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
            image_.data() + rva_to_offset(
                static_cast<u32>(thunk->u1.AddressOfData)));
        return export_by_name(module, reinterpret_cast<const char*>(import->Name));
    }

    uptr remote_module_base(const char* module_name) {
        struct process_basic_information {
            void* reserved1;
            void* peb_base_address;
            void* reserved2[2];
            unsigned long long unique_process_id;
            void* reserved3;
        };
        using nt_query_t = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        auto nt_query = reinterpret_cast<nt_query_t>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                           "NtQueryInformationProcess"));
        if (!nt_query)
            return 0;

        process_basic_information pbi{};
        if (nt_query(proc_, 0, &pbi, sizeof(pbi), nullptr) != 0 || !pbi.peb_base_address)
            return 0;

        uptr ldr = rpm_t<uptr>(proc_, reinterpret_cast<uptr>(pbi.peb_base_address) + 0x18);
        if (!ldr)
            return 0;

        uptr list_head = ldr + 0x10;
        uptr first = rpm_t<uptr>(proc_, list_head);
        uptr current = first;

        wchar_t wanted[64]{};
        MultiByteToWideChar(CP_UTF8, 0, module_name, -1, wanted, 64);

        for (int guard = 0; guard < 1024 && current; ++guard) {
            if (current == list_head)
                break;
            uptr entry = current;
            uptr dll_base = rpm_t<uptr>(proc_, entry + 0x30);
            u16 name_len = rpm_t<u16>(proc_, entry + 0x58);
            uptr name_buf = rpm_t<uptr>(proc_, entry + 0x60);
            if (name_len && name_buf && name_len < 260) {
                wchar_t remote_name[260]{};
                rpm(proc_, name_buf, remote_name, name_len);
                if (_wcsicmp(remote_name, wanted) == 0)
                    return dll_base;
            }
            current = rpm_t<uptr>(proc_, current);
        }
        return 0;
    }

    uptr load_remote_module(const char* module_name) {
        uptr k32 = remote_module_base("kernel32.dll");
        if (!k32)
            k32 = remote_module_base("KernelBase.dll");
        if (!k32)
            return 0;
        uptr load_library = export_by_name(k32, "LoadLibraryA");
        if (!load_library) {
            uptr kb = remote_module_base("KernelBase.dll");
            if (kb)
                load_library = export_by_name(kb, "LoadLibraryA");
        }
        if (!load_library)
            return 0;

        size_t len = std::strlen(module_name) + 1;
        u8* remote_str = reinterpret_cast<u8*>(VirtualAllocEx(
            proc_, nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!remote_str)
            return 0;
        if (!wpm(remote_str, module_name, len)) {
            VirtualFreeEx(proc_, remote_str, 0, MEM_RELEASE);
            return 0;
        }

        HANDLE thread = CreateRemoteThread(
            proc_, nullptr, 0,
            reinterpret_cast<LPTHREAD_START_ROUTINE>(load_library),
            remote_str, 0, nullptr);
        if (!thread) {
            VirtualFreeEx(proc_, remote_str, 0, MEM_RELEASE);
            return 0;
        }
        WaitForSingleObject(thread, 10000);
        DWORD exit_code = 0;
        GetExitCodeThread(thread, &exit_code);
        CloseHandle(thread);
        VirtualFreeEx(proc_, remote_str, 0, MEM_RELEASE);

        if (!exit_code)
            return 0;
        return remote_module_base(module_name);
    }

    uptr export_by_name(uptr module, const char* name) {
        auto dos = rpm_t<IMAGE_DOS_HEADER>(proc_, module);
        auto nt = rpm_t<IMAGE_NT_HEADERS>(proc_, module + dos.e_lfanew);
        auto& dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress)
            return 0;

        uptr exp = module + dir.VirtualAddress;
        u32 num_names = rpm_t<u32>(proc_, exp + 0x18);
        u32 addr_names = rpm_t<u32>(proc_, exp + 0x20);
        u32 addr_funcs = rpm_t<u32>(proc_, exp + 0x1C);
        u32 addr_ords = rpm_t<u32>(proc_, exp + 0x24);

        for (u32 i = 0; i < num_names; ++i) {
            u32 name_rva = rpm_t<u32>(proc_, module + addr_names + i * 4);
            char remote_name[256]{};
            rpm(proc_, module + name_rva, remote_name, sizeof(remote_name) - 1);
            if (std::strcmp(remote_name, name) != 0)
                continue;
            u16 ord = rpm_t<u16>(proc_, module + addr_ords + i * 2);
            u32 func_rva = rpm_t<u32>(proc_, module + addr_funcs + ord * 4);
            if (!func_rva)
                return 0;
            if (func_rva >= dir.VirtualAddress &&
                func_rva < dir.VirtualAddress + dir.Size) {
                char forwarder[256]{};
                rpm(proc_, module + func_rva, forwarder, sizeof(forwarder) - 1);
                return resolve_forwarder(forwarder);
            }
            return module + func_rva;
        }
        (void)addr_funcs;
        return 0;
    }

    uptr resolve_forwarder(const char* forwarder) {
        static thread_local int depth = 0;
        if (depth > 4)
            return 0;
        struct guard { int& d; guard(int& x) : d(x) { ++d; } ~guard() { --d; } } g(depth);

        const char* dot = std::strrchr(forwarder, '.');
        if (!dot)
            return 0;
        std::string dll(forwarder, dot - forwarder);
        std::string sym(dot + 1);
        dll += ".dll";

        uptr mod = remote_module_base(dll.c_str());
        if (!mod && is_api_set(dll.c_str())) {
            mod = remote_module_base("kernelbase.dll");
            if (!mod)
                mod = remote_module_base("kernel32.dll");
        }
        if (!mod) {
            printf("[*] forwarder target '%s' not loaded; loading\n", dll.c_str());
            mod = load_remote_module(dll.c_str());
        }
        if (!mod)
            return 0;
        if (sym[0] == '#')
            return export_by_ordinal(mod, static_cast<u16>(std::atoi(sym.c_str() + 1)));
        return export_by_name(mod, sym.c_str());
    }

    uptr export_by_ordinal(uptr module, u16 ordinal) {
        auto dos = rpm_t<IMAGE_DOS_HEADER>(proc_, module);
        auto nt = rpm_t<IMAGE_NT_HEADERS>(proc_, module + dos.e_lfanew);
        auto& dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress)
            return 0;
        uptr exp = module + dir.VirtualAddress;
        u32 addr_funcs = rpm_t<u32>(proc_, exp + 0x1C);
        u32 func_rva = rpm_t<u32>(proc_, module + addr_funcs + ordinal * 4);
        return func_rva ? module + func_rva : 0;
    }

    uptr find_export(const char* name) {
        uptr b = reinterpret_cast<uptr>(base_);
        auto dos = rpm_t<IMAGE_DOS_HEADER>(proc_, b);
        auto nt = rpm_t<IMAGE_NT_HEADERS>(proc_, b + dos.e_lfanew);
        auto& dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress)
            return 0;

        uptr exp = b + dir.VirtualAddress;
        u32 num_names = rpm_t<u32>(proc_, exp + 0x18);
        u32 addr_names = rpm_t<u32>(proc_, exp + 0x20);
        u32 addr_funcs = rpm_t<u32>(proc_, exp + 0x1C);
        u32 addr_ords = rpm_t<u32>(proc_, exp + 0x24);

        for (u32 i = 0; i < num_names; ++i) {
            u32 name_rva = rpm_t<u32>(proc_, b + addr_names + i * 4);
            char export_name[256]{};
            rpm(proc_, b + name_rva, export_name, sizeof(export_name) - 1);
            if (std::strcmp(export_name, name) != 0)
                continue;
            u16 ord = rpm_t<u16>(proc_, b + addr_ords + i * 2);
            u32 func_rva = rpm_t<u32>(proc_, b + addr_funcs + ord * 4);
            return b + func_rva;
        }
        return 0;
    }

    static std::vector<u8> build_entry_stub(u64 image_base, u64 entry) {
        std::vector<u8> code;
        auto emit = [&](std::initializer_list<u8> bytes) {
            for (u8 b : bytes)
                code.push_back(b);
        };
        auto emit64 = [&](u64 v) {
            for (int i = 0; i < 8; ++i)
                code.push_back(static_cast<u8>((v >> (i * 8)) & 0xFF));
        };

        emit({0x48, 0x83, 0xEC, 0x28});
        emit({0x48, 0xB9}); emit64(image_base);
        emit({0xBA, 0x01, 0x00, 0x00, 0x00});
        emit({0x45, 0x33, 0xC0});
        emit({0x48, 0xB8}); emit64(entry);
        emit({0xFF, 0xD0});
        emit({0x48, 0x83, 0xC4, 0x28});
        emit({0xC3});
        return code;
    }

    HANDLE proc_;
    std::vector<u8> image_;
    IMAGE_DOS_HEADER* dos_ = nullptr;
    IMAGE_NT_HEADERS* nt_ = nullptr;
    u8* base_ = nullptr;
    u32 image_size_ = 0;
    u32 entry_rva_ = 0;
    u64 delta_ = 0;
};

// ===========================================================================
int main(int argc, char** argv) {
    const wchar_t* target_exe = L"cs2.exe";
    wchar_t target_buf[64] = L"cs2.exe";
    if (argc > 2) {
        MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, target_buf, 64);
        target_exe = target_buf;
    }

    printf("=====================================================\n");
    printf("  LaczDLC manual-map loader\n");
    printf("=====================================================\n");

    DWORD pid = find_process(target_exe);
    if (!pid) {
        printf("\n[!] Target process 'cs2.exe' is NOT running!\n");
        printf("[!] Please start Counter-Strike 2 first, wait for the main menu, and try again.\n");
        return pause_exit(1);
    }
    printf("[+] Target process PID: %lu\n", pid);
    printf("[*] Inject ONLY at the main menu (not loading / not in-match).\n");
    printf("[*] Mapping mid-load is what freezes CS2 — wait for the menu.\n");
    printf("[*] Tip: open the scoreboard (or spectate) once before injecting so\n");
    printf("[*]      the game has entity slot data; on an empty main menu the\n");
    printf("[*]      client has no pawns and the cheat has nothing to read.\n");

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) {
        DWORD err = GetLastError();
        printf("\n[!] OpenProcess failed with error code %lu.\n", err);
        if (err == ERROR_ACCESS_DENIED) {
            printf("[!] ACCESS DENIED: Please right-click LaczLoader.exe and select 'Run as Administrator'.\n");
        }
        return pause_exit(1);
    }

    std::string dll_path = resolve_dll_path(argc, argv);
    auto image = read_file(dll_path.c_str());
    if (image.empty()) {
        printf("\n[!] Could not read cheat DLL from '%s'!\n", dll_path.c_str());
        printf("[!] Please make sure LaczDLC.dll is compiled and placed in the loader folder.\n");
        CloseHandle(proc);
        return pause_exit(1);
    }
    printf("[+] Loaded %zu bytes from %s\n", image.size(), dll_path.c_str());

    mapper map(proc, std::move(image));
    if (!map.map()) {
        printf("[!] Manual mapping failed.\n");
        CloseHandle(proc);
        return pause_exit(1);
    }

    if (!map.run_entry()) {
        printf("[!] DllMain entry execution failed.\n");
        CloseHandle(proc);
        return pause_exit(1);
    }

    map.call_export("LaczMarkManualMapped");

    printf("\n[+] SUCCESS! LaczDLC successfully loaded into CS2.\n");
    printf("[+] Press INSERT key inside CS2 to open the menu.\n");
    printf("[+] Press END key inside CS2 to unload the cheat.\n");
    CloseHandle(proc);
    return pause_exit(0);
}
