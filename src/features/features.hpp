#pragma once
#include "../core/common.hpp"
#include "../sdk/entity.hpp"

// ---------------------------------------------------------------------------
// Feature facade. The render loop calls on_frame (state update), on_render
// (world drawing) and on_render_overlay (screen UI), in that order.
// ---------------------------------------------------------------------------
namespace lacz::features {

void initialize();
void on_frame();            // called once per Present, before ImGui NewFrame
void on_render();           // world/ESP drawing
void on_render_overlay();   // watermark, bomb timer, spectator list

const char* status_text();
const char* build_info();

// Shared target selection result, refreshed each frame.
struct target_t {
    c_cs_player_pawn* pawn = nullptr;
    int   hitbox = 0;
    vec3  point;
    float fov = 999.f;
    float distance = 0.f;
    int   damage = 0;
    bool  visible = false;
};

target_t* current_target();

// ---- Feature modules ------------------------------------------------------
namespace aimbot {
    void run();
}
namespace triggerbot {
    void run();
}
namespace rage {
    void run();
    void apply_accuracy();
}
namespace antiaim {
    void run(void* cmd);
}
namespace esp {
    void draw();
    void draw_overlay();
}
namespace chams {
    void run();
    void draw();
}
namespace skins {
    void apply_now();
    void run();
}
namespace misc {
    void run();
    void run_movement(void* cmd, float frame_time);
}

// Movement hooks invoked from CreateMove.
namespace movement {
    // Installs the CreateMove detour. Called once from initialize().
    bool install();
    void create_move(void* cmd, float fl_input);
}

} // namespace lacz::features