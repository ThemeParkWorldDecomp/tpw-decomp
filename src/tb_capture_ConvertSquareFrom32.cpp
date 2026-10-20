// Unit v5953: ConvertSquareFrom32 (0x10003454), PT_Load (0x1000bed0), CSwitch::Do (0x100328ac),
// COpenGLPrintf::Close (0x10046c64).
#include "tb_capture.h"
#include <cstdio>
#include <cstring>
#include "Wavelet.h"
#include "platform/indirectx.h"
#include "ppc_math.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_printf_opengl.h"
#include "tb_switch.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_proctexture.h"
#include "platform/upixels.h"
#include "platform/samsutils.h"

extern "C" void glDeleteLists(GLuint list, GLsizei range);

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

uint32_t PT_Load(char* path, int32_t mode) {
    if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesBlocked) == 0) {
        if (gei_HField == nullptr) return 1;
        return static_cast<uint32_t>(gei_HField->LoadProcTextures(path, mode));
    }
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    SamsUtilities::gLogger->SetContext("engine", "tb_hfield.cpp", 0x86, 0x80000000u);
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    SamsUtilities::gLogger->Log("Procedural textures not allowed, refer to enginedebug.txt for possible explaination");
    return 1;
}

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

bool COpenGLPrintf::Close() {
    int32_t* list = reinterpret_cast<int32_t*>(this);   // the display list base is the first word
    if (*list != 0) {
        glDeleteLists(static_cast<GLuint>(*list), 0x100);
        *list = 0;
    }
    return glGetError() == 0;
}
