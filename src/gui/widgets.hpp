#pragma once
#include "../core/common.hpp"
#include "../render/renderer.hpp"
#include <imgui.h>

namespace lacz::gui {

// ---------------------------------------------------------------------------
// Animated widget kit.
//
// Every control keeps its own eased animation value so transitions feel smooth
// at any framerate. State is keyed by the widget's ImGui ID, which lets the
// menu re-layout without losing animation continuity.
// ---------------------------------------------------------------------------
struct anim_state {
    float value = 0.f;
    float velocity = 0.f;
};

class animated {
public:
    // Exponential ease toward `target` with a frame-rate independent factor.
    static float approach(anim_state& s, float target, float speed, float dt);
    static anim_state& get(ImGuiID id);
    static void clear();
};

// ---- Controls -------------------------------------------------------------
// All return true when the value changed this frame.

bool checkbox(const char* label, bool* value, const char* description = nullptr);
bool switch_toggle(const char* label, bool* value, const char* description = nullptr);
bool slider_float(const char* label, float* value, float min, float max,
                  const char* format = "%.2f", const char* description = nullptr);
bool slider_int(const char* label, int* value, int min, int max,
                const char* format = "%d", const char* description = nullptr);
bool combo(const char* label, int* index, const char* const items[], int count,
           const char* description = nullptr);
bool multi_combo(const char* label, int* mask, const char* const items[], int count,
                 const char* description = nullptr);
bool keybind(const char* label, int* key, const char* description = nullptr);
bool color_edit(const char* label, float color[4], bool alpha = true);
bool text_input(const char* label, char* buffer, size_t size, const char* description = nullptr);
bool button(const char* label, const ImVec2& size = ImVec2(0, 0), bool accent = false);

// Section header inside a tab body.
void section(const char* label);
void separator_text(const char* label);

// Applies the curated dark theme once at startup.
void theme();

// The animated tab strip used by the menu body.
int tab_bar(const char* id, const char* const tabs[], int count, int current);

// Convenience renderer for a group of two-column controls.
void begin_columns();
void end_columns();
void next_column();

ImU32 to_u32(const float color[4], float alpha_mul = 1.0f);
ImVec4 to_vec4(const float color[4]);

} // namespace lacz::gui