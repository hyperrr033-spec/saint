# LaczDLC CS2 Internal

A from-scratch CS2 (Source 2) internal cheat, built around a premium animated
ImGui menu. Written in C++20, MSVC/clang-cl, x64.

The upstream repo this was requested against (`Parcus123/LaczDLC-Cs2-Internal`)
contains only a `LICENSE` and a `README.md` on its `main` branch and its `cs2`
tag — there is no source in it. This tree is therefore a clean-room
implementation of the same idea: a merged "Noxium-style" internal with a
substantially larger feature set and a custom UI.

## Feature set

**Legit / Rage**
- Aimbot with hitbox, target-priority, FOV, smoothing, distance, visibility and
  smoke gating.
- Triggerbot with hitchance, delay, distance and hold-key.
- Rage aimbot with full penetration (autowall) scoring, minimum damage,
  minimum hitchance, safe-point preference, body fallback and hitbox bitmask.
- Accuracy overrides: no spread / no recoil, auto scope, auto stop, auto
  revolver.

**Anti-aim**
- Desync, lower-body yaw, configurable yaw base, yaw offset, static / random /
  spin jitter, pitch modes, fake lag with choke limit, on-shot override.

**Visuals**
- ESP: full / corner / filled / 3D boxes, name, health (bar + text), armor,
  weapon, ammo, distance, snaplines, skeletons, through-walls coloring.
- Chams with separate visible / hidden / backtrack / desync colors.
- Radar reveal, sound ESP, glow.
- Watermark, bomb timer, spectator list.

**Skins**
- Knife model swap, glove swap, agent/music kit toggles, paint kit, wear,
  seed, StatTrak and custom name.

**Misc**
- Bunny hop, auto strafe, fast stop, auto peek, edge jump, jump bug.
- No flash / no smoke / no scope overlay / no visual recoil / remove legs.
- Third person, FOV changer, fake latency.

**Menu**
- Custom animated widget kit: easing springs on every control, animated
  checkbox fills and check strokes, pill switches, gradient sliders, animated
  sidebar selection pill, animated tab underlines and page cross-fades,
  animated header gradient, rainbow/dual-accent theming, config save/load.

## Architecture

```
src/
  core/       memory, pattern scanner, hooks (MinHook), settings + config I/O
  sdk/        math, offsets, schema walker, interfaces, entity, weapon
  render/     D3D11 Present/ResizeBuffers hook, drawing primitives
  gui/        animated widget kit + the menu
  features/   shared context, trace/autowall, aimbot, triggerbot, rage,
              antiaim, esp, chams, skins, misc
  dllmain.cpp entry point
```

Offset resolution is layered: exported symbols first (`dwLocalPlayerController`,
`dwGameEntitySystem`, `dwGlobalVars`), then pattern scans, and netvars are
resolved at runtime through the CS2 schema system rather than hardcoded — so a
game update that only moves fields needs no source change.

## Build

Requirements: Visual Studio 2022/2026 (or LLVM `clang-cl`) with the Windows 10
SDK, CMake 3.20+.

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

ImGui (docking, v1.91.5) and MinHook (v1.3.4) are pulled automatically via
FetchContent. Output: `build/Release/LaczDLC.dll`.

## Manual-map loader

`loader/loader.cpp` is a self-contained x64 manual mapper. It maps the DLL into
`cs2.exe` without `LoadLibrary`, so the image never appears in the PEB module
list; imports and exports are resolved by hand and relocations are applied in
place.

```powershell
cl /nologo /std:c++20 /O2 /EHsc loader\loader.cpp /Fe:loader\LaczLoader.exe
```

Run `LaczLoader.exe` (as administrator) with `cs2.exe` already at the main
menu. It reads `build\Release\LaczDLC.dll` by default; pass a path as the first
argument to override.

What it does, in order:

1. finds `cs2.exe`, opens it for full access
2. reads the DLL from disk
3. `VirtualAllocEx` at the image's preferred base (falls back to any region)
4. copies headers and sections, applying `IMAGE_REL_BASED_DIR64` fixups
5. resolves the import table against the target's PEB loader list
6. runs a code-cave stub that calls `DllMain(DLL_PROCESS_ATTACH)`
7. calls the `LaczMarkManualMapped` export so the cheat never unloads itself

The cheat was made manual-map-clean for this: it contains **no `thread_local`
state** (the two sites that used it were rewritten to stack/plain static), so
the DLL has no TLS directory and the mapper needs no `__tls_index` emulation.
`LaczShutdown()` is exported for the loader to request a clean teardown; on a
manually mapped image it unwinds the hooks and returns instead of calling
`FreeLibraryAndExitThread`.

## Notes on state

- The trace, schema and interface layer validates every level before descending,
  so a mismatched build degrades to "feature inactive" rather than crashing.
- `offsets.cpp` is the single file to touch when Valve reshuffles a build; the
  signature strings there are the ones that most commonly move.
- Chams currently routes through the ESP overlay until the model-material hook
  is wired into a specific build's `DrawModel` address.