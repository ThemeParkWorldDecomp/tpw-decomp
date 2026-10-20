// Elegant Chaos "indirectx" shim (DirectDraw on OpenGL + DrawSprocket): the DirectDraw
// methods game-side code calls. They are plain (non-virtual) C++ member functions in the
// shim library, imported by name. Declarations only; the platform layer implements them
// (analysis/platform-layer-design.md).
#pragma once
#include <cstdint>

#include "platform/ddraw_types.h"
#include "platform/samsutils.h"

namespace NS_MacDoze {
struct InternalHandle;   // Win32 HANDLE stand-in (opaque here)
struct _GUID { uint8_t bytes[16]; };
class IUnknown {
public:
    virtual ~IUnknown();
    virtual int32_t QueryInterface(const _GUID&, void**);
    uint32_t Release();
    // Unit w231 (src/LbSurface.cpp): macdoze imported member, glue 0x1006bc4c.
    uint32_t AddRef();
};
}

struct DDDEVICEIDENTIFIER {
    char driver[512];
    char description[512];
    uint32_t driverVersionHigh, driverVersionLow;
    uint32_t vendorID, deviceID, subsystemID, revision;
    NS_MacDoze::_GUID identifier;
};
struct DDCAPS_DX6 {
    uint32_t dwSize;
    uint32_t reserved04[14];
    uint32_t dwVidMemTotal; // 0x3c
    uint32_t reserved40[(0x17c - 0x40) / 4];
};
struct DDBLTFX { uint32_t dwSize; uint32_t words[24]; };
class IDirectDrawClipper : public NS_MacDoze::IUnknown {
public:
    int32_t SetHWnd(uint32_t flags, GrafPort* window);
};
class IDirectDraw : public NS_MacDoze::IUnknown {
public:
    int32_t CreateClipper(uint32_t, IDirectDrawClipper**, NS_MacDoze::IUnknown*);
    int32_t SetCooperativeLevel(GrafPort*, uint32_t);
    int32_t SetDisplayMode(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    int32_t RestoreDisplayMode();
    int32_t GetCaps(DDCAPS_DX6*, DDCAPS_DX6*);
    // Unit w231 (src/LbSurface.cpp): original descriptor import, glue 0x1006bd24.
    int32_t CreateSurface(DDSURFACEDESC*, class IDirectDrawSurface**, NS_MacDoze::IUnknown*);
};
class IDirectDraw4 : public IDirectDraw {
public:
    int32_t GetDeviceIdentifier(DDDEVICEIDENTIFIER*, uint32_t);
    int32_t GetDisplayMode(DDSURFACEDESC2*);
    int32_t CreateSurface(DDSURFACEDESC2*, class IDirectDrawSurface4**, NS_MacDoze::IUnknown*);
};
extern const NS_MacDoze::_GUID IID_IDirectDraw2, IID_IDirectDraw4;
int32_t DirectDrawCreate(NS_MacDoze::_GUID*, IDirectDraw**, NS_MacDoze::IUnknown*);
int32_t DirectDrawEnumerateA(int32_t (*callback)(NS_MacDoze::_GUID*, char*, char*, void*), void*);

class IDirectDrawSurface : public NS_MacDoze::IUnknown {
public:
    // Returns an HRESULT (0 = DD_OK).
    int32_t Unlock(void* rect);
    int32_t Restore();
    int32_t SetClipper(IDirectDrawClipper*);
    // Unit w231 (src/LbSurface.cpp): original nonvirtual imports (bullfrog glue).
    int32_t GetSurfaceDesc(DDSURFACEDESC*); // 0x1006ba6c
    int32_t Lock(SamsUtilities::URectangle::LongRect*, DDSURFACEDESC*, uint32_t,
                 NS_MacDoze::InternalHandle*); // 0x1006bc7c
    int32_t IsLost(); // 0x1006bc94
    int32_t SetColorKey(uint32_t, DDCOLORKEY*); // 0x1006bcc4
    int32_t Blt(SamsUtilities::URectangle::LongRect*, IDirectDrawSurface*,
                SamsUtilities::URectangle::LongRect*, uint32_t, DDBLTFX*); // 0x1006bcdc
    int32_t BltFast(uint32_t, uint32_t, IDirectDrawSurface*,
                    SamsUtilities::URectangle::LongRect*, uint32_t); // 0x1006bcf4
};

class IDirectDrawSurface3 : public IDirectDrawSurface {
public:
    int32_t GetSurfaceDesc(DDSURFACEDESC2*);
};
class IDirectDrawSurface4 : public IDirectDrawSurface3 {
public:
    // Fills desc->lpSurface and desc->lPitch; returns an HRESULT (0 = DD_OK).
    int32_t Lock(SamsUtilities::URectangle::LongRect* rect, DDSURFACEDESC2* desc, uint32_t flags,
                 NS_MacDoze::InternalHandle* event);
    int32_t Blt(SamsUtilities::URectangle::LongRect*, IDirectDrawSurface4*,
                SamsUtilities::URectangle::LongRect*, uint32_t, DDBLTFX*);
    int32_t GetAttachedSurface(DDSCAPS2*, IDirectDrawSurface4**);
};

// IdxDirectDraw: the shim's DirectDraw object; IdxMakeOpenGLImplementation makes the OpenGL
// one (IdxDirectDrawOpenGL, which embeds the UOpenGL context). PARTIAL: only the virtual
// function the game calls (vtable slot 0x28 on the Mac, after the destructor and the
// Display{Initialise,Switch,Restore,MakeSurfaces,Flip,GetInfo,GetID} slots).
class IdxDirectDraw {
public:
    static IdxDirectDraw* mImplementation;
    virtual ~IdxDirectDraw();
    virtual SamsUtilities::UOpenGL* DisplayGetContext() = 0;
};

// indirectx 0x100027c8: NULL when it fails.
IdxDirectDraw* IdxMakeOpenGLImplementation(SamsUtilities::DeviceKind device, GrafPort* window, void* devices);
