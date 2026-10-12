#include "entity.hpp"
#include "weapon.hpp"
#include "../core/memory.hpp"
#include "../core/pattern.hpp"
#include "../core/log.hpp"
#include "interfaces.hpp"

namespace lacz {

bool is_valid_entity_ptr(uptr ptr) {
    if (!ptr || ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF)
        return false;
    // Fast path: SEH probe instead of VirtualQuery (which was the #1 perf killer).
    __try {
        volatile uptr test = *reinterpret_cast<uptr*>(ptr);
        (void)test;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Resolve the actual CGameEntitySystem* from the signature pointer.
static uptr get_entity_system_ptr() {
    // Try the interface first (it's the fastest).
    if (iface::entity_system) {
        uptr sys = reinterpret_cast<uptr>(iface::entity_system);
        if (sys > 0x10000 && is_valid_entity_ptr(sys))
            return sys;
    }

    // The signature points to a global pointer that holds the system address.
    uptr sig = off::sigs.entity_list;
    if (!sig)
        return 0;

    // Try dereferencing: *sig gives us the entity system object.
    __try {
        uptr deref = *reinterpret_cast<uptr*>(sig);
        if (deref > 0x10000 && is_valid_entity_ptr(deref))
            return deref;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}

    // sig itself might be the entity system (if resolved from interface).
    if (is_valid_entity_ptr(sig))
        return sig;

    return 0;
}

c_cs_player_pawn* entity_by_index(int index) {
    if (index <= 0 || index >= 0x7FFF)
        return nullptr;

    uptr system = get_entity_system_ptr();
    if (!system)
        return nullptr;

    int chunk_idx = index >> 9;
    int slot_idx = index & 0x1FF;

    // CS2 entity system layout (2025/2026 builds):
    //   system + 0x10 = pointer to chunk-list array (or inline array)
    //   chunk_list[chunk_idx] = pointer to chunk
    //   chunk[slot_idx * stride] = pointer to entity identity / entity
    //
    // We try both 2-level (ptr to array) and 1-level (inline array) layouts,
    // and both common strides (0x78 for CEntityIdentity, 0x20 for older).

    static const u32 list_offsets[] = { 0x10, 0x08, 0x18, 0x00 };
    static const u32 strides[] = { 0x78, 0x20, 0x70, 0x80 };

    for (u32 list_off : list_offsets) {
        // 2-level: system+list_off is a pointer to the chunk array.
        __try {
            uptr list_ptr = *reinterpret_cast<uptr*>(system + list_off);
            if (list_ptr > 0x10000 && list_ptr < 0x7FFFFFFFFFFF) {
                uptr chunk = *reinterpret_cast<uptr*>(list_ptr + 8ULL * chunk_idx);
                if (chunk > 0x10000 && chunk < 0x7FFFFFFFFFFF) {
                    for (u32 stride : strides) {
                        uptr entity = *reinterpret_cast<uptr*>(chunk + (u64)stride * slot_idx);
                        if (entity > 0x10000 && entity < 0x7FFFFFFFFFFF) {
                            return reinterpret_cast<c_cs_player_pawn*>(entity);
                        }
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        // 1-level: system+list_off IS the chunk array (inline).
        __try {
            uptr chunk = *reinterpret_cast<uptr*>(system + list_off + 8ULL * chunk_idx);
            if (chunk > 0x10000 && chunk < 0x7FFFFFFFFFFF) {
                for (u32 stride : strides) {
                    uptr entity = *reinterpret_cast<uptr*>(chunk + (u64)stride * slot_idx);
                    if (entity > 0x10000 && entity < 0x7FFFFFFFFFFF) {
                        return reinterpret_cast<c_cs_player_pawn*>(entity);
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    return nullptr;
}

int max_clients() {
    if (global_vars && global_vars->max_clients > 0 && global_vars->max_clients <= 64)
        return global_vars->max_clients;
    return 64;
}

c_cs_player_controller* controller_by_index(int index) {
    auto* ent = entity_by_index(index);
    if (!ent)
        return nullptr;
    return reinterpret_cast<c_cs_player_controller*>(ent);
}

c_cs_player_controller* local_controller() {
    auto* ctrl = reinterpret_cast<c_cs_player_controller*>(get_local_player());
    if (ctrl && is_valid_entity_ptr(reinterpret_cast<uptr>(ctrl)))
        return ctrl;

    // Fallback: walk controllers and find the local one.
    for (int i = 1; i <= max_clients(); ++i) {
        auto* c = controller_by_index(i);
        if (!c) continue;
        if (c->is_local_player())
            return c;
    }

    return nullptr;
}

c_cs_player_pawn* local_pawn() {
    // 1. Direct pawn pointer (fastest).
    auto* direct = reinterpret_cast<c_cs_player_pawn*>(get_local_pawn_direct());
    if (direct && is_valid_entity_ptr(reinterpret_cast<uptr>(direct))) {
        // Verify it looks like a valid pawn by checking if it has a non-zero origin.
        vec3 origin = direct->get_abs_origin();
        if (!origin.is_zero() && std::isfinite(origin.x))
            return direct;
    }

    // 2. Controller -> pawn handle.
    auto* ctrl = local_controller();
    if (ctrl) {
        auto* p = ctrl->pawn();
        if (p && is_valid_entity_ptr(reinterpret_cast<uptr>(p)))
            return p;
    }

    // 3. Walk entities and find the one marked as local.
    for (int i = 1; i <= 1024; ++i) {
        auto* p = entity_by_index(i);
        if (!p || !is_valid_entity_ptr(reinterpret_cast<uptr>(p)))
            continue;
        if (p->is_local_player())
            return p;
    }

    return nullptr;
}

std::vector<c_cs_player_controller*> all_controllers() {
    std::vector<c_cs_player_controller*> out;
    for (int i = 1; i <= max_clients(); ++i) {
        auto* ctrl = controller_by_index(i);
        if (ctrl)
            out.push_back(ctrl);
    }
    return out;
}

std::vector<c_cs_player_pawn*> all_pawns() {
    // Perf: cache at ~15Hz. Entity walks are expensive.
    static DWORD s_last = 0;
    static std::vector<c_cs_player_pawn*> s_cache;
    DWORD now = GetTickCount();
    if (now - s_last < 66 && !s_cache.empty())
        return s_cache;
    s_last = now;

    std::vector<c_cs_player_pawn*> out;
    std::vector<uptr> seen;

    // Method 1: Controller -> Pawn (most reliable).
    for (auto* ctrl : all_controllers()) {
        if (!ctrl)
            continue;
        auto* pawn = ctrl->pawn();
        if (!pawn || !is_valid_entity_ptr(reinterpret_cast<uptr>(pawn)))
            continue;

        // Don't filter by team or alive here — let the caller decide.
        // Just check it looks like a valid entity with an origin.
        vec3 pos = pawn->get_abs_origin();
        if (pos.is_zero() && !std::isfinite(pos.x))
            continue;

        uptr addr = reinterpret_cast<uptr>(pawn);
        if (std::find(seen.begin(), seen.end(), addr) == seen.end()) {
            seen.push_back(addr);
            out.push_back(pawn);
        }
    }

    // Method 2: Direct entity sweep for pawn indices (65..1024).
    // Pawns use higher indices than controllers in CS2.
    for (int i = 65; i <= 1024; ++i) {
        auto* entity = entity_by_index(i);
        if (!entity || !is_valid_entity_ptr(reinterpret_cast<uptr>(entity)))
            continue;

        vec3 pos = entity->get_abs_origin();
        if (pos.is_zero())
            continue;

        uptr addr = reinterpret_cast<uptr>(entity);
        if (std::find(seen.begin(), seen.end(), addr) == seen.end()) {
            seen.push_back(addr);
            out.push_back(entity);
        }
    }

    s_cache = out;
    return out;
}

c_base_weapon* c_cs_player_pawn::active_weapon() const {
    void* services = weapon_services();
    if (!services)
        return nullptr;
    // m_hActiveWeapon is at offset 0x60 inside CPlayerWeaponServices.
    u32 handle = mem::read<u32>(reinterpret_cast<uptr>(services) + off::netvars.m_hActiveWeapon, 0);
    if (!handle || handle == 0xFFFFFFFF) {
        // Try alternate offsets inside weapon services.
        handle = mem::read<u32>(reinterpret_cast<uptr>(services) + 0x58, 0);
    }
    if (!handle || handle == 0xFFFFFFFF)
        return nullptr;
    int index = handle & 0x7FFF;
    return reinterpret_cast<c_base_weapon*>(entity_by_index(index));
}

int c_base_weapon::max_clip() const {
    return weapon::info::max_clip(item_definition_index());
}

int c_base_weapon::clip() const {
    // Build 14190: C_BasePlayerWeapon::m_iClip1.
    const u32 candidates[] = { 0x1928, 0x1638, 0x1630, 0x1920, 0x1640 };
    for (u32 o : candidates) {
        int v = mem::read<int>(reinterpret_cast<uptr>(this) + o, -1);
        if (v >= 0 && v <= 250)
            return v;
    }
    return -1;
}

int c_base_weapon::ammo() const {
    return weapon::info::max_clip(item_definition_index());
}

bool c_base_weapon::is_gun() const {
    int idx = item_definition_index();
    return idx >= 1 && idx <= 64 && !is_knife() && !is_grenade() && !is_taser();
}
bool c_base_weapon::is_knife() const {
    int idx = item_definition_index();
    return (idx >= 500 && idx <= 526) || idx == 59 || idx == 42 || idx == 80;
}
bool c_base_weapon::is_grenade() const {
    int idx = item_definition_index();
    return (idx >= 43 && idx <= 48) || idx == 68 || (idx >= 5002 && idx <= 5008);
}
bool c_base_weapon::is_sniper() const {
    int idx = item_definition_index();
    return idx == 9 || idx == 11 || idx == 38 || idx == 40;
}
bool c_base_weapon::is_taser() const {
    return item_definition_index() == 31;
}

float c_base_weapon::inaccuracy() const {
    if (!off::sigs.get_inaccuracy)
        return 0.f;
    return mem::call<float>(off::sigs.get_inaccuracy, this);
}

float c_base_weapon::spread() const {
    if (!off::sigs.get_spread)
        return 0.f;
    return mem::call<float>(off::sigs.get_spread, this);
}

static bool bone_ptr_looks_valid(uptr ptr) {
    if (!ptr || ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF)
        return false;
    __try {
        vec3 v = *reinterpret_cast<vec3*>(ptr);
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
            && (v.x != 0.f || v.y != 0.f || v.z != 0.f)
            && std::abs(v.x) < 20000.f && std::abs(v.y) < 20000.f && std::abs(v.z) < 20000.f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool c_cs_player_pawn::get_bones(std::array<vec3, BONE_COUNT>& out) const {
    uptr node = game_scene_node();
    if (!node)
        return false;

    uptr bones = 0;
    const u32 bone_offsets[] = { 0x1E0, 0x1A0, 0x1B0, 0x200, 0x80, 0xA0 };
    for (u32 o : bone_offsets) {
        __try {
            uptr ptr = *reinterpret_cast<uptr*>(node + o);
            if (bone_ptr_looks_valid(ptr)) {
                bones = ptr;
                break;
            }
            uptr model_state_bone = *reinterpret_cast<uptr*>(node + o + 0x80);
            if (bone_ptr_looks_valid(model_state_bone)) {
                bones = model_state_bone;
                break;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (!bones)
        return false;

    __try {
        for (int i = 0; i < BONE_COUNT; ++i) {
            bone_data bd = *reinterpret_cast<bone_data*>(bones + i * sizeof(bone_data));
            out[i] = bd.origin;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool c_cs_player_pawn::get_bone(int index, vec3& out) const {
    if (index < 0 || index >= kMaxBones)
        return false;
    uptr node = game_scene_node();
    if (!node)
        return false;

    uptr bones = 0;
    const u32 bone_offsets[] = { 0x1E0, 0x1A0, 0x1B0, 0x200, 0x80, 0xA0 };
    for (u32 o : bone_offsets) {
        __try {
            uptr ptr = *reinterpret_cast<uptr*>(node + o);
            if (bone_ptr_looks_valid(ptr)) {
                bones = ptr;
                break;
            }
            uptr model_state_bone = *reinterpret_cast<uptr*>(node + o + 0x80);
            if (bone_ptr_looks_valid(model_state_bone)) {
                bones = model_state_bone;
                break;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    if (!bones)
        return false;

    __try {
        bone_data bd = *reinterpret_cast<bone_data*>(bones + index * sizeof(bone_data));
        out = bd.origin;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return !out.is_zero();
}

bool c_cs_player_pawn::get_hitbox(int hitbox_index, vec3& mins, vec3& maxs) const {
    vec3 center = hitbox_center(hitbox_index);
    if (center.is_zero())
        return false;
    float r = 4.f;
    switch (hitbox_index) {
        case HITBOX_HEAD:    r = 4.f;  break;
        case HITBOX_NECK:    r = 3.f;  break;
        case HITBOX_CHEST:   r = 9.f;  break;
        case HITBOX_STOMACH: r = 9.f;  break;
        case HITBOX_PELVIS:  r = 8.f;  break;
        case HITBOX_LEGS:    r = 6.f;  break;
        case HITBOX_ARMS:    r = 5.f;  break;
    }
    mins = center - vec3(r, r, r);
    maxs = center + vec3(r, r, r);
    return true;
}

vec3 c_cs_player_pawn::hitbox_center(int hitbox_index) const {
    std::array<vec3, BONE_COUNT> bones{};
    if (!get_bones(bones))
        return {};
    switch (hitbox_index) {
        case HITBOX_HEAD:    return bones[BONE_HEAD];
        case HITBOX_NECK:    return bones[BONE_NECK];
        case HITBOX_CHEST:   return (bones[BONE_SPINE_2] + bones[BONE_SPINE_3]) * 0.5f;
        case HITBOX_STOMACH: return bones[BONE_SPINE_1];
        case HITBOX_PELVIS:  return bones[BONE_PELVIS];
        case HITBOX_LEGS:    return (bones[BONE_KNEE_L] + bones[BONE_KNEE_R]) * 0.5f;
        case HITBOX_ARMS:    return (bones[BONE_ARM_LOWER_L] + bones[BONE_ARM_LOWER_R]) * 0.5f;
    }
    return {};
}

bool c_cs_player_pawn::is_visible() const {
    auto* local = local_pawn();
    if (!local)
        return false;
    return spotted();
}

} // namespace lacz
