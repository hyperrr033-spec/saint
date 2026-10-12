#include "features.hpp"
#include "shared.hpp"
#include "trace.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../sdk/interfaces.hpp"
#include "../sdk/offsets.hpp"

namespace lacz::features::misc {

using namespace shared;

// ---------------------------------------------------------------------------
// Movement is driven by the usercmd the movement services build each tick. We
// keep a shadow copy of the last command so stateful actions (bhop, strafe)
// can read previous inputs.
// ---------------------------------------------------------------------------
struct usercmd {
    char pad0[0x8];
    int    command_number;
    int    tick_count;
    char pad1[0x18];
    ang    view_angles;
    vec3   aim_direction;
    int    buttons;
    float  forwardmove;
    float  sidemove;
    float  upmove;
};

static float forward_length(const vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

// Rotation of a vector by a yaw angle.
static vec3 rotate_yaw(const vec3& v, float yaw_deg) {
    float rad = D2R(yaw_deg);
    float c = std::cos(rad), s = std::sin(rad);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// Best strafe direction based on current velocity and desired movement.
static void auto_strafe(usercmd& cmd, float& sidemove, float& forwardmove) {
    vec3 velocity = mem::read<vec3>(reinterpret_cast<uptr>(g_ctx.local) + 0x3D8);
    float speed = forward_length(velocity);
    if (speed < 1.f)
        return;

    // Desired movement direction from the joystick input.
    vec3 wish{};
    float dir = std::atan2(cmd.sidemove, cmd.forwardmove) * 180.f / PI;

    // Rotate movement to be perpendicular to velocity for max gain.
    float vel_angle = std::atan2(velocity.y, velocity.x) * 180.f / PI;
    float ideal = vel_angle + 90.f;
    if (cmd.sidemove < 0.f)
        ideal -= 180.f;

    // Only rotate while airborne.
    if (!g_ctx.local->on_ground()) {
        cmd.sidemove = std::cos(D2R(ideal - cmd.view_angles.yaw)) * 450.f;
        cmd.forwardmove = std::sin(D2R(ideal - cmd.view_angles.yaw)) * 450.f;
    }
    (void)dir;
    (void)wish;
    (void)sidemove;
    (void)forwardmove;
}

// Called from the CreateMove hook with the command and its input frame time.
void run_movement(void* cmd_ptr, float) {
    if (!cmd_ptr || !g_ctx.valid || !g_ctx.local)
        return;
    auto* cmd = reinterpret_cast<usercmd*>(cmd_ptr);

    // Bunny hop.
    if (cfg::g.misc.bhop) {
        static bool was_ground = false;
        bool ground = g_ctx.local->on_ground();
        if (ground && !was_ground) {
            cmd->buttons |= 2;   // IN_JUMP
        } else if (ground) {
            cmd->buttons |= 2;
        } else {
            cmd->buttons &= ~2;
        }
        was_ground = ground;
    }

    if (cfg::g.misc.auto_strafe) {
        float f = cmd->forwardmove, s = cmd->sidemove;
        auto_strafe(*cmd, s, f);
    }

    if (cfg::g.misc.fast_stop) {
        // Counteract current velocity when no movement keys are held.
        vec3 velocity = mem::read<vec3>(reinterpret_cast<uptr>(g_ctx.local) + 0x3D8);
        bool moving = (cmd->buttons & (8 | 16 | 512 | 1024)) != 0; // WASD
        if (!moving && forward_length(velocity) > 20.f) {
            vec3 vel_dir = velocity.normalized();
            // Convert world velocity to local angles and invert it.
            float vel_yaw = std::atan2(vel_dir.y, vel_dir.x) * 180.f / PI;
            float rel = vel_yaw - cmd->view_angles.yaw;
            cmd->forwardmove = -std::cos(D2R(rel)) * 450.f;
            cmd->sidemove = -std::sin(D2R(rel)) * 450.f;
        }
    }

    if (cfg::g.misc.edge_jump) {
        // Jump when about to walk off a ledge: on ground, no jump held, and no
        // ground entity underneath after a short trace.
        if (g_ctx.local->on_ground() && !(cmd->buttons & 2)) {
            vec3 origin = g_ctx.local->get_abs_origin();
            vec3 ahead = origin + rotate_yaw(cmd->view_angles.to_forward() * 24.f, 0.f);
            ahead.z = origin.z - 2.f;
            void* filter = nullptr;
            trace::filter_storage filter_store;
            trace::init_filter(filter_store, g_ctx.local, 0, 0x4600400B);
            filter = filter_store.bytes;
            auto tr = trace::trace(origin + vec3(0, 0, 8), ahead, filter, false);
            if (tr.fraction >= 1.f)
                cmd->buttons |= 2;
        }
    }

    if (cfg::g.misc.jump_bug) {
        // Jump the instant before landing to preserve the duck crouch boost.
        static int delay = 0;
        if (g_ctx.local->on_ground()) {
            if (delay == 0)
                cmd->buttons |= 4; // IN_DUCK
            if (delay++ > 2 && delay < 8)
                cmd->buttons |= 2; // IN_JUMP
        } else {
            delay = 0;
        }
    }
}

void run() {
    if (!g_ctx.valid || !g_ctx.local)
        return;

    // No Flash
    if (cfg::g.misc.no_flash) {
        float flash = mem::read<float>(reinterpret_cast<uptr>(g_ctx.local) + off::netvars.m_flFlashDuration, 0.f);
        if (flash > 0.f) {
            mem::write<float>(reinterpret_cast<uptr>(g_ctx.local) + off::netvars.m_flFlashDuration, 0.f);
            if (off::netvars.m_flFlashMaxAlpha)
                mem::write<float>(reinterpret_cast<uptr>(g_ctx.local) + off::netvars.m_flFlashMaxAlpha, 0.f);
        }
    }

    // FOV Changer
    if (cfg::g.misc.fov_changer > 0) {
        if (off::netvars.m_iFOV)
            mem::write<int>(reinterpret_cast<uptr>(g_ctx.local) + off::netvars.m_iFOV, cfg::g.misc.fov_changer);
    }

    // Standalone Bunny Hop fallback (spacebar active)
    if (cfg::g.misc.bhop && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
        if (g_ctx.local->on_ground()) {
            keybd_event(VK_SPACE, 0, 0, 0);
            keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);
        }
    }
}

} // namespace lacz::features::misc