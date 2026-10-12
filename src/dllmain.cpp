#include "core/common.hpp"
#include "core/hooks.hpp"
#include "core/log.hpp"
#include "sdk/offsets.hpp"
#include "sdk/interfaces.hpp"
#include "render/render.hpp"
#include "features/features.hpp"
#include "gui/menu.hpp"

using namespace lacz;

// ---------------------------------------------------------------------------
// Entry point.
//
// Lifecycle:
//   1. resolve offsets / interfaces from the already-loaded game
//   2. install the D3D11 present + wndproc hooks
//   3. the present detour finishes setup on the first rendered frame
//   4. END key (or the exported LaczShutdown) unloads in reverse order
//
// Manual-map compatibility:
//   A self-mapped image has no loader entry, so FreeLibraryAndExitThread would
//   tear down the wrong thing. A mapper calls LaczMarkManualMapped() right
//   after mapping; shutdown then unwinds hooks and just returns, leaving the
//   loader responsible for freeing the image. TLS was removed from the hot
//   paths so no __tls_index emulation is required either.
// ---------------------------------------------------------------------------

static HMODULE g_self = nullptr;
static std::atomic<bool> g_running{true};
static std::atomic<bool> g_manual_mapped{false};

static DWORD WINAPI main_thread(LPVOID);

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, main_thread, nullptr, 0, nullptr);
        if (thread)
            CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        // Process teardown (reserved != nullptr): the OS reclaims everything.
        if (reserved == nullptr) {
            g_running = false;
            render::shutdown();
            hooks::manager::get().shutdown();
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Exports used by a manual-mapping loader.
// ---------------------------------------------------------------------------
extern "C" __declspec(dllexport) void LaczMarkManualMapped() {
    g_manual_mapped = true;
}

// Ask the cheat to shut down cleanly. Returns immediately; the worker thread
// performs the actual unwind. Safe to call from the loader's thread.
extern "C" __declspec(dllexport) void LaczShutdown() {
    g_running = false;
}

extern "C" __declspec(dllexport) bool LaczIsRunning() {
    return g_running.load();
}

static DWORD WINAPI main_thread(LPVOID) {
    log::write("=== LaczDLC %s starting (manual_mapped=%d) ===",
               kVersion, g_manual_mapped.load() ? 1 : 0);

    // Wait for the game client module before touching anything.
    int waited = 0;
    while (g_running && !GetModuleHandleW(L"client.dll")) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (++waited > 150) {   // 30 seconds
            log::write("client.dll never appeared; aborting");
            return 1;
        }
    }
    if (!g_running)
        return 1;
    log::write("client.dll found");

    log::write("resolving offsets/interfaces...");
    if (!off::resolve()) {
        log::write("FAIL: offset/signature resolution failed. Mandatory sigs: "
                   "entity_list=%d global_vars=%d trace_ray=%d",
                   off::sigs.entity_list ? 1 : 0,
                   off::sigs.global_vars ? 1 : 0,
                   off::sigs.trace_ray ? 1 : 0);
        log::write("This means the signature patterns do not match your game "
                   "build. Update src/sdk/offsets.cpp.");
        MessageBoxW(nullptr,
                    L"LaczDLC could not resolve the required offsets.\n"
                    L"The game build is probably unsupported.\n\n"
                    L"See %APPDATA%\\LaczDLC\\lacz.log for details.",
                    L"LaczDLC", MB_OK | MB_ICONERROR);
        return 1;
    }
    log::write("offsets OK (entity_list=%llX trace=%llX)",
               static_cast<unsigned long long>(off::sigs.entity_list),
               static_cast<unsigned long long>(off::sigs.trace_ray));

    log::write("installing render hooks...");
    if (!render::initialize()) {
        log::write("FAIL: render::initialize (swap chain hook)");
        return 1;
    }
    log::write("render hooks installed");

    // Idle until unload is requested (END key or LaczShutdown).
    while (g_running) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            g_running = false;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    log::write("shutting down");
    render::shutdown();
    hooks::manager::get().shutdown();

    if (g_manual_mapped)
        return 0;

    FreeLibraryAndExitThread(g_self, 0);
    return 0;
}