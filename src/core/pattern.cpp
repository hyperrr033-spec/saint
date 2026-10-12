#include "pattern.hpp"
#include "memory.hpp"

namespace lacz::pattern {

parsed compile(std::string signature) {
    parsed out{};
    // Strip an optional "client.dll!"-style module prefix.
    auto bang = signature.find('!');
    if (bang != std::string::npos)
        signature = signature.substr(bang + 1);

    std::string token;
    auto flush = [&] {
        if (token.empty())
            return;
        if (token == "?" || token == "??" || token.find('?') != std::string::npos) {
            out.bytes.push_back(0x00);
            out.mask.push_back(false);
        } else {
            out.bytes.push_back(static_cast<u8>(std::stoul(token, nullptr, 16)));
            out.mask.push_back(true);
        }
        token.clear();
    };

    for (char c : signature) {
        if (c == ' ')
            flush();
        else
            token.push_back(c);
    }
    flush();
    out.ok = !out.bytes.empty();
    return out;
}

uptr scan(const wchar_t* module, const parsed& sig) {
    if (!sig.ok)
        return 0;
    uptr base = mem::module_base(module);
    uptr size = mem::module_size(module);
    if (!base || !size)
        return 0;

    // Perf: restrict to the executable code section. Scanning the whole
    // 50MB+ image per pattern x40 = GBs of page touches on the game's
    // memory = the inject hitch. .text is where every code pattern lives.
    uptr scan_base = base;
    uptr scan_size = size;
    __try {
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) {
                auto* sec = IMAGE_FIRST_SECTION(nt);
                for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
                    char name[9]{};
                    std::memcpy(name, sec->Name, 8);
                    if (std::strcmp(name, ".text") == 0 && sec->Misc.VirtualSize) {
                        scan_base = base + sec->VirtualAddress;
                        scan_size = sec->Misc.VirtualSize;
                        break;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    auto* data = reinterpret_cast<u8*>(scan_base);
    const size_t pattern_len = sig.bytes.size();
    if (pattern_len > scan_size || pattern_len == 0)
        return 0;

    // First-byte skip: find first non-wildcard to stride faster.
    size_t first = 0;
    while (first < pattern_len && !sig.mask[first])
        ++first;
    u8 first_byte = first < pattern_len ? sig.bytes[first] : 0;

    for (size_t i = 0; i + pattern_len <= scan_size; ++i) {
        if (first < pattern_len && data[i + first] != first_byte) {
            continue;
        }
        bool matched = true;
        for (size_t j = 0; j < pattern_len; ++j) {
            if (sig.mask[j] && data[i + j] != sig.bytes[j]) {
                matched = false;
                break;
            }
        }
        if (matched)
            return scan_base + i;
    }
    return 0;
}

uptr find(const wchar_t* module, std::string signature) {
    return scan(module, compile(std::move(signature)));
}

uptr resolve_relative(uptr instruction, i32 offset, i32 instruction_size) {
    auto disp = mem::read<i32>(instruction + offset, 0);
    return instruction + instruction_size + disp;
}

uptr resolve_rip(uptr instruction) {
    // Most CS2 lea/mov rel32 instructions are 7 bytes long.
    return resolve_relative(instruction, 0x3, 0x7);
}

} // namespace lacz::pattern