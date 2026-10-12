#pragma once
#include "../core/common.hpp"

struct IDXGISwapChain;

namespace lacz::render {

// Hook initialization: installs the Present/ResizeBuffers hooks through a
// throwaway dummy device so we never need a window handle.
bool initialize();
void shutdown();

// Our own ImGui frame, invoked from the Present detour.
void begin_frame();
void end_frame();

// Called when the game resizes the back buffer.
void on_resize(UINT width, UINT height);

// Native D3D objects -- shared with the renderer and any world drawing.
ID3D11Device* device();
ID3D11DeviceContext* context();
IDXGISwapChain* swap_chain();
UINT buffer_width();
UINT buffer_height();

} // namespace lacz::render