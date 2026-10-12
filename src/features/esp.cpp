#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../render/renderer.hpp"
#include "../sdk/weapon.hpp"
#include "../sdk/offsets.hpp"
#include <imgui.h>
#include <unordered_map>

namespace lacz::features::esp {

using namespace shared;

// Convert a settings float color array into ImU32.
static ImU32 gui_color(const float color[4]) {
    return IM_COL32(static_cast<int>(color[0] * 255), static_cast<int>(color[1] * 255),
                    static_cast<int>(color[2] * 255), static_cast<int>(color[3] * 255));
}

// Player bounding box from bone extremes, projected.
struct screen_box {
    vec2 min;
    vec2 max;
    bool valid = false;
};

static bool build_box(c_cs_player_pawn* pawn, screen_box& out) {
    if (!pawn)
        return false;

    vec3 origin = pawn->get_abs_origin();
    if (origin.is_zero())
        return false;

    vec3 top = origin + vec3(0.f, 0.f, 72.f);
    vec2 s_origin{}, s_top{};
    if (!world_to_screen(origin, s_origin) || !world_to_screen(top, s_top))
        return false;

    float h = std::abs(s_origin.y - s_top.y);
    if (h < 2.f || h > 4000.f)
        return false;

    float w = h * 0.45f;
    float center_x = (s_origin.x + s_top.x) * 0.5f;
    float top_y = std::min(s_origin.y, s_top.y);
    float bot_y = std::max(s_origin.y, s_top.y);

    out.min = { center_x - w * 0.5f, top_y };
    out.max = { center_x + w * 0.5f, bot_y };
    out.valid = true;
    return true;
}

static void draw_box(const screen_box& box, ImU32 color) {
    switch (cfg::g.esp.box_style) {
        case 0: // full
            render::rect(box.min, box.max, color, 1.4f);
            break;
        case 1: // corner
            render::corner_box(box.min, box.max, color, 2.f, 0.25f);
            break;
        case 2: { // filled
            ImU32 fill = (color & 0x00FFFFFF) | (40 << 24);
            render::rect_filled(box.min, box.max, fill, 2.f);
            render::rect(box.min, box.max, color, 1.2f);
            break;
        }
        case 3: { // 3d wireframe-ish
            render::rect(box.min, box.max, color, 1.2f);
            float inset = 4.f;
            render::rect(vec2(box.min.x + inset, box.min.y + inset),
                         vec2(box.max.x - inset, box.max.y - inset),
                         (color & 0x00FFFFFF) | (90 << 24), 1.f);
            break;
        }
    }
}

static void draw_health_bar(const screen_box& box, int health, int armor) {
    if (!cfg::g.esp.health_bar)
        return;
    float w = 4.f;
    vec2 bar_min(box.min.x - w - 4.f, box.min.y);
    vec2 bar_max(box.min.x - 4.f, box.max.y);
    float h = bar_max.y - bar_min.y;
    float frac = std::clamp(health / 100.f, 0.f, 1.f);

    render::rect_filled(bar_min, bar_max, IM_COL32(15, 15, 20, 200), 2.f);
    vec2 fill_min(bar_min.x, bar_max.y - h * frac);
    render::rect_filled(fill_min, bar_max, render::health_color(health), 2.f);
    if (armor > 0) {
        float af = std::clamp(armor / 100.f, 0.f, 1.f);
        vec2 a_min(bar_max.x + 2.f, bar_max.y - h * af);
        render::rect_filled(a_min, vec2(bar_max.x + 5.f, bar_max.y),
                            IM_COL32(90, 150, 255, 220), 2.f);
    }
}

static void draw_name(c_cs_player_pawn* pawn, const screen_box& box,
                       const std::unordered_map<uptr, const char*>& names) {
    if (!cfg::g.esp.name)
        return;
    // Controller->name map built once per draw() — the old per-pawn
    // all_controllers() walk was O(n^2) entity walks per frame (the menu lag).
    const char* name = "player";
    auto it = names.find(reinterpret_cast<uptr>(pawn));
    if (it != names.end() && it->second && it->second[0])
        name = it->second;
    vec2 size = render::text_size(name);
    render::text_shadow(vec2((box.min.x + box.max.x) * 0.5f - size.x * 0.5f,
                             box.min.y - 16.f),
                        IM_COL32(235, 235, 242, 255), "%s", name);
}

static void draw_skeleton(c_cs_player_pawn* pawn, ImU32 color) {
    std::array<vec3, BONE_COUNT> bones{};
    if (!pawn->get_bones(bones))
        return;

    static const int links[][2] = {
        {BONE_HEAD, BONE_NECK}, {BONE_NECK, BONE_SPINE_3},
        {BONE_SPINE_3, BONE_SPINE_2}, {BONE_SPINE_2, BONE_SPINE_1},
        {BONE_SPINE_1, BONE_PELVIS},
        {BONE_NECK, BONE_SHOULDER_L}, {BONE_SHOULDER_L, BONE_ARM_UPPER_L},
        {BONE_ARM_UPPER_L, BONE_ARM_LOWER_L}, {BONE_ARM_LOWER_L, BONE_HAND_L},
        {BONE_NECK, BONE_SHOULDER_R}, {BONE_SHOULDER_R, BONE_ARM_UPPER_R},
        {BONE_ARM_UPPER_R, BONE_ARM_LOWER_R}, {BONE_ARM_LOWER_R, BONE_HAND_R},
        {BONE_PELVIS, BONE_HIP_L}, {BONE_HIP_L, BONE_KNEE_L},
        {BONE_KNEE_L, BONE_FOOT_L},
        {BONE_PELVIS, BONE_HIP_R}, {BONE_HIP_R, BONE_KNEE_R},
        {BONE_KNEE_R, BONE_FOOT_R},
    };

    for (auto& l : links) {
        vec2 a, b;
        if (world_to_screen(bones[l[0]], a) && world_to_screen(bones[l[1]], b))
            render::line(a, b, color, 1.6f);
    }
    // Head marker.
    vec2 head;
    if (world_to_screen(bones[BONE_HEAD], head))
        render::circle(head, 3.f, color, 12, 1.4f);
}

void draw() {
    if (!g_ctx.valid && !g_ctx.in_game)
        return;

    // One controller sweep per frame → pawn->name map. Every draw_name /
    // weapon / distance call below is then a hash lookup, not a walk.
    std::unordered_map<uptr, const char*> names;
    if (cfg::g.esp.name) {
        for (auto* ctrl : all_controllers()) {
            if (!ctrl)
                continue;
            auto* p = ctrl->pawn();
            if (p)
                names[reinterpret_cast<uptr>(p)] = ctrl->name();
        }
    }

    auto pawns = all_pawns();
    for (auto* pawn : pawns) {
        if (!pawn || !pawn->alive())
            continue;
        if (g_ctx.local && pawn == g_ctx.local)
            continue;

        bool enemy = is_enemy(pawn);

        float dist = pawn->get_abs_origin().distance(g_ctx.eye_pos) / 16.f;
        if (dist > cfg::g.esp.max_dist && cfg::g.esp.max_dist > 0.f)
            continue;

        bool visible = pawn->spotted();

        screen_box box;
        if (!build_box(pawn, box))
            continue;

        ImU32 color = enemy ? (visible ? gui_color(cfg::g.esp.vis_color)
                                       : gui_color(cfg::g.esp.hidden_color))
                            : IM_COL32(90, 170, 255, 230); // Teammate cyan/blue

        if (cfg::g.esp.box)
            draw_box(box, color);

        if (cfg::g.esp.snapline) {
            vec2 origin;
            float sw = ImGui::GetIO().DisplaySize.x;
            float sh = ImGui::GetIO().DisplaySize.y;
            switch (cfg::g.esp.snapline_origin) {
                case 0: origin = {sw * 0.5f, sh}; break;
                case 1: origin = {sw * 0.5f, sh * 0.5f}; break;
                case 2: origin = {sw * 0.5f, sh * 0.5f}; break;
            }
            vec2 target = {(box.min.x + box.max.x) * 0.5f, box.max.y};
            render::line(origin, target, color, 1.2f);
        }

        draw_health_bar(box, pawn->health() > 0 ? pawn->health() : 100, pawn->armor());
        draw_name(pawn, box, names);

        if (cfg::g.esp.weapon) {
            auto* weapon = pawn->active_weapon();
            if (weapon) {
                const char* wn = weapon::name(weapon->item_definition_index());
                vec2 size = render::text_size(wn);
                render::text_shadow(vec2((box.min.x + box.max.x) * 0.5f - size.x * 0.5f,
                                         box.max.y + 2.f),
                                    IM_COL32(220, 220, 228, 255), "%s", wn);
            }
        }

        if (cfg::g.esp.distance) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.0fm", dist);
            vec2 size = render::text_size(buf);
            render::text_shadow(vec2((box.min.x + box.max.x) * 0.5f - size.x * 0.5f,
                                     box.max.y + 14.f),
                                IM_COL32(200, 200, 210, 255), "%s", buf);
        }

        if (cfg::g.esp.skeleton)
            draw_skeleton(pawn, color);
    }

    // Sound ESP: pulsing rings where enemies recently made noise.
    if (cfg::g.esp.sound) {
        // Best-effort: ring at the last known eye position.
        for (auto* pawn : collect_enemies(true)) {
            vec2 s;
            if (!world_to_screen(pawn->get_abs_origin() + vec3(0, 0, 64), s))
                continue;
            float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.f);
            render::circle(s, 10.f + pulse * 8.f,
                           IM_COL32(255, 200, 60, static_cast<int>(pulse * 140) + 40),
                           24, 1.6f);
        }
    }
}

void draw_overlay() {
    // Radar reveal: mark enemies spotted so the game's radar shows them.
    if (cfg::g.esp.radar) {
        for (auto* pawn : collect_enemies(true)) {
            if (!pawn->is_dormant())
                pawn->set_spotted(true);
        }
    }

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float sw = ImGui::GetIO().DisplaySize.x;

    if (cfg::g.misc.water_mark) {
        char buf[128];
        snprintf(buf, sizeof(buf), "LaczDLC %s", lacz::kVersion);
        vec2 size = render::text_size(buf);
        vec2 pos(sw - size.x - 20.f, 16.f);
        render::rect_filled(vec2(pos.x - 10.f, pos.y - 6.f),
                            vec2(pos.x + size.x + 10.f, pos.y + size.y + 6.f),
                            IM_COL32(12, 12, 18, 200), 6.f);
        render::rect(vec2(pos.x - 10.f, pos.y - 6.f),
                     vec2(pos.x + size.x + 10.f, pos.y + size.y + 6.f),
                     render::accent(0.f), 6.f, 1.4f);
        render::text(pos, IM_COL32(240, 240, 248, 255), "%s", buf);
    }

    if (cfg::g.misc.bomb_timer) {
        for (int i = 1; i <= 64; ++i) {
            auto* ent = entity_by_index(i);
            if (!ent)
                continue;
            bool ticking = mem::read<bool>(reinterpret_cast<uptr>(ent) + off::netvars.m_bBombTicking, false);
            if (!ticking)
                continue;
            float blow = mem::read<float>(reinterpret_cast<uptr>(ent) + off::netvars.m_flC4Blow, 0.f);
            float remaining = blow - g_ctx.cur_time;
            if (remaining <= 0.f)
                continue;
            char buf[32];
            snprintf(buf, sizeof(buf), "C4  %.1fs", remaining);
            vec2 size = render::text_size(buf);
            vec2 pos(sw - size.x - 20.f, 44.f);
            render::text_shadow(pos, IM_COL32(255, 90, 90, 255), "%s", buf);
        }
    }

    if (cfg::g.misc.spectator_list) {
        int y = 100;
        for (auto* ctrl : all_controllers()) {
            if (!ctrl)
                continue;
            if (ctrl->pawn() && ctrl->pawn()->alive())
                continue;
            const char* name = ctrl->name();
            if (!name || !*name)
                continue;
            render::text_shadow(vec2(20.f, static_cast<float>(y)), IM_COL32(200,200,210,255),
                                "spectating: %s", name);
            y += 16;
        }
    }
}

} // namespace lacz::features::esp