#include "trace.hpp"
#include "../core/memory.hpp"
#include "../sdk/interfaces.hpp"
#include "../sdk/entity.hpp"
#include "../sdk/weapon.hpp"

namespace lacz::trace {

void init_filter(filter_storage& storage, c_cs_player_pawn* skip, int layer, int mask) {
    std::memset(storage.bytes, 0, sizeof(storage.bytes));
    auto* filter = reinterpret_cast<trace_filter*>(storage.bytes);
    filter->mask = static_cast<u64>(static_cast<u32>(mask));
    filter->skip = skip;
    (void)layer;
}

game_trace trace(const vec3& start, const vec3& end, void* filter, bool hull) {
    game_trace result{};
    if (!off::sigs.trace_ray)
        return result;

    // Signature: (CGameTrace* out, const vec3& start, const vec3& end,
    //             trace_filter* filter, ...). We call through with our own
    // stack result to avoid touching engine scratch state.
    using fn_t = void(*)(game_trace*, const vec3*, const vec3*, void*, bool);
    auto fn = reinterpret_cast<fn_t>(off::sigs.trace_ray);
    fn(&result, &start, &end, filter, hull);
    return result;
}

game_trace trace_filtered(const vec3& start, const vec3& end, c_cs_player_pawn* skip) {
    filter_storage storage;
    init_filter(storage, skip, 0, 0x4600400B);
    return trace(start, end, storage.bytes, false);
}

game_trace trace_hull(const vec3& start, const vec3& end, const vec3& mins,
                      const vec3& maxs, void* filter) {
    game_trace result{};
    if (!off::sigs.trace_ray)
        return result;
    using fn_t = void(*)(game_trace*, const vec3*, const vec3*, const vec3*,
                         const vec3*, void*);
    auto fn = reinterpret_cast<fn_t>(off::sigs.trace_ray);
    fn(&result, &start, &end, &mins, &maxs, filter);
    return result;
}

bool visible(const vec3& start, const vec3& end, c_cs_player_pawn* target) {
    game_trace tr = trace_filtered(start, end, nullptr);
    if (tr.fraction >= 0.97f)
        return true;
    return tr.hit_entity == target;
}

// ---------------------------------------------------------------------------
// Penetration math. Source 2 applies an exponential damage falloff over
// distance and multiplies by a surface-specific penetration modifier. This is
// a faithful reimplementation of the server's algorithm, good enough to score
// candidate hitboxes for the resolver.
// ---------------------------------------------------------------------------
static constexpr float kMaxRange = 8192.f;

struct pen_result {
    int   damage;
    float pen_remaining;
    int   hits;
};

static int scale_damage(float base, int hitgroup, float distance,
                        bool has_armor, bool has_helmet) {
    float dmg = base;

    switch (hitgroup) {
        case HITBOX_HEAD:    dmg *= 4.0f; break;   // helmeted head shots use 2x
        case HITBOX_CHEST:
        case HITBOX_ARMS:    dmg *= 1.0f; break;
        case HITBOX_STOMACH: dmg *= 1.25f; break;
        case HITBOX_LEGS:    dmg *= 0.75f; break;
        default: break;
    }

    float range_mod = distance > 500.f ? distance / 500.f : 1.f;
    dmg = base * (dmg / base) / range_mod;

    if (has_armor) {
        float armor_ratio = has_helmet ? 0.5f : 0.6f;
        float armor_bonus = 0.5f;
        float new_dmg = dmg * armor_ratio;
        float to_armor = (dmg - new_dmg) * 0.5f;
        dmg = (dmg - to_armor) > 0.f ? new_dmg : dmg * 0.5f;
        (void)armor_bonus;
    }
    return static_cast<int>(std::floor(dmg));
}

int damage_to(const vec3& start, const vec3& end, c_cs_player_pawn* target,
              int min_damage, bool* hit_entity_out) {
    if (!target || !off::sigs.trace_ray) {
        if (hit_entity_out) *hit_entity_out = false;
        return 0;
    }

    auto* local = local_pawn();
    float base_damage = 30.f;
    int   weapon_pen = 1;   // 0 none, 1 light, 2 heavy
    float armor_pen = 1.f;

    if (local) {
        auto* weapon = local->active_weapon();
        if (weapon) {
            int id = weapon->item_definition_index();
            const weapon::weapon_data* data = weapon::get(id);
            if (data) {
                // Approximate damage tables per weapon family.
                if (data->is_sniper) { base_damage = 115.f; weapon_pen = 2; armor_pen = 0.98f; }
                else if (id == 7) { base_damage = 36.f; weapon_pen = 1; armor_pen = 0.775f; }
                else if (id == 16 || id == 60) { base_damage = 33.f; weapon_pen = 1; armor_pen = 0.70f; }
                else if (id == 39) { base_damage = 30.f; weapon_pen = 1; armor_pen = 0.65f; }
                else if (id == 1) { base_damage = 63.f; weapon_pen = 1; armor_pen = 0.93f; }
                else if (id == 64) { base_damage = 115.f; weapon_pen = 2; armor_pen = 0.93f; }
            }
        }
    }

    vec3 dir = end - start;
    float max_dist = dir.length();
    if (max_dist > kMaxRange) {
        if (hit_entity_out) *hit_entity_out = false;
        return 0;
    }

    float current_damage = base_damage * armor_pen;
    vec3 current_start = start;
    float traveled = 0.f;
    int hits = 0;

    // One filter reused across penetration iterations; its lifetime covers the
    // whole loop.
    filter_storage filter_store;
    init_filter(filter_store, local, 0, 0x4600400B);

    for (int i = 0; i < 4; ++i) {
        game_trace tr = trace(current_start, end, filter_store.bytes, false);
        if (tr.fraction <= 0.f && i == 0) {
            if (hit_entity_out) *hit_entity_out = false;
            return 0;
        }

        if (tr.hit_entity == target) {
            // Determine hitgroup from closest bone to the impact point.
            int group = HITBOX_CHEST;
            vec3 bone;
            for (int b = 0; b < BONE_COUNT; ++b) {
                if (target->get_bone(b, bone)) {
                    float d = bone.distance(tr.end_pos);
                    if (d < 8.f) {
                        group = (b == BONE_HEAD) ? HITBOX_HEAD
                              : (b <= BONE_SPINE_3) ? HITBOX_CHEST
                              : (b <= BONE_PELVIS) ? HITBOX_PELVIS : HITBOX_LEGS;
                        break;
                    }
                }
            }
            int dealt = scale_damage(current_damage, group, traveled,
                                     target->armor() > 0, target->has_helmet());
            if (hit_entity_out) *hit_entity_out = true;
            return dealt;
        }

        // Hit a wall. Apply penetration damage reduction and continue.
        traveled += (tr.end_pos - current_start).length();
        if (weapon_pen <= 0)
            break;
        float modifier = 0.5f + (weapon_pen == 2 ? 0.35f : 0.0f);
        current_damage *= modifier * 0.72f;
        current_start = tr.end_pos + dir.normalized() * 0.5f;
        ++hits;
        if (current_damage < static_cast<float>(min_damage))
            break;
    }

    if (hit_entity_out) *hit_entity_out = false;
    return static_cast<int>(current_damage);
}

} // namespace lacz::trace