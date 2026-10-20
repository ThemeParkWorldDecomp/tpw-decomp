// OpenGL start-up, Mac 2.04b3. Log contexts identify tb_init_opengl.cpp;
// these functions are filed with tb_printf_opengl.cpp in the inventory.
#include "tb_printf_opengl.h"
#include "tb_error.h"
#include "tb_resource_display.h"
#include "platform/indirectx.h"
#include "platform/opengl.h"
#include <new>

COpenGL* gei_OpenGL = nullptr;
SamsUtilities::UMemoryAlignedBase* gOpenGLTextureMemory = nullptr;
void* gOpenGLTextureData = nullptr;
CDrawPrimitive* gOpenGLDrawPrimitive = nullptr;
CClipping* gei_Clipping = nullptr;
uint32_t gOpenGLBlendModes[12];

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
void COpenGLTexture::Initialise() {
    if (!gOpenGLTextureMemory) {
        // Allocate the block before creating the default allocator, as on the Mac.
        auto* storage = static_cast<SamsUtilities::UMemoryAlignedBase*>(
            ::operator new(sizeof(SamsUtilities::UMemoryAlignedBase)));
        if (!SamsUtilities::gMemory) SamsUtilities::gMemory = new SamsUtilities::UAllocator;
        new (storage) SamsUtilities::UMemoryAlignedBase(0x10000, 0x10, 4, SamsUtilities::gMemory);
        gOpenGLTextureMemory = storage;
        gOpenGLTextureData = storage->mData;
    }
}
void COpenGLTexture::Cleanup() {
    if (gOpenGLTextureMemory) {
        delete gOpenGLTextureMemory;
        gOpenGLTextureMemory = nullptr;
        gOpenGLTextureData = nullptr;
    }
}
void COpenGLPrintf::printf_xy(int32_t x, int32_t y, char* text) {
    mText = text; mX = x; mY = y;
}
COpenGLDrawPrimitive::COpenGLDrawPrimitive() {
    mF10 = 0; mPasses = 0; mTexture = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
    const uint32_t modes[] = {0, 1, 0x300, 0x301, 0x302, 0x303, 0x304, 0x305, 0x306, 0x307, 0x308};
    for (int i = 0; i < 11; ++i) gOpenGLBlendModes[i + 1] = modes[i];
    mFields[2] = 0x3000;
}
int32_t DrawPrimitiveOpenGL_Init() {
    if (gOpenGLDrawPrimitive) return 0;
    gOpenGLDrawPrimitive = new COpenGLDrawPrimitive;
    COpenGLTexture::Initialise();
    return gOpenGLDrawPrimitive->Init();
}
int32_t DrawPrimitiveOpenGL_Close() {
    if (!gOpenGLDrawPrimitive) return 0;
    delete gOpenGLDrawPrimitive;
    gOpenGLDrawPrimitive = nullptr;
    COpenGLTexture::Cleanup();
    return 1;
}
int32_t Clipping_Init() {
    if (gei_Clipping) return 0;
    gei_Clipping = new CClipping;
    return 1;
}
int32_t Clipping_Close() {
    if (!gei_Clipping) return 0;
    delete gei_Clipping;
    gei_Clipping = nullptr;
    return 1;
}
