#pragma once
#include "../core/common.hpp"
#include <imgui.h>

namespace lacz::gui::menu {

void initialize();
void render();

void toggle();
bool is_open();
bool wants_input();

// Called by the WndProc to route input only while open.
void set_input_active(bool active);

} // namespace lacz::gui::menu