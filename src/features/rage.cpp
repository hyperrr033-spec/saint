#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../sdk/weapon.hpp"
#include "trace.hpp"

namespace lacz::features::rage {

using namespace shared;

// ---------------------------------------------------------------------------
// Hitbox candidate model. Each candidate carries a bone, a nominal hitgroup and
// a multiplier used to estimate post-penetration damage.
// ---------------------------------------------------------------------------
struct candidate {
    int   bone;
    int   hitbox;
    int   nominal_damage;
};

static std::vector<candidate> build_candidates(int mask) {
    std::vector<candidate> out;
    // Head (bit 0): highest damage, smallest.
    if (mask & 1)
        out.push_back({BONE_HEAD, HITBOX_HEAD, 100});
    // Body (bit 1).
    if (mask & 2) {
        out.push_back({BONE_SPINE_2, HITBOX_CHEST, 60});
        out.push_back({BONE_SPINE_1, HITBOX_STOMACH, 55});
        out.push_back({BONE_PELVIS, HITBOX_PELVIS, 50});
    }
    // Legs (bit 2).
    if (mask & 4)
        out.push_back({BONE_KNEE_L, HITBOX_LEGS, 40});
    // Arms (bit 3).
    if (mask & 8)
        out.push_back({BONE_ARM_LOWER_L, HITBOX_ARMS, 35});
    // Neck (bit 4).
    if (mask & 16)
        out.push_back({BONE_NECK, HITBOX_NECK, 80});
    return out;
}

struct scored {
    c_cs_player_pawn* pawn = nullptr;
    vec3  point;
    int   damage = 0;
    float hitchance = 0.f;
    float distance = 0.f;
    bool  visible = false;
};

// Estimate hitchance from current weapon inaccuracy at the given distance.
static float estimate_hitchance(c_cs_player_pawn* local, c_base_weapon* weapon,
                                float distance) {
    if (!local || !weapon)
        return 100.f;
    if (cfg::g.rage.no_spread)
        return 100.f;

    float inacc = weapon->inaccuracy();
    float spread = weapon->spread();
    float total = inacc + spread;
    if (total <= 0.f)
        return 100.f;
    // Angular error scaled by range; convert to a 0..100 confidence.
    float error_dist = std::tan(total) * distance;
    float hit = std::clamp(100.f - error_dist * 22.f, 0.f, 100.f);
    return hit;
}

static bool select_best(scored& best) {
    auto enemies = collect_enemies();
    auto* local = g_ctx.local;
    auto* weapon = local ? local->active_weapon() : nullptr;

    bool found = false;
    float best_score = -1e9f;

    for (auto* pawn : enemies) {
        float distance = pawn->get_abs_origin().distance(g_ctx.eye_pos);
        if (distance > 8192.f)
            continue;

        std::array<vec3, BONE_COUNT> bones{};
        if (!pawn->get_bones(bones))
            continue;

        auto candidates = build_candidates(cfg::g.rage.hitboxes);
        for (auto& c : candidates) {
            vec3 point = bones[c.bone];
            if (point.is_zero())
                continue;

            float fov = fov_to(point, g_ctx.view_angles);
            if (fov > cfg::g.rage.fov)
                continue;

            int damage = 0;
            bool hit = false;

            if (cfg::g.rage.auto_wall) {
                damage = trace::damage_to(g_ctx.shoot_pos, point, pawn,
                                          cfg::g.rage.min_damage, &hit);
            } else {
                trace::game_trace tr = trace::trace_filtered(g_ctx.shoot_pos, point, local);
                hit = (tr.hit_entity == pawn);
                damage = hit ? c.nominal_damage : 0;
            }

            if (!hit || damage < cfg::g.rage.min_damage)
                continue;

            float hitchance = estimate_hitchance(local, weapon, distance);
            if (hitchance < cfg::g.rage.min_hitchance)
                continue;

            // Score: prefer head and high damage, penalize distance and fov.
            float score = static_cast<float>(damage) * 2.f
                        - distance * 0.05f
                        - fov * 0.5f;
            if (c.bone == BONE_HEAD && cfg::g.rage.prefer_head)
                score += 60.f;
            if (cfg::g.rage.force_safe_point && c.bone == BONE_HEAD)
                score -= 15.f;   // safe point slightly prefers larger volumes

            if (score > best_score) {
                best_score = score;
                best.pawn = pawn;
                best.point = point;
                best.damage = damage;
                best.hitchance = hitchance;
                best.distance = distance;
                best.visible = hit;
                found = true;
            }
        }

        // Early-out: if a perfect headshot is available, take it.
        if (found && cfg::g.rage.prefer_head && best.damage >= 100 &&
            best.point.distance(g_ctx.eye_pos) < 1000.f)
            break;
    }
    return found;
}

// ---------------------------------------------------------------------------
// Accuracy overrides. These are written straight into the weapon so the
// engine's spread/recoil math returns zero for the coming shot.
// ---------------------------------------------------------------------------
void apply_accuracy() {
    if (!g_ctx.local)
        return;
    auto* weapon = g_ctx.local->active_weapon();
    if (!weapon)
        return;

    if (cfg::g.rage.no_spread) {
        // m_flSpread / m_flInaccuracy are recomputed each tick from fields we
        // can zero: standing + movetype + duck amounts. Zero the punch and
        // clear the "shots fired" accumulator so recovery is instant.
        mem::write<int>(reinterpret_cast<uptr>(g_ctx.local) + off::netvars.m_iShotsFired, 0);
    }
    if (cfg::g.rage.no_recoil) {
        mem::write<ang>(reinterpret_cast<uptr>(g_ctx.local) + off::netvars.m_aimPunchAngle, ang{});
    }
}

void run() {
    if (!g_ctx.valid || !g_ctx.local)
        return;
    if (!key_active(cfg::g.rage.key) && !cfg::g.rage.auto_fire)
        return;

    apply_accuracy();

    scored best;
    if (!select_best(best))
        return;

    auto* shared_target = current_target();
    shared_target->pawn = best.pawn;
    shared_target->point = best.point;
    shared_target->damage = best.damage;
    shared_target->distance = best.distance;
    shared_target->visible = best.visible;

    if (cfg::g.rage.auto_stop)
        stop_movement();

    if (cfg::g.rage.auto_scope && !g_ctx.local->scoped()) {
        // Request scope; on snipers this is a secondary attack.
    }

    ang want = calc_angle(g_ctx.shoot_pos, best.point);
    apply_aim(want, cfg::g.rage.silent);

    if (cfg::g.rage.auto_fire) {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}

} // namespace lacz::features::rage