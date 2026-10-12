#pragma once
#include "math.hpp"
#include "offsets.hpp"
#include "interfaces.hpp"
#include "../core/memory.hpp"
#include <array>

namespace lacz {

// CS2 hitbox indices. CModelState's transform array (node + 0x1C0) is indexed
// by these values -- the game's own CalculateHitboxData() reads
// data[HEAD/NECK/CHEST/...].pos. The previous enum invented its own 0..19
// ordering (head=5), which read the wrong body parts and broke skeleton ESP and
// aimbot hitboxes.
enum bone_index : int {
    BONE_PELVIS     = 2,    // PELVIS
    BONE_SPINE_1    = 3,    // STOMACH
    BONE_SPINE_2    = 4,    // CHEST
    BONE_SPINE_3    = 8,    // RIGHT_CHEST (upper chest)
    BONE_NECK       = 5,    // NECK
    BONE_HEAD       = 6,    // HEAD
    BONE_SHOULDER_L = 13,   // LEFT_CHEST (closest to left shoulder)
    BONE_ARM_UPPER_L = 13,
    BONE_ARM_LOWER_L = 13,
    BONE_HAND_L     = 13,
    BONE_SHOULDER_R = 8,
    BONE_ARM_UPPER_R = 8,
    BONE_ARM_LOWER_R = 8,
    BONE_HAND_R     = 8,
    BONE_HIP_L      = 23,   // L_LEG
    BONE_KNEE_L     = 23,
    BONE_FOOT_L     = 24,   // L_FEET
    BONE_HIP_R      = 26,   // R_LEG
    BONE_KNEE_R     = 26,
    BONE_FOOT_R     = 27,   // R_FEET
    BONE_COUNT      = 28,
};

// Highest bone/hitbox index we ever touch. The model's transform array holds
// ~28 hitbox entries (indices valid up to 27).
inline constexpr int kMaxBones = 128;

// CS2 bone data: 32 bytes, 16-aligned. pos (0x0), scale (0xC), rot quaternion
// (0x10). The old definition omitted `scale`, giving a 28-byte stride so every
// bone after the first was misread.
struct alignas(16) bone_data {
    vec3  pos;
    float scale;
    float rot[4];
};

enum hitbox_group : int {
    HITBOX_HEAD = 0,
    HITBOX_NECK,
    HITBOX_CHEST,
    HITBOX_STOMACH,
    HITBOX_PELVIS,
    HITBOX_LEGS,
    HITBOX_ARMS,
};

struct hitbox_t {
    u32   bone;
    u32   group;
    vec3  mins;
    vec3  maxs;
    float radius;
};

struct c_cs_player_pawn;
struct c_cs_player_controller;
struct c_base_weapon;
struct c_cs_player_weapon_base;
struct c_game_entity_system;

bool is_valid_entity_ptr(uptr ptr);
c_cs_player_pawn* entity_by_index(int index);
c_cs_player_pawn* local_pawn();
c_cs_player_controller* local_controller();

struct c_base_entity {
    void*  vtable;
    int    pad[0x8];
    int    m_iEntIndex;

bool   is_dormant() const {
        // Build 14190: m_bDormant lives on CGameSceneNode (+0x103), not on the
        // entity. Reading it at entity+0x103 would hit unrelated bytes.
        uptr node = mem::read<uptr>(reinterpret_cast<uptr>(this) + off::netvars.m_pGameSceneNode, 0);
        if (!node || !is_valid_entity_ptr(node))
            return false;
        return mem::read<bool>(node + off::netvars.m_bDormant, false);
    }
int    health() const {
        if (!this) return 0;
        // Authoritative build-14190 offset. The old candidate scan returned the
        // first in-range value, so weapons/props (which have unrelated data at
        // other candidate offsets) reported a plausible health and masqueraded
        // as players -- that is why all_pawns() found 29 "pawns" and hp=374.
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_iHealth, 0);
    }
    int    team() const {
        if (!this) return 0;
        return mem::read<uint8_t>(reinterpret_cast<uptr>(this) + off::netvars.m_iTeamNum, 0);
    }
int    life_state() const {
        if (!this) return 0;
        return mem::read<uint8_t>(reinterpret_cast<uptr>(this) + off::netvars.m_lifeState, 0);
    }
vec3   origin() const {
        // Build 14190: the origin lives on CGameSceneNode (m_vecAbsOrigin at
        // node+0xC8), not on the entity. Try the scene node first, then the
        // legacy on-entity offsets as a fallback for other builds.
        uptr node = mem::read<uptr>(reinterpret_cast<uptr>(this) + off::netvars.m_pGameSceneNode, 0);
        if (node && is_valid_entity_ptr(node)) {
            const u32 node_offsets[] = { 0xC8, 0x80, 0xD0 };
            for (u32 o : node_offsets) {
                vec3 v = mem::read<vec3>(node + o);
                if (!v.is_zero() && std::isfinite(v.x) && std::isfinite(v.y))
                    return v;
            }
        }
        const u32 candidates[] = { 0x138, 0x140, 0x88, 0x80, 0xD0, 0xC8 };
        for (u32 o : candidates) {
            if (!o) continue;
            vec3 v = mem::read<vec3>(reinterpret_cast<uptr>(this) + o);
            if (!v.is_zero() && std::isfinite(v.x) && std::isfinite(v.y))
                return v;
        }
        return {};
    }
    vec3   eye_position() const {
        // Build 14190: eye = abs origin + m_vecViewOffset (C_BaseModelEntity,
        // pawn+0xF60). The old code returned the scene-node abs origin (the
        // feet), so aim FOV / trace start were ~64 units low.
        vec3 view = mem::read<vec3>(reinterpret_cast<uptr>(this) + 0xF60);
        if (view.z > 0.f && view.z < 200.f && std::isfinite(view.z))
            return origin() + view;
        return origin() + vec3(0, 0, 64.f);
    }
    int    index() const { return m_iEntIndex; }
};

struct c_cs_player_pawn : c_base_entity {
    bool alive() const {
        // Authoritative: life state LIFE_ALIVE (0) + positive health. The old
        // version also accepted any entity with a non-zero scene-node origin,
        // which is true for most non-players -- another source of phantom
        // "pawns". Require the pawn to actually be alive and on a real team.
        if (!this)
            return false;
        if (life_state() != 0)
            return false;
        int hp = health();
        return hp > 0 && hp <= 200;
    }
    bool is_enemy(const c_cs_player_pawn* local) const {
        if (!local) return false;
        int lt = local->team();
        int t = team();
        if (lt >= 2 && t >= 2)
            return lt != t;
        return true;
    }
bool is_local_player() const;   // defined in entity.cpp (needs controller layout)
    bool is_visible() const;

uptr game_scene_node() const {
        static u32 cached = 0;
        if (cached) {
            uptr node = mem::read<uptr>(reinterpret_cast<uptr>(this) + cached, 0);
            if (node && is_valid_entity_ptr(node))
                return node;
            cached = 0;
        }
        const u32 offsets[] = {
            off::netvars.m_pGameSceneNode,
            0x330, 0x328, 0x320, 0x338, 0x318, 0x310
        };
        for (u32 o : offsets) {
            if (!o) continue;
            uptr node = mem::read<uptr>(reinterpret_cast<uptr>(this) + o, 0);
            if (node && is_valid_entity_ptr(node)) {
                cached = o;
                return node;
            }
        }
        return 0;
    }
vec3 get_abs_origin() const {
        uptr node = game_scene_node();
        if (node && is_valid_entity_ptr(node)) {
            // 0xC8 = CGameSceneNode::m_vecAbsOrigin (build 14190). 0x80 is the
            // quantized local origin. Do NOT probe 0xD0 first: it straddles
            // vecAbsOrigin and m_angAbsRotation and yields a plausible-looking
            // but wrong vector.
            const u32 offsets[] = { 0xC8, 0x80, 0x138, 0x140, 0x148, 0x150, 0x58, 0x60 };
            for (u32 o : offsets) {
                vec3 pos = mem::read<vec3>(node + o);
                if (!pos.is_zero() && std::isfinite(pos.x) && std::isfinite(pos.y))
                    return pos;
            }
        }
        vec3 o = origin();
        if (!o.is_zero() && std::isfinite(o.x) && std::isfinite(o.y))
            return o;
        return {};
    }
    ang get_eye_angles() const {
        return mem::read<ang>(reinterpret_cast<uptr>(this) + off::netvars.m_angEyeAngles);
    }
    void set_eye_angles(const ang& a) const {
        mem::write<ang>(reinterpret_cast<uptr>(this) + off::netvars.m_angEyeAngles, a);
    }
    int flags() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_fFlags, 0);
    }
    bool on_ground() const { return (flags() & 1) != 0; }
    int shots_fired() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_iShotsFired, 0);
    }
    ang aim_punch() const {
        // Build 14190: C_CSPlayerPawn::m_aimPunchAngle @ pawn+0x177C, and
        // m_aimPunchCache (CUtlVector<QAngle>) @ pawn+0x17A0. Prefer the live
        // cache's last entry (matches the game's RCS), fall back to the direct
        // angle. The old +0x1598 services pointer / +0xA4 read the wrong object.
        const uptr base = reinterpret_cast<uptr>(this);
        const int size = mem::read<int>(base + 0x17A0 + 0x0, 0);   // m_Size
        const uptr data = mem::read<uptr>(base + 0x17A0 + 0x8, 0); // m_Data
        if (size > 0 && size < 0x81 && data) {
            ang cached = mem::read<ang>(data + static_cast<uptr>(size - 1) * sizeof(ang));
            if (std::isfinite(cached.pitch) && std::isfinite(cached.yaw))
                return cached;
        }
        return mem::read<ang>(base + 0x177C);
    }
    bool scoped() const {
        return mem::read<bool>(reinterpret_cast<uptr>(this) + off::netvars.m_bIsScoped, false);
    }
    int armor() const {
        int v = mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_ArmorValue, 0);
        if (v < 0 || v > 200) v = 0;
        return v;
    }
    bool has_helmet() const {
        // m_bHasHelmet lives inside CCSPlayer_ItemServices (+0x49), not on the
        // pawn. Build 14190: m_pItemServices @ pawn+0x12F8.
        uptr base = reinterpret_cast<uptr>(this);
        void* item_svc = mem::read<void*>(base + off::netvars.m_pItemServices, nullptr);
        if (!item_svc)
            item_svc = mem::read<void*>(base + 0x12F8, nullptr);
        if (item_svc && mem::is_readable(reinterpret_cast<uptr>(item_svc) + 0x49, 1))
            return mem::read<bool>(reinterpret_cast<uptr>(item_svc) + 0x49, false);
        return false;
    }
    float flash_duration() const {
        return mem::read<float>(reinterpret_cast<uptr>(this) + off::netvars.m_flFlashDuration, 0.f);
    }
    void set_flash_duration(float v) const {
        mem::write<float>(reinterpret_cast<uptr>(this) + off::netvars.m_flFlashDuration, v);
    }
    int fov() const {
        // m_iFOV lives inside CameraServices (+0x298), not on the pawn.
        void* cam = mem::read<void*>(reinterpret_cast<uptr>(this) + off::netvars.m_pCameraServices, nullptr);
        if (cam && mem::is_readable(reinterpret_cast<uptr>(cam) + 0x298, sizeof(int))) {
            int v = mem::read<int>(reinterpret_cast<uptr>(cam) + 0x298, 0);
            if (v >= 1 && v <= 200) return v;
        }
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_iFOV, 0);
    }
    bool spotted() const {
        // Build 14190: C_CSPlayerPawn::m_entitySpottedState @ +0x1E88,
        // EntitySpottedState_t::m_bSpotted @ +0x8.
        return mem::read<bool>(reinterpret_cast<uptr>(this) + 0x1E88 + 0x8, false);
    }
    void set_spotted(bool v) const {
        mem::write<bool>(reinterpret_cast<uptr>(this) + 0x1E88 + 0x8, v);
    }
    float simulation_time() const {
        return mem::read<float>(reinterpret_cast<uptr>(this) + off::netvars.m_flSimulationTime, 0.f);
    }

    c_base_weapon* active_weapon() const;
    void* weapon_services() const {
        return mem::read<void*>(reinterpret_cast<uptr>(this) + off::netvars.m_pWeaponServices, nullptr);
    }
    void* movement_services() const {
        return mem::read<void*>(reinterpret_cast<uptr>(this) + off::netvars.m_pMovementServices, nullptr);
    }
    void* observer_services() const {
        return mem::read<void*>(reinterpret_cast<uptr>(this) + off::netvars.m_pObserverServices, nullptr);
    }
    void* camera_services() const {
        return mem::read<void*>(reinterpret_cast<uptr>(this) + off::netvars.m_pCameraServices, nullptr);
    }

    bool get_bones(std::array<vec3, BONE_COUNT>& out) const;
    bool get_bone(int index, vec3& out) const;
    bool get_hitbox(int hitbox_index, vec3& mins, vec3& maxs) const;
    vec3 hitbox_center(int hitbox_index) const;
};

struct c_cs_player_controller : c_base_entity {
    // A controller is a real player controller only if it has a readable name
    // and a sane team/connected state. The old code accepted ANY entity as a
    // controller, so weapons/props leaked into all_controllers() and were then
    // probed for a pawn handle.
    bool is_valid_controller() const {
        if (!this)
            return false;
        char name[16]{};
        if (!mem::read_raw(reinterpret_cast<uptr>(this) + 0x6FC, name, sizeof(name)))
            return false;
        if (name[0] == '\0')
            return false;
        // Require the first bytes to be printable ASCII so random data at the
        // name offset on a non-controller entity does not pass.
        for (int i = 0; i < 4 && name[i]; ++i) {
            if (static_cast<unsigned char>(name[i]) < 0x20 ||
                static_cast<unsigned char>(name[i]) > 0x7E)
                return false;
        }
        return true;
    }
    const char* name() const {
        // Build 14190: CBasePlayerController::m_iszPlayerName @ +0x6FC
        // (char[128], inline). The old 0x630/0x610 guesses read controller
        // fields that are not the name, so ESP showed garbage.
        uptr name_ptr = reinterpret_cast<uptr>(this) + 0x6FC;
        static char buf[128];
        if (mem::read_raw(name_ptr, buf, sizeof(buf) - 1) && buf[0] != '\0') {
            buf[sizeof(buf) - 1] = 0;
            return buf;
        }
        return "player";
    }
    u32 steam_id() const {
        return mem::read<u32>(reinterpret_cast<uptr>(this) + 0x788, 0);
    }
    int money() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_iAccount, 0);
    }
    c_cs_player_pawn* pawn() const {
        if (!this)
            return nullptr;

        // Authoritative build-14190 handles, tried first:
        //   0x92C CCSPlayerController::m_hPlayerPawn  (real pawn link)
        //   0x6BC CBasePlayerController::m_hPawn      (fallback)
        static u32 cached = 0;
        if (cached) {
            u32 h = mem::read<u32>(reinterpret_cast<uptr>(this) + cached, 0);
            if (h != 0 && h != 0xFFFFFFFF) {
                int idx = h & 0x7FFF;
                if (idx > 0 && idx < 0x7FFF) {
                    auto* p = entity_by_index(idx);
                    if (p && is_valid_entity_ptr(reinterpret_cast<uptr>(p)))
                        return p;
                }
            }
            cached = 0;   // handle went stale; re-resolve below
        }

        const u32 offsets[] = { 0x92C, 0x6BC, 0x6C4 };
        for (u32 o : offsets) {
            u32 h = mem::read<u32>(reinterpret_cast<uptr>(this) + o, 0);
            if (h != 0 && h != 0xFFFFFFFF) {
                int idx = h & 0x7FFF;
                if (idx > 0 && idx < 0x7FFF) {
                    auto* p = entity_by_index(idx);
                    if (p && is_valid_entity_ptr(reinterpret_cast<uptr>(p)) && p->alive()) {
                        cached = o;
                        return p;
                    }
                }
            }
        }

        return nullptr;
    }
    bool is_local_player() const {
        return mem::read<bool>(reinterpret_cast<uptr>(this) + off::netvars.m_bIsLocalPlayer, false);
    }
    bool is_alive() const {
        auto* p = pawn();
        return p && p->alive();
    }
};

struct c_base_weapon : c_base_entity {
    // The econ item fields (definition index, paint kit, wear, ...) are not on
    // the weapon directly. On build 14190 the path is:
    //   C_EconEntity -> m_AttributeManager (+0x1290) -> m_Item (+0x50)
    // giving a sub-object offset of 0x12E0 from the weapon base to C_EconItemView.
    static constexpr u32 kItemViewOffset = 0x12E0;
    int item_definition_index() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + kItemViewOffset +
                              off::netvars.m_iItemDefinitionIndex, 0);
    }
    int item_id_high() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + kItemViewOffset +
                              off::netvars.m_iItemIDHigh, 0);
    }
    void set_item_id_high(int v) const {
        mem::write<int>(reinterpret_cast<uptr>(this) + kItemViewOffset +
                        off::netvars.m_iItemIDHigh, v);
    }
    int fallback_paint_kit() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_nFallbackPaintKit, 0);
    }
    void set_paint_kit(int v) const {
        mem::write<int>(reinterpret_cast<uptr>(this) + off::netvars.m_nFallbackPaintKit, v);
    }
    int fallback_seed() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_nFallbackSeed, 0);
    }
    void set_seed(int v) const {
        mem::write<int>(reinterpret_cast<uptr>(this) + off::netvars.m_nFallbackSeed, v);
    }
    float fallback_wear() const {
        return mem::read<float>(reinterpret_cast<uptr>(this) + off::netvars.m_flFallbackWear, 0.f);
    }
    void set_wear(float v) const {
        mem::write<float>(reinterpret_cast<uptr>(this) + off::netvars.m_flFallbackWear, v);
    }
    int fallback_stat_trak() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + off::netvars.m_nFallbackStatTrak, 0);
    }
    void set_stat_trak(int v) const {
        mem::write<int>(reinterpret_cast<uptr>(this) + off::netvars.m_nFallbackStatTrak, v);
    }
    int max_clip() const;
    int clip() const;
    bool is_gun() const;
    bool is_knife() const;
    bool is_grenade() const;
    bool is_sniper() const;
    bool is_taser() const;
    int ammo() const;
    float inaccuracy() const;
    float spread() const;
};

struct c_cs_player_weapon_base : c_base_weapon {
    int recoil_index() const {
        return mem::read<int>(reinterpret_cast<uptr>(this) + 0x0, 0);
    }
    float next_primary_attack() const {
        return mem::read<float>(reinterpret_cast<uptr>(this) + 0x0, 0.f);
    }
};

c_cs_player_pawn* local_pawn();
c_cs_player_controller* local_controller();
c_cs_player_controller* controller_by_index(int index);
int max_clients();
std::vector<c_cs_player_controller*> all_controllers();
std::vector<c_cs_player_pawn*> all_pawns();

} // namespace lacz
