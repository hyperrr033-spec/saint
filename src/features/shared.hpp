#pragma once
#include "features.hpp"
#include "../sdk/math.hpp"

namespace lacz::features::shared {

// Refreshed once per frame. Everything downstream reads these.
struct context {
    c_cs_player_pawn* local = nullptr;
    c_cs_player_controller* local_controller = nullptr;
    ang   view_angles;
    ang   aim_punch;
    vec3  eye_pos;
    vec3  shoot_pos;
    view_matrix matrix;
    int   screen_w = 0;
    int   screen_h = 0;
    float cur_time = 0.f;
    float interval = 0.f;
    bool  in_game = false;
    bool  valid = false;
};

extern context g_ctx;

void update();

// Target collection / scoring. `for_rage` applies wall-penetration scoring.
std::vector<c_cs_player_pawn*> collect_enemies(bool include_dormant = false);
bool  is_enemy(c_cs_player_pawn* pawn);
bool  valid_target(c_cs_player_pawn* pawn);
float fov_to(const vec3& point, const ang& view);
ang   calc_angle(const vec3& from, const vec3& to);
void  normalize(ang& a);

// World-to-screen using the cached matrix.
bool world_to_screen(const vec3& world, vec2& out);

// Aim input application.
void apply_aim(const ang& want, bool silent);
void set_view_angles(const ang& angles);

// Movement helpers.
void stop_movement();
bool key_active(int key);

} // namespace lacz::features::shared