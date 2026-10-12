#include "menu.hpp"
#include "widgets.hpp"
#include "../core/settings.hpp"
#include "../render/renderer.hpp"
#include "../features/features.hpp"
#include <imgui_internal.h>
#include <ctime>

namespace lacz::gui::menu {

using namespace lacz::gui;

static bool g_open = true;
static bool g_focus_input = false;
static float g_tab_anim = 0.f;
static int   g_tab = 0;
static int   g_prev_tab = 0;

// Config editor state.
static char g_config_name[32] = "default";
static std::vector<std::string> g_configs;

// Tab definitions.
enum tab_index : int {
    TAB_COMBAT,
    TAB_RAGE,
    TAB_ANTIAIM,
    TAB_VISUALS,
    TAB_SKINS,
    TAB_MISC,
    TAB_SETTINGS,
    TAB_COUNT,
};

static const char* kTabs[TAB_COUNT] = {
    "Legit", "Rage", "Anti-Aim", "Visuals", "Skins", "Misc", "Settings",
};

static const char* kAimbotHitboxes[] = {"Head", "Neck", "Chest", "Pelvis", "Nearest"};
static const char* kTargetPriority[] = {"Distance", "FOV", "Health", "Threat"};
static const char* kBoxStyles[] = {"Full", "Corner", "Filled", "3D"};
static const char* kSnaplines[] = {"Bottom", "Center", "Crosshair"};
static const char* kPitchModes[] = {"Down", "Up", "Zero", "Jitter", "Fake"};
static const char* kYawBase[] = {"Local view", "At target", "Forward", "Fake"};
static const char* kJitter[] = {"Off", "Static", "Random", "Spin"};
static const char* kKnives[] = {
    "Default", "Bayonet", "Flip", "Gut", "Karambit", "M9 Bayonet", "Huntsman",
    "Falchion", "Bowie", "Butterfly", "Shadow Daggers", "Ursus", "Navaja",
    "Stiletto", "Talon", "Classic", "Paracord", "Survival", "Nomad", "Skeleton",
};
static const char* kGloves[] = {
    "Default", "Bloodhound", "Sport", "Driver", "Hand Wraps", "Moto",
    "Specialist", "Hydra", "Broken Fang",
};
static const char* kRageHitboxes[] = {"Head", "Body", "Legs", "Arms", "Neck"};

void initialize() {
    theme();
    cfg::load("default");
    g_configs = cfg::list();
}

void toggle() { g_open = !g_open; }
bool is_open() { return g_open; }
bool wants_input() { return g_focus_input; }
void set_input_active(bool active) { g_focus_input = active; }

// ---------------------------------------------------------------------------
// Animated title bar / watermark strip at the top of the window.
// ---------------------------------------------------------------------------
static void draw_header(ImDrawList* dl, ImVec2 min, ImVec2 max) {
    // Slow-moving gradient.
    float t = static_cast<float>(ImGui::GetTime()) * 0.2f * cfg::g.gui.anim_speed;
    float phase = std::fmod(t, 1.0f);
    ImU32 c1 = render::accent(0.f);
    ImU32 c2 = render::accent(phase * 0.5f);
    dl->AddRectFilledMultiColor(min, max, c1, c2, c1, c2);

    // Logo mark.
    ImVec2 center(min.x + 26.f, (min.y + max.y) * 0.5f);
    dl->AddCircleFilled(center, 11.f, IM_COL32(255, 255, 255, 40), 32);
    dl->AddCircle(center, 11.f, IM_COL32(255, 255, 255, 200), 32, 1.6f);
    dl->AddText(ImVec2(center.x - 5.f, center.y - 7.f), IM_COL32(255,255,255,255), "L");

    dl->AddText(ImVec2(min.x + 46.f, min.y + 14.f), IM_COL32(255,255,255,255),
                "LACZ");
    dl->AddText(ImVec2(min.x + 46.f + ImGui::CalcTextSize("LACZ").x + 4.f, min.y + 14.f),
                IM_COL32(255,255,255,180), "DLC");

    // Version badge on the right.
    char badge[64];
    snprintf(badge, sizeof(badge), "v%s", lacz::kVersion);
    ImVec2 bs = ImGui::CalcTextSize(badge);
    dl->AddText(ImVec2(max.x - bs.x - 16.f, min.y + 14.f),
                IM_COL32(255,255,255,200), badge);
}

// ---------------------------------------------------------------------------
// Sidebar with animated selection pill.
// ---------------------------------------------------------------------------
static int draw_sidebar(ImDrawList* dl, ImVec2 min, ImVec2 max) {
    dl->AddRectFilled(min, max, IM_COL32(14, 14, 20, 255));

    // Vertical accent line.
    dl->AddRectFilled(ImVec2(max.x - 1.f, min.y), max, IM_COL32(40, 40, 52, 255));

    const float item_h = 46.f;
    float y = min.y + 12.f;
    int result = g_tab;

    for (int i = 0; i < TAB_COUNT; ++i) {
        ImVec2 item_min(min.x + 8.f, y);
        ImVec2 item_max(max.x - 8.f, y + item_h - 6.f);
        ImRect bb(item_min, item_max);

        ImGuiID id = ImGui::GetCurrentWindow()->GetID(kTabs[i]);
        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, id)) {
            y += item_h;
            continue;
        }

        bool hovered = false, held = false;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed)
            result = i;

        bool selected = (g_tab == i);
        auto& anim = animated::get(id);
        // Ease the selection background and the hover glow separately.
        float t = animated::approach(anim, selected ? 1.f : (hovered ? 0.5f : 0.f),
                                     12.f * std::clamp(cfg::g.gui.anim_speed, 0.2f, 3.f),
                                     std::clamp(ImGui::GetIO().DeltaTime, 0.001f, 0.05f));

        if (t > 0.01f) {
            ImU32 bg = render::accent_alpha(t * (selected ? 0.22f : 0.12f));
            dl->AddRectFilled(item_min, item_max, bg, 8.f);
            // Left indicator bar.
            dl->AddRectFilled(ImVec2(item_min.x, item_min.y + 6.f),
                              ImVec2(item_min.x + 3.f, item_max.y - 6.f),
                              render::accent_alpha(t), 2.f);
        }

        ImU32 text_col = IM_COL32(150 + static_cast<int>(t * 105),
                                  150 + static_cast<int>(t * 105),
                                  165 + static_cast<int>(t * 90), 255);
        dl->AddText(ImVec2(item_min.x + 20.f,
                           item_min.y + (item_max.y - item_min.y - 14.f) * 0.5f),
                    text_col, kTabs[i]);

        y += item_h;
    }

    // Bottom status.
    ImVec2 status(min.x + 16.f, max.y - 30.f);
    const char* status_txt = features::status_text();
    dl->AddCircleFilled(ImVec2(status.x, status.y + 6.f), 3.f,
                        IM_COL32(80, 220, 120, 255), 12);
    dl->AddText(ImVec2(status.x + 12.f, status.y), IM_COL32(120,120,135,255), status_txt);

    return result;
}

// ---------------------------------------------------------------------------
// Each page.
// ---------------------------------------------------------------------------
static void page_legit() {
    auto& a = cfg::g.aimbot;
    section("Aimbot");
    switch_toggle("Enable aimbot", &a.enabled, "Master switch for the legit resolver");
    checkbox("Silent aim", &a.silent, "Do not move the visual view angle");
    checkbox("Auto fire", &a.auto_fire, "Fire when a target passes checks");
    checkbox("Auto scope", &a.auto_scope, "Scope with snipers before firing");
    checkbox("Auto stop", &a.auto_stop, "Stop movement for full accuracy");
    keybind("Aim key", &a.key);
    combo("Hitbox", &a.hitbox, kAimbotHitboxes, IM_ARRAYSIZE(kAimbotHitboxes));
    combo("Target priority", &a.priority, kTargetPriority, IM_ARRAYSIZE(kTargetPriority));
    slider_float("FOV", &a.fov, 0.1f, 30.f, "%.1f deg");
    slider_float("Smoothing", &a.smooth, 1.f, 20.f, "%.1f");
    slider_float("Max distance", &a.max_dist, 10.f, 300.f, "%.0f m");
    checkbox("Visibility check", &a.visible_check, "Only aim at visible targets");
    checkbox("Smoke check", &a.smoke_check, "Ignore targets behind smoke");

    section("Triggerbot");
    auto& t = cfg::g.trigger;
    checkbox("Enable triggerbot", &t.enabled);
    checkbox("Hold key", &t.on_key, "Require key to be held");
    keybind("Trigger key", &t.key);
    checkbox("Visible only", &t.visible_only);
    slider_float("Hitchance", &t.hitchance, 0.f, 100.f, "%.0f%%");
    slider_int("Delay", &t.delay_ms, 0, 500, "%d ms");
    slider_float("Max distance", &t.max_dist, 10.f, 300.f, "%.0f m");
}

static void page_rage() {
    auto& r = cfg::g.rage;
    section("Rage aimbot");
    switch_toggle("Enable rage", &r.enabled, "Maximum-damage aim, wall penetration aware");
    checkbox("Silent aim", &r.silent);
    checkbox("Auto fire", &r.auto_fire);
    keybind("Rage key", &r.key);
    combo("Hitboxes", &r.hitboxes, kRageHitboxes, IM_ARRAYSIZE(kRageHitboxes));
    slider_int("Minimum damage", &r.min_damage, 1, 130, "%d hp");
    slider_float("Minimum hitchance", &r.min_hitchance, 0.f, 100.f, "%.0f%%");
    slider_float("FOV", &r.fov, 1.f, 180.f, "%.0f deg");

    section("Accuracy");
    checkbox("Auto wall", &r.auto_wall, "Shoot through penetrable surfaces");
    checkbox("No spread", &r.no_spread, "Remove weapon spread");
    checkbox("No recoil", &r.no_recoil, "Remove aim punch");
    checkbox("Prefer head", &r.prefer_head, "Always evaluate head first");
    checkbox("Body fallback", &r.body_aim_fallback, "Fall back to body if head is unsafe");
    checkbox("Force safe point", &r.force_safe_point, "Only use safe-point hitboxes");
    checkbox("Auto scope", &r.auto_scope);
    checkbox("Auto stop", &r.auto_stop);
    checkbox("Auto revolver", &r.auto_revolver, "Fire the R8 optimally");
}

static void page_antiaim() {
    auto& a = cfg::g.antiaim;
    section("Anti-aim");
    switch_toggle("Enable anti-aim", &a.enabled);
    checkbox("Desync", &a.desync, "Desync the server-side body yaw");
    checkbox("Lower body yaw", &a.lower_body, "Fake the lower body independently");
    slider_float("Desync amount", &a.desync_amount, 0.f, 60.f, "%.0f");
    combo("Yaw base", &a.yaw_base, kYawBase, IM_ARRAYSIZE(kYawBase));
    slider_float("Yaw offset", &a.yaw_add, -180.f, 180.f, "%.0f deg");
    combo("Jitter", &a.jitter, kJitter, IM_ARRAYSIZE(kJitter));

    section("Pitch");
    checkbox("Enable pitch", &a.pitch_enabled);
    combo("Pitch mode", &a.pitch_mode, kPitchModes, IM_ARRAYSIZE(kPitchModes));

    section("Fake lag");
    checkbox("Enable fake lag", &a.fake_lag, "Choke ticks to hide your real position");
    slider_int("Choke limit", &a.fake_lag_limit, 1, 16, "%d ticks");
    checkbox("Disable on shot", &a.on_shot, "Unchoke while shooting to stay accurate");
}

static void page_visuals() {
    auto& e = cfg::g.esp;
    auto& c = cfg::g.chams;
    section("Player ESP");
    switch_toggle("Enable ESP", &e.enabled);
    combo("Box style", &e.box_style, kBoxStyles, IM_ARRAYSIZE(kBoxStyles));
    checkbox("Box", &e.box);
    checkbox("Name", &e.name);
    checkbox("Health", &e.health);
    checkbox("Health bar", &e.health_bar);
    checkbox("Armor", &e.armor);
    checkbox("Weapon", &e.weapon);
    checkbox("Ammo", &e.ammo);
    checkbox("Distance", &e.distance);
    checkbox("Snapline", &e.snapline);
    combo("Snapline origin", &e.snapline_origin, kSnaplines, IM_ARRAYSIZE(kSnaplines));
    checkbox("Skeleton", &e.skeleton);
    checkbox("Through walls", &e.through_walls);
    slider_float("Max render distance", &e.max_dist, 10.f, 500.f, "%.0f m");

    section("Colors");
    color_edit("Box color", e.box_color);
    color_edit("Visible color", e.vis_color);
    color_edit("Occluded color", e.hidden_color);

    section("Chams");
    switch_toggle("Enable chams", &c.enabled);
    checkbox("Players", &c.players);
    checkbox("Hands", &c.hands);
    checkbox("Weapons", &c.weapons);
    checkbox("Backtrack", &c.backtrack);
    checkbox("Desync", &c.desync);
    checkbox("Model glow", &c.model_glow);
    color_edit("Visible", c.vis_color);
    color_edit("Hidden", c.hidden_color);
    color_edit("Backtrack", c.bt_color);
    color_edit("Desync", c.desync_color);

    section("Other visuals");
    checkbox("Radar reveal", &e.radar, "Spots all enemies on radar");
    checkbox("Sound ESP", &e.sound, "Show a ring where enemies make noise");
    checkbox("Glow", &e.glow);
}

static void page_skins() {
    auto& s = cfg::g.skins;
    section("Skin changer");
    switch_toggle("Enable skins", &s.enabled);
    checkbox("Knives", &s.knives);
    combo("Knife model", &s.knife_model, kKnives, IM_ARRAYSIZE(kKnives));
    checkbox("Gloves", &s.gloves);
    combo("Glove model", &s.glove_model, kGloves, IM_ARRAYSIZE(kGloves));
    checkbox("Agents", &s.agents);
    checkbox("Music kit", &s.music_kit);

    section("Weapon finish");
    slider_int("Paint kit", &s.paint_kit, 0, 2000, "%d");
    slider_float("Wear", &s.wear, 0.f, 1.f, "%.3f");
    slider_int("Seed", &s.seed, 0, 1000, "%d");
    slider_int("StatTrak", &s.stat_trak, 0, 999999, "%d");
    text_input("Custom name", s.custom_name, sizeof(s.custom_name));

    if (button("Apply to held weapon", ImVec2(0, 32), true))
        features::skins::apply_now();
}

static void page_misc() {
    auto& m = cfg::g.misc;
    section("Movement");
    checkbox("Bunny hop", &m.bhop);
    checkbox("Auto strafe", &m.auto_strafe);
    checkbox("Fast stop", &m.fast_stop);
    checkbox("Auto peek", &m.auto_peek);
    checkbox("Edge jump", &m.edge_jump);
    checkbox("Jump bug", &m.jump_bug);

    section("Removals");
    checkbox("No flash", &m.no_flash);
    checkbox("No smoke", &m.no_smoke);
    checkbox("No scope overlay", &m.no_scope);
    checkbox("No visual recoil", &m.no_visual_recoil);
    checkbox("Remove legs", &m.remove_legs);

    section("World");
    checkbox("Third person", &m.thirdperson);
    keybind("Third person key", &m.tp_key);
    slider_float("Third person distance", &m.tp_dist, 40.f, 300.f, "%.0f");
    slider_int("FOV changer", &m.fov_changer, 0, 140, "%d");
    checkbox("Fake latency", &m.fakelatency);

    section("Overlay");
    checkbox("Watermark", &m.water_mark);
    checkbox("Bomb timer", &m.bomb_timer);
    combo("Bomb style", &m.bomb_style, kBoxStyles, 2);
    checkbox("Spectator list", &m.spectator_list);
}

static void page_settings() {
    auto& g = cfg::g.gui;
    section("Menu");
    keybind("Menu key", &g.menu_key);
    checkbox("Rainbow accent", &g.rainbow);
    checkbox("Animations", &g.animation);
    checkbox("Streamproof", &cfg::g.streamproof);
    slider_float("Animation speed", &g.anim_speed, 0.2f, 3.f, "%.2fx");
    color_edit("Accent color", g.accent);
    color_edit("Accent color 2", g.accent2);

    section("Config");
    text_input("Config name", g_config_name, sizeof(g_config_name));
    if (button("Save", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 30.f), true)) {
        cfg::save(g_config_name);
        g_configs = cfg::list();
    }
    ImGui::SameLine();
    if (button("Load", ImVec2(0, 30.f), true)) {
        cfg::load(g_config_name);
    }
    ImGui::Dummy(ImVec2(0, 6));

    if (!g_configs.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.62f, 0.70f, 1.f));
        ImGui::TextUnformatted("Saved configs");
        ImGui::PopStyleColor();
        for (auto& name : g_configs) {
            bool selected = (name == g_config_name);
            if (ImGui::Selectable(name.c_str(), selected)) {
                snprintf(g_config_name, sizeof(g_config_name), "%s", name.c_str());
            }
        }
    }

    section("Info");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.62f, 1.f));
    ImGui::Text("Build: %s %s", lacz::kName, lacz::kVersion);
    ImGui::Text("%s", features::build_info());
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Main render
// ---------------------------------------------------------------------------
void render() {
    if (!g_open)
        return;

    const float dt = std::clamp(ImGui::GetIO().DeltaTime, 0.001f, 0.05f);
    float speed = std::clamp(cfg::g.gui.anim_speed, 0.2f, 3.f);

    static anim_state open_anim;
    static anim_state tab_anim;
    float open_t = animated::approach(open_anim, 1.f, 14.f * speed, dt);
    if (g_tab != g_prev_tab) {
        tab_anim.value = 0.f;
        g_prev_tab = g_tab;
    }
    float tab_t = animated::approach(tab_anim, 1.f, 16.f * speed, dt);

    const float width = 760.f;
    const float height = 540.f;
    ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
                           ImGui::GetIO().DisplaySize.y * 0.5f);

    // Entrance animation: slide up + fade.
    float off = (1.f - open_t) * 24.f;
    ImVec2 win_min(center.x - width * 0.5f, center.y - height * 0.5f + off);
    ImVec2 win_max(win_min.x + width, win_min.y + height);

    ImGui::SetNextWindowPos(win_min);
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoResize;
    ImGui::Begin("##lacz_menu", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Window body with rounded corners + shadow.
    dl->AddRectFilled(ImVec2(win_min.x + 2.f, win_min.y + 6.f),
                      ImVec2(win_max.x + 2.f, win_max.y + 12.f),
                      IM_COL32(0, 0, 0, 90), 14.f);
    dl->AddRectFilled(win_min, win_max, IM_COL32(18, 18, 25, 250), 14.f);

    // Header strip.
    ImVec2 header_min(win_min.x, win_min.y);
    ImVec2 header_max(win_max.x, win_min.y + 44.f);
    dl->PushClipRect(header_min, header_max, true);
    draw_header(dl, header_min, header_max);
    dl->PopClipRect();

    // Sidebar.
    ImVec2 side_min(win_min.x, header_max.y);
    ImVec2 side_max(win_min.x + 168.f, win_max.y);
    int new_tab = draw_sidebar(dl, side_min, side_max);
    if (new_tab != g_tab)
        g_tab = new_tab;

    // Content area.
    ImVec2 body_min(side_max.x + 16.f, header_max.y + 12.f);
    ImVec2 body_max(win_max.x - 16.f, win_max.y - 12.f);

    // Animated tab transition: content fades/slides in.
    ImVec2 content_min(body_min.x, body_min.y + (1.f - tab_t) * 10.f);
    ImGui::SetCursorScreenPos(content_min);
    ImGui::PushClipRect(body_min, body_max, true);

    // Begin a child for scrolling.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    if (ImGui::BeginChild("##content", ImVec2(body_max.x - body_min.x,
                                              body_max.y - body_min.y),
                          false, ImGuiWindowFlags_NoBackground)) {
        // Fade the whole page in with tab_t.
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(tab_t, 0.f, 1.f));
        switch (g_tab) {
            case TAB_COMBAT:   page_legit(); break;
            case TAB_RAGE:     page_rage(); break;
            case TAB_ANTIAIM:  page_antiaim(); break;
            case TAB_VISUALS:  page_visuals(); break;
            case TAB_SKINS:    page_skins(); break;
            case TAB_MISC:     page_misc(); break;
            case TAB_SETTINGS: page_settings(); break;
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopClipRect();

    // Footer.
    dl->AddText(ImVec2(win_min.x + 20.f, win_max.y - 22.f),
                IM_COL32(90, 90, 105, 255),
                "LaczDLC internal  -  insert to toggle");

    ImGui::End();
}

} // namespace lacz::gui::menu
