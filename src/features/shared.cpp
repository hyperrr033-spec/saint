#include "../core/log.hpp"
#include "shared.hpp"
#include "../core/memory.hpp"
#include "../core/settings.hpp"
#include "../sdk/offsets.hpp"
#include "../sdk/interfaces.hpp"
#include "../sdk/weapon.hpp"
#include "trace.hpp"
#include <imgui.h>

namespace lacz::features::shared {

context g_ctx{};

bool key_active(int key) {
    if (key == 0)
        return true;
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

void normalize(ang& a) {
    a.normalize();
}

ang calc_angle(const vec3& from, const vec3& to) {
    vec3 delta = to - from;
    ang out;
    float hyp = delta.length_2d();
    out.pitch = lacz::R2D(std::atan2(-delta.z, hyp));
    out.yaw = lacz::R2D(std::atan2(delta.y, delta.x));
    out.roll = 0.f;
    return out;
}

float fov_to(const vec3& point, const ang& view) {
    ang want = calc_angle(g_ctx.eye_pos, point);
    ang delta = want - view;
    normalize(delta);
    return std::sqrt(delta.pitch * delta.pitch + delta.yaw * delta.yaw);
}

bool is_enemy(c_cs_player_pawn* pawn) {
    if (!pawn)
        return false;
    if (g_ctx.local && pawn == g_ctx.local)
        return false;
    int t = pawn->team();
    if (t < 2)
        return false;
    int lt = g_ctx.local ? g_ctx.local->team() : 0;
    if (lt < 2 && g_ctx.local_controller)
        lt = g_ctx.local_controller->team();
    if (lt >= 2 && lt == t)
        return false;
    return true;
}

bool valid_target(c_cs_player_pawn* pawn) {
    if (!pawn)
        return false;
    if (!pawn->alive())
        return false;
    if (pawn->is_dormant())
        return false;
    if (!is_enemy(pawn))
        return false;
    return true;
}

std::vector<c_cs_player_pawn*> collect_enemies(bool include_dormant) {
    std::vector<c_cs_player_pawn*> out;
    for (auto* pawn : all_pawns()) {
        if (!pawn)
            continue;
        if (!pawn->alive())
            continue;
        if (!include_dormant && pawn->is_dormant())
            continue;
        if (!is_enemy(pawn))
            continue;
        out.push_back(pawn);
    }
    return out;
}

void update() {
    // Perf: Present runs at display rate (144Hz+). Entity walks cost
    // thousands of guarded reads. Refresh cheap frame data every call,
    // heavy identity (controller/pawn/services) at ~30Hz.
    static DWORD s_last_heavy = 0;
    static c_cs_player_controller* s_ctrl = nullptr;
    static c_cs_player_pawn* s_pawn = nullptr;
    DWORD now = GetTickCount();
    bool heavy = (now - s_last_heavy) >= 33 || !s_last_heavy;
    if (heavy) {
        s_last_heavy = now;
        s_ctrl = local_controller();
        // Validate cached pawn before re-walking: cheap liveness check.
        if (s_pawn && (!is_valid_entity_ptr(reinterpret_cast<uptr>(s_pawn)) || !s_pawn->alive())) {
            auto* fresh = local_pawn();
            s_pawn = fresh;
        } else if (!s_pawn) {
            s_pawn = local_pawn();
        }
        g_ctx.local_controller = s_ctrl;
        g_ctx.local = s_pawn;
    } else {
        g_ctx.local_controller = s_ctrl;
        g_ctx.local = s_pawn;
    }

    auto* gv = get_global_vars();
    if (gv && mem::is_readable(reinterpret_cast<uptr>(gv), sizeof(global_vars_base))) {
        g_ctx.cur_time = gv->cur_time;
        g_ctx.interval = gv->interval_per_tick;
    }

    g_ctx.screen_w = static_cast<int>(ImGui::GetIO().DisplaySize.x);
    g_ctx.screen_h = static_cast<int>(ImGui::GetIO().DisplaySize.y);

    if (off::sigs.view_matrix && mem::is_readable(off::sigs.view_matrix, sizeof(view_matrix)))
        g_ctx.matrix = mem::read<view_matrix>(off::sigs.view_matrix);

    static int s_log_tick = 0;
    if (heavy && ++s_log_tick % 90 == 0) {
        auto pawns = all_pawns();
        auto enemies = collect_enemies();

        // Bone/eye sanity: a real player should report a non-zero head bone and
        // an eye origin ~64 units above the feet. If head=(0,0,0) the bone path
        // regressed; if eye.z == origin.z the view-offset path regressed.
        vec3 head{}, eye{}, org{};
        if (g_ctx.local && g_ctx.local->alive()) {
            g_ctx.local->get_bone(BONE_HEAD, head);
            eye = g_ctx.local->eye_position();
            org = g_ctx.local->get_abs_origin();
        }

        log::write("tick: pawn=0x%p pawns=%d enemies=%d hp=%d team=%d head=(%.0f,%.0f,%.0f) eye.z=%.0f org.z=%.0f",
                   g_ctx.local,
                   (int)pawns.size(),
                   (int)enemies.size(),
                   g_ctx.local ? g_ctx.local->health() : 0,
                   g_ctx.local ? g_ctx.local->team() : 0,
                   head.x, head.y, head.z,
                   eye.z, org.z);
    }

    if (g_ctx.local && g_ctx.local->alive()) {
        g_ctx.in_game = true;
        g_ctx.valid = true;

        vec3 origin = g_ctx.local->get_abs_origin();
        ang view = g_ctx.local->get_eye_angles();
        g_ctx.view_angles = view;
        if (heavy)
            g_ctx.aim_punch = g_ctx.local->aim_punch();

        float eye_height = 64.f;
        if (heavy) {
            if (auto* movement = g_ctx.local->movement_services()) {
                bool ducking = mem::read<bool>(reinterpret_cast<uptr>(movement) + 0x50, false);
                if (ducking)
                    eye_height = 46.f;
            }
        } else {
            eye_height = (g_ctx.eye_pos.z - origin.z) > 0.5f ? (g_ctx.eye_pos.z - origin.z) : 64.f;
        }
        g_ctx.eye_pos = origin + vec3(0.f, 0.f, eye_height);
        g_ctx.shoot_pos = g_ctx.eye_pos;
    } else if (g_ctx.local_controller) {
        g_ctx.in_game = true;
        g_ctx.valid = false;
    } else {
        g_ctx.in_game = false;
        g_ctx.valid = false;
    }
}

bool world_to_screen(const vec3& world, vec2& out) {
    int sw = g_ctx.screen_w > 0 ? g_ctx.screen_w : static_cast<int>(ImGui::GetIO().DisplaySize.x);
    int sh = g_ctx.screen_h > 0 ? g_ctx.screen_h : static_cast<int>(ImGui::GetIO().DisplaySize.y);
    if (sw <= 0 || sh <= 0) {
        sw = GetSystemMetrics(SM_CXSCREEN);
        sh = GetSystemMetrics(SM_CYSCREEN);
    }
    return g_ctx.matrix.world_to_screen(world, out, sw, sh);
}

// ---------------------------------------------------------------------------
// Aim application.
// ---------------------------------------------------------------------------
static ang g_real_view{};
static bool g_view_saved = false;

void set_view_angles(const ang& angles) {
    if (off::sigs.set_view_angles && g_ctx.local) {
        mem::call<void>(off::sigs.set_view_angles, g_ctx.local, &angles);
    }
    if (g_ctx.local) {
        g_ctx.local->set_eye_angles(angles);
    }
}

void apply_aim(const ang& want, bool silent) {
    ang target = want;
    normalize(target);

    // Remove aim punch so recoil does not pull the shot off.
    target.pitch -= g_ctx.aim_punch.pitch * 2.0f;
    target.yaw -= g_ctx.aim_punch.yaw * 2.0f;
    normalize(target);

    if (silent) {
        set_view_angles(target);
    } else {
        ang current = g_ctx.view_angles;
        ang delta = target - current;
        normalize(delta);

        float smooth = std::max(cfg::g.aimbot.smooth, 1.f);
        ang step = delta / smooth;
        ang next = current + step;
        normalize(next);
        set_view_angles(next);
        g_ctx.view_angles = next;

        float sens_factor = 0.022f * 2.5f;
        int dx = static_cast<int>(step.yaw / sens_factor);
        int dy = static_cast<int>(-step.pitch / sens_factor);
        if (dx != 0 || dy != 0) {
            mouse_event(MOUSEEVENTF_MOVE, dx, dy, 0, 0);
        }
    }
}

void stop_movement() {
    if (!g_ctx.local)
        return;
    // Zero the local velocity through the movement services so accuracy hits
    // its maximum. The engine clamps this the next tick.
    void* services = g_ctx.local->movement_services();
    if (!services)
        return;
    // m_vecVelocity at a stable offset within the movement services.
    mem::write<vec3>(reinterpret_cast<uptr>(services) + 0x4C, vec3{});
}

} // namespace lacz::features::shared