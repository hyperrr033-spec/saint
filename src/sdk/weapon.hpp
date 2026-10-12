#pragma once
#include "math.hpp"

namespace lacz::weapon {

// ---------------------------------------------------------------------------
// Static weapon metadata. Not a replacement for the game's data -- used only
// for cheap UI/ESP decisions (icon selection, max clip, sniper detection).
// ---------------------------------------------------------------------------
struct weapon_data {
    int  id;
    const char* name;
    int  max_clip;
    int  price;
    bool is_sniper;
    bool is_knife;
    bool is_grenade;
};

const weapon_data* get(int item_definition_index);
const char* name(int item_definition_index);
int max_clip(int item_definition_index);
bool is_sniper(int item_definition_index);
bool is_knife(int item_definition_index);

// Paint kit <-> weapon compatibility, used by the skin changer.
bool paint_kit_fits_weapon(int paint_kit, int item_definition_index);

namespace info {
    inline int  max_clip(int id)  { return weapon::max_clip(id); }
    inline bool is_sniper(int id) { return weapon::is_sniper(id); }
}

} // namespace lacz::weapon