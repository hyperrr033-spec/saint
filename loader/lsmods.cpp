// Enumerate modules in a target process two ways:
//   1. PEB loader walk (the same technique the loader uses)
//   2. EnumProcessModules (OS API) as ground truth
// Usage: lsmods.exe <pid>
#include <Windows.h>
#include <Psapi.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <string>

#pragma comment(lib, "psapi.lib")

using uptr = std::uintptr_t;

static bool rpm(HANDLE p, uptr a, void* out, size_t n) {
    return ReadProcessMemory(p, reinterpret_cast<void*>(a), out, n, nullptr) != 0;
}
template <typename T> static T rp(HANDLE p, uptr a) {
    T v{}; rpm(p, a, &v, sizeof(T)); return v;
}

int main(int argc, char** argv) {
    DWORD pid = (argc > 1) ? static_cast<DWORD>(std::atoi(argv[1])) : 0;
    if (!pid) { printf("usage: lsmods <pid>\n"); return 1; }

    HANDLE proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc) { printf("OpenProcess failed %lu\n", GetLastError()); return 1; }

    printf("=== PEB walk ===\n");
    typedef struct _PROCESS_BASIC_INFORMATION {
        void* Reserved1;
        void* PebBaseAddress;
        void* Reserved2[2];
        unsigned long long UniqueProcessId;
        void* Reserved3;
    } PROCESS_BASIC_INFORMATION;
    PROCESS_BASIC_INFORMATION pbi{};
    using ntq_t = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    auto ntq = reinterpret_cast<ntq_t>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                      "NtQueryInformationProcess"));
    LONG st = ntq(proc, 0, &pbi, sizeof(pbi), nullptr);
    if (st != 0) {
        printf("NtQueryInformationProcess failed status=0x%lX (buffer was too small!)\n", st);
    } else {
        printf("PEB=%p\n", pbi.PebBaseAddress);
        uptr ldr = rp<uptr>(proc, reinterpret_cast<uptr>(pbi.PebBaseAddress) + 0x18);
        printf("Ldr=%p\n", reinterpret_cast<void*>(ldr));
        uptr head = ldr + 0x10;
        uptr cur = rp<uptr>(proc, head);
        int n = 0;
        for (int i = 0; i < 512 && cur && cur != head; ++i) {
            uptr entry = cur;
            uptr base = rp<uptr>(proc, entry + 0x30);
            unsigned short len = rp<unsigned short>(proc, entry + 0x58);
            uptr nbuf = rp<uptr>(proc, entry + 0x60);
            if (len && len < 520 && nbuf) {
                wchar_t name[260]{};
                rpm(proc, nbuf, name, len);
                printf("  [%2d] %016llX  %ls\n", n, (unsigned long long)base, name);
            }
            ++n;
            cur = rp<uptr>(proc, cur);
        }
        printf("PEB walk found %d entries\n", n);
    }

    printf("=== EnumProcessModules ===\n");
    HMODULE mods[1024]; DWORD needed = 0;
    if (EnumProcessModules(proc, mods, sizeof(mods), &needed)) {
        int count = needed / sizeof(HMODULE);
        for (int i = 0; i < count; ++i) {
            wchar_t name[MAX_PATH]{};
            GetModuleBaseNameW(proc, mods[i], name, MAX_PATH);
            printf("  [%2d] %p  %ls\n", i, (void*)mods[i], name);
        }
    } else {
        printf("EnumProcessModules failed %lu\n", GetLastError());
    }

    CloseHandle(proc);
    return 0;
}