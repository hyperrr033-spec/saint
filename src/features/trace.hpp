#pragma once
#include "../sdk/math.hpp"
#include "../sdk/offsets.hpp"

namespace lacz {
struct c_cs_player_pawn;
namespace trace {

// ---------------------------------------------------------------------------
// CGameTrace is large; we only need a handful of fields. Field order matches
// Source 2's trace_t layout for the current build.
// ---------------------------------------------------------------------------
struct game_trace {
    void*  surface;
    void*  hit_entity;
    void*  hitbox_data;
    char   pad0[0x30];
    vec3   start_pos;
    vec3   end_pos;
    char   pad1[0x1C];
    vec3   normal;
    char   pad2[0x4];
    float  fraction;
    char   pad3[0xC];
    bool   all_solid;
    bool   start_solid;
};

struct ray_t {
    vec3 start;
    vec3 end;
    vec3 mins;
    vec3 maxs;
    char pad[0x40];
    int  type;
};

// ---------------------------------------------------------------------------
// Trace API. `trace_ray` is the game's CGameTrace::TraceShape / TraceRay.
// We pass a ray and a filter and let the engine fill the result.
// ---------------------------------------------------------------------------
struct trace_filter {
    char pad[0x8];
    u64  mask;
    char pad2[0x38];
    void* skip;
};

// Filter storage must outlive every trace that uses it; keep one on the
// caller's stack.
struct filter_storage {
    alignas(16) unsigned char bytes[0x400];
};

void init_filter(filter_storage& storage, c_cs_player_pawn* skip, int layer, int mask);

game_trace trace(const vec3& start, const vec3& end, void* filter, bool hull);

// Convenience: builds a default filter on this stack frame and traces.
game_trace trace_filtered(const vec3& start, const vec3& end, c_cs_player_pawn* skip);

game_trace trace_hull(const vec3& start, const vec3& end, const vec3& mins,
                      const vec3& maxs, void* filter);

// Full penetration damage calculation. Returns last damage dealt to `hit` on
// the path from start to end, applying damage falloff and wall penetration.
int damage_to(const vec3& start, const vec3& end, c_cs_player_pawn* target,
              int min_damage, bool* hit_entity_out = nullptr);

// Whether a ray from eye to point reaches the target unobstructed.
bool visible(const vec3& start, const vec3& end, c_cs_player_pawn* target);

} // namespace trace
} // namespace lacz