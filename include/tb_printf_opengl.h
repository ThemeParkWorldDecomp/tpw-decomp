// tb_printf_opengl.h: the OpenGL renderer (engine library, original tb_printf_opengl.cpp).
//
// PARTIAL: renderer start-up and declarations used by translated files.
#pragma once
#include <cstdint>
#include "tb_resource_system.h"
#include "tb_drawprimitive.h"
#include "tb_texturecache.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"

// PARTIAL: start-up members and the virtual slots required by the renderer interface.
// COpenGL is 0x14 bytes on the Mac: virtual-base pointer 0, vptr 4, mode 8,
// CRenderSystem base 0xc (vptr, complete-object adjustment).
class COpenGL : public virtual CRenderSystem {
public:
    COpenGL(); // 0x10045370
    int32_t Init(int32_t mode); // 0x100453dc
    int32_t Close(); // 0x100455f4
    int32_t TempShutdown() override; // 0x10045648
    int32_t ReInitialize() override; // 0x100456a4
    int32_t ClearViewport() override; // 0x100456f4
    int32_t FlipScreen() override;
    int32_t GetTextureMemory() override;
    int32_t GetVRAMSize() override;
    void* CreateTextureBase() override;
    int32_t InitBackground(); // 0x100458e4
    int32_t InitRenderStates(); // 0x1004595c
    int32_t MaxTextures(int32_t size, int32_t format); // 0x10045a90
    int32_t mMode; // 0x08
};
extern COpenGL* gei_OpenGL; // engine BSS 0x102b3640

// The OpenGL renderer's texture page: 0x34 bytes on the Mac (+0 the pointer to the
// CTextureBase at +0x1c, +4 the vtable). Pages are RGBA, 4 bytes a pixel, and are filled
// through one shared upload buffer (gOpenGLTextureData, 256 x 256 pixels).
class COpenGLTexture : public virtual CTextureBase {
public:
    static void Initialise(); // 0x10045bf0, no this or defined return value
    static void Cleanup(); // 0x10045cec

    COpenGLTexture(); // 0x10045d88
    ~COpenGLTexture() override; // 0x10045e58
    double TestUploadSpeed() override; // 0x10045f24
    int32_t Create(int32_t width, int32_t height, int32_t format, int32_t mipmaps) override; // 0x1004602c
    int32_t Destroy() override; // 0x100461d4
    // Binds the page and brings its filter and wrap state to `state`.
    void Select(int32_t state); // 0x10046264
    void SetState(int32_t state); // 0x10046308
    int32_t LockSurface() override; // 0x10046480
    // Sends the upload buffer to the page.
    int32_t UnlockSurface() override; // 0x100464d0
    int32_t Download(CTextureEntry* entry, int32_t x, int32_t y) override; // 0x100465f8, untranslated
    int32_t MipmapsUsed() override; // 0x10046a58
    int32_t GetPixelCount() override; // 0x10046a94
    void GetTexture() override; // 0x10046adc, empty

    enum : int32_t {
        kStateNearest = 0x20000,     // both filters GL_NEAREST
        kStateNoMipmaps = 0x40000,   // minification GL_LINEAR instead of the page's own filter
        kStateRepeat = 0x400000,     // wrap, not clamp
    };

    GLuint mTexture; // 0x08, the OpenGL texture name
    int32_t mMipmaps; // 0x0c
    int32_t mBytesPerPixel; // 0x10, 4 once created
    int32_t mState; // 0x14, what the texture's parameters were last set for
    int32_t mMinFilter; // 0x18
};
extern SamsUtilities::UMemoryAlignedBase* gOpenGLTextureMemory; // 0x102b3644
extern void* gOpenGLTextureData; // 0x102b3648

class COpenGLPrintf {
public:
    void printf_xy(int32_t x, int32_t y, char* text); // 0x10046cec
    bool Close(); // 0x10046c64 (src/tb_capture_ConvertSquareFrom32.cpp): deletes the 0x100 lists at word 0; glGetError == 0
    uint8_t mOpaque[0x10];
    char* mText; // 0x10
    int32_t mX, mY; // 0x14, 0x18
};

class COpenGLDrawPrimitive : public virtual CDrawPrimitive {
public:
    COpenGLDrawPrimitive(); // 0x10042df8, 0x4c bytes on the Mac
    ~COpenGLDrawPrimitive() override; // untranslated
    int32_t Init() override;
    int32_t SetBlendingMode(int32_t) override;
    int32_t SetTexture(int32_t, void*) override;
    int32_t Execute(SPrimitive*) override;
    int32_t ExecuteShadow(SPrimitive*) override;
    int32_t SetFogParameters(float, float, uint32_t) override;
    int32_t BeginDraw() override;
    int32_t EndDraw() override;
    int32_t DrawTestPoly() override;
    int32_t DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) override;
    // The rest of the drawing members (src/tb_opengl_draw.cpp). Execute, DrawTestPoly and
    // DrawFilledBox above are 0x100431ac, 0x10043b70 and 0x10043eb0.
    int32_t DrawEdgeBox() override;   // 0x10044420
    int32_t DrawPoly(XZType*, int32_t, int32_t, float) override;   // 0x100441f4
    int32_t DrawProfileBox() override;   // 0x10044fa4: 0
    float GetPixelFillRate() override;   // 0x10044c08
    int32_t DrawWiredframedBoundingBox(SVertex*, int32_t, int32_t) override;   // 0x10044724
    int32_t GetPixel(int32_t, int32_t, uint32_t*) override;   // 0x10044b38
    int32_t DrawDetectPoly(float, int32_t, int32_t, float, float, float, float) override;   // 0x10043d08
    // Draws `indexCount` indices of `vertices` (vertexType 0x1c4 only: x, y, z, w, colour,
    // specular, u, v, 0x20 bytes each); the last argument's bit 0x8000 asks for a second pass
    // with the specular colour, bit 1 for a glFinish. primitiveType is D3D's: 4 or 3.
    // Always returns 0.
    int32_t DrawIndexedPrimitive(int32_t primitiveType, uint32_t vertexType, void* vertices, uint32_t vertexCount,
                                 uint16_t* indices, uint32_t indexCount, uint32_t flags);   // 0x1004390c
    int32_t SupportsOnePassMultiTexture(); // 0x10044e98: 0
    void* mTexture; // 0x08, the COpenGLTexture in use: -1 after construction, NULL for none
    int32_t mPasses, mF10, mF14; // 0x0c..0x14
};
extern uint32_t gOpenGLBlendModes[12]; // 0x100520fc, element 0 untouched
extern CDrawPrimitive* gOpenGLDrawPrimitive; // 0x102b363c, renderer-specific instance
struct SFVF_TLCTEX;
struct VertexClipList;
struct sAlignedBox;
class CMatrix;
#include "tb_clipping.h"   // CClipping: was a two-word placeholder here, now the real class
extern CClipping* gei_Clipping; // engine BSS 0x102b3638
int32_t Clipping_Init(); // 0x1003f3bc
int32_t Clipping_Close(); // 0x1003f45c
int32_t DrawPrimitiveOpenGL_Init(); // 0x10042c70
int32_t DrawPrimitiveOpenGL_Close(); // 0x10042d60

// Creates the COpenGL renderer (once) and stores its CRenderSystem in
// gei_System.mRenderSystem; returns 1 on success.
int32_t OpenGL_Init(int32_t mode);     // 0x100451a0
// Destroys it; returns 1 if there was one.
int32_t OpenGL_Close();                // 0x100452e0

// Unit sinit_tb_printf_opengl_cpp
extern COpenGLPrintf gOpenGLPrintf;     // 0x102b364c
extern uint8_t gOpenGLPrintfNode[16];   // 0x102b3668, the registration node
void COpenGLPrintf_destroy(COpenGLPrintf* self, int16_t flag);   // 0x10046bdc, ~COpenGLPrintf (not translated)
void __sinit_tb_printf_opengl_cpp();    // 0x10046d30
