#pragma once
#include "math.hpp"
#include "offsets.hpp"

namespace lacz {

// ---------------------------------------------------------------------------
// Opaque interface handles. We only ever call known virtuals on these, so the
// concrete class layout is irrelevant.
// ---------------------------------------------------------------------------
namespace iface {

struct create_interface_fn {
    void* (*fn)(const char* name, int* return_code);
};

void* get(const char* module, const char* name);

// Resolve every interface we need. Returns false on a missing mandatory one.
bool resolve();

inline void* game_client = nullptr;      // CSource2Client
inline void* engine = nullptr;           // CEngineClient
inline void* schema_system = nullptr;    // CSchemaSystem
inline void* entity_system = nullptr;    // CGameEntitySystem
inline void* input_system = nullptr;     // CInputSystem
inline void* cvar = nullptr;             // CCvar
inline void* material_system = nullptr;  // CMaterialSystem2
inline void* file_system = nullptr;      // CFileSystem
inline void* localize = nullptr;         // CLocalize
inline void* network_client = nullptr;   // CNetworkClientService

} // namespace iface

// ---------------------------------------------------------------------------
// Globals pulled out of client.dll by signature.
// ---------------------------------------------------------------------------
struct global_vars_base {
    float real_time = 0.f;
    int   frame_count = 0;
    float abs_frame_time = 0.f;
    float abs_frame_start_time = 0.f;
    float cur_time = 0.f;
    float frame_time = 0.f;
    int   max_clients = 0;
    int   tick_count = 0.f ? 0 : 0;
    float interval_per_tick = 0.f;
    float interpolation_amount = 0.f;
    int   sim_ticks_this_frame = 0;
    int   network_protocol = 0;
    void* m_pCurrentTime = nullptr;
};

struct player_info_t {
    u64 version = 0;
    u64 xuid = 0;
    char name[128]{};
    int user_id = 0;
    char guid[33]{};
    u32 friends_id = 0;
    char friends_name[128]{};
    bool fake_player = false;
    bool is_hltv = false;
    u32 custom_files[4]{};
    u8 files_downloaded = 0;
};

inline global_vars_base* global_vars = nullptr;

void* get_entity_system();
void* get_local_player();
void* get_local_pawn_direct();
global_vars_base* get_global_vars();

} // namespace lacz