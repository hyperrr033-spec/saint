#pragma once
#include "common.hpp"

namespace lacz::cfg {

struct aimbot_t {
    bool  enabled       = true;
    bool  silent        = false;
    bool  auto_fire     = false;
    bool  auto_scope    = false;
    bool  auto_stop     = false;
    bool  visible_check = false;
    bool  smoke_check   = false;
    int   hitbox        = 0;      // 0 head, 1 neck, 2 chest, 3 pelvis, 4 nearest
    float fov           = 15.0f;
    float smooth        = 2.0f;
    float max_dist      = 300.0f;
    int   key           = 0;      // 0 = always active
    int   priority      = 1;      // 1 fov
};

struct rage_t {
    bool  enabled       = false;
    bool  silent        = true;
    bool  auto_fire     = false;
    bool  auto_wall     = true;
    bool  auto_scope    = true;
    bool  auto_stop     = false;
    bool  auto_revolver = false;
    bool  no_spread     = true;
    bool  no_recoil     = true;
    bool  prefer_head   = true;
    bool  body_aim_fallback = true;
    bool  force_safe_point = false;
    int   min_damage    = 100;
    float min_hitchance = 55.0f;
    float fov           = 180.0f;
    int   hitboxes      = 0x3;
    int   key           = VK_XBUTTON2;
};

struct trigger_t {
    bool  enabled    = false;
    bool  visible_only = true;
    bool  on_key     = false;
    int   key        = VK_MENU;
    float hitchance  = 60.0f;
    int   delay_ms   = 0;
    float max_dist   = 200.0f;
};

struct antiaim_t {
    bool  enabled    = false;
    bool  desync     = true;
    bool  lower_body = true;
    bool  pitch_enabled = true;
    int   pitch_mode = 0;
    int   yaw_base   = 0;
    float yaw_add    = 180.0f;
    float desync_amount = 58.0f;
    int   jitter     = 0;
    bool  on_shot    = true;
    bool  fake_lag   = true;
    int   fake_lag_limit = 14;
};

struct esp_t {
    bool  enabled    = true;
    bool  box        = true;
    int   box_style  = 0;
    bool  name       = true;
    bool  health     = true;
    bool  health_bar = true;
    bool  armor      = true;
    bool  weapon     = true;
    bool  ammo       = false;
    bool  distance   = true;
    bool  snapline   = false;
    int   snapline_origin = 0;
    bool  skeleton   = false;
    bool  chams      = false;
    bool  chams_vis  = false;
    bool  glow       = false;
    bool  radar      = false;
    bool  sound      = false;
    bool  through_walls = true;
    float box_color[4]    = {1.f, 1.f, 1.f, 1.f};
    float vis_color[4]    = {1.f, 0.3f, 0.3f, 1.f};
    float hidden_color[4] = {0.5f, 0.5f, 0.5f, 1.f};
    float max_dist   = 260.0f;
};

struct chams_t {
    bool  enabled      = false;
    bool  players      = true;
    bool  hands        = true;
    bool  weapons      = true;
    bool  backtrack    = true;
    bool  desync       = true;
    bool  model_glow   = false;
    float vis_color[4]    = {1.f, 1.f, 1.f, 1.f};
    float hidden_color[4] = {0.4f, 0.4f, 1.f, 1.f};
    float bt_color[4]     = {0.6f, 0.6f, 0.6f, 1.f};
    float desync_color[4] = {1.f, 0.5f, 0.f, 1.f};
};

struct skins_t {
    bool  enabled    = false;
    bool  knives     = true;
    bool  gloves     = true;
    bool  agents     = false;
    bool  music_kit  = false;
    int   knife_model = 0;
    int   glove_model = 0;
    int   paint_kit  = 0;
    float wear       = 0.01f;
    int   seed       = 0;
    int   stat_trak  = 0;
    char  custom_name[32] = "";
};

struct misc_t {
    bool  bhop       = true;
    bool  auto_strafe = false;
    bool  fast_stop  = false;
    bool  auto_peek  = false;
    bool  edge_jump  = false;
    bool  jump_bug   = false;
    bool  no_flash   = false;
    bool  no_smoke   = false;
    bool  no_scope   = false;
    bool  no_visual_recoil = false;
    bool  thirdperson = false;
    int   tp_key     = 'C';
    float tp_dist    = 120.0f;
    bool  water_mark = true;
    bool  bomb_timer = false;
    int   bomb_style = 0;
    bool  spectator_list = false;
    int   fov_changer = 0;
    bool  remove_legs = false;
    bool  fakelatency = false;
};

struct gui_t {
    int   menu_key   = VK_INSERT;
    bool  rainbow    = true;
    float accent[4]  = {0.55f, 0.30f, 0.90f, 1.0f};
    float accent2[4] = {0.20f, 0.70f, 1.00f, 1.0f};
    bool  animation  = true;
    bool  blur       = true;
    float anim_speed = 1.0f;
    int   tab_style  = 0;
};

struct container {
    aimbot_t  aimbot;
    rage_t    rage;
    trigger_t trigger;
    antiaim_t antiaim;
    esp_t     esp;
    chams_t   chams;
    skins_t   skins;
    misc_t    misc;
    gui_t     gui;
    bool      streamproof = false;
};

inline container g;

bool save(const std::string& name);
bool load(const std::string& name);
std::vector<std::string> list();
std::string directory();

} // namespace lacz::cfg
