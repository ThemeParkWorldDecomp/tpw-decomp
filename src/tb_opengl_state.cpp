// The OpenGL renderer's state keeping (engine library of SimTheme Park 2.04b3, in the
// inventory's tb_printf_opengl.cpp range): the rest of COpenGL's interface, the draw
// primitive's start-up and its blending, texture and fog setters, and the texture page.
// The constructors and start-up entry points are in tb_printf_opengl.cpp. PARTIAL: the
// draw primitive's drawing members (Execute, DrawIndexedPrimitive, the boxes and polys,
// GetPixel, GetPixelFillRate) and COpenGLTexture::Download are still to do.
#include <new>

#include "platform/opengl.h"
#include "tb_ddraw.h"
#include "tb_printf_opengl.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"

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

int32_t COpenGLDrawPrimitive::ExecuteShadow(SPrimitive*) { return 1; }

int32_t COpenGLDrawPrimitive::BeginDraw() {
    glDisable(GL_TEXTURE_2D);
    mTexture = nullptr;
    return glGetError() == 0;
}

int32_t COpenGLDrawPrimitive::EndDraw() {
    glFlush();
    return glGetError() == 0;
}

int32_t COpenGLDrawPrimitive::SupportsOnePassMultiTexture() { return 0; }

// The destructor (0x10044ee8) is in src/tb_opengl_texture_download.cpp.

// ---- COpenGLTexture ---------------------------------------------------------------------
COpenGLTexture::COpenGLTexture() {
    mTexture = 0;
    mMipmaps = 0;
    mBytesPerPixel = 0;
    mPitch = 0;
    mWidth = 0;
    mHeight = 0;
    mMinFilter = GL_NEAREST;
    mState = 0;
}

COpenGLTexture::~COpenGLTexture() { Destroy(); }

// Sixteen lock and unlock rounds timed: bytes sent per clock tick.
double COpenGLTexture::TestUploadSpeed() {
    const int32_t start = gProfiler.GetClockCycleCount10h();
    const int32_t width = mWidth;
    const int32_t height = mHeight;
    for (int32_t i = 0; i < 16; ++i) {
        LockSurface();
        UnlockSurface();
    }
    const int32_t end = gProfiler.GetClockCycleCount10h();
    return static_cast<double>(static_cast<float>(width * height * 0x10) /
                               static_cast<float>(static_cast<uint32_t>((end - start) * 0x10)));
}

int32_t COpenGLTexture::Create(int32_t width, int32_t height, int32_t format, int32_t mipmaps) {
    const CTextureSettings& settings = gei_TextureSettings;
    if ((settings.mFlags & 2) == 0) mipmaps = 0;
    mWidth = width;
    mHeight = height;
    mMipmaps = mipmaps;
    mBytesPerPixel = 4;
    mPitch = width * mBytesPerPixel;
    glGenTextures(1, &mTexture);
    if (glGetError() != 0) return 0;
    glBindTexture(GL_TEXTURE_2D, mTexture);
    if (glGetError() != 0) {
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
        return 0;
    }
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 8448.0f);   // GL_MODULATE
    if (mipmaps < 1) mMinFilter = GL_LINEAR;
    else if ((settings.mFlags & 8) != 0) mMinFilter = GL_LINEAR_MIPMAP_LINEAR;
    else mMinFilter = GL_LINEAR_MIPMAP_NEAREST;
    mState = kStateNearest | kStateNoMipmaps;
    SetState(kStateRepeat);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, gOpenGLTextureData);
    return 1;
}

int32_t COpenGLTexture::Destroy() {
    if (mTexture != 0) {
        glDeleteTextures(1, &mTexture);
        mTexture = 0;
    }
    return glGetError() == 0;
}

void COpenGLTexture::Select(int32_t state) {
    glBindTexture(GL_TEXTURE_2D, mTexture);
    if (glGetError() == 0) SetState(state);
    else glDisable(GL_TEXTURE_2D);
}

// Only what differs from the state the texture was last given is sent.
void COpenGLTexture::SetState(int32_t state) {
    const uint32_t changed = static_cast<uint32_t>(mState ^ state);
    if (changed == 0) return;
    if ((changed & kStateNearest) != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (state & kStateNearest) == 0 ? GL_LINEAR : GL_NEAREST);
    }
    if ((changed & (kStateNearest | kStateNoMipmaps)) != 0) {
        if ((state & kStateNearest) != 0) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        else if ((state & kStateNoMipmaps) != 0) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        else glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mMinFilter);
    }
    if ((changed & kStateRepeat) != 0) {
        const GLint wrap = (state & kStateRepeat) == 0 ? GL_CLAMP : GL_REPEAT;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    }
    mState = state;
}

int32_t COpenGLTexture::LockSurface() { return gOpenGLTextureData != nullptr; }

int32_t COpenGLTexture::UnlockSurface() {
    if (gOpenGLTextureData == nullptr || mTexture == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (mMipmaps == 0) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, gOpenGLTextureData);
    } else {
        gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA4, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, gOpenGLTextureData);
    }
    return glGetError() == 0;
}

int32_t COpenGLTexture::MipmapsUsed() { return mMipmaps; }

int32_t COpenGLTexture::GetPixelCount() { return mWidth * mHeight; }

void COpenGLTexture::GetTexture() {}
