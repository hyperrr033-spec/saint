#include "settings.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <exception>

namespace lacz::cfg {

std::string directory() {
    char path[MAX_PATH]{};
    GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
    std::string dir = std::string(path) + "\\LaczDLC\\configs";
    std::filesystem::create_directories(dir);
    return dir;
}

// ---------------------------------------------------------------------------
// Minimal key/value serializer. One section header per struct, flat key=value
// under it. Keeps the format human-editable with zero dependencies.
// ---------------------------------------------------------------------------
static std::string serialize() {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(4);

    auto bool_s = [](bool v) { return v ? 1 : 0; };

    o << "[aimbot]\n"
      << "enabled=" << bool_s(g.aimbot.enabled) << "\n"
      << "silent=" << bool_s(g.aimbot.silent) << "\n"
      << "auto_fire=" << bool_s(g.aimbot.auto_fire) << "\n"
      << "auto_scope=" << bool_s(g.aimbot.auto_scope) << "\n"
      << "auto_stop=" << bool_s(g.aimbot.auto_stop) << "\n"
      << "visible_check=" << bool_s(g.aimbot.visible_check) << "\n"
      << "smoke_check=" << bool_s(g.aimbot.smoke_check) << "\n"
      << "hitbox=" << g.aimbot.hitbox << "\n"
      << "fov=" << g.aimbot.fov << "\n"
      << "smooth=" << g.aimbot.smooth << "\n"
      << "max_dist=" << g.aimbot.max_dist << "\n"
      << "key=" << g.aimbot.key << "\n"
      << "priority=" << g.aimbot.priority << "\n";

    o << "[rage]\n"
      << "enabled=" << bool_s(g.rage.enabled) << "\n"
      << "silent=" << bool_s(g.rage.silent) << "\n"
      << "auto_fire=" << bool_s(g.rage.auto_fire) << "\n"
      << "auto_wall=" << bool_s(g.rage.auto_wall) << "\n"
      << "auto_scope=" << bool_s(g.rage.auto_scope) << "\n"
      << "auto_stop=" << bool_s(g.rage.auto_stop) << "\n"
      << "auto_revolver=" << bool_s(g.rage.auto_revolver) << "\n"
      << "no_spread=" << bool_s(g.rage.no_spread) << "\n"
      << "no_recoil=" << bool_s(g.rage.no_recoil) << "\n"
      << "prefer_head=" << bool_s(g.rage.prefer_head) << "\n"
      << "body_aim_fallback=" << bool_s(g.rage.body_aim_fallback) << "\n"
      << "force_safe_point=" << bool_s(g.rage.force_safe_point) << "\n"
      << "min_damage=" << g.rage.min_damage << "\n"
      << "min_hitchance=" << g.rage.min_hitchance << "\n"
      << "fov=" << g.rage.fov << "\n"
      << "hitboxes=" << g.rage.hitboxes << "\n"
      << "key=" << g.rage.key << "\n";

    o << "[trigger]\n"
      << "enabled=" << bool_s(g.trigger.enabled) << "\n"
      << "visible_only=" << bool_s(g.trigger.visible_only) << "\n"
      << "on_key=" << bool_s(g.trigger.on_key) << "\n"
      << "key=" << g.trigger.key << "\n"
      << "hitchance=" << g.trigger.hitchance << "\n"
      << "delay_ms=" << g.trigger.delay_ms << "\n"
      << "max_dist=" << g.trigger.max_dist << "\n";

    o << "[antiaim]\n"
      << "enabled=" << bool_s(g.antiaim.enabled) << "\n"
      << "desync=" << bool_s(g.antiaim.desync) << "\n"
      << "lower_body=" << bool_s(g.antiaim.lower_body) << "\n"
      << "pitch_enabled=" << bool_s(g.antiaim.pitch_enabled) << "\n"
      << "pitch_mode=" << g.antiaim.pitch_mode << "\n"
      << "yaw_base=" << g.antiaim.yaw_base << "\n"
      << "yaw_add=" << g.antiaim.yaw_add << "\n"
      << "desync_amount=" << g.antiaim.desync_amount << "\n"
      << "jitter=" << g.antiaim.jitter << "\n"
      << "on_shot=" << bool_s(g.antiaim.on_shot) << "\n"
      << "fake_lag=" << bool_s(g.antiaim.fake_lag) << "\n"
      << "fake_lag_limit=" << g.antiaim.fake_lag_limit << "\n";

    o << "[esp]\n"
      << "enabled=" << bool_s(g.esp.enabled) << "\n"
      << "box=" << bool_s(g.esp.box) << "\n"
      << "box_style=" << g.esp.box_style << "\n"
      << "name=" << bool_s(g.esp.name) << "\n"
      << "health=" << bool_s(g.esp.health) << "\n"
      << "health_bar=" << bool_s(g.esp.health_bar) << "\n"
      << "armor=" << bool_s(g.esp.armor) << "\n"
      << "weapon=" << bool_s(g.esp.weapon) << "\n"
      << "ammo=" << bool_s(g.esp.ammo) << "\n"
      << "distance=" << bool_s(g.esp.distance) << "\n"
      << "snapline=" << bool_s(g.esp.snapline) << "\n"
      << "snapline_origin=" << g.esp.snapline_origin << "\n"
      << "skeleton=" << bool_s(g.esp.skeleton) << "\n"
      << "chams=" << bool_s(g.esp.chams) << "\n"
      << "chams_vis=" << bool_s(g.esp.chams_vis) << "\n"
      << "glow=" << bool_s(g.esp.glow) << "\n"
      << "radar=" << bool_s(g.esp.radar) << "\n"
      << "sound=" << bool_s(g.esp.sound) << "\n"
      << "through_walls=" << bool_s(g.esp.through_walls) << "\n"
      << "max_dist=" << g.esp.max_dist << "\n";

    o << "[chams]\n"
      << "enabled=" << bool_s(g.chams.enabled) << "\n"
      << "players=" << bool_s(g.chams.players) << "\n"
      << "hands=" << bool_s(g.chams.hands) << "\n"
      << "weapons=" << bool_s(g.chams.weapons) << "\n"
      << "backtrack=" << bool_s(g.chams.backtrack) << "\n"
      << "desync=" << bool_s(g.chams.desync) << "\n"
      << "model_glow=" << bool_s(g.chams.model_glow) << "\n";

    o << "[skins]\n"
      << "enabled=" << bool_s(g.skins.enabled) << "\n"
      << "knives=" << bool_s(g.skins.knives) << "\n"
      << "gloves=" << bool_s(g.skins.gloves) << "\n"
      << "agents=" << bool_s(g.skins.agents) << "\n"
      << "music_kit=" << bool_s(g.skins.music_kit) << "\n"
      << "knife_model=" << g.skins.knife_model << "\n"
      << "glove_model=" << g.skins.glove_model << "\n"
      << "paint_kit=" << g.skins.paint_kit << "\n"
      << "wear=" << g.skins.wear << "\n"
      << "seed=" << g.skins.seed << "\n"
      << "stat_trak=" << g.skins.stat_trak << "\n";

    o << "[misc]\n"
      << "bhop=" << bool_s(g.misc.bhop) << "\n"
      << "auto_strafe=" << bool_s(g.misc.auto_strafe) << "\n"
      << "fast_stop=" << bool_s(g.misc.fast_stop) << "\n"
      << "auto_peek=" << bool_s(g.misc.auto_peek) << "\n"
      << "edge_jump=" << bool_s(g.misc.edge_jump) << "\n"
      << "jump_bug=" << bool_s(g.misc.jump_bug) << "\n"
      << "no_flash=" << bool_s(g.misc.no_flash) << "\n"
      << "no_smoke=" << bool_s(g.misc.no_smoke) << "\n"
      << "no_scope=" << bool_s(g.misc.no_scope) << "\n"
      << "no_visual_recoil=" << bool_s(g.misc.no_visual_recoil) << "\n"
      << "thirdperson=" << bool_s(g.misc.thirdperson) << "\n"
      << "tp_key=" << g.misc.tp_key << "\n"
      << "tp_dist=" << g.misc.tp_dist << "\n"
      << "water_mark=" << bool_s(g.misc.water_mark) << "\n"
      << "bomb_timer=" << bool_s(g.misc.bomb_timer) << "\n"
      << "bomb_style=" << g.misc.bomb_style << "\n"
      << "spectator_list=" << bool_s(g.misc.spectator_list) << "\n"
      << "fov_changer=" << g.misc.fov_changer << "\n"
      << "remove_legs=" << bool_s(g.misc.remove_legs) << "\n"
      << "fakelatency=" << bool_s(g.misc.fakelatency) << "\n";

    o << "[gui]\n"
      << "menu_key=" << g.gui.menu_key << "\n"
      << "rainbow=" << bool_s(g.gui.rainbow) << "\n"
      << "animation=" << bool_s(g.gui.animation) << "\n"
      << "blur=" << bool_s(g.gui.blur) << "\n"
      << "anim_speed=" << g.gui.anim_speed << "\n"
      << "tab_style=" << g.gui.tab_style << "\n";

    o << "[visuals_extra]\n"
      << "streamproof=" << bool_s(g.streamproof) << "\n";

    return o.str();
}

static bool write_cfg(const std::string& file) {
    std::ofstream out(file, std::ios::trunc);
    if (!out.is_open())
        return false;

    const std::string data = serialize();
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    const bool ok = out.good();
    out.close();
    return ok && !out.fail();
}

bool save(const std::string& name) {
    // Config names are file stems, not paths. Reject empty names and path
    // separators to avoid accidental writes outside the config directory.
    if (name.empty() || name == "." || name == ".." ||
        name.find_first_of("/\\\\:") != std::string::npos)
        return false;

    try {
        const auto dir = directory();
        return write_cfg(dir + "\\" + name + ".lacz");
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    } catch (const std::ios_base::failure&) {
        return false;
    }
}

// ---------------------------------------------------------------------------
// Parser. Deliberately forgiving: unknown keys are ignored, missing keys keep
// their default. This keeps old configs loadable after we add features.
// ---------------------------------------------------------------------------
static void parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    std::string section;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        if (line.front() == '[') {
            section = line.substr(1, line.find(']') - 1);
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        auto b = [&] { return val == "1"; };
        auto i = [&](int d) {
            if (val.empty())
                return d;
            try {
                std::size_t used = 0;
                const int parsed = std::stoi(val, &used, 10);
                return used == val.size() ? parsed : d;
            } catch (const std::exception&) {
                return d;
            }
        };
        auto f = [&](float d) {
            if (val.empty())
                return d;
            try {
                std::size_t used = 0;
                const float parsed = std::stof(val, &used);
                return used == val.size() && std::isfinite(parsed) ? parsed : d;
            } catch (const std::exception&) {
                return d;
            }
        };

        if (section == "aimbot") {
            if (key=="enabled") g.aimbot.enabled=b();
            else if (key=="silent") g.aimbot.silent=b();
            else if (key=="auto_fire") g.aimbot.auto_fire=b();
            else if (key=="auto_scope") g.aimbot.auto_scope=b();
            else if (key=="auto_stop") g.aimbot.auto_stop=b();
            else if (key=="visible_check") g.aimbot.visible_check=b();
            else if (key=="smoke_check") g.aimbot.smoke_check=b();
            else if (key=="hitbox") g.aimbot.hitbox=i(0);
            else if (key=="fov") g.aimbot.fov=f(6.f);
            else if (key=="smooth") g.aimbot.smooth=f(4.f);
            else if (key=="max_dist") g.aimbot.max_dist=f(180.f);
            else if (key=="key") g.aimbot.key=i(VK_XBUTTON1);
            else if (key=="priority") g.aimbot.priority=i(0);
        } else if (section == "rage") {
            if (key=="enabled") g.rage.enabled=b();
            else if (key=="silent") g.rage.silent=b();
            else if (key=="auto_fire") g.rage.auto_fire=b();
            else if (key=="auto_wall") g.rage.auto_wall=b();
            else if (key=="auto_scope") g.rage.auto_scope=b();
            else if (key=="auto_stop") g.rage.auto_stop=b();
            else if (key=="auto_revolver") g.rage.auto_revolver=b();
            else if (key=="no_spread") g.rage.no_spread=b();
            else if (key=="no_recoil") g.rage.no_recoil=b();
            else if (key=="prefer_head") g.rage.prefer_head=b();
            else if (key=="body_aim_fallback") g.rage.body_aim_fallback=b();
            else if (key=="force_safe_point") g.rage.force_safe_point=b();
            else if (key=="min_damage") g.rage.min_damage=i(100);
            else if (key=="min_hitchance") g.rage.min_hitchance=f(55.f);
            else if (key=="fov") g.rage.fov=f(180.f);
            else if (key=="hitboxes") g.rage.hitboxes=i(0x3);
            else if (key=="key") g.rage.key=i(VK_XBUTTON2);
        } else if (section == "trigger") {
            if (key=="enabled") g.trigger.enabled=b();
            else if (key=="visible_only") g.trigger.visible_only=b();
            else if (key=="on_key") g.trigger.on_key=b();
            else if (key=="key") g.trigger.key=i(VK_MENU);
            else if (key=="hitchance") g.trigger.hitchance=f(60.f);
            else if (key=="delay_ms") g.trigger.delay_ms=i(0);
            else if (key=="max_dist") g.trigger.max_dist=f(200.f);
        } else if (section == "antiaim") {
            if (key=="enabled") g.antiaim.enabled=b();
            else if (key=="desync") g.antiaim.desync=b();
            else if (key=="lower_body") g.antiaim.lower_body=b();
            else if (key=="pitch_enabled") g.antiaim.pitch_enabled=b();
            else if (key=="pitch_mode") g.antiaim.pitch_mode=i(0);
            else if (key=="yaw_base") g.antiaim.yaw_base=i(0);
            else if (key=="yaw_add") g.antiaim.yaw_add=f(180.f);
            else if (key=="desync_amount") g.antiaim.desync_amount=f(58.f);
            else if (key=="jitter") g.antiaim.jitter=i(0);
            else if (key=="on_shot") g.antiaim.on_shot=b();
            else if (key=="fake_lag") g.antiaim.fake_lag=b();
            else if (key=="fake_lag_limit") g.antiaim.fake_lag_limit=i(14);
        } else if (section == "esp") {
            if (key=="enabled") g.esp.enabled=b();
            else if (key=="box") g.esp.box=b();
            else if (key=="box_style") g.esp.box_style=i(0);
            else if (key=="name") g.esp.name=b();
            else if (key=="health") g.esp.health=b();
            else if (key=="health_bar") g.esp.health_bar=b();
            else if (key=="armor") g.esp.armor=b();
            else if (key=="weapon") g.esp.weapon=b();
            else if (key=="ammo") g.esp.ammo=b();
            else if (key=="distance") g.esp.distance=b();
            else if (key=="snapline") g.esp.snapline=b();
            else if (key=="snapline_origin") g.esp.snapline_origin=i(0);
            else if (key=="skeleton") g.esp.skeleton=b();
            else if (key=="chams") g.esp.chams=b();
            else if (key=="chams_vis") g.esp.chams_vis=b();
            else if (key=="glow") g.esp.glow=b();
            else if (key=="radar") g.esp.radar=b();
            else if (key=="sound") g.esp.sound=b();
            else if (key=="through_walls") g.esp.through_walls=b();
            else if (key=="max_dist") g.esp.max_dist=f(260.f);
        } else if (section == "chams") {
            if (key=="enabled") g.chams.enabled=b();
            else if (key=="players") g.chams.players=b();
            else if (key=="hands") g.chams.hands=b();
            else if (key=="weapons") g.chams.weapons=b();
            else if (key=="backtrack") g.chams.backtrack=b();
            else if (key=="desync") g.chams.desync=b();
            else if (key=="model_glow") g.chams.model_glow=b();
        } else if (section == "skins") {
            if (key=="enabled") g.skins.enabled=b();
            else if (key=="knives") g.skins.knives=b();
            else if (key=="gloves") g.skins.gloves=b();
            else if (key=="agents") g.skins.agents=b();
            else if (key=="music_kit") g.skins.music_kit=b();
            else if (key=="knife_model") g.skins.knife_model=i(0);
            else if (key=="glove_model") g.skins.glove_model=i(0);
            else if (key=="paint_kit") g.skins.paint_kit=i(0);
            else if (key=="wear") g.skins.wear=f(0.01f);
            else if (key=="seed") g.skins.seed=i(0);
            else if (key=="stat_trak") g.skins.stat_trak=i(0);
        } else if (section == "misc") {
            if (key=="bhop") g.misc.bhop=b();
            else if (key=="auto_strafe") g.misc.auto_strafe=b();
            else if (key=="fast_stop") g.misc.fast_stop=b();
            else if (key=="auto_peek") g.misc.auto_peek=b();
            else if (key=="edge_jump") g.misc.edge_jump=b();
            else if (key=="jump_bug") g.misc.jump_bug=b();
            else if (key=="no_flash") g.misc.no_flash=b();
            else if (key=="no_smoke") g.misc.no_smoke=b();
            else if (key=="no_scope") g.misc.no_scope=b();
            else if (key=="no_visual_recoil") g.misc.no_visual_recoil=b();
            else if (key=="thirdperson") g.misc.thirdperson=b();
            else if (key=="tp_key") g.misc.tp_key=i('C');
            else if (key=="tp_dist") g.misc.tp_dist=f(120.f);
            else if (key=="water_mark") g.misc.water_mark=b();
            else if (key=="bomb_timer") g.misc.bomb_timer=b();
            else if (key=="bomb_style") g.misc.bomb_style=i(0);
            else if (key=="spectator_list") g.misc.spectator_list=b();
            else if (key=="fov_changer") g.misc.fov_changer=i(0);
            else if (key=="remove_legs") g.misc.remove_legs=b();
            else if (key=="fakelatency") g.misc.fakelatency=b();
        } else if (section == "gui") {
            if (key=="menu_key") g.gui.menu_key=i(VK_INSERT);
            else if (key=="rainbow") g.gui.rainbow=b();
            else if (key=="animation") g.gui.animation=b();
            else if (key=="blur") g.gui.blur=b();
            else if (key=="anim_speed") g.gui.anim_speed=f(1.f);
            else if (key=="tab_style") g.gui.tab_style=i(0);
        } else if (section == "visuals_extra") {
            if (key=="streamproof") g.streamproof=b();
        }
    }
}

bool load(const std::string& name) {
    if (name.empty() || name == "." || name == ".." ||
        name.find_first_of("/\\\\:") != std::string::npos)
        return false;

    try {
        std::ifstream in(directory() + "\\" + name + ".lacz");
        if (!in.is_open())
            return false;

        std::stringstream buf;
        buf << in.rdbuf();
        if (in.bad())
            return false;

        parse(buf.str());
        return true;
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    } catch (const std::ios_base::failure&) {
        return false;
    }
}

std::vector<std::string> list() {
    std::vector<std::string> out;
    try {
        const auto dir = directory();
        if (!std::filesystem::exists(dir))
            return out;

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lacz")
                out.push_back(entry.path().stem().string());
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Return the entries found so far (or an empty list) if enumeration
        // fails; callers can still display an empty config list.
    }
    return out;
}

} // namespace lacz::cfg