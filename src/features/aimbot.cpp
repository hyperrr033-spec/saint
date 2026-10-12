#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../sdk/weapon.hpp"
#include "trace.hpp"

namespace lacz::features::aimbot {

using namespace shared;

// Map the menu hitbox selection to skeleton bones: head, neck, chest, pelvis,
// and a nearest-point search.
static vec3 hitbox_point(c_cs_player_pawn* pawn, int hitbox) {
    std::array<vec3, BONE_COUNT> bones{};
    if (!pawn->get_bones(bones))
        return pawn->get_abs_origin() + vec3(0.f, 0.f, 64.f);

    switch (hitbox) {
        case 0: return bones[BONE_HEAD];
        case 1: return bones[BONE_NECK];
        case 2: return bones[BONE_SPINE_3];
        case 3: return bones[BONE_PELVIS];
        case 4: { // nearest point to crosshair among the above
            vec3 best = bones[BONE_SPINE_3];
            float best_fov = 1e9f;
            const int candidates[] = {BONE_HEAD, BONE_NECK, BONE_SPINE_2, BONE_PELVIS};
            for (int c : candidates) {
                float f = fov_to(bones[c], g_ctx.view_angles);
                if (f < best_fov) { best_fov = f; best = bones[c]; }
            }
            return best;
        }
    }
    return bones[BONE_HEAD];
}

static float threat_score(c_cs_player_pawn* pawn) {
    // Cheap approximation: closer + scoped sniper = more threatening.
    float dist = pawn->get_abs_origin().distance(g_ctx.eye_pos);
    float score = 300.f - dist;
    if (pawn->scoped())
        score += 120.f;
    return score;
}

static c_cs_player_pawn* select_target(vec3& out_point, int& out_hitbox) {
    auto enemies = collect_enemies();
    c_cs_player_pawn* best = nullptr;
    float best_score = -1e9f;
    vec3 best_point;
    int best_hitbox = 0;

    for (auto* pawn : enemies) {
        float dist = pawn->get_abs_origin().distance(g_ctx.eye_pos);
        if (dist > cfg::g.aimbot.max_dist)
            continue;

        vec3 point = hitbox_point(pawn, cfg::g.aimbot.hitbox);
        if (point.is_zero())
            continue;

        float fov = fov_to(point, g_ctx.view_angles);
        if (fov > cfg::g.aimbot.fov)
            continue;

        // Optional visibility gate.
        if (cfg::g.aimbot.visible_check) {
            bool vis = pawn->spotted();
            if (!vis && off::sigs.trace_ray) {
                bool hit = false;
                trace::damage_to(g_ctx.shoot_pos, point, pawn, 0, &hit);
                vis = hit;
            } else if (!off::sigs.trace_ray) {
                vis = true;
            }
            if (!vis)
                continue;
        }

        float score = 0.f;
        switch (cfg::g.aimbot.priority) {
            case 0: score = -dist; break;
            case 1: score = -fov; break;
            case 2: score = static_cast<float>(pawn->health()); break;
            case 3: score = threat_score(pawn); break;
            default: score = -dist; break;
        }
        if (score > best_score) {
            best_score = score;
            best = pawn;
            best_point = point;
            best_hitbox = cfg::g.aimbot.hitbox;
        }
    }

    out_point = best_point;
    out_hitbox = best_hitbox;
    return best;
}

void run() {
    if (!g_ctx.valid || !g_ctx.local)
        return;
    if (!key_active(cfg::g.aimbot.key))
        return;

    vec3 point;
    int hitbox;
    c_cs_player_pawn* target = select_target(point, hitbox);
    if (!target)
        return;

    auto* shared_target = current_target();
    shared_target->pawn = target;
    shared_target->point = point;
    shared_target->hitbox = hitbox;
    shared_target->visible = true;

    if (cfg::g.aimbot.auto_scope && g_ctx.local->scoped() == false) {
        // Scope via the secondary attack path is handled by the game if we
        // simply request it; leave the actual input to auto_fire below.
    }

    if (cfg::g.aimbot.auto_stop)
        stop_movement();

    ang want = calc_angle(g_ctx.shoot_pos, point);
    apply_aim(want, cfg::g.aimbot.silent);

    if (cfg::g.aimbot.auto_fire) {
        // Fire through the input system's attack button.
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        SendInput(1, &input, sizeof(INPUT));
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}

} // namespace lacz::features::aimbot