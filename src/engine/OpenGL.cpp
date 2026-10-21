// OpenGL.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "platform/indirectx.h"
#include "platform/opengl.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_printf_opengl.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"
#include <new>

// OpenGL start-up, Mac 2.04b3. Log contexts identify tb_init_opengl.cpp;
// these functions are filed with tb_printf_opengl.cpp in the inventory.

COpenGL* gei_OpenGL = nullptr;
namespace {
SamsUtilities::ULogManager& logger() {
    if (!SamsUtilities::gLogger) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    return *SamsUtilities::gLogger;
}
void log(int32_t line, const char* text) {
    logger().SetContext("engine", "tb_init_opengl.cpp", line, 0x80000000);
    logger().Log(text);
}
}

int32_t OpenGL_Init(int32_t mode) {
    if (gei_OpenGL) return 0;
    gei_OpenGL = new COpenGL;
    if (!gei_OpenGL->Init(mode == 1)) {
        ::operator delete(gei_OpenGL);
        gei_OpenGL = nullptr;
        gei_ErrorHandler.Push(3, static_cast<int32_t>(0xb0754005));
        return 0;
    }
    gei_System.mRenderSystem = gei_OpenGL;
    return 1;
}
int32_t OpenGL_Close() {
    if (!gei_OpenGL) return 0;
    gei_System.mRenderSystem = nullptr;
    gei_OpenGL->Close();
    ::operator delete(gei_OpenGL);
    gei_OpenGL = nullptr;
    return 1;
}
COpenGL::COpenGL() { mMode = 0; }
int32_t COpenGL::Init(int32_t mode) {
    mMode = mode;
    if (!InitBackground()) SamsUtilities::UDebug("bailed: %s was false", "InitBackground()");
    else if (!ClearViewport()) SamsUtilities::UDebug("bailed: %s was false", "ClearViewport()");
    else if (!InitRenderStates()) SamsUtilities::UDebug("bailed: %s was false", "InitRenderStates()");
    else { log(0x7f, "OpenGL initialized succesfully"); return 1; }
    SamsUtilities::UDebug("bailed out");
    log(0x89, " OpenGL Init FAILED");
    Close();
    return 0;
}
int32_t COpenGL::Close() { return glGetError() == 0; }
int32_t COpenGL::TempShutdown() { return glGetError() == 0; }
int32_t COpenGL::ReInitialize() { return Init(mMode); }
int32_t COpenGL::ClearViewport() {
    glPushAttrib(0x100);
    glDepthMask(1);
    glClear(0x4100);
    glPopAttrib();
    return glGetError() == 0;
}

// The OpenGL renderer's state keeping (engine library of SimTheme Park 2.04b3, in the
// inventory's tb_printf_opengl.cpp range): the rest of COpenGL's interface, the draw
// primitive's start-up and its blending, texture and fog setters, and the texture page.
// The constructors and start-up entry points are in tb_printf_opengl.cpp. PARTIAL: the
// draw primitive's drawing members (Execute, DrawIndexedPrimitive, the boxes and polys,
// GetPixel, GetPixelFillRate) and COpenGLTexture::Download are still to do.

// ---- COpenGL: the rest of the renderer interface ----------------------------------------
int32_t COpenGL::FlipScreen() {
    SamsUtilities::UOpenGL::GetCurrentInstance()->Swap();
    return glGetError() == 0;
}

int32_t COpenGL::GetVRAMSize() { return static_cast<int32_t>(gei_DDraw->mHardwareCaps.dwVidMemTotal); }

void* COpenGL::CreateTextureBase() {
    COpenGLTexture* texture = new (std::nothrow) COpenGLTexture;
    return texture != nullptr ? static_cast<CTextureBase*>(texture) : nullptr;
}

int32_t COpenGL::GetTextureMemory() { return 0; }

int32_t COpenGL::InitBackground() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    return glGetError() == 0;
}
int32_t COpenGL::InitRenderStates() {
    if (mMode == 0) { glDisable(0xb71); glDepthMask(0); }
    else { glEnable(0xb71); glDepthFunc(0x203); glDepthMask(1); }
    glEnableClientState(0x8076);
    glEnableClientState(0x8074);
    glEnableClientState(0x8078);
    glViewport(0, 0, gei_Display.mWidth, gei_Display.mHeight);
    glMatrixMode(0x1701); glLoadIdentity();
    glMatrixMode(0x1700); glLoadIdentity();
    glFrontFace(0x900);
    return glGetError() == 0;
}
int32_t COpenGL::MaxTextures(int32_t size, int32_t format) {
    auto* context = IdxDirectDraw::mImplementation->DisplayGetContext();
    if (format == 5) return context->GetMaxTextures(0, size, 0x1908);
    if (format == 4) return context->GetMaxTextures(0, size, 0x8056);
    SamsUtilities::UDebug("bad texture format");
    return 0;
}
