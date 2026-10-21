// DirectDraw.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Light.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/indirectx.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_meshdelay.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include "Wavelet.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

// Unit v5953: ConvertSquareFrom32 (0x10003454), PT_Load (0x1000bed0), CSwitch::Do (0x100328ac),
// COpenGLPrintf::Close (0x10046c64).

// Converts a size x size block of 32-bit ARGB pixels at `source` into the texture format of the
// settings (format 5: 10, format 4: 4, any other: 13) at `dest`.
void ConvertSquareFrom32(void* dest, void* source, int32_t size) {
    int32_t format;
    switch (gei_TextureSettings.mFormat) {
        case 5: format = 10; break;
        case 4: format = 4; break;
        default: format = 13; break;
    }
    SamsUtilities::UPixels from(SamsUtilities::kPixels32ARGB, source, size, size, -1, nullptr);
    SamsUtilities::UPixels to(format, dest, size, size, -1, nullptr);
    to.Convert(from, false);
}

// tb_mesh_frame.cpp: the mesh object's per-frame members (engine library of SimTheme Park
// 2.04b3, 0x1001db1c..0x1001e418; the inventory files the range under tb_profiler.cpp, the
// original name is not in the binary; "tb_mesh.cpp" is in StaticCalculateLights' assertion)
// and two small functions of the same library, ColorVertices (0x10003570) and DotProduct
// (0x10025cac).
//
// Prepare is what the map's lists call for every mesh in view (CMapWho::PrepareWorldspace,
// the global lists, CMeshDelay::Prepare): at most once per window pass, it walks the mesh's
// nodes with the view matrix, and every node in view is lit, added to the render list and has
// its textures asked for.

void ColorVertices(SFVF_TLCTEX* vertices, uint32_t colour, int32_t count, int32_t stride) {
    const uint32_t alpha = colour >> 24;
    const uint32_t red = (colour >> 16) & 0xff;
    const uint32_t green = (colour >> 8) & 0xff;
    const uint32_t blue = colour & 0xff;
    uint8_t* destination = reinterpret_cast<uint8_t*>(vertices);
    for (uint32_t remaining = static_cast<uint32_t>(count); remaining != 0; --remaining) {
        // Always the first vertex's colour, as the original (see tb_mesh.h).
        const uint32_t old = vertices->colour;
        const uint32_t product = ((blue * (old & 0xff)) >> 8) | ((green * ((old >> 8) & 0xff)) & 0xff00) |
                                 (((red * ((old >> 16) & 0xff)) << 8) & 0xff0000) |
                                 (((alpha * (old >> 24)) << 16) & 0xff000000);
        std::memcpy(destination + offsetof(SFVF_TLCTEX, colour), &product, sizeof product);
        destination += stride;
    }
}

// tb_ddraw.cpp: engine display start-up, Mac 2.04b3, 0x100036b0..0x100050d0.

CDirectDraw* gei_DDraw = nullptr;
int32_t gDDResult = 0;

namespace {
SamsUtilities::ULogManager& logger() {
    if (!SamsUtilities::gLogger) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    return *SamsUtilities::gLogger;
}
void context(int32_t line) {
    logger().SetContext("engine", "tb_ddraw.cpp", line, 0x80000000);
}
void log(int32_t line, const char* text) {
    context(line);
    logger().Log(text);
}
template<class T> void release(T*& p) {
    if (p) { p->Release(); p = nullptr; }
}
SBitData channel(uint32_t mask) {
    const int32_t shift = ShiftFactor(mask);
    const int32_t bits = BitsSet(mask);
    return {mask, shift, bits};
}
}

int32_t DDraw_Init(GrafPort* window, int32_t width, int32_t height, int32_t depth,
                   int32_t device, int32_t fullscreen) {
    if (gei_DDraw) return 0;
    gei_DDraw = new CDirectDraw;
    gei_System.mWindow = window;
    if (!gei_DDraw->Init(width, height, depth, device, fullscreen, gei_System.mRenderer != 4)) {
        delete gei_DDraw;
        gei_DDraw = nullptr;
        gei_ErrorHandler.Push(3, static_cast<int32_t>(0xb0754004));
        return 0;
    }
    return 1;
}
int32_t DDraw_Close() {
    if (!gei_DDraw) return 0;
    gei_DDraw->Close();
    delete gei_DDraw;
    gei_DDraw = nullptr;
    return 1;
}
CDirectDraw::CDirectDraw() {
    mFlags = 0;
    mDirectDraw1 = mDirectDraw2 = nullptr;
    mDirectDraw = nullptr;
    mClipper = nullptr;
    mPrimary = mBack = nullptr;
    mSurface18 = nullptr;
    mDeviceCount = mDevice = 0;
    mF780 = 1;
}
CDirectDraw::~CDirectDraw() { Close(); }

int32_t CDirectDraw::FindDeviceCallback(NS_MacDoze::_GUID* guid, char*, char*, void* data) {
    auto& dd = *static_cast<CDirectDraw*>(data);
    if (dd.mDeviceCount < 4) {
        auto& dest = dd.mDevices[dd.mDeviceCount++];
        if (guid) dest = *guid;
        else std::memset(&dest, 0, sizeof dest);
    }
    return 1;
}

int32_t CDirectDraw::Init(int32_t width, int32_t height, int32_t depth, int32_t device,
                         int32_t fullscreen, int32_t videoMemory) {
    log(0x91, "Initializing DirectDraw");
    gei_Display.SetDisplay(width, height, depth);
    DDSURFACEDESC2 desc;
    auto fail = [this]() {
        context(0x1a5);
        logger().Log(" --- FAILED, exiting (DD error code %x (%d))", gDDResult, gDDResult);
        gei_ErrorHandler.Push(3, gDDResult);
        Close();
        return 0;
    };
    if (!mDirectDraw1) {
        if (!(gei_System.mFlags & 0x10)) {
            mDeviceCount = 0;
            DirectDrawEnumerateA(FindDeviceCallback, this);
            // The original compares GUID bytes as C strings, stopping at the first NUL.
            for (int32_t i = 0; i < mDeviceCount - 1; ++i)
                for (int32_t j = i + 1; j < mDeviceCount; ++j)
                    if (std::strncmp(reinterpret_cast<char*>(&mDevices[i]),
                                     reinterpret_cast<char*>(&mDevices[j]), 16) > 0)
                        std::swap(mDevices[i], mDevices[j]);
            mDevice = device;
            gDDResult = DirectDrawCreate(&mDevices[mDevice], &mDirectDraw1, nullptr);
        } else {
            mDeviceCount = 1;
            mDevice = device;
            gDDResult = DirectDrawCreate(nullptr, &mDirectDraw1, nullptr);
        }
        if (gDDResult) return fail();
        context(0xc9);
        logger().Log("DirectDraw Devices: found %d, current %d", mDeviceCount, mDevice);
        gDDResult = mDirectDraw1->QueryInterface(IID_IDirectDraw2, reinterpret_cast<void**>(&mDirectDraw2));
        if (gDDResult) return fail();
        gDDResult = mDirectDraw1->QueryInterface(IID_IDirectDraw4, reinterpret_cast<void**>(&mDirectDraw));
        if (gDDResult) {
            log(0xd4, " --- ERROR: DIRECTX6 NOT INSTALLED");
            gei_ErrorHandler.Push(3, static_cast<int32_t>(0xb0754016));
            return fail();
        }
        SetupCaps();
    } else if (device != mDevice) {
        Close();
        mDevice = device;
        gDDResult = DirectDrawCreate(&mDevices[mDevice], &mDirectDraw1, nullptr);
        if (gDDResult) return fail();
        gDDResult = mDirectDraw1->QueryInterface(IID_IDirectDraw4, reinterpret_cast<void**>(&mDirectDraw));
        if (gDDResult) {
            gei_ErrorHandler.Push(3, static_cast<int32_t>(0xb0754016));
            return fail();
        }
    }
    if (fullscreen == 1) {
        context(0xf6);
        logger().Log("Fullscreen %d:%d:%d", width, height, depth);
        gDDResult = mDirectDraw->SetCooperativeLevel(gei_System.mWindow, 0x851);
        if (gDDResult) return fail();
        log(0xfd, "Done SetCooperativeLevel");
        gDDResult = mDirectDraw->SetDisplayMode(width, height, depth, 0, 0);
        if (gDDResult) return fail();
        log(0x103, "Done SetDisplayMode");
        desc = {};
        desc.dwSize = sizeof desc;
        desc.dwFlags = 0x21;
        desc.dwBackBufferCount = (gei_Display.mFlags & 1) ? 2 : 1;
        desc.ddsCaps.dwCaps = videoMemory ? 0x6218 : 0x4218;
        gDDResult = mDirectDraw->CreateSurface(&desc, &mPrimary, nullptr);
        if (gDDResult) return fail();
        log(0x126, "Done CreateSurface");
        DDBLTFX fx;
        fx.words[19] = 0;
        NS_MacDoze::ZeroMemory(&fx, sizeof fx);
        fx.dwSize = sizeof fx;
        mPrimary->Blt(nullptr, nullptr, nullptr, 0x1000400, &fx);
        log(0x130, "Done Blt");
        DDSCAPS2 caps;
        caps.dwCaps = 4;
        gDDResult = mPrimary->GetAttachedSurface(&caps, &mBack);
        if (gDDResult) return fail();
        log(0x137, "Done GetAttachedSurface");
        mFlags |= 1;
    } else {
        context(0x13e);
        logger().Log("Windowed %d:%d:??", width, height);
        NS_MacDoze::ShowWindow(gei_System.mWindow, 9);
        NS_MacDoze::UpdateWindow(gei_System.mWindow);
        desc = {};
        desc.dwSize = sizeof desc;
        gDDResult = mDirectDraw->GetDisplayMode(&desc);
        if (gDDResult) return fail();
        gei_Display.mBitsPerPixel = desc.ddpfPixelFormat.dwRGBBitCount;
        gDDResult = mDirectDraw->SetCooperativeLevel(gei_System.mWindow, 0x808);
        if (gDDResult) return fail();
        SamsUtilities::URectangle::LongRect client, window;
        NS_MacDoze::GetClientRect(gei_System.mWindow, &client);
        NS_MacDoze::GetWindowRect(gei_System.mWindow, &window);
        window.right += gei_Display.mWidth - (client.right - client.left);
        window.bottom += gei_Display.mHeight - (client.bottom - client.top);
        NS_MacDoze::MoveWindow(gei_System.mWindow, window.left, window.top,
                               window.right - window.left, window.bottom - window.top, 0);
        desc = {};
        desc.dwSize = sizeof desc;
        desc.dwFlags = 1;
        desc.ddsCaps.dwCaps = 0x200;
        gDDResult = mDirectDraw->CreateSurface(&desc, &mPrimary, nullptr);
        if (gDDResult) return fail();
        desc.dwFlags = 7;
        desc.ddsCaps.dwCaps = 0x6040;
        desc.dwHeight = height;
        desc.dwWidth = width;
        gDDResult = mDirectDraw->CreateSurface(&desc, &mBack, nullptr);
        if (gDDResult) return fail();
        gDDResult = mDirectDraw->CreateClipper(0, &mClipper, nullptr);
        if (gDDResult) return fail();
        gDDResult = mClipper->SetHWnd(0, gei_System.mWindow);
        if (gDDResult) return fail();
        gDDResult = mPrimary->SetClipper(mClipper);
        if (gDDResult) return fail();
        mFlags &= ~1u;
        mPrimary->GetSurfaceDesc(&desc);
        gei_Display.mRedMask = desc.ddpfPixelFormat.dwRBitMask;
        gei_Display.mGreenMask = desc.ddpfPixelFormat.dwGBitMask;
        gei_Display.mBlueMask = desc.ddpfPixelFormat.dwBBitMask;
        gei_Display.mRGBMask = gei_Display.mRedMask | gei_Display.mGreenMask | gei_Display.mBlueMask;
        gei_Display.SetMaxBits();
    }
    SetupCaps();
    desc.dwSize = sizeof desc;
    mDirectDraw->GetDisplayMode(&desc); // Deliberately leaves gDDResult from SetupCaps.
    if (gDDResult) return fail();
    const auto alpha = channel(desc.ddpfPixelFormat.dwRGBAlphaBitMask);
    const auto red = channel(desc.ddpfPixelFormat.dwRBitMask);
    const auto green = channel(desc.ddpfPixelFormat.dwGBitMask);
    const auto blue = channel(desc.ddpfPixelFormat.dwBBitMask);
    gei_Display.SetBitPattern(alpha, red, green, blue);
    log(0x1a0, "Directdraw succesfully initialized");
    return 1;
}

int32_t CDirectDraw::SetupCaps() {
    gDDResult = mDirectDraw->GetDeviceIdentifier(&mDeviceInfo, 1);
    if (gDDResult) return 0;
    context(0x1b5); logger().Log("Graphics Card - Driver: %s", mDeviceInfo.driver);
    context(0x1b6); logger().Log("                Description: %s", mDeviceInfo.description);
    context(0x1b7); logger().Log("                Driver version: %x", mDeviceInfo.driverVersionHigh, mDeviceInfo.driverVersionLow);
    context(0x1b8); logger().Log("                VendorID %d, DeviceID %d", mDeviceInfo.vendorID, mDeviceInfo.deviceID);
    mHardwareCaps = {}; mSoftwareCaps = {};
    mSoftwareCaps.dwSize = mHardwareCaps.dwSize = sizeof(DDCAPS_DX6);
    gDDResult = mDirectDraw->GetCaps(&mHardwareCaps, &mSoftwareCaps);
    return gDDResult == 0;
}
int32_t CDirectDraw::Close() {
    if ((mFlags & 1) && mDirectDraw) mDirectDraw->RestoreDisplayMode();
    release(mBack); release(mPrimary); release(mSurface18); release(mClipper);
    release(mDirectDraw1); release(mDirectDraw2); release(mDirectDraw);
    return 1;
}
int32_t CDirectDraw::TempShutdown() {
    if (gei_System.mRenderer == 1 || gei_System.mRenderer == 3) return Close();
    return 1;
}
int32_t CDirectDraw::ReInitialize() {
    if (gei_System.mRenderer == 1 || gei_System.mRenderer == 3)
        return Init(gei_Display.mWidth, gei_Display.mHeight, gei_Display.mBitsPerPixel,
                    mDevice, mFlags & 1, 1);
    gDDResult = mPrimary->Restore();
    if (gDDResult) return 0;
    gDDResult = mBack->Restore();
    return gDDResult == 0;
}
DDSURFACEDESC2 CDirectDraw::LockBackbuffer() {
    DDSURFACEDESC2 desc{};
    desc.dwSize = sizeof desc;
    gDDResult = mBack->Lock(nullptr, &desc, 1, nullptr);
    if (gDDResult) { gei_ErrorHandler.Push(3, gDDResult); desc = {}; }
    return desc;
}
int32_t CDirectDraw::UnlockBackbuffer() {
    gDDResult = mBack->Unlock(nullptr);
    return 1;
}

namespace SamsUtilities {
// 0x10006218 (byte twin of USwapInplace16<unsigned short> 0x10043db8)
void USwapInplace16UShort_10006218(uint16_t& value) { value = static_cast<uint16_t>((value >> 8) | (value << 8)); }

// 0x100061d0 (byte twin of USwapInplaceFloat 0x10021bb4)
void USwapInplace32UInt_100061d0(uint32_t& value) { value = __builtin_bswap32(value); }
}  // namespace SamsUtilities
