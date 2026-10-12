#include "render.hpp"
#include "renderer.hpp"
#include "../gui/menu.hpp"
#include "../features/features.hpp"
#include "../core/hooks.hpp"
#include "../core/settings.hpp"
#include "../core/log.hpp"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace lacz::render {

using present_t = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using resize_t  = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using wndproc_t = LRESULT(__stdcall*)(HWND, UINT, WPARAM, LPARAM);

static present_t o_present = nullptr;
static resize_t  o_resize = nullptr;
static wndproc_t o_wndproc = nullptr;

static ID3D11Device*           g_device = nullptr;
static ID3D11DeviceContext*    g_context = nullptr;
static IDXGISwapChain*         g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static HWND                    g_hwnd = nullptr;
static WNDPROC                 g_original_wndproc = nullptr;
static UINT                    g_width = 0, g_height = 0;
static bool                    g_imgui_ready = false;
static bool                    g_initialized = false;
static ImVec2                  g_mouse_pos{ 0.f, 0.f };

ID3D11Device* device() { return g_device; }
ID3D11DeviceContext* context() { return g_context; }
IDXGISwapChain* swap_chain() { return g_swap; }
UINT buffer_width() { return g_width; }
UINT buffer_height() { return g_height; }

static void create_rtv() {
    if (!g_swap || !g_device)
        return;
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void**>(&back))) && back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}

static void release_rtv() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

static LRESULT __stdcall wndproc_detour(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // If the swap-chain init never finished (or tore down), fall straight
    // through. Calling CallWindowProcW with a null original here is a hard AV
    // when the game pumps WM_ messages mid-injection.
    if (!o_wndproc)
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    if (msg == WM_KEYDOWN && wparam == static_cast<WPARAM>(cfg::g.gui.menu_key)) {
        if (!(lparam & 0x40000000)) {
            gui::menu::toggle();
            if (gui::menu::is_open()) {
                ClipCursor(nullptr);
            }
        }
        return TRUE;
    }

    if (g_imgui_ready) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
            return TRUE;
    }

if (gui::menu::is_open()) {
        switch (msg) {
            case WM_ACTIVATE:
            case WM_ACTIVATEAPP:
            case WM_NCACTIVATE:
                // Never swallow activation while the menu is open: the game
                // stops receiving focus/redraws and the window appears frozen.
                break;
            case WM_MOUSEMOVE:
            case WM_NCMOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_INPUT:
            case WM_CHAR:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SETCURSOR:
                return TRUE;
            default:
                break;
        }
    }

    return CallWindowProcW(o_wndproc, hwnd, msg, wparam, lparam);
}

static HRESULT __stdcall present_detour(IDXGISwapChain* swap, UINT sync, UINT flags) {
    g_swap = swap;

    if (!g_initialized) {
        if (SUCCEEDED(swap->GetDevice(__uuidof(ID3D11Device),
                                      reinterpret_cast<void**>(&g_device))) && g_device) {
            g_device->GetImmediateContext(&g_context);

            DXGI_SWAP_CHAIN_DESC desc{};
            if (SUCCEEDED(swap->GetDesc(&desc)))
                g_hwnd = desc.OutputWindow;

            if (!g_hwnd)
                g_hwnd = FindWindowW(L"SDL_app", L"Counter-Strike 2");
            if (!g_hwnd)
                g_hwnd = GetForegroundWindow();

            create_rtv();

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;

            if (g_hwnd) {
                ImGui_ImplWin32_Init(g_hwnd);
                g_original_wndproc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                                      reinterpret_cast<LONG_PTR>(wndproc_detour)));
                o_wndproc = reinterpret_cast<wndproc_t>(g_original_wndproc);
            }
            ImGui_ImplDX11_Init(g_device, g_context);

            init_draw(g_device, g_context);
            gui::menu::initialize();
            features::initialize();

            g_width = desc.BufferDesc.Width;
            g_height = desc.BufferDesc.Height;

            g_imgui_ready = true;
            g_initialized = true;
        }
    }

    if (g_initialized) {
        __try {
            features::on_frame();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Stale entity pointer from the 66ms cache must never take the game down.
        }

        ImGui_ImplDX11_NewFrame();
        if (g_hwnd)
            ImGui_ImplWin32_NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = gui::menu::is_open();

        ImGui::NewFrame();

        __try {
            features::on_render();
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        gui::menu::render();
        __try {
            features::on_render_overlay();
        } __except (EXCEPTION_EXECUTE_HANDLER) {}

        ImGui::Render();
        if (g_rtv)
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return o_present(swap, sync, flags);
}

static HRESULT __stdcall resize_detour(IDXGISwapChain* swap, UINT count, UINT width,
                                       UINT height, DXGI_FORMAT format, UINT flags) {
    release_rtv();
    HRESULT hr = o_resize(swap, count, width, height, format, flags);
    if (SUCCEEDED(hr)) {
        g_width = width;
        g_height = height;
        create_rtv();
        on_resize(width, height);
    }
    return hr;
}

bool initialize() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"LaczDummy";
    RegisterClassExW(&wc);

    HWND dummy = CreateWindowExW(0, L"LaczDummy", L"LaczDummy", WS_OVERLAPPEDWINDOW,
                                 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
    if (!dummy)
        return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1;
    desc.BufferDesc.Width = 100;
    desc.BufferDesc.Height = 100;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = dummy;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swap = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL level{};
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
        D3D11_SDK_VERSION, &desc, &swap, &dev, &level, &ctx);
    if (FAILED(hr))
        return false;

    void** swap_vtable = *reinterpret_cast<void***>(swap);
    void* present_target = swap_vtable[8];
    void* resize_target = swap_vtable[14];

    auto& hooks = hooks::manager::get();
    if (!hooks.initialize()) {
        swap->Release(); dev->Release(); ctx->Release();
        DestroyWindow(dummy); UnregisterClassW(L"LaczDummy", wc.hInstance);
        return false;
    }

    bool present_ok = hooks.attach(present_target, hooks::as_ptr(&present_detour),
                                   reinterpret_cast<void**>(&o_present), "Present");
    bool resize_ok  = hooks.attach(resize_target, hooks::as_ptr(&resize_detour),
                                   reinterpret_cast<void**>(&o_resize), "ResizeBuffers");

    swap->Release();
    dev->Release();
    ctx->Release();
    DestroyWindow(dummy);
    UnregisterClassW(L"LaczDummy", wc.hInstance);

    if (!present_ok) {
        log::write("FAIL: Failed to hook IDXGISwapChain::Present (0x%p)", present_target);
        return false;
    }
    return true;
}

void shutdown() {
    if (g_hwnd && g_original_wndproc) {
        SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(g_original_wndproc));
        g_original_wndproc = nullptr;
    }
    if (g_imgui_ready) {
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplDX11_Shutdown();
        ImGui::DestroyContext();
        g_imgui_ready = false;
    }
    shutdown_draw();
    release_rtv();
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device)  { g_device->Release();  g_device = nullptr; }
    g_initialized = false;
}

void on_resize(UINT, UINT) {}

} // namespace lacz::render
