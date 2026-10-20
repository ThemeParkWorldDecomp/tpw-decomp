// tb_ddraw.cpp: engine display start-up, Mac 2.04b3, 0x100036b0..0x100050d0.
#include "tb_ddraw.h"

#include "Wavelet.h"
#include "tb_error.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "platform/macdoze.h"
#include <cstring>
#include <utility>

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
