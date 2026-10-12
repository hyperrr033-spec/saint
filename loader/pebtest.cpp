// PEB walk offset diagnostic. Walks this process's loader list with both
// candidate conventions and cross-checks against GetModuleHandle.
#include <Windows.h>
#include <cstdio>
#include <cstdint>

using uptr = std::uintptr_t;

struct ldr_unicode { unsigned short len, max; wchar_t* buf; };

int main() {
    // PEB->Ldr (offset 0x18 on x64).
    auto* peb = reinterpret_cast<std::uint8_t*>(__readgsqword(0x60));
    uptr ldr = *reinterpret_cast<uptr*>(peb + 0x18);
    printf("PEB=%p Ldr=%p\n", peb, reinterpret_cast<void*>(ldr));

    // Candidate A: InLoadOrderModuleList at Ldr+0x10, entry = link - 0x00.
    // Candidate B: InMemoryOrderModuleList at Ldr+0x20, entry = link - 0x10.
    struct probe { const char* name; uptr list_off; uptr entry_bias; };
    probe probes[] = {
        {"A InLoadOrder  (list+0x10, bias 0x00)", 0x10, 0x00},
        {"B InMemoryOrder(list+0x20, bias 0x10)", 0x20, 0x10},
        {"C InLoadOrder  (list+0x10, bias 0x10) [current code]", 0x10, 0x10},
    };

    for (auto& p : probes) {
        uptr head = ldr + p.list_off;
        uptr first = *reinterpret_cast<uptr*>(head);
        uptr cur = first;
        int found_kernel32 = 0, found_ntdll = 0, count = 0;
        for (int i = 0; i < 512 && cur && cur != head; ++i) {
            uptr entry = cur - p.entry_bias;
            uptr dll_base = *reinterpret_cast<uptr*>(entry + 0x30);
            auto* name = reinterpret_cast<ldr_unicode*>(entry + 0x58);
            if (name->buf && name->len && name->len < 260) {
                if (_wcsicmp(name->buf, L"kernel32.dll") == 0) found_kernel32 = 1;
                if (_wcsicmp(name->buf, L"ntdll.dll") == 0)    found_ntdll = 1;
            }
            ++count;
            cur = *reinterpret_cast<uptr*>(cur);
        }
        printf("%s -> %d entries, kernel32=%d ntdll=%d\n",
               p.name, count, found_kernel32, found_ntdll);
    }

    HMODULE real = GetModuleHandleW(L"kernel32.dll");
    printf("GetModuleHandle(kernel32)=%p\n", real);
    return 0;
}