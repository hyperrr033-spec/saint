#include "weapon.hpp"

namespace lacz::weapon {

static const weapon_data kWeapons[] = {
    {1,   "deagle",        7,   700,  false, false, false},
    {2,   "elite",         30,  500,  false, false, false},
    {3,   "fiveseven",     20,  500,  false, false, false},
    {4,   "glock",         20,  200,  false, false, false},
    {7,   "ak47",          30,  2700, false, false, false},
    {8,   "aug",           30,  3300, false, false, false},
    {9,   "awp",           5,   4750, true,  false, false},
    {10,  "famas",         25,  2050, false, false, false},
    {11,  "g3sg1",         20,  5000, true,  false, false},
    {13,  "galil",         35,  1800, false, false, false},
    {14,  "m249",          100, 5200, false, false, false},
    {16,  "m4a4",          30,  3100, false, false, false},
    {17,  "mac10",         30,  1050, false, false, false},
    {19,  "p90",           50,  2350, false, false, false},
    {24,  "ump45",         25,  1200, false, false, false},
    {25,  "xm1014",        7,   2000, false, false, false},
    {26,  "bizon",         64,  1400, false, false, false},
    {27,  "mag7",          5,   1300, false, false, false},
    {28,  "negev",         150, 1700, false, false, false},
    {29,  "sawedoff",      7,   1100, false, false, false},
    {30,  "tec9",          18,  500,  false, false, false},
    {31,  "taser",         1,   200,  false, false, false},
    {32,  "hkp2000",       13,  200,  false, false, false},
    {33,  "mp7",           30,  1500, false, false, false},
    {34,  "mp9",           30,  1250, false, false, false},
    {35,  "nova",          8,   1050, false, false, false},
    {36,  "p250",          13,  300,  false, false, false},
    {37,  "shields",       0,   0,    false, false, false},
    {38,  "ssg08",         10,  1700, true,  false, false},
    {39,  "sg556",         30,  3000, false, false, false},
    {40,  "scar20",        20,  5000, true,  false, false},
    {60,  "m4a1_silencer", 20,  2900, false, false, false},
    {61,  "usp_silencer",  12,  200,  false, false, false},
    {63,  "cz75a",         12,  500,  false, false, false},
    {64,  "revolver",      8,   600,  false, false, false},
    {42,  "knife",         0,   0,    false, true,  false},
    {59,  "knife_t",       0,   0,    false, true,  false},
    {80,  "knife_ghost",   0,   0,    false, true,  false},
    {43,  "flashbang",     1,   200,  false, false, true},
    {44,  "hegrenade",     1,   300,  false, false, true},
    {45,  "smokegrenade",  1,   300,  false, false, true},
    {46,  "molotov",       1,   400,  false, false, true},
    {47,  "decoy",         1,   50,   false, false, true},
    {48,  "incgrenade",    1,   600,  false, false, true},
    {68,  "tagrenade",     1,   0,    false, false, true},
};

const weapon_data* get(int id) {
    for (auto& w : kWeapons)
        if (w.id == id)
            return &w;
    return nullptr;
}

const char* name(int id) {
    auto* data = get(id);
    return data ? data->name : "unknown";
}

int max_clip(int id) {
    auto* data = get(id);
    return data ? data->max_clip : 0;
}

bool is_sniper(int id) {
    auto* data = get(id);
    return data && data->is_sniper;
}

bool is_knife(int id) {
    auto* data = get(id);
    return data && data->is_knife;
}

bool paint_kit_fits_weapon(int paint_kit, int id) {
    (void)paint_kit;
    (void)id;
    // A full compatibility table is huge; the game itself validates the kit at
    // apply time, so we only block obviously wrong pairings here.
    if (is_knife(id))
        return paint_kit == 0 || (paint_kit >= 400 && paint_kit < 1000);
    return true;
}

} // namespace lacz::weapon