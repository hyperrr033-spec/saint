#pragma once
#include "common.hpp"

namespace lacz::pattern {

// Parses "48 8B 05 ?? ?? ?? ?? 48 85 C0" style signatures.
struct parsed {
    std::vector<u8> bytes;
    std::vector<bool> mask;
    bool ok = false;
};

parsed compile(std::string signature);

// Scan a single module (default: client.dll) for a compiled pattern.
uptr scan(const wchar_t* module, const parsed& sig);

// Convenience: compile + scan.
uptr find(const wchar_t* module, std::string signature);

// Resolve a `lea reg, [rip+disp]` / `call rel32` instruction at `instruction`
// and return the absolute target.
uptr resolve_relative(uptr instruction, i32 offset, i32 instruction_size);
uptr resolve_rip(uptr instruction);

} // namespace lacz::pattern