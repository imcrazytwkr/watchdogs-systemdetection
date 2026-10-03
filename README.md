# Fixed patched systemdetection64

This repo contains source code for the fixed version of the [patched version](https://www.nexusmods.com/watchdogs/mods/476) of the `systemdetection64.dll` with [performance improvements](https://www.nexusmods.com/watchdogs/mods/393) at the cost of assessment accuracy which we don't really need for a 10yo game.

## Build

Successful build produces `systemdetection64.dll`.

### Linux (MinGW)

Make sure you have `mingw-w64-gcc` and `make` installed in your system, then call:

```sh
make
```

### Windows (MSVS)

From an x64 version of "Native Tools Command Prompt for VS" call:

```
build.bat
```

## Usage

Put the built `systemdetection64.dll` next to `watch_dogs.exe`, replacing the original. Optionally, backup of the original.

## Screen resolution regression check (Windows)

`GetPrimaryScreenResolution` must report physical display pixels regardless of
the caller's DPI awareness. On a display with Windows scaling above 100%, run
the following from an x64 Native Tools Command Prompt for VS:

```bat
cl /nologo /LD /O1 /GR- /EHs- /std:c++17 /W3 main.cpp user32.lib /Fe:systemdetection64.dll
cl /nologo /O1 /std:c++17 /EHsc /W3 tests\screen_resolution.cpp user32.lib /Fe:screen_resolution_test.exe
screen_resolution_test.exe .\systemdetection64.dll
screen_resolution_test.exe .\systemdetection64.dll --dpi-aware
```

The check loads the built DLL and calls its exported hardware interface. Both
runs must report the current physical resolution and exit with code 0. Before
the fix, a DPI-unaware process on a 2560 x 1600 display at 150% scaling reports
1707 x 1067 instead. At 100% scaling, the old implementation may also pass.
The test only changes DPI awareness for its own process; it does not change
Windows display settings.

Windows documents that [GetWindowRect is DPI-virtualized](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect),
while [EnumDisplaySettings always returns physical pixels](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsa).

## License

This code is not considered a proper piece of software or a library and thus, to stay compatible with the original mod licenses, is licensed under [CC-BY-NC-4.0](https://spdx.org/licenses/CC-BY-NC-4.0.html).
