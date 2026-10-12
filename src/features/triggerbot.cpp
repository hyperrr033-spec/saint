#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "trace.hpp"

namespace lacz::features::triggerbot {

using namespace shared;

static float g_last_shot = 0.f;

void run() {
    if (!g_ctx.valid || !g_ctx.local)
        return;
    if (cfg::g.trigger.on_key && !key_active(cfg::g.trigger.key))
        return;

    // The target under the crosshair, resolved by ray to the aim point.
    ang view = g_ctx.view_angles;
    vec3 dir = view.to_forward();
    vec3 end = g_ctx.shoot_pos + dir * cfg::g.trigger.max_dist;

    void* filter = nullptr;
    trace::filter_storage filter_store;
    trace::init_filter(filter_store, g_ctx.local, 0, 0x4600400B);
    filter = filter_store.bytes;
    trace::game_trace tr = trace::trace(g_ctx.shoot_pos, end, filter, false);
    if (!tr.hit_entity)
        return;

    auto* target = reinterpret_cast<c_cs_player_pawn*>(tr.hit_entity);
    if (!valid_target(target))
        return;

    if (cfg::g.trigger.visible_only) {
        bool hit = false;
        trace::damage_to(g_ctx.shoot_pos, tr.end_pos, target, 0, &hit);
        if (!hit)
            return;
    }

    // Delay gate.
    float now = g_ctx.cur_time;
    float delay = cfg::g.trigger.delay_ms / 1000.f;
    if (now - g_last_shot < delay)
        return;

    // Hitchance: only fire once our spray accuracy is good enough.
    auto* weapon = g_ctx.local->active_weapon();
    if (weapon) {
        float inacc = weapon->inaccuracy();
        float spread = weapon->spread();
        float total = inacc + spread;
        if (total > 0.f) {
            // Convert angular error to a rough hitchance percentage.
            float hit = std::clamp(100.f - total * 250.f, 0.f, 100.f);
            if (hit < cfg::g.trigger.hitchance)
                return;
        }
    }

    g_last_shot = now;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

} // namespace lacz::features::triggerbot