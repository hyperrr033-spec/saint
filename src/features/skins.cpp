#include "features.hpp"
#include "shared.hpp"
#include "../core/settings.hpp"
#include "../core/memory.hpp"
#include "../sdk/weapon.hpp"
#include "../sdk/offsets.hpp"

namespace lacz::features::skins {

using namespace shared;

// ---------------------------------------------------------------------------
// Knife / glove / weapon finish application.
//
// Item definition indices for every knife variant, indexed by the menu order.
// ---------------------------------------------------------------------------
static const int kKnifeItemDefs[] = {
    0,    // Default (do nothing)
    500,  // Bayonet
    505,  // Flip
    506,  // Gut
    507,  // Karambit
    508,  // M9 Bayonet
    509,  // Huntsman
    512,  // Falchion
    514,  // Bowie
    515,  // Butterfly
    516,  // Shadow Daggers
    519,  // Ursus
    520,  // Navaja
    522,  // Stiletto
    523,  // Talon
    525,  // Classic
    521,  // Paracord
    503,  // Survival
    502,  // Nomad
    526,  // Skeleton
};

static const int kGloveItemDefs[] = {
    0,    // Default
    5027, // Bloodhound
    5030, // Sport
    5031, // Driver
    5032, // Hand Wraps
    5033, // Moto
    5034, // Specialist
    5035, // Hydra
    5036, // Broken Fang
};

// Apply a finish to a weapon entity.
static void apply_finish(c_base_weapon* weapon, int item_def) {
    if (!weapon)
        return;

    auto& s = cfg::g.skins;

    // Force the entity to be treated as a fresh item so the game rebuilds it.
    weapon->set_item_id_high(-1);
    weapon->set_item_id_high(0);

    // Fallback fields are what the client uses when the item is not owned.
    if (s.paint_kit > 0 && weapon::paint_kit_fits_weapon(s.paint_kit, item_def))
        weapon->set_paint_kit(s.paint_kit);
    weapon->set_seed(s.seed);
    weapon->set_wear(s.wear);
    weapon->set_stat_trak(s.stat_trak);
    (void)item_def;
}

// Swap the local knife model by writing the weapon's item definition index.
static void apply_knife() {
    auto& s = cfg::g.skins;
    if (!s.knives || s.knife_model <= 0)
        return;
    if (s.knife_model >= static_cast<int>(sizeof(kKnifeItemDefs) / sizeof(int)))
        return;

    int target_def = kKnifeItemDefs[s.knife_model];
    if (target_def == 0)
        return;

    auto* local = g_ctx.local;
    if (!local)
        return;

    auto* weapon = local->active_weapon();
    if (!weapon || !weapon->is_knife())
        return;

    // Replace the networkable class by definition index. The game regenerates
    // the model from the new def on the next frame.
    mem::write<int>(reinterpret_cast<uptr>(weapon) + off::netvars.m_iItemDefinitionIndex, target_def);
    apply_finish(weapon, target_def);
}

static void apply_gloves() {
    auto& s = cfg::g.skins;
    if (!s.gloves || s.glove_model <= 0)
        return;
    if (s.glove_model >= static_cast<int>(sizeof(kGloveItemDefs) / sizeof(int)))
        return;

    int target_def = kGloveItemDefs[s.glove_model];
    if (target_def == 0)
        return;

    // Gloves are wearables attached to the pawn; find the first wearable slot
    // and rewrite its definition index.
    auto* local = g_ctx.local;
    if (!local)
        return;

    uptr wearables = reinterpret_cast<uptr>(local) + off::netvars.m_hMyWearables;
    u32 handle = mem::read<u32>(wearables, 0);
    if (!handle)
        return;
    int index = handle & 0x7FFF;
    auto* glove_ent = entity_by_index(index);
    if (!glove_ent)
        return;
    mem::write<int>(reinterpret_cast<uptr>(glove_ent) + off::netvars.m_iItemDefinitionIndex, target_def);
}

void apply_now() {
    if (!g_ctx.valid || !g_ctx.local)
        return;
    auto* weapon = g_ctx.local->active_weapon();
    if (weapon)
        apply_finish(weapon, weapon->item_definition_index());
    apply_knife();
    apply_gloves();
}

void run() {
    if (!g_ctx.valid || !g_ctx.local)
        return;

    // Re-apply only when the held weapon or its finish differs from config.
    static int last_def = 0;
    static int last_kit = -1;
    static int last_knife = -1;

    auto* weapon = g_ctx.local->active_weapon();
    if (weapon) {
        int def = weapon->item_definition_index();
        bool needs = false;
        if (def != last_def) { last_def = def; needs = true; }
        if (cfg::g.skins.paint_kit != last_kit) { last_kit = cfg::g.skins.paint_kit; needs = true; }
        if (needs)
            apply_finish(weapon, def);
    }

    if (cfg::g.skins.knife_model != last_knife) {
        last_knife = cfg::g.skins.knife_model;
        apply_knife();
    }
    apply_gloves();
}

} // namespace lacz::features::skins