#include "entity.hpp"
#include "interfaces.hpp"
#include "../core/memory.hpp"
#include "../core/pattern.hpp"

namespace lacz::iface {

static void* create_interface(const char* module_name, const char* interface_name) {
    if (!module_name || !interface_name)
        return nullptr;

    wchar_t wmod[128]{};
    MultiByteToWideChar(CP_UTF8, 0, module_name, -1, wmod, 128);

    HMODULE mod = GetModuleHandleW(wmod);
    if (!mod)
        mod = LoadLibraryW(wmod);
    if (!mod)
        return nullptr;

    typedef void* (*create_interface_fn)(const char* name, int* return_code);
    auto fn = reinterpret_cast<create_interface_fn>(GetProcAddress(mod, "CreateInterface"));
    if (fn) {
        void* result = fn(interface_name, nullptr);
        if (result)
            return result;
    }
    return nullptr;
}

void* get(const char* module, const char* name) {
    return create_interface(module, name);
}

bool resolve() {
    game_client     = get("client.dll", "Source2Client002");
    if (!game_client)
        game_client = get("client.dll", "Source2Client001");

    engine          = get("engine2.dll", "Source2EngineToClient001");
    if (!engine)
        engine      = get("engine2.dll", "Source2EngineToClient002");

    schema_system   = get("schemasystem.dll", "SchemaSystem_001");
    input_system    = get("inputsystem.dll", "InputSystemVersion001");
    cvar            = get("tier0.dll", "VEngineCvar007");
    material_system = get("materialsystem2.dll", "VMaterialSystem2_001");
    file_system     = get("filesystem_stdio.dll", "VFileSystem017");
    localize        = get("localize.dll", "Localize_001");

    entity_system   = get("client.dll", "Source2EntitySystem001");
    if (!entity_system)
        entity_system = get("client.dll", "Source2ClientEntityList001");

    return game_client && engine && schema_system;
}

} // namespace lacz::iface

namespace lacz {

static uptr g_cached_entity_system_ptr = 0;
static bool g_entity_system_searched = false;

static uptr g_cached_local_player_ptr = 0;
static bool g_local_player_searched = false;

static uptr g_cached_global_vars_ptr = 0;
static bool g_global_vars_searched = false;

void* get_entity_system() {
    // The signature/global path (dwGameEntitySystem) is the layout the entity
    // walker was validated against on build 14190. The Source2EntitySystem001
    // interface is a different object on some builds, so it is only a fallback.
    uptr ptr = off::sigs.entity_list;
    if (ptr && mem::is_readable(ptr, sizeof(void*))) {
        void* real_system = *reinterpret_cast<void**>(ptr);
        if (real_system && is_valid_entity_ptr(reinterpret_cast<uptr>(real_system)))
            return real_system;
    }

    if (iface::entity_system && is_valid_entity_ptr(reinterpret_cast<uptr>(iface::entity_system)))
        return iface::entity_system;

    return nullptr;
}

void* get_local_player() {
    if (off::sigs.local_player && mem::is_readable(off::sigs.local_player, sizeof(void*))) {
        void* ptr = *reinterpret_cast<void**>(off::sigs.local_player);
        if (ptr && is_valid_entity_ptr(reinterpret_cast<uptr>(ptr)))
            return ptr;
    }
    return nullptr;
}

void* get_local_pawn_direct() {
    if (off::sigs.local_pawn && mem::is_readable(off::sigs.local_pawn, sizeof(void*))) {
        uptr level1 = *reinterpret_cast<uptr*>(off::sigs.local_pawn);
        if (level1 && is_valid_entity_ptr(level1)) {
            return reinterpret_cast<void*>(level1);
        }
    }
    return nullptr;
}

global_vars_base* get_global_vars() {
    if (off::sigs.global_vars && mem::is_readable(off::sigs.global_vars, sizeof(void*))) {
        uptr gv = *reinterpret_cast<uptr*>(off::sigs.global_vars);
        if (gv && is_valid_entity_ptr(gv))
            return reinterpret_cast<global_vars_base*>(gv);
    }
    return nullptr;
}

} // namespace lacz
