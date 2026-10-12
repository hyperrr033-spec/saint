#pragma once
#include "../core/common.hpp"
#include "../sdk/math.hpp"
#include <imgui.h>

namespace lacz::render {

bool init_draw(ID3D11Device* device, ID3D11DeviceContext* context);
void shutdown_draw();

// ---------------------------------------------------------------------------
// Primitive helpers. All coordinates are in screen space; the menu and the
// world ESP share these so styling stays consistent.
// ---------------------------------------------------------------------------
void line(const vec2& a, const vec2& b, ImU32 color, float thickness = 1.0f);
void rect(const vec2& min, const vec2& max, ImU32 color, float thickness = 1.0f, float rounding = 0.f);
void rect_filled(const vec2& min, const vec2& max, ImU32 color, float rounding = 0.f);
void rect_filled_multi(const vec2& min, const vec2& max,
                       ImU32 top_left, ImU32 top_right, ImU32 bottom_right, ImU32 bottom_left,
                       float rounding = 0.f);
void corner_box(const vec2& min, const vec2& max, ImU32 color, float thickness, float fraction);
void circle(const vec2& center, float radius, ImU32 color, int segments = 32, float thickness = 1.f);
void circle_filled(const vec2& center, float radius, ImU32 color, int segments = 32);
void triangle(const vec2& a, const vec2& b, const vec2& c, ImU32 color, float thickness = 1.f);
void triangle_filled(const vec2& a, const vec2& b, const vec2& c, ImU32 color);
void text(const vec2& pos, ImU32 color, const char* fmt, ...);
void text_shadow(const vec2& pos, ImU32 color, const char* fmt, ...);
void text_centered(const vec2& center, ImU32 color, const char* fmt, ...);
vec2 text_size(const char* text);
void gradient_rect(const vec2& min, const vec2& max, ImU32 top, ImU32 bottom);

// Health gradient (green -> red).
ImU32 health_color(float health);
// Animated accent built from the two configured accent colors.
ImU32 accent(float offset = 0.f);
ImU32 accent_alpha(float alpha, float offset = 0.f);

// Screen dimensions as ImGui currently sees them.
vec2 screen_size();

// Rounded/rotating spinner used by some widgets.
void spinner(const vec2& center, float radius, float thickness, ImU32 color);

} // namespace lacz::render