#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../sdk/interfaces.hpp"

namespace lacz::features::antiaim {

using namespace shared;

// ---------------------------------------------------------------------------
// Anti-aim works on the usercmd that CreateMove hands the movement services.
// We rewrite the view angles the server receives while leaving the local view
// untouched (visual desync).
// ---------------------------------------------------------------------------
struct usercmd {
    char pad0[0x8];
    int    command_number;
    int    tick_count;
    char pad1[0x18];
    ang    view_angles;
    vec3   aim_direction;
    char pad2[0x4];
    int    buttons;
    char pad3[0xC];
    float  forwardmove;
    float  sidemove;
    float  upmove;
    char pad4[0x10];
    int    impulse;
    char pad5[0x4];
    int    weaponselect;
    char pad6[0x8];
    int    random_seed;
    int    mousedx;
    int    mousedy;
    bool   has_been_predicted;
};

static float g_jitter_offset = 0.f;
static int   g_jitter_tick = 0;

static float random_float(float min, float max) {
    float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return min + t * (max - min);
}

static float base_yaw() {
    switch (cfg::g.antiaim.yaw_base) {
        case 0: return g_ctx.view_angles.yaw;
        case 1: {
            auto* target = current_target()->pawn;
            if (target) {
                ang a = calc_angle(g_ctx.eye_pos, target->get_abs_origin());
                return a.yaw;
            }
            return g_ctx.view_angles.yaw;
        }
        case 2: return 0.f;
        case 3: return g_ctx.view_angles.yaw + 180.f;
    }
    return g_ctx.view_angles.yaw;
}

// Compute the pitch the server will see.
static float anti_pitch() {
    switch (cfg::g.antiaim.pitch_mode) {
        case 0: return 89.f;
        case 1: return -89.f;
        case 2: return 0.f;
        case 3: return (rand() % 2) ? 89.f : -89.f;
        case 4: return 88.f;   // fake pitch the engine clamps on the client
    }
    return 89.f;
}

void run(void* cmd_ptr) {
    if (!g_ctx.valid || !g_ctx.local)
        return;
    if (!cmd_ptr)
        return;

    auto* cmd = reinterpret_cast<usercmd*>(cmd_ptr);

    // Don't fight the aimbot on the tick we are firing.
    bool shooting = (cmd->buttons & 1) != 0;
    if (shooting && cfg::g.antiaim.on_shot)
        return;

    ang out = cmd->view_angles;

    if (cfg::g.antiaim.pitch_enabled)
        out.pitch = anti_pitch();

    float yaw = base_yaw() + cfg::g.antiaim.yaw_add;

    switch (cfg::g.antiaim.jitter) {
        case 1:
            yaw += (g_jitter_tick % 2) ? 180.f : 0.f;
            break;
        case 2:
            if (++g_jitter_tick % 2 == 0)
                g_jitter_offset = random_float(-180.f, 180.f);
            yaw += g_jitter_offset;
            break;
        case 3:
            yaw += std::fmod(static_cast<float>(g_ctx.cur_time) * 720.f, 360.f);
            break;
        default: break;
    }

    if (cfg::g.antiaim.desync) {
        // Alternate the body yaw by the desync amount each tick so the server
        // sees a body that does not match the head.
        float amount = std::clamp(cfg::g.antiaim.desync_amount, 0.f, 60.f);
        bool flip = (g_jitter_tick % 2) == 0;
        yaw += flip ? amount : -amount;
    }

    out.yaw = yaw;
    out.roll = 0.f;
    normalize(out);
    cmd->view_angles = out;

    // Lower body yaw: drive the pawn's own yaw offset so the legs lag behind.
    if (cfg::g.antiaim.lower_body) {
        static float lby = 0.f;
        float delta = std::fabs(out.yaw - lby);
        if (delta > 35.f) {
            lby = out.yaw;
            // m_flLowerBodyYawTarget lives on the pawn.
            mem::write<float>(reinterpret_cast<uptr>(g_ctx.local) + 0x3A40, out.yaw);
        }
    }
}

} // namespace lacz::features::antiaim