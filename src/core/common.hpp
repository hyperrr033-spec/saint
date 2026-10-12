#pragma once
// LaczDLC CS2 -- common prelude.
// Central include so every translation unit sees the same Win32 / STL surface.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_4.h>
#include <dwmapi.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include <thread>
#include <algorithm>
#include <functional>
#include <unordered_map>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using uptr = std::uintptr_t;

namespace lacz {
    inline constexpr const char* kName = "LaczDLC";
    inline constexpr const char* kVersion = "2.0.0";
    inline constexpr const wchar_t* kGameModule = L"client.dll";
    inline constexpr const wchar_t* kEngineModule = L"engine2.dll";
}