// Switch.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "platform/indirectx.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "Wavelet.h"
#include <cstdio>
#include <cstring>
#include <new>

// CSwitch's start-up and shutdown (engine library of SimTheme Park 2.04b3,
// 0x100326f0..0x1003287c; the inventory files the range under Wavelet.cpp).

CSwitch* gei_Switch = nullptr;

int32_t Switch_Init() {
    if (gei_Switch != nullptr) return 0;
    gei_Switch = new (std::nothrow) CSwitch;
    if (gei_Switch == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t Switch_Close() {
    if (gei_Switch == nullptr) return 0;
    delete gei_Switch;
    gei_Switch = nullptr;
    return 1;
}

int32_t ToggleFullScreen();   // 0x1003281c (keytable_actions.h declares it void; the original answers 1)

// 0x1003281c
int32_t ToggleFullScreen() {
    if (gei_Switch != nullptr) gei_Switch->ModeSwitch();
    return 1;
}

// Unit v5953: ConvertSquareFrom32 (0x10003454), PT_Load (0x1000bed0), CSwitch::Do (0x100328ac),
// COpenGLPrintf::Close (0x10046c64).

// The original has the display in a register and the DirectDraw object at gei_DDraw (0x10052ad0).
int32_t CSwitch::Do(int32_t toggle) {
    int32_t ok = 1;
    if (gei_System.mRenderer == 3) {
        ok &= gei_TCache->Close();
        ok &= gei_OpenGL->Close();
    }
    mF00 = static_cast<int32_t>(gei_DDraw->mFlags & 1);
    int32_t fullscreen;
    if (toggle != 0) fullscreen = (mF00 == 0) ? 1 : 0;
    else fullscreen = mF00;
    ok &= gei_DDraw->Close();
    if (ok == 0) return 0;
    if (toggle != 0) mF00 = 1 - mF00;
    int32_t height = mF08;
    if (height == 0) height = gei_Display.mHeight;
    int32_t width = mF04;
    if (width == 0) width = gei_Display.mWidth;
    int32_t device = gei_DDraw->mDevice;
    if (gei_DDraw->Init(width, height, gei_Display.mBitsPerPixel, device, fullscreen, 1) == 0) {
        gei_ErrorHandler.Push(3, -0x4f8abff3);
        return 0;
    }
    if (gei_System.mRenderer == 3) {
        if (gei_OpenGL->ReInitialize() == 0) return 0;   // virtual call through the render system's table
        if (!EngineAdd_Reinit()) return 0;
        if (gei_TCache->Init(gei_TextureSettings.mFormat) == 0) return 0;
    }
    return 1;
}

// 0x10032aa4: width and height 0 mean "the current display", toggles the mode.
void CSwitch::ModeSwitch() {
    mF08 = 0;
    mF04 = 0;
    Do(1);
}

// 0x10032b00
void CSwitch::RezSwitch(int32_t width, int32_t height) {
    mF04 = width;
    mF08 = height;
    Do(0);
}
