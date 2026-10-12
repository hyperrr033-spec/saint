#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/hooks.hpp"
#include "../sdk/interfaces.hpp"
#include "../sdk/offsets.hpp"
#include "../sdk/schema.hpp"
#include "../core/memory.hpp"
#include "../core/log.hpp"

namespace lacz::features {
using namespace shared;


static target_t g_target{};
static char g_status[64] = "ready";

target_t* current_target() { return &g_target; }

// ---------------------------------------------------------------------------
// CreateMove detour. This is where movement and anti-aim run because the
// usercmd exists only inside this call.
// ---------------------------------------------------------------------------
using create_move_t = bool(*)(void* movement_services, void* cmd);
static create_move_t o_create_move = nullptr;

static bool __fastcall create_move_detour(void* movement_services, void* cmd) {
    __try {
        bool result = o_create_move ? o_create_move(movement_services, cmd) : false;
        if (cmd && g_ctx.valid && cfg::g.antiaim.enabled)
            antiaim::run(cmd);
        if (cmd && g_ctx.valid)
            misc::run_movement(cmd, 0.f);
        return result;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return o_create_move ? o_create_move(movement_services, cmd) : false;
    }
}

// ---------------------------------------------------------------------------
// FrameStageNotify detour: apply skins once per stage change (post data update)
// and refresh the shared context so features see post-update state.
// ---------------------------------------------------------------------------
using frame_stage_t = void(*)(void* client, int stage);
static frame_stage_t o_frame_stage = nullptr;

static void __fastcall frame_stage_detour(void* client, int stage) {
    __try {
        if (stage == 6 && g_ctx.valid && cfg::g.skins.enabled)
            skins::run();

        if (o_frame_stage)
            o_frame_stage(client, stage);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (o_frame_stage)
            o_frame_stage(client, stage);
    }
}

static bool install_hooks() {
    auto& hook_manager = hooks::manager::get();
    bool attempted = false;
    bool all_ok = true;

    if (off::sigs.create_move) {
        attempted = true;
        const bool ok = hook_manager.attach(
            reinterpret_cast<void*>(off::sigs.create_move),
            hooks::as_ptr(&create_move_detour),
            reinterpret_cast<void**>(&o_create_move), "CreateMove");
        all_ok = all_ok && ok;
    } else {
        log::write("CreateMove hook skipped: signature unavailable");
        all_ok = false;
    }

    if (off::sigs.frame_stage) {
        attempted = true;
        const bool ok = hook_manager.attach(
            reinterpret_cast<void*>(off::sigs.frame_stage),
            hooks::as_ptr(&frame_stage_detour),
            reinterpret_cast<void**>(&o_frame_stage), "FrameStageNotify");
        all_ok = all_ok && ok;
    } else {
        log::write("FrameStageNotify hook skipped: signature unavailable");
        all_ok = false;
    }

    if (!attempted)
        log::write("No feature hooks attempted");
    return attempted && all_ok;
}

void initialize() {
    const bool ok = install_hooks();
    snprintf(g_status, sizeof(g_status), "%s",
             ok ? "hooks installed" : "hook install failed");
    log::write("feature hook initialization: %s", g_status);
}

void on_frame() {
    shared::update();

    misc::run();
    chams::run();

    if (cfg::g.skins.enabled)
        skins::run();

    if (cfg::g.aimbot.enabled)
        aimbot::run();

    if (cfg::g.trigger.enabled)
        triggerbot::run();

    if (cfg::g.rage.enabled)
        rage::run();
}

void on_render() {
    if (cfg::g.chams.enabled)
        chams::draw();

    if (cfg::g.esp.enabled)
        esp::draw();
}

void on_render_overlay() {
    // Single overlay pass. The radar reveal (an entity walk) used to run in
    // on_frame *and* again here whenever a text overlay was on, doubling the
    // cost of the most expensive list walk every frame.
    if (cfg::g.esp.radar || cfg::g.misc.water_mark || cfg::g.misc.bomb_timer ||
        cfg::g.misc.spectator_list)
        esp::draw_overlay();
}

const char* status_text() { return g_status; }

const char* build_info() {
    static char info[160];
    snprintf(info, sizeof(info), "trace:%s schema:%s",
             off::sigs.trace_ray ? "ok" : "missing",
             iface::schema_system ? "ok" : "missing");
    return info;
}

} // namespace lacz::features