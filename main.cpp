#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <new>

// Compatibility of virtual destructor definitions: MSVC properly generates
// a single destructor method while mingw generates 2 so we have to simulate
// MSVC behaviour manually to produce a drop-in replacement DLL.
#ifdef _MSC_VER
#define VDTOR(T) virtual ~T()
#else
#define VDTOR(T)            \
    virtual void _dtor() {} \
    ~T()
#endif

struct GearCPU {
    VDTOR(GearCPU) {}
    virtual const char* GetName() { return "Intel(R) Core(TM) i5-6600 CPU @ 3.30GHz"; }
    uint64_t clockFrequencyHz = 3312000000;
    uint32_t nbHardwareThreads = 4;
    uint32_t nbCores = 4;
    uint32_t nbCPUs = 1;
    int32_t vendor = 4;
    int32_t isa = 8;
};

GearCPU staticCPU;

struct GearDisk {
    VDTOR(GearDisk) {}
    virtual const char* GetDriveLetter() const { return "C:"; }
    virtual const char* GetDeviceId() const { return "SCSI\\DISK&VEN_NVME&PROD_VMWARE_VIRTUAL_N\\5&25A13950&0&000000"; }
    virtual const char* GetFileSystem() const { return "NTFS"; }
    uint64_t freeSpace = 40145424384;
    uint64_t usedSpace = 458645016576;
    uint64_t totalSpace = 498790440960;
};

struct GearLogicalDisks {
    VDTOR(GearLogicalDisks) {}
    virtual const GearDisk* GetDisk(uint32_t i) {
        static GearDisk gd;
        return i == 0 ? &gd : NULL;
    }
};

GearLogicalDisks staticDisks;

class Memory {
    int32_t memoryType = 0;
    uint64_t totalPhys = 17139720192;
    uint64_t clockFrequencyHz = 0;
};

Memory staticMemory;

struct Network {
    bool available = true;
    uint32_t bandwidth = 1000000000;
};

Network staticNetwork;

class GearOS {
    int32_t windowsVersion = 8;
    uint32_t servicePack = 0;
    bool supports64Bits = true;
    char isoRegionName[9] = "US";
};

GearOS staticOS;

static void* HeapAllocZero(SIZE_T n) {
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n);
}

static BOOL QueryDisplayMode(const char* deviceName, DWORD modeNum, DEVMODEA* out) {
    ZeroMemory(out, sizeof(*out));
    out->dmSize = sizeof(*out);
    return EnumDisplaySettingsA(deviceName, modeNum, out);
}

static uint32_t ParseAdapterFamily(const char* name) {
    if (!name) {
        return 0;
    }
    for (const char* p = name; *p;) {
        if (*p >= '0' && *p <= '9') {
            uint32_t v = 0;
            int digits = 0;
            while (*p >= '0' && *p <= '9') {
                v = v * 10 + (uint32_t)(*p - '0');
                ++digits;
                ++p;
            }
            if (digits >= 3 && digits <= 5 && v >= 100) {
                return v;
            }
            continue;
        }
        ++p;
    }
    return 0;
}

static UINT FindD3DAdapter(IDirect3D9* d3d, const char* gdiName) {
    if (!d3d || !gdiName) {
        return UINT_MAX;
    }
    UINT n = d3d->GetAdapterCount();
    for (UINT i = 0; i < n; ++i) {
        D3DADAPTER_IDENTIFIER9 id;
        ZeroMemory(&id, sizeof(id));
        if (FAILED(d3d->GetAdapterIdentifier(i, 0, &id))) {
            continue;
        }
        if (lstrcmpiA(id.DeviceName, gdiName) == 0) {
            return i;
        }
    }
    return UINT_MAX;
}

struct DisplayMode {
    VDTOR(DisplayMode) {}

    DisplayMode() = default;

    DisplayMode(const D3DDISPLAYMODE& m) {
        width = m.Width;
        height = m.Height;
        bbp = 32;
        refreshRate = m.RefreshRate;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bbp = 0;
    uint32_t refreshRate = 0;
};

static void FillModesFromD3D(IDirect3D9* d3d, UINT adapter, DisplayMode** outModes, uint32_t* outCount) {
    UINT n = d3d->GetAdapterModeCount(adapter, D3DFMT_X8R8G8B8);
    DisplayMode* modes = (DisplayMode*)HeapAllocZero(n * sizeof(DisplayMode));
    uint32_t count = 0;
    if (modes) {
        for (UINT i = 0; i < n; ++i) {
            D3DDISPLAYMODE m;
            if (SUCCEEDED(d3d->EnumAdapterModes(adapter, D3DFMT_X8R8G8B8, i, &m))) {
                DisplayMode tmp(m);
                modes[count++] = tmp;
            }
        }
    }
    *outModes = modes;
    *outCount = count;
}

static void FillModesFromGDI(const char* deviceName, DisplayMode** outModes, uint32_t* outCount) {
    uint32_t n = 0;
    DEVMODEA probe;
    for (DWORD iMode = 0; QueryDisplayMode(deviceName, iMode, &probe); ++iMode) {
        if (probe.dmBitsPerPel == 32) {
            ++n;
        }
    }
    DisplayMode* modes = n ? (DisplayMode*)HeapAllocZero(n * sizeof(DisplayMode)) : NULL;
    uint32_t count = 0;
    if (modes) {
        for (DWORD iMode = 0; QueryDisplayMode(deviceName, iMode, &probe); ++iMode) {
            if (probe.dmBitsPerPel != 32) {
                continue;
            }
            DisplayMode tmp;
            tmp.width = probe.dmPelsWidth;
            tmp.height = probe.dmPelsHeight;
            tmp.bbp = probe.dmBitsPerPel;
            tmp.refreshRate = probe.dmDisplayFrequency;
            modes[count++] = tmp;
        }
    }
    *outModes = modes;
    *outCount = count;
}

struct MultiSampling {
    int32_t type;
    uint32_t qualityLevel;
};

struct Output {
    Output(IDirect3D9* d3d, UINT d3dAdapter, const char* gdiName) {
        displayModes = NULL;
        modeCount = 0;
        if (d3d && d3dAdapter != UINT_MAX) {
            FillModesFromD3D(d3d, d3dAdapter, &displayModes, &modeCount);
        } else {
            FillModesFromGDI(gdiName, &displayModes, &modeCount);
        }
    }

    VDTOR(Output) {
        if (displayModes) {
            HeapFree(GetProcessHeap(), 0, displayModes);
            displayModes = NULL;
        }
    }

    virtual const DisplayMode* GetAvailableDisplayMode(uint32_t i) {
        if (i < modeCount) {
            return &displayModes[i];
        }

        return NULL;
    }

    DisplayMode* displayModes;
    uint32_t modeCount;
};

struct DisplayAdapter {
    DisplayAdapter(DISPLAY_DEVICEA& d, IDirect3D9* d3d) {
        lstrcpynA(name, d.DeviceString, MAX_DEVICE_IDENTIFIER_STRING);
        outputs = NULL;
        outputCount = 0;
        msCount = 0;
        caps = NULL;
        ZeroMemory(&d3dCaps, sizeof(d3dCaps));

        UINT d3dAdapter = FindD3DAdapter(d3d, d.DeviceName);
        if (d3d && d3dAdapter != UINT_MAX) {
            D3DADAPTER_IDENTIFIER9 id;
            ZeroMemory(&id, sizeof(id));
            if (SUCCEEDED(d3d->GetAdapterIdentifier(d3dAdapter, 0, &id))) {
                if (id.Description[0]) {
                    lstrcpynA(name, id.Description, MAX_DEVICE_IDENTIFIER_STRING);
                }
                vendor = (int32_t)id.VendorId;
                deviceID = id.DeviceId;
            }
            D3DDISPLAYMODE cur;
            if (SUCCEEDED(d3d->GetAdapterDisplayMode(d3dAdapter, &cur))) {
                currentDisplayMode = DisplayMode(cur);
            }
            static const D3DMULTISAMPLE_TYPE kSamples[] = {
                D3DMULTISAMPLE_2_SAMPLES, D3DMULTISAMPLE_4_SAMPLES, D3DMULTISAMPLE_8_SAMPLES};
            for (int si = 0; si < 3; ++si) {
                DWORD quality = 0;
                D3DMULTISAMPLE_TYPE s = kSamples[si];
                if (SUCCEEDED(d3d->CheckDeviceMultiSampleType(d3dAdapter, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, s,
                                                              &quality)) &&
                    quality > 0 && msCount < 3) {
                    multiSampling[msCount].type = (int32_t)s;
                    multiSampling[msCount].qualityLevel = quality;
                    ++msCount;
                }
            }
            if (SUCCEEDED(d3d->GetDeviceCaps(d3dAdapter, D3DDEVTYPE_HAL, &d3dCaps))) {
                caps = &d3dCaps;
            }
        }

        uint32_t parsedFamily = ParseAdapterFamily(name);
        if (parsedFamily) {
            family = parsedFamily;
        }

        uint32_t nOut = 0;
        DISPLAY_DEVICEA monitor;
        monitor.cb = sizeof(monitor);
        DWORD iMon = 0;
        while (EnumDisplayDevicesA(d.DeviceName, iMon++, &monitor, 0)) {
            if (monitor.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {
                continue;
            }
            if (!(monitor.StateFlags & (DISPLAY_DEVICE_ACTIVE | DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))) {
                continue;
            }
            ++nOut;
            monitor.cb = sizeof(monitor);
        }
        if (nOut == 0 && (d.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
            nOut = 1;
        }

        if (nOut) {
            outputs = (Output*)HeapAllocZero(nOut * sizeof(Output));
        }
        if (outputs) {
            iMon = 0;
            monitor.cb = sizeof(monitor);
            while (EnumDisplayDevicesA(d.DeviceName, iMon++, &monitor, 0)) {
                if (monitor.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {
                    continue;
                }
                if (!(monitor.StateFlags & (DISPLAY_DEVICE_ACTIVE | DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))) {
                    continue;
                }
                new (&outputs[outputCount]) Output(d3d, d3dAdapter, d.DeviceName);
                ++outputCount;
                monitor.cb = sizeof(monitor);
            }
            if (outputCount == 0 && nOut == 1) {
                new (&outputs[outputCount]) Output(d3d, d3dAdapter, d.DeviceName);
                ++outputCount;
            }
        }
    }

    VDTOR(DisplayAdapter) {
        if (outputs) {
            for (uint32_t i = 0; i < outputCount; ++i) {
                outputs[i].~Output();
            }
            HeapFree(GetProcessHeap(), 0, outputs);
            outputs = NULL;
        }
    }

    virtual const char* GetName() { return name; }
    virtual MultiSampling* GetMultiSampling(uint32_t i) {
        if (i < msCount) {
            return &multiSampling[i];
        }
        return NULL;
    }
    virtual Output* GetOutput(uint32_t i) {
        if (i < outputCount) {
            return &outputs[i];
        }

        return NULL;
    }
    int32_t lastVersion = 4;
    int32_t vendor = 0;
    uint32_t deviceID = 0;
    uint32_t family = 0;
    uint32_t model = 0;
    uint32_t linkedGpu = 60;
    void* caps = NULL;
    DisplayMode currentDisplayMode;
    uint64_t memoryBytes = 0;

    Output* outputs;
    uint32_t outputCount;
    MultiSampling multiSampling[3];
    uint32_t msCount;
    D3DCAPS9 d3dCaps;
    char name[MAX_DEVICE_IDENTIFIER_STRING];
};

struct GearGraphics {
    GearGraphics() {
        displays = NULL;
        displayCount = 0;
        d3d = NULL;
        d3dModule = NULL;
        d3dModule = LoadLibraryA("d3d9.dll");
        if (d3dModule) {
            typedef IDirect3D9*(WINAPI * PFN_Direct3DCreate9)(UINT);
            PFN_Direct3DCreate9 create = (PFN_Direct3DCreate9)GetProcAddress(d3dModule, "Direct3DCreate9");
            if (create) {
                d3d = create(D3D_SDK_VERSION);
            }
        }

        uint32_t n = 0;
        DISPLAY_DEVICEA adapter;
        adapter.cb = sizeof(adapter);
        DWORD iDev = 0;
        while (EnumDisplayDevicesA(NULL, iDev++, &adapter, 0)) {
            if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {
                continue;
            }
            if (!(adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
                continue;
            }
            ++n;
            adapter.cb = sizeof(adapter);
        }
        if (n) {
            displays = (DisplayAdapter*)HeapAllocZero(n * sizeof(DisplayAdapter));
        }
        iDev = 0;
        adapter.cb = sizeof(adapter);
        while (displays && EnumDisplayDevicesA(NULL, iDev++, &adapter, 0)) {
            if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) {
                continue;
            }
            if (!(adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
                continue;
            }
            new (&displays[displayCount]) DisplayAdapter(adapter, d3d);
            ++displayCount;
            adapter.cb = sizeof(adapter);
        }
    }

    VDTOR(GearGraphics) {
        if (displays) {
            for (uint32_t i = 0; i < displayCount; ++i) {
                displays[i].~DisplayAdapter();
            }
            HeapFree(GetProcessHeap(), 0, displays);
            displays = NULL;
        }
        if (d3d) {
            d3d->Release();
            d3d = NULL;
        }
        if (d3dModule) {
            FreeLibrary(d3dModule);
            d3dModule = NULL;
        }
    }

    virtual DisplayAdapter* GetDisplayAdapter(uint32_t i) {
        if (i < displayCount) {
            return &displays[i];
        }

        return NULL;
    }

    virtual uint32_t GetNumberOfScreens() { return GetSystemMetrics(SM_CMONITORS); }

    virtual void GetPrimaryScreenResolution(uint32_t* x, uint32_t* y) {
        // Display modes use physical pixels; GetWindowRect is DPI-virtualized.
        DEVMODEA mode = {};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &mode)) {
            *x = mode.dmPelsWidth;
            *y = mode.dmPelsHeight;
            return;
        }

        RECT rect = {};
        GetWindowRect(GetDesktopWindow(), &rect);
        *x = rect.right - rect.left;
        *y = rect.bottom - rect.top;
    }

    uint32_t directXMajor = 11;
    uint32_t directXMinor = 0;
    uint32_t smMajor = 0;
    uint32_t smMinor = 5;
    DisplayAdapter* displays;
    uint32_t displayCount;
    IDirect3D9* d3d;
    HMODULE d3dModule;
};

static GearGraphics staticGraphics;

struct Audio {
    bool available = true;
    const char* szDescription = "Speakers (High Definition Audio Device)";
    const char* szHardwareID = "HDAUDIO\\FUNC_01&VEN_15AD&DEV_1975&SUBSYS_15AD1975&REV_1001\\5&217be3d6&0&0001";
    uint32_t manufacturerID = 0;
    uint32_t productID = 0;
    const char* szDriverName = "audioendpoint.sys";
    const char* szDriverVersion = "10.0.17763.1";
    const char* szDriverDateEnglish = "9/4/2018 5:00:00 PM";
};

static Audio staticAudio;

struct GearHardware {
    VDTOR(GearHardware) {}
    virtual GearCPU* GetCPU() { return &staticCPU; }
    virtual GearLogicalDisks* GetLogicalDisks() { return &staticDisks; }
    virtual Memory* GetMemory() { return &staticMemory; }
    virtual Network* GetNetwork() { return &staticNetwork; }
    virtual GearOS* GetOS() { return &staticOS; }
    virtual GearGraphics* GetGraphics() { return &staticGraphics; }
    virtual Audio* GetAudio() { return &staticAudio; }
};

struct GearScore {
    VDTOR(GearScore) {}

    virtual float GetScore(int32_t info, int32_t version, float* out) {
        switch (info) {
        case 0:
            if (out) {
                *out = 1.f;
            }

            return 1891.25537f;
        case 1:
            if (out) {
                *out = 0.f;
            }

            return 0.f;
        default:
            if (out) {
                *out = 176.203873f;
            }

            return 1.87323334e+31;
        }
    }

    virtual void Unk3() {}
};

GearHardware staticHardware;
GearScore staticScore;

extern "C" {
__declspec(dllexport) GearHardware* GetHardwareInstance() {
    return &staticHardware;
}
__declspec(dllexport) GearScore* GetScoreInstance() {
    return &staticScore;
}
}
