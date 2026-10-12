#include "renderer.hpp"
#include "render.hpp"
#include "../core/settings.hpp"
#include <imgui_internal.h>
#include <cstdarg>

namespace lacz::render {

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;

bool init_draw(ID3D11Device* device, ID3D11DeviceContext* context) {
    g_device = device;
    g_context = context;
    return true;
}

void shutdown_draw() {
    g_device = nullptr;
    g_context = nullptr;
}

static ImDrawList* list() { return ImGui::GetBackgroundDrawList(); }

void line(const vec2& a, const vec2& b, ImU32 color, float thickness) {
    list()->AddLine(ImVec2(a.x, a.y), ImVec2(b.x, b.y), color, thickness);
}

void rect(const vec2& min, const vec2& max, ImU32 color, float thickness, float rounding) {
    list()->AddRect(ImVec2(min.x, min.y), ImVec2(max.x, max.y), color, rounding, 0, thickness);
}

void rect_filled(const vec2& min, const vec2& max, ImU32 color, float rounding) {
    list()->AddRectFilled(ImVec2(min.x, min.y), ImVec2(max.x, max.y), color, rounding);
}

void rect_filled_multi(const vec2& min, const vec2& max,
                       ImU32 tl, ImU32 tr, ImU32 br, ImU32 bl, float rounding) {
    list()->AddRectFilledMultiColor(ImVec2(min.x, min.y), ImVec2(max.x, max.y),
                                    tl, tr, br, bl);
    (void)rounding;
}

void corner_box(const vec2& min, const vec2& max, ImU32 color, float thickness, float fraction) {
    float w = max.x - min.x;
    float h = max.y - min.y;
    float len = std::min(w, h) * fraction;

    // top-left
    list()->AddLine(ImVec2(min.x, min.y), ImVec2(min.x + len, min.y), color, thickness);
    list()->AddLine(ImVec2(min.x, min.y), ImVec2(min.x, min.y + len), color, thickness);
    // top-right
    list()->AddLine(ImVec2(max.x - len, min.y), ImVec2(max.x, min.y), color, thickness);
    list()->AddLine(ImVec2(max.x, min.y), ImVec2(max.x, min.y + len), color, thickness);
    // bottom-left
    list()->AddLine(ImVec2(min.x, max.y - len), ImVec2(min.x, max.y), color, thickness);
    list()->AddLine(ImVec2(min.x, max.y), ImVec2(min.x + len, max.y), color, thickness);
    // bottom-right
    list()->AddLine(ImVec2(max.x - len, max.y - len), ImVec2(max.x, max.y - len), color, thickness);
    list()->AddLine(ImVec2(max.x - len, max.y), ImVec2(max.x, max.y), color, thickness);
}

void circle(const vec2& center, float radius, ImU32 color, int segments, float thickness) {
    list()->AddCircle(ImVec2(center.x, center.y), radius, color, segments, thickness);
}

void circle_filled(const vec2& center, float radius, ImU32 color, int segments) {
    list()->AddCircleFilled(ImVec2(center.x, center.y), radius, color, segments);
}

void triangle(const vec2& a, const vec2& b, const vec2& c, ImU32 color, float thickness) {
    list()->AddTriangle(ImVec2(a.x, a.y), ImVec2(b.x, b.y), ImVec2(c.x, c.y), color, thickness);
}

void triangle_filled(const vec2& a, const vec2& b, const vec2& c, ImU32 color) {
    list()->AddTriangleFilled(ImVec2(a.x, a.y), ImVec2(b.x, b.y), ImVec2(c.x, c.y), color);
}

void text(const vec2& pos, ImU32 color, const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    list()->AddText(ImVec2(pos.x, pos.y), color, buffer);
}

void text_shadow(const vec2& pos, ImU32 color, const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    list()->AddText(ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, 180), buffer);
    list()->AddText(ImVec2(pos.x, pos.y), color, buffer);
}

void text_centered(const vec2& center, ImU32 color, const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    ImVec2 size = ImGui::CalcTextSize(buffer);
    list()->AddText(ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f), color, buffer);
}

vec2 text_size(const char* text) {
    ImVec2 size = ImGui::CalcTextSize(text);
    return {size.x, size.y};
}

void gradient_rect(const vec2& min, const vec2& max, ImU32 top, ImU32 bottom) {
    list()->AddRectFilledMultiColor(ImVec2(min.x, min.y), ImVec2(max.x, max.y),
                                    top, top, bottom, bottom);
}

ImU32 health_color(float health) {
    float h = std::clamp(health, 0.f, 100.f) / 100.f;
    int r = static_cast<int>((1.f - h) * 255.f);
    int g = static_cast<int>(h * 255.f);
    return IM_COL32(r, g, 0, 255);
}

static float hue_shift(float base, float offset) {
    float v = std::fmod(base + offset, 1.0f);
    return v < 0.f ? v + 1.f : v;
}

static ImU32 hsv(float h, float s, float v, int a = 255) {
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255),
                    static_cast<int>(b * 255), a);
}

ImU32 accent(float offset) {
    const auto& gui = cfg::g.gui;
    if (gui.rainbow) {
        float t = static_cast<float>(ImGui::GetTime()) * 0.15f * gui.anim_speed;
        return hsv(hue_shift(0.78f, t + offset), 0.62f, 0.95f);
    }
    float r = gui.accent[0], g = gui.accent[1], b = gui.accent[2];
    // Blend toward accent2 by offset for gradients.
    if (offset > 0.f) {
        float k = std::clamp(offset, 0.f, 1.f);
        r = r * (1.f - k) + gui.accent2[0] * k;
        g = g * (1.f - k) + gui.accent2[1] * k;
        b = b * (1.f - k) + gui.accent2[2] * k;
    }
    return IM_COL32(static_cast<int>(r * 255), static_cast<int>(g * 255),
                    static_cast<int>(b * 255), 255);
}

ImU32 accent_alpha(float alpha, float offset) {
    ImU32 c = accent(offset);
    return (c & 0x00FFFFFF) | (static_cast<int>(std::clamp(alpha, 0.f, 1.f) * 255.f) << 24);
}

vec2 screen_size() {
    ImGuiIO& io = ImGui::GetIO();
    return {io.DisplaySize.x, io.DisplaySize.y};
}

void spinner(const vec2& center, float radius, float thickness, ImU32 color) {
    ImDrawList* dl = list();
    float time = static_cast<float>(ImGui::GetTime());
    const int segments = 30;
    const float start = time * 4.f;
    const float arc = 4.5f;
    dl->PathClear();
    for (int i = 0; i <= segments; ++i) {
        float a = start + (arc * static_cast<float>(i) / segments);
        dl->PathLineTo(ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
    }
    dl->PathStroke(color, 0, thickness);
}

} // namespace lacz::render