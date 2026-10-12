#include "widgets.hpp"
#include "../core/settings.hpp"
#include <imgui_internal.h>
#include <unordered_map>

namespace lacz::gui {

static std::unordered_map<ImGuiID, anim_state> g_anims;

float animated::approach(anim_state& s, float target, float speed, float dt) {
    float k = 1.f - std::exp(-speed * dt);
    s.value += (target - s.value) * k;
    if (std::fabs(target - s.value) < 0.0005f)
        s.value = target;
    return s.value;
}

anim_state& animated::get(ImGuiID id) {
    return g_anims[id];
}

void animated::clear() {
    g_anims.clear();
}

static float dt() {
    return std::clamp(ImGui::GetIO().DeltaTime, 0.001f, 0.05f);
}

static float anim_speed() {
    return std::clamp(cfg::g.gui.anim_speed, 0.2f, 3.0f);
}

void theme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowPadding = ImVec2(0, 0);
    s.FramePadding = ImVec2(14, 8);
    s.ItemSpacing = ImVec2(10, 12);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.ScrollbarSize = 6.f;
    s.GrabMinSize = 6.f;
    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.PopupBorderSize = 0.f;
    s.FrameBorderSize = 0.f;
    s.WindowRounding = 12.f;
    s.ChildRounding = 8.f;
    s.FrameRounding = 6.f;
    s.PopupRounding = 8.f;
    s.ScrollbarRounding = 12.f;
    s.GrabRounding = 4.f;
    s.TabRounding = 6.f;
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    auto* c = s.Colors;
    c[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.96f, 1.00f);
    c[ImGuiCol_TextDisabled]          = ImVec4(0.45f, 0.45f, 0.52f, 1.00f);
    c[ImGuiCol_WindowBg]              = ImVec4(0.055f, 0.055f, 0.075f, 1.00f);
    c[ImGuiCol_ChildBg]               = ImVec4(0.075f, 0.075f, 0.098f, 0.85f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.10f, 0.96f);
    c[ImGuiCol_Border]                = ImVec4(0.22f, 0.22f, 0.28f, 0.60f);
    c[ImGuiCol_FrameBg]               = ImVec4(0.11f, 0.11f, 0.14f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive]         = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.35f, 0.42f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.45f, 0.45f, 0.55f, 1.00f);
    c[ImGuiCol_CheckMark]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]            = ImVec4(0.70f, 0.70f, 0.80f, 1.00f);
    c[ImGuiCol_SliderGrabActive]      = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_Button]                = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.20f, 0.26f, 1.00f);
    c[ImGuiCol_ButtonActive]          = ImVec4(0.26f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_Header]                = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
    c[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    c[ImGuiCol_HeaderActive]          = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    c[ImGuiCol_Separator]             = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    c[ImGuiCol_TextSelectedBg]        = ImVec4(0.55f, 0.30f, 0.90f, 0.45f);
    c[ImGuiCol_NavHighlight]          = ImVec4(0.55f, 0.30f, 0.90f, 1.00f);
}

ImU32 to_u32(const float color[4], float alpha_mul) {
    return IM_COL32(static_cast<int>(color[0] * 255),
                    static_cast<int>(color[1] * 255),
                    static_cast<int>(color[2] * 255),
                    static_cast<int>(std::clamp(color[3] * alpha_mul, 0.f, 1.f) * 255));
}

ImVec4 to_vec4(const float color[4]) {
    return ImVec4(color[0], color[1], color[2], color[3]);
}

bool checkbox(const char* label, bool* value, const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float box = 20.f;
    const float row_h = (description && *description) ? 42.f : 30.f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        *value = !*value;

    auto& anim = animated::get(id);
    float fill = animated::approach(anim, *value ? 1.f : 0.f, 14.f * anim_speed(), dt());

    ImDrawList* dl = window->DrawList;
    ImVec2 box_min(pos.x, pos.y + (row_h - box) * 0.5f);
    ImVec2 box_max(box_min.x + box, box_min.y + box);

    ImU32 bg = IM_COL32(28, 28, 36, 255);
    ImU32 border = hovered ? render::accent_alpha(0.9f)
                           : IM_COL32(60, 60, 72, 255);

    ImU32 fill_col = render::accent_alpha(fill);
    dl->AddRectFilled(box_min, box_max, bg, 5.f);
    if (fill > 0.01f) {
        dl->AddRectFilled(ImVec2(box_min.x, box_max.y - box * fill),
                          box_max, fill_col, 5.f);
    }
    dl->AddRect(box_min, box_max, border, 5.f, 0, hovered ? 1.8f : 1.4f);

    if (fill > 0.15f) {
        ImU32 check = IM_COL32(255, 255, 255,
                               static_cast<int>(std::clamp(fill, 0.f, 1.f) * 255.f));
        ImVec2 p1(box_min.x + box * 0.24f, box_min.y + box * 0.52f);
        ImVec2 p2(box_min.x + box * 0.44f, box_min.y + box * 0.72f);
        ImVec2 p3(box_min.x + box * 0.78f, box_min.y + box * 0.28f);
        float t = std::clamp((fill - 0.15f) / 0.85f, 0.f, 1.f);
        ImVec2 mid(p1.x + (p2.x - p1.x) * std::min(t * 2.f, 1.f),
                   p1.y + (p2.y - p1.y) * std::min(t * 2.f, 1.f));
        dl->AddLine(p1, mid, check, 2.4f);
        if (t > 0.5f) {
            float t2 = (t - 0.5f) * 2.f;
            ImVec2 mid2(p2.x + (p3.x - p2.x) * t2, p2.y + (p3.y - p2.y) * t2);
            dl->AddLine(p2, mid2, check, 2.4f);
        }
    }

    ImU32 label_col = hovered ? IM_COL32(255, 255, 255, 255)
                              : IM_COL32(220, 220, 228, 255);
    dl->AddText(ImVec2(box_max.x + 12.f, pos.y + (description ? 3.f : (row_h - 14.f) * 0.5f)),
                label_col, label);
    if (description && *description)
        dl->AddText(ImVec2(box_max.x + 12.f, pos.y + 21.f),
                    IM_COL32(110, 110, 125, 255), description);

    return pressed;
}

bool switch_toggle(const char* label, bool* value, const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float w = 44.f, h = 22.f;
    const float row_h = (description && *description) ? 42.f : 32.f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        *value = !*value;

    auto& anim = animated::get(id);
    float t = animated::approach(anim, *value ? 1.f : 0.f, 14.f * anim_speed(), dt());

    ImDrawList* dl = window->DrawList;
    ImVec2 sw_min(pos.x + avail.x - w, pos.y + (row_h - h) * 0.5f);
    ImVec2 sw_max(sw_min.x + w, sw_min.y + h);

    ImU32 off_col = IM_COL32(52, 52, 64, 255);
    ImU32 on_col = render::accent(0.f);
    auto lerp_u32 = [](ImU32 a, ImU32 b, float t) {
        int ar=(a>>0)&0xFF, ag=(a>>8)&0xFF, ab=(a>>16)&0xFF, aa=(a>>24)&0xFF;
        int br=(b>>0)&0xFF, bg=(b>>8)&0xFF, bb=(b>>16)&0xFF, ba=(b>>24)&0xFF;
        return IM_COL32((int)(ar+(br-ar)*t), (int)(ag+(bg-ag)*t),
                        (int)(ab+(bb-ab)*t), (int)(aa+(ba-aa)*t));
    };
    dl->AddRectFilled(sw_min, sw_max, lerp_u32(off_col, on_col, t), h * 0.5f);

    float knob_r = h * 0.5f - 3.f;
    float knob_x = sw_min.x + h * 0.5f + (w - h) * t;
    dl->AddCircleFilled(ImVec2(knob_x, sw_min.y + h * 0.5f), knob_r,
                        IM_COL32(240, 240, 245, 255), 24);

    ImU32 label_col = hovered ? IM_COL32(255,255,255,255) : IM_COL32(220,220,228,255);
    dl->AddText(ImVec2(pos.x, pos.y + (description ? 3.f : (row_h - 14.f) * 0.5f)),
                label_col, label);
    if (description && *description)
        dl->AddText(ImVec2(pos.x, pos.y + 21.f), IM_COL32(110,110,125,255), description);

    return pressed;
}

bool slider_float(const char* label, float* value, float min, float max,
                  const char* format, const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float row_h = (description && *description) ? 44.f : 36.f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false, held = false;
    bool active = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_MouseButtonLeft);
    if (held) {
        float mouse_x = ImGui::GetIO().MousePos.x;
        float frac = std::clamp((mouse_x - pos.x) / avail.x, 0.f, 1.f);
        *value = min + frac * (max - min);
    }
    bool changed = active;

    ImDrawList* dl = window->DrawList;
    ImVec2 track_min(pos.x, pos.y + (description ? 28.f : 20.f));
    ImVec2 track_max(pos.x + avail.x, track_min.y + 6.f);

    auto& anim = animated::get(id);
    float frac = std::clamp((*value - min) / (max - min), 0.f, 1.f);
    float shown = animated::approach(anim, frac, 18.f * anim_speed(), dt());

    dl->AddRectFilled(track_min, track_max, IM_COL32(40, 40, 50, 255), 3.f);
    ImVec2 fill_max(track_min.x + (track_max.x - track_min.x) * shown, track_max.y);
    dl->AddRectFilledMultiColor(track_min, fill_max,
                                render::accent(0.f), render::accent(0.5f),
                                render::accent(0.5f), render::accent(0.f));
    if (hovered || active)
        dl->AddCircleFilled(ImVec2(fill_max.x, track_min.y + 3.f), 6.f,
                            IM_COL32(255, 255, 255, 255), 20);
    else
        dl->AddCircleFilled(ImVec2(fill_max.x, track_min.y + 3.f), 5.f,
                            render::accent(0.f), 20);

    dl->AddText(ImVec2(pos.x, pos.y), IM_COL32(220,220,228,255), label);
    char buf[64];
    snprintf(buf, sizeof(buf), format, *value);
    ImVec2 tsz = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(pos.x + avail.x - tsz.x, pos.y), render::accent(0.f), buf);
    if (description && *description)
        dl->AddText(ImVec2(pos.x, pos.y + 14.f), IM_COL32(110,110,125,255), description);

    return changed;
}

bool slider_int(const char* label, int* value, int min, int max,
                const char* format, const char* description) {
    float f = static_cast<float>(*value);
    bool changed = slider_float(label, &f, static_cast<float>(min),
                                static_cast<float>(max), format, description);
    int rounded = static_cast<int>(std::round(f));
    if (rounded != *value) {
        *value = rounded;
        return true;
    }
    return changed;
}

bool combo(const char* label, int* index, const char* const items[], int count,
           const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float row_h = (description && *description) ? 44.f : 34.f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        ImGui::OpenPopup(label);

    ImDrawList* dl = window->DrawList;
    ImVec2 box_min(pos.x + avail.x - 150.f, pos.y + 2.f);
    ImVec2 box_max(pos.x + avail.x, pos.y + row_h - 2.f);
    dl->AddRectFilled(box_min, box_max, IM_COL32(28, 28, 36, 255), 6.f);
    dl->AddRect(box_min, box_max,
                hovered ? render::accent_alpha(0.9f) : IM_COL32(60, 60, 72, 255),
                6.f, 0, hovered ? 1.6f : 1.2f);

    const char* current = (*index >= 0 && *index < count) ? items[*index] : "none";
    dl->AddText(ImVec2(box_min.x + 10.f, box_min.y + (box_max.y - box_min.y - 14.f) * 0.5f),
                IM_COL32(230, 230, 238, 255), current);
    dl->AddTriangleFilled(
        ImVec2(box_max.x - 18.f, box_min.y + 12.f),
        ImVec2(box_max.x - 10.f, box_min.y + 12.f),
        ImVec2(box_max.x - 14.f, box_min.y + 18.f),
        render::accent(0.f));

    dl->AddText(ImVec2(pos.x, pos.y + 3.f), IM_COL32(220,220,228,255), label);
    if (description && *description)
        dl->AddText(ImVec2(pos.x, pos.y + 21.f), IM_COL32(110,110,125,255), description);

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 4.f));
    ImGui::SetNextWindowSize(ImVec2(box_max.x - box_min.x, 0));
    if (ImGui::BeginPopup(label)) {
        for (int i = 0; i < count; ++i) {
            bool selected = (*index == i);
            if (ImGui::Selectable(items[i], selected)) {
                *index = i;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    return changed;
}

bool multi_combo(const char* label, int* mask, const char* const items[], int count,
                 const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float row_h = (description && *description) ? 44.f : 34.f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        ImGui::OpenPopup(label);

    ImDrawList* dl = window->DrawList;
    ImVec2 box_min(pos.x + avail.x - 150.f, pos.y + 2.f);
    ImVec2 box_max(pos.x + avail.x, pos.y + row_h - 2.f);
    dl->AddRectFilled(box_min, box_max, IM_COL32(28, 28, 36, 255), 6.f);
    dl->AddRect(box_min, box_max,
                hovered ? render::accent_alpha(0.9f) : IM_COL32(60, 60, 72, 255),
                6.f, 0, hovered ? 1.6f : 1.2f);

    std::string text;
    int selected_count = 0;
    for (int i = 0; i < count; ++i) {
        if (*mask & (1 << i)) {
            if (!text.empty()) text += ", ";
            text += items[i];
            selected_count++;
        }
    }
    if (selected_count == 0) text = "none";
    else if (selected_count > 2) text = std::to_string(selected_count) + " selected";

    dl->AddText(ImVec2(box_min.x + 10.f, box_min.y + (box_max.y - box_min.y - 14.f) * 0.5f),
                IM_COL32(230, 230, 238, 255), text.c_str());

    dl->AddText(ImVec2(pos.x, pos.y + 3.f), IM_COL32(220,220,228,255), label);
    if (description && *description)
        dl->AddText(ImVec2(pos.x, pos.y + 21.f), IM_COL32(110,110,125,255), description);

    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(box_min.x, box_max.y + 4.f));
    ImGui::SetNextWindowSize(ImVec2(box_max.x - box_min.x, 0));
    if (ImGui::BeginPopup(label)) {
        for (int i = 0; i < count; ++i) {
            bool active = (*mask & (1 << i)) != 0;
            if (ImGui::Checkbox(items[i], &active)) {
                if (active) *mask |= (1 << i);
                else *mask &= ~(1 << i);
                changed = true;
            }
        }
        ImGui::EndPopup();
    }
    return changed;
}

static const char* key_name(int vk) {
    if (vk == 0) return "none";
    if (vk == VK_LBUTTON) return "M1";
    if (vk == VK_RBUTTON) return "M2";
    if (vk == VK_MBUTTON) return "M3";
    if (vk == VK_XBUTTON1) return "M4";
    if (vk == VK_XBUTTON2) return "M5";
    if (vk == VK_SHIFT) return "SHIFT";
    if (vk == VK_CONTROL) return "CTRL";
    if (vk == VK_MENU) return "ALT";

    static char buf[16]{};
    UINT sc = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (GetKeyNameTextA(sc << 16, buf, sizeof(buf)))
        return buf;
    snprintf(buf, sizeof(buf), "0x%X", vk);
    return buf;
}

bool keybind(const char* label, int* key, const char* description) {
    static ImGuiID listening = 0;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float row_h = (description && *description) ? 44.f : 34.f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    ImVec2 box_min(pos.x + avail.x - 110.f, pos.y + 2.f);
    ImVec2 box_max(pos.x + avail.x, pos.y + row_h - 2.f);
    ImRect box_bb(box_min, box_max);

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);
    if (pressed) {
        listening = (listening == id) ? 0 : id;
    }
    bool is_listening = (listening == id);

    bool changed = false;
    if (is_listening) {
        for (int vk = 1; vk < 256; ++vk) {
            if (vk == VK_INSERT) continue;
            if (GetAsyncKeyState(vk) & 0x8000) {
                if (vk == VK_ESCAPE) { *key = 0; changed = true; }
                else { *key = vk; changed = true; }
                listening = 0;
                break;
            }
        }
    }

    ImDrawList* dl = window->DrawList;
    ImU32 border = is_listening ? render::accent(0.f)
                 : hovered ? render::accent_alpha(0.9f) : IM_COL32(60, 60, 72, 255);
    dl->AddRectFilled(box_min, box_max, IM_COL32(28, 28, 36, 255), 6.f);
    dl->AddRect(box_min, box_max, border, 6.f, 0, (hovered || is_listening) ? 1.6f : 1.2f);

    const char* txt = is_listening ? "press a key" : key_name(*key);
    ImVec2 tsz = ImGui::CalcTextSize(txt);
    dl->AddText(ImVec2(box_min.x + (box_max.x - box_min.x - tsz.x) * 0.5f,
                       box_min.y + (box_max.y - box_min.y - tsz.y) * 0.5f),
                is_listening ? render::accent(0.f) : IM_COL32(230, 230, 238, 255), txt);

    dl->AddText(ImVec2(pos.x, pos.y + 3.f), IM_COL32(220,220,228,255), label);
    if (description && *description)
        dl->AddText(ImVec2(pos.x, pos.y + 21.f), IM_COL32(110,110,125,255), description);

    return changed;
}

bool color_edit(const char* label, float color[4], bool alpha) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiID id = window->GetID(label);
    const float row_h = 34.f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImRect bb(pos, ImVec2(pos.x + avail.x, pos.y + row_h));

    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    ImVec2 box_min(pos.x + avail.x - 60.f, pos.y + 4.f);
    ImVec2 box_max(pos.x + avail.x, pos.y + row_h - 4.f);
    ImRect box_bb(box_min, box_max);

    bool hovered = false, held = false;
    bool pressed = ImGui::ButtonBehavior(box_bb, id, &hovered, &held);
    if (pressed)
        ImGui::OpenPopup(label);

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(box_min, box_max, IM_COL32(200, 200, 200, 255), 5.f);
    dl->AddRectFilledMultiColor(box_min, box_max,
                                to_u32(color, 1.f), to_u32(color, 1.f),
                                to_u32(color, 1.f), to_u32(color, 1.f));
    dl->AddRect(box_min, box_max,
                hovered ? render::accent_alpha(0.9f) : IM_COL32(60, 60, 72, 255),
                5.f, 0, hovered ? 1.6f : 1.2f);

    dl->AddText(ImVec2(pos.x, pos.y + 8.f), IM_COL32(220,220,228,255), label);

    bool changed = false;
    if (ImGui::BeginPopup(label)) {
        changed = ImGui::ColorPicker4("##picker", color,
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar |
            ImGuiColorEditFlags_NoInputs);
        ImGui::EndPopup();
    }
    return changed;
}

bool text_input(const char* label, char* buffer, size_t size, const char* description) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const float row_h = (description && *description) ? 44.f : 34.f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + 3.f),
        IM_COL32(220,220,228,255), label);
    if (description && *description)
        ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + 21.f),
            IM_COL32(110,110,125,255), description);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + avail.x - 150.f, pos.y + 2.f));
    ImGui::SetNextItemWidth(150.f);
    bool changed = ImGui::InputText(label, buffer, size);
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + row_h));
    return changed;
}

bool button(const char* label, const ImVec2& size, bool accent) {
    ImVec2 sz = size;
    if (sz.x == 0.f)
        sz = ImVec2(ImGui::GetContentRegionAvail().x, 32.f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    if (accent) {
        ImVec4 a = ImGui::ColorConvertU32ToFloat4(render::accent(0.f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(a.x, a.y, a.z, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.x, a.y, a.z, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(a.x, a.y, a.z, 0.65f));
    }
    bool pressed = ImGui::Button(label, sz);
    if (accent)
        ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return pressed;
}

void section(const char* label) {
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.62f, 0.70f, 1.f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::CalcTextSize(label);
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pos.x, pos.y - 3.f), ImVec2(pos.x + sz.x + 2.f, pos.y - 1.f),
        render::accent(0.f));
    ImGui::Dummy(ImVec2(0, 2));
}

void separator_text(const char* label) {
    ImGui::Dummy(ImVec2(0, 3));
    section(label);
}

int tab_bar(const char* id, const char* const tabs[], int count, int current) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* dl = window->DrawList;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float tab_w = w / static_cast<float>(count);
    const float h = 38.f;

    int result = current;
    for (int i = 0; i < count; ++i) {
        ImRect rect(ImVec2(pos.x + i * tab_w, pos.y), ImVec2(pos.x + (i + 1) * tab_w, pos.y + h));
        ImGuiID tab_id = window->GetID((std::string(id) + tabs[i]).c_str());

        ImGui::ItemSize(rect);
        if (!ImGui::ItemAdd(rect, tab_id))
            continue;

        bool hovered = false, held = false;
        bool pressed = ImGui::ButtonBehavior(rect, tab_id, &hovered, &held);
        if (pressed)
            result = i;

        bool selected = (current == i);
        auto& anim = animated::get(tab_id);
        float t = animated::approach(anim, selected ? 1.f : (hovered ? 0.4f : 0.f),
                                     14.f * anim_speed(), dt());

        ImU32 col = IM_COL32(160, 160, 175,
                             static_cast<int>(140 + t * 115));
        ImVec2 tsz = ImGui::CalcTextSize(tabs[i]);
        dl->AddText(ImVec2(rect.Min.x + (tab_w - tsz.x) * 0.5f,
                           rect.Min.y + (h - tsz.y) * 0.5f), col, tabs[i]);

        if (t > 0.01f) {
            float bar_w = tab_w * 0.55f;
            float bx = rect.Min.x + (tab_w - bar_w) * 0.5f;
            dl->AddRectFilled(ImVec2(bx, rect.Max.y - 2.f),
                              ImVec2(bx + bar_w, rect.Max.y),
                              render::accent_alpha(t));
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + h));
    return result;
}

void begin_columns() {
    ImGui::BeginGroup();
    ImGui::BeginChild("##col_left", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 6.f, 0),
                      false, ImGuiWindowFlags_NoBackground);
}

void end_columns() {
    ImGui::EndChild();
    ImGui::EndGroup();
}

void next_column() {
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##col_right", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);
}

} // namespace lacz::gui
