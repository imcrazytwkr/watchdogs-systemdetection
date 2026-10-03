#include "../main.cpp"
#include <stdio.h>
#include <string.h>

// Exercise the built DLL through its exported interface. Run in separate
// processes with and without DPI awareness on a display scaled above 100%.
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--dpi-aware") != 0)) {
        fprintf(stderr, "Usage: screen_resolution_test.exe DLL [--dpi-aware]\n");
        return 2;
    }
    if (argc == 3 && !SetProcessDPIAware()) {
        fprintf(stderr, "Cannot enable DPI awareness\n");
        return 2;
    }

    DEVMODEA expected = {};
    expected.dmSize = sizeof(expected);
    if (!EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &expected)) {
        fprintf(stderr, "Cannot query the current display mode\n");
        return 2;
    }

    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) {
        fprintf(stderr, "Cannot load DLL (error %lu)\n", GetLastError());
        return 2;
    }
    typedef GearHardware* (*GetHardwareInstanceFn)();
    GetHardwareInstanceFn getHardware = (GetHardwareInstanceFn)GetProcAddress(module, "GetHardwareInstance");
    if (!getHardware) {
        fprintf(stderr, "Missing GetHardwareInstance export\n");
        FreeLibrary(module);
        return 2;
    }

    uint32_t width = 0, height = 0;
    getHardware()->GetGraphics()->GetPrimaryScreenResolution(&width, &height);
    printf("%s: reported %u x %u; physical %lu x %lu\n", argc == 3 ? "DPI aware" : "DPI unaware",
           width, height, expected.dmPelsWidth, expected.dmPelsHeight);
    bool matches = width == expected.dmPelsWidth && height == expected.dmPelsHeight;
    FreeLibrary(module);
    return matches ? 0 : 1;
}
