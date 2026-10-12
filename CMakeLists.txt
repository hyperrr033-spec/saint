cmake_minimum_required(VERSION 3.20)
project(LaczDLC VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---------------------------------------------------------------------------
# Toolchain requirement: clang-cl (LLVM) + MSVC SDK headers.
#   cmake -G "Ninja" -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release ..
# ---------------------------------------------------------------------------
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Build is tuned for clang-cl; MSVC also works.")
endif()

add_compile_definitions(
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

# ---------------------------------------------------------------------------
# Release tuning. The Visual Studio generator is multi-config, so
# CMAKE_BUILD_TYPE is empty at configure time and the old single-config check
# silently skipped every optimization flag -- the loaded DLL could end up built
# with default settings (slow hot paths). Apply per-config instead.
# ---------------------------------------------------------------------------
add_compile_options(
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Release>:/GS->
    $<$<CONFIG:Release>:/Gy>
    $<$<CONFIG:Release>:/Gw>
    $<$<CONFIG:Release>:/favor:AMD64>
    $<$<CONFIG:RelWithDebInfo>:/O2>
)
add_link_options(
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
    $<$<CONFIG:Release>:/LTCG>
)

# ---------------------------------------------------------------------------
# Third party: ImGui (docking) + ImGui addons + MinHook, fetched from source.
#   Kept as FetchContent so the tree stays self contained.
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5-docking
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG v1.3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(minhook)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

# ---------------------------------------------------------------------------
# The cheat itself.
# ---------------------------------------------------------------------------
add_library(LaczDLC SHARED
    src/dllmain.cpp
    src/core/memory.cpp
    src/core/pattern.cpp
    src/core/hooks.cpp
    src/core/config.cpp
    src/core/log.cpp
    src/sdk/interfaces.cpp
    src/sdk/offsets.cpp
    src/sdk/schema.cpp
    src/sdk/entity.cpp
    src/sdk/weapon.cpp
    src/render/renderer.cpp
    src/render/hook_d3d.cpp
    src/gui/widgets.cpp
    src/gui/menu.cpp
    src/features/features.cpp
    src/features/shared.cpp
    src/features/trace.cpp
    src/features/aimbot.cpp
    src/features/triggerbot.cpp
    src/features/rage.cpp
    src/features/antiaim.cpp
    src/features/esp.cpp
    src/features/chams.cpp
    src/features/skins.cpp
    src/features/misc.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(LaczDLC PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${minhook_SOURCE_DIR}/include
)

target_link_libraries(LaczDLC PRIVATE
    minhook
    d3d11
    dxgi
    d3dcompiler
    dwmapi
    winmm
)

set_target_properties(LaczDLC PROPERTIES
    OUTPUT_NAME "LaczDLC"
    PREFIX ""
    SUFFIX ".dll"
)

# ---------------------------------------------------------------------------
# Loader executable.
# ---------------------------------------------------------------------------
add_executable(LaczLoader loader/loader.cpp)
set_target_properties(LaczLoader PROPERTIES
    OUTPUT_NAME "LaczLoader"
)