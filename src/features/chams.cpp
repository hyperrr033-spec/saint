#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../render/renderer.hpp"
#include <imgui.h>

namespace lacz::features::chams {

using namespace shared;

static ImU32 to_color(const float c[4], float alpha_scale = 1.0f) {
    int a = std::clamp(static_cast<int>(c[3] * 255.f * alpha_scale), 0, 255);
    return IM_COL32(static_cast<int>(c[0] * 255.f),
                    static_cast<int>(c[1] * 255.f),
                    static_cast<int>(c[2] * 255.f),
                    a);
}

void run() {
    if (!g_ctx.valid || !cfg::g.chams.enabled)
        return;
}

void draw() {
    if (!g_ctx.valid || !cfg::g.chams.enabled)
        return;

    static const int bone_links[][2] = {
        {BONE_HEAD, BONE_NECK},
        {BONE_NECK, BONE_SPINE_3},
        {BONE_SPINE_3, BONE_SPINE_2},
        {BONE_SPINE_2, BONE_SPINE_1},
        {BONE_SPINE_1, BONE_PELVIS},
        {BONE_NECK, BONE_SHOULDER_L},
        {BONE_SHOULDER_L, BONE_ARM_UPPER_L},
        {BONE_ARM_UPPER_L, BONE_ARM_LOWER_L},
        {BONE_ARM_LOWER_L, BONE_HAND_L},
        {BONE_NECK, BONE_SHOULDER_R},
        {BONE_SHOULDER_R, BONE_ARM_UPPER_R},
        {BONE_ARM_UPPER_R, BONE_ARM_LOWER_R},
        {BONE_ARM_LOWER_R, BONE_HAND_R},
        {BONE_PELVIS, BONE_HIP_L},
        {BONE_HIP_L, BONE_KNEE_L},
        {BONE_KNEE_L, BONE_FOOT_L},
        {BONE_PELVIS, BONE_HIP_R},
        {BONE_HIP_R, BONE_KNEE_R},
        {BONE_KNEE_R, BONE_FOOT_R},
    };

    for (auto* pawn : collect_enemies()) {
        if (!pawn || !pawn->alive())
            continue;

        bool visible = pawn->spotted();
        ImU32 base_col = visible ? to_color(cfg::g.chams.vis_color, 0.85f)
                                 : to_color(cfg::g.chams.hidden_color, 0.75f);
        ImU32 glow_col = visible ? to_color(cfg::g.chams.vis_color, 0.35f)
                                 : to_color(cfg::g.chams.hidden_color, 0.25f);

        std::array<vec3, BONE_COUNT> bones{};
        bool has_bones = pawn->get_bones(bones);

        if (has_bones) {
            // Render thick connecting limb capsules for chams effect
            for (auto& link : bone_links) {
                vec2 s1{}, s2{};
                if (world_to_screen(bones[link[0]], s1) && world_to_screen(bones[link[1]], s2)) {
                    if (cfg::g.chams.model_glow)
                        render::line(s1, s2, glow_col, 8.0f);
                    render::line(s1, s2, base_col, 4.5f);
                }
            }

            // Render joint spheres
            for (int i = 0; i < BONE_COUNT; ++i) {
                if (bones[i].is_zero()) continue;
                vec2 s{};
                if (world_to_screen(bones[i], s)) {
                    float r = (i == BONE_HEAD) ? 7.0f : (i == BONE_SPINE_2 || i == BONE_SPINE_3) ? 6.0f : 4.0f;
                    if (cfg::g.chams.model_glow)
                        render::circle_filled(s, r + 4.0f, glow_col, 16);
                    render::circle_filled(s, r, base_col, 16);
                }
            }
        } else {
            // Fallback capsule silhouette when bones aren't ready
            vec3 origin = pawn->get_abs_origin();
            vec3 top = origin + vec3(0.f, 0.f, 72.f);
            vec2 s_origin{}, s_top{};
            if (world_to_screen(origin, s_origin) && world_to_screen(top, s_top)) {
                float h = std::abs(s_origin.y - s_top.y);
                float w = h * 0.40f;
                float cx = (s_origin.x + s_top.x) * 0.5f;
                float ty = std::min(s_origin.y, s_top.y);
                float by = std::max(s_origin.y, s_top.y);
                vec2 min_pt(cx - w * 0.5f, ty);
                vec2 max_pt(cx + w * 0.5f, by);
                render::rect_filled(min_pt, max_pt, glow_col, 8.f);
                render::rect(min_pt, max_pt, base_col, 2.f, 8.f);
            }
        }

        // Backtrack / Desync Ghost Shadow
        if (cfg::g.chams.desync || cfg::g.chams.backtrack) {
            vec3 origin = pawn->get_abs_origin();
            vec2 ghost_pos{};
            if (world_to_screen(origin + vec3(4.f, 4.f, 0.f), ghost_pos)) {
                ImU32 ghost_col = to_color(cfg::g.chams.desync_color, 0.4f);
                render::circle_filled(ghost_pos, 5.f, ghost_col, 12);
            }
        }
    }
}

} // namespace lacz::features::chams