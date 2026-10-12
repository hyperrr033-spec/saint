// Minimal long-running target used to validate the manual mapper without
// risking a real game process. It loads d3d11/dxgi/dwmapi/winmm so the import
// resolution path has the same dependencies the cheat needs.
#include <Windows.h>
#include <cstdio>

int main() {
    // Force-load the same system DLLs the cheat imports, so a mapper test
    // exercises identical import resolution.
    LoadLibraryA("d3d11.dll");
    LoadLibraryA("dxgi.dll");
    LoadLibraryA("dwmapi.dll");
    LoadLibraryA("winmm.dll");
    LoadLibraryA("d3dcompiler_47.dll");
    LoadLibraryA("imm32.dll");

    printf("dummytarget pid=%lu ready\n", GetCurrentProcessId());
    fflush(stdout);
    for (;;)
        Sleep(1000);
    return 0;
}