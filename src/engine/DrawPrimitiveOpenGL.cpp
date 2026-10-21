// DrawPrimitiveOpenGL.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "platform/indirectx.h"
#include "platform/opengl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include "Wavelet.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// OpenGL start-up, Mac 2.04b3. Log contexts identify tb_init_opengl.cpp;
// these functions are filed with tb_printf_opengl.cpp in the inventory.

CDrawPrimitive* gOpenGLDrawPrimitive = nullptr;
uint32_t gOpenGLBlendModes[12];

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
COpenGLDrawPrimitive::COpenGLDrawPrimitive() {
    mF10 = 0; mPasses = 0; mTexture = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
    const uint32_t modes[] = {0, 1, 0x300, 0x301, 0x302, 0x303, 0x304, 0x305, 0x306, 0x307, 0x308};
    for (int i = 0; i < 11; ++i) gOpenGLBlendModes[i + 1] = modes[i];
    mFields[2] = 0x3000;
}

// The OpenGL renderer's state keeping (engine library of SimTheme Park 2.04b3, in the
// inventory's tb_printf_opengl.cpp range): the rest of COpenGL's interface, the draw
// primitive's start-up and its blending, texture and fog setters, and the texture page.
// The constructors and start-up entry points are in tb_printf_opengl.cpp. PARTIAL: the
// draw primitive's drawing members (Execute, DrawIndexedPrimitive, the boxes and polys,
// GetPixel, GetPixelFillRate) and COpenGLTexture::Download are still to do.

// ---- COpenGLDrawPrimitive: start-up and state --------------------------------------------
// The base's mFields[4] is the blending mode last sent to OpenGL; -1 forces everything.
int32_t COpenGLDrawPrimitive::Init() {
    mPasses = SupportsOnePassMultiTexture() ? 1 : 2;
    glShadeModel(GL_SMOOTH);
    glDisable(GL_CULL_FACE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glDisable(GL_DITHER);
    glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GEQUAL, 1.0f);
    glEnable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 8448.0f);   // GL_MODULATE
    // An ATI Rage Pro (vendor 0x1002, device 0x4742) gets a display flag of its own.
    if (gei_DDraw->mDeviceInfo.vendorID == 0x1002 && gei_DDraw->mDeviceInfo.deviceID == 0x4742) {
        gei_Display.mFlags |= 0x08000000;
    }
    gei_Display.mFlags |= 0x20000000;
    mFields[4] = 0xffffffff;
    SetBlendingMode(0);
    return glGetError() == 0;
}

int32_t COpenGLDrawPrimitive::BeginDraw() {
    glDisable(GL_TEXTURE_2D);
    mTexture = nullptr;
    return glGetError() == 0;
}

int32_t COpenGLDrawPrimitive::EndDraw() {
    glFlush();
    return glGetError() == 0;
}

// The OpenGL draw primitive's drawing members (engine library of SimTheme Park 2.04b3, in the
// inventory's tb_printf_opengl.cpp range): where primitives become OpenGL calls. Execute
// hands a queued primitive to DrawIndexedPrimitive, which sets the vertex arrays and draws;
// the boxes, the test polygon and the profiling members build their own vertices on the
// stack and draw them the same way. The state setters, construction and Init are in
// tb_opengl_state.cpp and tb_printf_opengl.cpp.
//
// Every vertex is 0x20 bytes (format 0x1c4: x, y, z, w, colour, specular, u, v). The
// positions of the boxes are in screen space, -1..1, scaled by the display's 2 / width and
// 2 / height. The members that report success return whether gDDResult, the HRESULT of the
// last DirectDraw call, is zero, and push it as an error otherwise; nothing in this file
// writes it.

namespace {

// OpenGL names the engine uses that platform/opengl.h does not have.
enum : GLenum {
    kLineStrip = 3,
    kTriangles = 4,
    kSrcAlpha = 0x0302,
    kOneMinusSrcAlpha = 0x0303,
    kBack = 0x0405,
    kPackAlignment = 0x0d05,
    kUnsignedShort = 0x1403,
    kFloat = 0x1406,
    kAllAttribBits = 0x000fffff,
};

// The vertex format 0x1c4 (D3DFVF_XYZRHW | DIFFUSE | SPECULAR | TEX1).
constexpr uint32_t kVertexFormat = 0x1c4;
constexpr GLsizei kVertexStride = 0x20;

struct DrawVertex {
    float mX, mY, mZ, mW;   // 0x00  screen position, depth and reciprocal w
    uint32_t mColour;       // 0x10
    uint32_t mSpecular;     // 0x14
    float mU, mV;           // 0x18
};
static_assert(sizeof(DrawVertex) == kVertexStride, "DrawVertex layout");

// SVertex as DrawWiredframedBoundingBox takes it: 0x10 bytes.
struct CornerLayout {
    uint32_t mX, mY, mZ, mW;   // float words, copied without looking at them
};

// The blending state words the base class keeps (CDrawPrimitive::mFields[4], what was last
// sent to OpenGL; see COpenGLDrawPrimitive::SetBlendingMode).
enum : uint32_t {
    kStateBlend = 0x100,
    kStateAlphaFunction = 0x200,
    kStateAlphaTest = 0x400,
    kStateNoDepthWrites = 0x800,
    kStateNoDepthTest = 0x8000,
    kStateSpecularPass = 0x80000,   // a primitive's mode: Execute draws it a second time
    kStateBit100000 = 0x100000,     // cleared by DrawWiredframedBoundingBox; no other use seen
};

constexpr float kBoxDepth = 0.001f;        // engine data 0x10050110
constexpr float kEdgeDepth = 0.0001f;      // 0x100500f8
constexpr uint16_t kTwoTriangles[6] = {0, 2, 1, 0, 3, 2};   // 0x10052148, 0x1005213c, 0x100521c4

// The colour and specular words of the first `count` vertices, byte-reversed in place. The
// words are 0xRRGGBBAA and glColorPointer(4, GL_UNSIGNED_BYTE) reads their bytes in memory
// order, which on the Mac is red first. The host is little-endian, so the words are turned
// into the Mac's memory order for the draw and turned back after it (twice is the identity).
void ReverseColourBytes(uint8_t* vertices, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t words[2];
        uint8_t* colours = vertices + static_cast<size_t>(kVertexStride) * i + 0x10;
        std::memcpy(words, colours, sizeof words);
        words[0] = __builtin_bswap32(words[0]);
        words[1] = __builtin_bswap32(words[1]);
        std::memcpy(colours, words, sizeof words);
    }
}

}  // namespace

// ---- Execute and DrawIndexedPrimitive --------------------------------------------------------

// Sends the primitive's blending mode and texture through the virtual setters, then draws it;
// bit 0x80000 of the mode asks for the second pass.
int32_t COpenGLDrawPrimitive::Execute(SPrimitive* primitive) {
    SetBlendingMode(static_cast<int32_t>(primitive->mBlendingMode));
    SetTexture(0, primitive->mTexture);
    const uint32_t flags = (primitive->mBlendingMode & kStateSpecularPass) != 0 ? 0x8000 : 0;
    DrawIndexedPrimitive(primitive->mPrimitiveType, primitive->mVertexFormat, primitive->mVertexData,
                         static_cast<uint32_t>(primitive->mVertexCount), primitive->mIndexData,
                         static_cast<uint32_t>(primitive->mIndexCount), flags);
    if (gDDResult != 0) gei_ErrorHandler.Push(2, gDDResult);
    return gDDResult == 0;
}

// `texture` is the COpenGLTexture itself (NULL: untextured). The stage is not used.
int32_t COpenGLDrawPrimitive::SetTexture(int32_t stage, void* texture) {
    if (mTexture != texture) {
        if (texture == nullptr) {
            glDisable(GL_TEXTURE_2D);
        } else {
            if (mTexture == nullptr) glEnable(GL_TEXTURE_2D);
            static_cast<COpenGLTexture*>(texture)->Select(static_cast<int32_t>(mFields[4]));
        }
        mTexture = texture;
    }
    return glGetError() == 0;
}

// Sends what differs between `mode` and the mode last sent. Bits: 0x400 alpha test, 0x200
// its function, 0x8000 no depth test, 0x800 no depth writes, 0x1000000 no dithering,
// 0x4000000 perspective hint "do not care", 0x100 blending, the low byte the two blend factors (indexes
// into gOpenGLBlendModes, source in the high nibble); 0x20000, 0x40000 and 0x400000 are
// the texture state bits, only remembered here.
int32_t COpenGLDrawPrimitive::SetBlendingMode(int32_t modeBits) {
    const uint32_t mode = static_cast<uint32_t>(modeBits);
    uint32_t& current = mFields[4];
    if (((mode ^ current) & 0x400) != 0) {
        if ((mode & 0x400) == 0) glDisable(GL_ALPHA_TEST);
        else glEnable(GL_ALPHA_TEST);
        current = (current & ~0x400u) | (mode & 0x400);
    }
    if (((mode ^ current) & 0x200) != 0) {
        if ((mode & 0x200) == 0) glAlphaFunc(GL_GREATER, 0.0f);
        else glAlphaFunc(GL_GEQUAL, 1.0f);
        current = (current & ~0x200u) | (mode & 0x200);
    }
    if (((mode ^ current) & 0x8000) != 0) {
        if ((mode & 0x8000) == 0) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        current = (current & ~0x8000u) | (mode & 0x8000);
    }
    if (((mode ^ current) & 0x800) != 0) {
        glDepthMask((mode & 0x800) == 0 ? 1 : 0);
        current = (current & ~0x800u) | (mode & 0x800);
    }
    if (((mode ^ current) & 0x1000000) != 0) {
        if ((mode & 0x1000000) == 0) glEnable(GL_DITHER);
        else glDisable(GL_DITHER);
        current = (current & ~0x1000000u) | (mode & 0x1000000);
    }
    if (((mode ^ current) & 0x4000000) != 0) {
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, (mode & 0x4000000) == 0 ? GL_NICEST : GL_DONT_CARE);
        current = (current & ~0x4000000u) | (mode & 0x4000000);
    }
    current = (mode & 0x20000) | (current & ~0x20000u);
    current = (mode & 0x40000) | (current & ~0x40000u);
    current = (mode & 0x400000) | (current & ~0x400000u);
    if ((gei_Display.mFlags & 0x80000000u) == 0) {
        if (current != mode) {
            if (((mode ^ current) & 0xff) != 0) {
                glBlendFunc(gOpenGLBlendModes[(mode >> 4) & 0xf], gOpenGLBlendModes[mode & 0xf]);
                current = (mode & 0xff) | (current & ~0xffu);
            }
            if (((mode ^ current) & 0x100) != 0) {
                if ((mode & 0x100) == 0) glDisable(GL_BLEND);
                else glEnable(GL_BLEND);
                current = (current & ~0x100u) | (mode & 0x100);
            }
        }
    } else {
        // The display's top flag: everything is blended, additively unless the mode asks
        // for source alpha over one (factors 5 and 2).
        if (((current ^ 0xffffffffu) & 0x100) != 0) {
            glEnable(GL_BLEND);
            current = (current & ~0x100u) | 0x100;
        }
        if ((mode & 0xff) == 0x52) {
            if (((current ^ 0x52) & 0xff) != 0) {
                glBlendFunc(gOpenGLBlendModes[5], gOpenGLBlendModes[2]);
                current = (current & ~0xffu) | 0x52;
            }
        } else if (((current ^ 0x22) & 0xff) != 0) {
            glBlendFunc(gOpenGLBlendModes[2], gOpenGLBlendModes[2]);
            current = (current & ~0xffu) | 0x22;
        }
    }
    return 1;
}

int32_t COpenGLDrawPrimitive::SetFogParameters(float start, float end, uint32_t colour) {
    const float a = static_cast<float>(colour >> 24) / 255.0f;
    const float b = static_cast<float>(colour >> 16 & 0xff) / 255.0f;
    const float c = static_cast<float>(colour >> 8 & 0xff) / 255.0f;
    const float d = static_cast<float>(colour & 0xff) / 255.0f;
    gei_Display.SetFogColor(colour);
    gei_Display.SetFogSpace(start, end);
    glClearColor(a, b, c, d);
    return 1;
}

// Quirk: a primitive type other than 3 and 4 is reported (UDebug is empty in this build) and
// drawn anyway, with whatever the compiler left in r29 as the OpenGL mode. In both callers
// that can pass such a type (Execute and DrawFilledBox) r29 holds `this`, so that is the mode
// here: OpenGL rejects it with an error and draws nothing. A vertex type other than 0x1c4 is
// reported and the arrays are left as they were.
int32_t COpenGLDrawPrimitive::DrawIndexedPrimitive(int32_t primitiveType, uint32_t vertexType, void* vertices,
                                                   uint32_t vertexCount, uint16_t* indices, uint32_t indexCount,
                                                   uint32_t flags) {
    tpw_probe::Hit(tpw_probe::kGLDrawIndexed);
    tpw_probe::Hit(tpw_probe::kGLVertices, vertexCount);
    GLenum mode;
    if (primitiveType == 4) {
        mode = kTriangles;
    } else if (primitiveType == 3) {
        mode = kLineStrip;
    } else {
        SamsUtilities::UDebug("bad primitive type: %d", primitiveType);
        mode = static_cast<GLenum>(reinterpret_cast<uintptr_t>(this));
    }
    uint8_t* bytes = static_cast<uint8_t*>(vertices);
    if (vertexType == kVertexFormat) {
        // GL is given the colours of the vertices it locks in the Mac's byte order; they are
        // put back after the unlock.
        ReverseColourBytes(bytes, vertexCount);
        glVertexPointer(4, kFloat, kVertexStride, vertices);
        glTexCoordPointer(2, kFloat, kVertexStride, bytes + 0x18);
        glColorPointer(4, GL_UNSIGNED_BYTE, kVertexStride, bytes + 0x10);
    } else {
        SamsUtilities::UDebug("bad vertex type: %d", static_cast<int32_t>(vertexType));
    }
    glLockArraysEXT(0, static_cast<GLsizei>(vertexCount));
    glDrawElements(mode, static_cast<GLsizei>(indexCount), kUnsignedShort, indices);
    if ((flags & 0x8000) != 0) {
        // The second pass: the specular colour over the picture, blended by its alpha, with
        // depth testing on, depth writes and the texture off.
        glPushAttrib(kAllAttribBits);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(0);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(kSrcAlpha, kOneMinusSrcAlpha);
        glColorPointer(4, GL_UNSIGNED_BYTE, kVertexStride, bytes + 0x14);
        glDrawElements(mode, static_cast<GLsizei>(indexCount), kUnsignedShort, indices);
        glPopAttrib();
    }
    glUnlockArraysEXT();
    if (vertexType == kVertexFormat) ReverseColourBytes(bytes, vertexCount);
    if ((flags & 1) != 0) glFinish();
    return 0;
}

namespace SamsUtilities {
// 0x10043b20: only stores its argument registers into the caller's frame; nothing observable.
// Weak: the platform layer (log.cpp) defines the logging port of the same function and wins at link time.
__attribute__((weak)) void UDebug(const char*, ...) {}

}  // namespace SamsUtilities

// 0x10046bdc: ~COpenGLPrintf, the hidden delete flag is the second argument.
void COpenGLPrintf_destroy(COpenGLPrintf* self, int16_t flag) {
    if (self != nullptr) {
        self->Close();
        if (flag > 0) operator delete(self);
    }
}

// A quad of four corner colours (white, 0xff0000ff, 0x00ff00ff, 0x0000ffff) 0.9 of the way to
// each screen corner, drawn between BeginDraw and EndDraw, which gives the result.
int32_t COpenGLDrawPrimitive::DrawTestPoly() {
    uint16_t indices[6];
    std::memcpy(indices, kTwoTriangles, sizeof indices);
    DrawVertex vertices[4] = {
        {-0.9f, -0.9f, 0.5f, 1.0f, 0xffffffff, 0, 0.0f, 0.0f},
        {0.9f, -0.9f, 0.5f, 1.0f, 0xff0000ff, 0, 0.0f, 0.0f},
        {0.9f, 0.9f, 0.5f, 1.0f, 0x00ff00ff, 0, 0.0f, 0.0f},
        {-0.9f, 0.9f, 0.5f, 1.0f, 0x0000ffff, 0, 0.0f, 0.0f},
    };
    BeginDraw();
    SetTexture(0, nullptr);
    DrawIndexedPrimitive(4, kVertexFormat, vertices, 4, indices, 6, 0);
    return EndDraw();
}

// A textured triangle at (0.5, 0.5) pixels of the back buffer, in immediate mode: corners
// (0.5, 0.5), (0.5 + size, 0.5) and (0.5, 0.5 + size) in pixels, mapped to the texture
// coordinates (u0, v0), (u1, v0) and (u0, v1). `colour` is read by glColor4ubv from the
// stack, so its bytes go in memory order of the Mac: the high byte first. The second integer
// is not used.
int32_t COpenGLDrawPrimitive::DrawDetectPoly(float size, int32_t colour, int32_t, float u0, float v0, float u1,
                                             float v1) {
    const float scaleX = gei_Display.mTwoOverWidth;
    const float scaleY = gei_Display.mTwoOverHeight;
    const float farEdge = 0.5f + size;
    const float left = ppc_fmadds(0.5f, scaleX, -1.0f);
    const float top = ppc_fmadds(0.5f, scaleY, -1.0f);
    const float right = ppc_fmsubs(farEdge, scaleX, 1.0f);
    const float bottom = ppc_fmsubs(farEdge, scaleY, 1.0f);
    const uint32_t packed = static_cast<uint32_t>(colour);
    const uint8_t components[4] = {static_cast<uint8_t>(packed >> 24), static_cast<uint8_t>(packed >> 16),
                                   static_cast<uint8_t>(packed >> 8), static_cast<uint8_t>(packed)};
    glDrawBuffer(kBack);
    glBegin(kTriangles);
    glColor4ubv(components);
    glTexCoord2f(u0, v0);
    glVertex4f(left, top, 1.0f, 1.0f);
    glTexCoord2f(u1, v0);
    glVertex4f(right, top, 1.0f, 1.0f);
    glTexCoord2f(u0, v1);
    glVertex4f(left, bottom, 1.0f, 1.0f);
    glEnd();
    glFlush();
    return glGetError() == 0;
}

// ---- The boxes and polygons ---------------------------------------------------------------------

// A box from (x1, y1) to (x2, y2) in pixels (y down), filled with `colour` and optionally
// textured with the whole of `texture`, drawn without depth testing or alpha testing. A
// colour whose low byte is 0xff is opaque (blending is switched off); any other is blended
// by source alpha (mode 0x56) with the alpha function reset. A corner is x * (2 / width) - 1
// and 1 - y * (2 / height), each a fused multiply-subtract.
int32_t COpenGLDrawPrimitive::DrawFilledBox(float x1, float y1, float x2, float y2, int32_t colour,
                                            CTextureBase* texture) {
    uint16_t indices[6];
    std::memcpy(indices, kTwoTriangles, sizeof indices);
    const float scaleX = gei_Display.mTwoOverWidth;
    const float scaleY = gei_Display.mTwoOverHeight;
    const float left = ppc_fmsubs(x1, scaleX, 1.0f);
    const float top = ppc_fnmsubs(y1, scaleY, 1.0f);
    const float right = ppc_fmsubs(x2, scaleX, 1.0f);
    const float bottom = ppc_fnmsubs(y2, scaleY, 1.0f);
    const uint32_t packed = static_cast<uint32_t>(colour);
    DrawVertex vertices[4] = {
        {left, top, kBoxDepth, 1.0f, packed, 0, 0.0f, 0.0f},
        {right, top, kBoxDepth, 1.0f, packed, 0, 1.0f, 0.0f},
        {right, bottom, kBoxDepth, 1.0f, packed, 0, 1.0f, 1.0f},
        {left, bottom, kBoxDepth, 1.0f, packed, 0, 0.0f, 1.0f},
    };
    uint32_t& state = mFields[4];
    if ((packed & 0xff) == 0xff) {
        if ((state & kStateBlend) != 0) {
            glDisable(GL_BLEND);
            state &= ~kStateBlend;
        }
    } else {
        if (((state ^ 0x56) & 0xff) != 0) {
            glBlendFunc(gOpenGLBlendModes[5], gOpenGLBlendModes[6]);
            state = (state & 0xffffff00u) | 0x56;
        }
        if ((~state & kStateBlend) != 0) {
            glEnable(GL_BLEND);
            state = (state & ~kStateBlend) | kStateBlend;
        }
        if ((state & kStateAlphaFunction) != 0) {
            glAlphaFunc(GL_GREATER, 0.0f);
            state &= ~kStateAlphaFunction;
        }
    }
    if ((~state & kStateNoDepthTest) != 0) {
        glDisable(GL_DEPTH_TEST);
        state = (state & ~kStateNoDepthTest) | kStateNoDepthTest;
    }
    if ((state & kStateAlphaTest) != 0) {
        glDisable(GL_ALPHA_TEST);
        state &= ~kStateAlphaTest;
    }
    if (texture != nullptr) {
        // The page's GetTexture slot (COpenGLTexture::GetTexture, 0x10046adc, an empty function
        // behind a thunk that moves `this` from the CTextureBase to the whole page) returns the
        // COpenGLTexture the setter wants.
        SetTexture(0, dynamic_cast<COpenGLTexture*>(texture));
    } else {
        SetTexture(0, nullptr);
    }
    DrawIndexedPrimitive(4, kVertexFormat, vertices, 4, indices, 6, 0);
    if (gDDResult != 0) gei_ErrorHandler.Push(2, gDDResult);
    return gDDResult == 0;
}

// Draws nothing. The original sets the texture to none, fills a block of vertices on its stack
// from the points (x and z scaled by `scale`, depth 0.000001, colour `colour`) and then
// does not use them: the drawing call is gone from this build. The block holds 17 vertices,
// so more than 17 points overwrite the caller's stack; that is not reproduced (nothing reads
// the block, so neither is it built).
int32_t COpenGLDrawPrimitive::DrawPoly(XZType* points, int32_t count, int32_t colour, float scale) {
    SetTexture(0, nullptr);
    if (gDDResult != 0) gei_ErrorHandler.Push(2, gDDResult);
    return gDDResult == 0;
}

// The screen's frame: a rectangle of the whole screen whose inner edge is 5 pixels in, opaque
// (colour 0xff) on the outer corners and clear on the inner ones, drawn as eight triangles
// with blending mode 0x9d56. The inner corners are -1 + 5 * (2 / size) as a double sum
// rounded to single; the far ones 1 - 5 * (2 / size) in single precision.
int32_t COpenGLDrawPrimitive::DrawEdgeBox() {
    static const uint16_t kFrameIndices[24] = {0, 3, 2, 0, 2, 1, 2, 3, 7, 2, 7, 6,
                                               5, 6, 7, 5, 7, 4, 0, 1, 5, 0, 5, 4};   // 0x10052154
    uint16_t indices[24];
    std::memcpy(indices, kFrameIndices, sizeof indices);
    const float borderX = 5.0f * gei_Display.mTwoOverWidth;
    const float borderY = 5.0f * gei_Display.mTwoOverHeight;
    const float innerLeft = static_cast<float>(-1.0 + static_cast<double>(borderX));
    const float innerRight = 1.0f - borderX;
    const float innerTop = static_cast<float>(-1.0 + static_cast<double>(borderY));
    const float innerBottom = 1.0f - borderY;
    const float outerLow = -1.0f;   // 0x10052184 and 0x10052194, the first word of each
    const float outerHigh = 1.0f;   // and their last
    DrawVertex vertices[8] = {
        {outerLow, outerLow, kEdgeDepth, 1.0f, 0xff, 0, 0.0f, 0.0f},
        {innerLeft, innerTop, kEdgeDepth, 1.0f, 0, 0, 0.0f, 0.0f},
        {innerRight, innerTop, kEdgeDepth, 1.0f, 0, 0, 0.0f, 0.0f},
        {outerHigh, outerLow, kEdgeDepth, 1.0f, 0xff, 0, 0.0f, 0.0f},
        {outerLow, outerHigh, kEdgeDepth, 1.0f, 0xff, 0, 0.0f, 0.0f},
        {innerLeft, innerBottom, kEdgeDepth, 1.0f, 0, 0, 0.0f, 0.0f},
        {innerRight, innerBottom, kEdgeDepth, 1.0f, 0, 0, 0.0f, 0.0f},
        {outerHigh, outerHigh, kEdgeDepth, 1.0f, 0xff, 0, 0.0f, 0.0f},
    };
    SetTexture(0, nullptr);
    SetBlendingMode(0x9d56);
    DrawIndexedPrimitive(4, kVertexFormat, vertices, 8, indices, 0x18, 0);
    if (gDDResult != 0) gei_ErrorHandler.Push(2, gDDResult);
    return 1;
}

// The eight corners of a box (SVertex x, y, z, w, one after another) as a wire frame of
// white lines, with the faces named by `faces` in 0xff0000ff: bit 0 the corners 0, 2, 4, 6;
// bit 1 the corners 1, 3, 5, 7; bit 2 0..3; bit 3 4..7; bit 4 2, 3, 6, 7; bit 5 0, 1, 4, 5. The
// math unit projects the corners first (in place, to the screen). Depth testing and writing
// are put on and blending off in the remembered state, with the matching OpenGL calls, after
// blending mode 0x56 is set; bits 0x80000 and 0x100000 of the state are dropped. Drawn as
// one line strip of 16 indices, ending in a glFinish. Always 1.
int32_t COpenGLDrawPrimitive::DrawWiredframedBoundingBox(SVertex* corners, int32_t, int32_t faces) {
    static const uint16_t kWireIndices[16] = {0, 2, 6, 4, 0, 1, 5, 4, 6, 7, 5, 1, 3, 2, 3, 7};   // 0x100521a4
    uint16_t indices[16];
    std::memcpy(indices, kWireIndices, sizeof indices);
    DrawVertex vertices[8];
    std::memset(vertices, 0, sizeof vertices);
    const CornerLayout* source = reinterpret_cast<const CornerLayout*>(corners);
    for (int32_t i = 0; i < 8; ++i) {
        std::memcpy(&vertices[i], &source[i], sizeof(CornerLayout));
        vertices[i].mColour = 0xffffffff;
    }
    constexpr uint32_t kMarked = 0xff0000ff;
    const uint32_t flags = static_cast<uint32_t>(faces);
    if ((flags & 1) != 0) vertices[6].mColour = vertices[4].mColour = vertices[2].mColour = vertices[0].mColour = kMarked;
    if ((flags & 2) != 0) vertices[7].mColour = vertices[5].mColour = vertices[3].mColour = vertices[1].mColour = kMarked;
    if ((flags & 4) != 0) vertices[3].mColour = vertices[2].mColour = vertices[1].mColour = vertices[0].mColour = kMarked;
    if ((flags & 8) != 0) vertices[7].mColour = vertices[6].mColour = vertices[5].mColour = vertices[4].mColour = kMarked;
    if ((flags & 0x10) != 0) vertices[7].mColour = vertices[6].mColour = vertices[3].mColour = vertices[2].mColour = kMarked;
    if ((flags & 0x20) != 0) vertices[5].mColour = vertices[4].mColour = vertices[1].mColour = vertices[0].mColour = kMarked;
    gei_MathUnit->Project(vertices, kVertexStride, 8, 0);
    SetTexture(0, nullptr);
    SetBlendingMode(0x56);
    uint32_t& state = mFields[4];
    if ((state & kStateBlend) != 0) {
        glDisable(GL_BLEND);
        state &= ~kStateBlend;
    }
    if ((state & kStateNoDepthTest) != 0) {
        glEnable(GL_DEPTH_TEST);
        state &= ~kStateNoDepthTest;
    }
    if ((state & kStateNoDepthWrites) != 0) {
        glDepthMask(1);
        state &= ~kStateNoDepthWrites;
    }
    if ((state & kStateSpecularPass) != 0) state &= ~kStateSpecularPass;
    if ((state & kStateBit100000) != 0) state &= ~kStateBit100000;
    DrawIndexedPrimitive(3, kVertexFormat, vertices, 8, indices, 0x10, 9);
    return 1;
}

// ---- Reading and timing -----------------------------------------------------------------------

// The colour of the back buffer's pixel (x, y), counted from 1: four bytes, red first, into
// *pixel.
int32_t COpenGLDrawPrimitive::GetPixel(int32_t x, int32_t y, uint32_t* pixel) {
    glPixelStorei(kPackAlignment, 1);
    glReadBuffer(kBack);
    glReadPixels(x + 1, y + 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    return glGetError() == 0;
}

// Draws a screen-sized quad of two triangles (24 copies of them, 48 indices) three times
// between BeginDraw and EndDraw in blending mode 0x9256 and returns the pixels filled per
// clock tick: width * height * 50 over the ticks taken, times 16. 0 when BeginDraw or EndDraw
// fails or a drawing call leaves a DirectDraw error. The ratio of two single-precision
// conversions, so a time of 0 gives infinity or NaN.
float COpenGLDrawPrimitive::GetPixelFillRate() {
    CProfiler* profiler = &gProfiler;
    const uint32_t pixels = static_cast<uint32_t>(gei_Display.mWidth) *
                            (static_cast<uint32_t>(gei_Display.mHeight) * 0x32u);
    uint16_t indices[0x30];
    for (int32_t i = 0; i < 0x30; ++i) indices[i] = kTwoTriangles[i % 6];
    DrawVertex vertices[4] = {
        {0.0f, 0.0f, 1.0f, 1.0f, 0xffffffff, 0, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f, 1.0f, 0xff0000ff, 0, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f, 0x00ff00ff, 0, 0.0f, 0.0f},
        {0.0f, 1.0f, 1.0f, 1.0f, 0x0000ffff, 0, 0.0f, 0.0f},
    };
    SetTexture(0, nullptr);
    if (BeginDraw() == 0) return 0.0f;
    SetBlendingMode(0x9256);
    const int32_t start = profiler->GetClockCycleCount10h();
    for (int32_t pass = 0; pass < 3; ++pass) {
        DrawIndexedPrimitive(4, kVertexFormat, vertices, 4, indices, 0x30, 0);
        if (gDDResult != 0) {
            EndDraw();
            return 0.0f;
        }
    }
    const int32_t end = profiler->GetClockCycleCount10h();
    const int32_t ticks = end - start;
    if (EndDraw() == 0) return 0.0f;
    return static_cast<float>(static_cast<int32_t>(pixels)) /
           static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(ticks) << 4));
}

int32_t COpenGLDrawPrimitive::SupportsOnePassMultiTexture() { return 0; }

// COpenGLTexture::Download (engine library of SimTheme Park 2.04b3, the inventory's
// tb_printf_opengl.cpp range, 0x100465f8, 1,068 bytes) and COpenGLDrawPrimitive's destructor
// (0x10044ee8, 136 bytes).
//
// Download copies a texture entry's 16-bit image into the OpenGL page's upload buffer
// (gOpenGLTextureData, one shared 256 x 256 RGBA buffer) at the place the cache gave the entry;
// CTextureCache::UploadToVRAM calls it through the page's CTextureBase vtable, and
// CTextureLink::PageUpdated later sends the buffer to OpenGL (UnlockSurface). Each 4-bit
// channel of a pixel becomes the high nibble of a byte, so the buffer holds red, green, blue
// and alpha bytes in that order (the Mac stored the word 0xRRGGBBAA big-endian; this writes the
// bytes in the same order).

// The original restores the vtable pointers of the object and of its virtual base, runs the
// base's destructor when it is the complete object (the flag in r4 is not zero) and frees the
// block when the flag is positive; the compiler does the same for this empty destructor.
COpenGLDrawPrimitive::~COpenGLDrawPrimitive() {}

int32_t COpenGLDrawPrimitive::DrawProfileBox() { return 0; }

int32_t COpenGLDrawPrimitive::ExecuteShadow(SPrimitive*) { return 1; }
