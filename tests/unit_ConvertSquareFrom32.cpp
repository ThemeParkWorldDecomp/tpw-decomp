// Test library for ConvertSquareFrom32, PT_Load, CSwitch::Do and COpenGLPrintf::Close (unit v5953; libtpw_unit_ConvertSquareFrom32.dylib,
// tests/test_ConvertSquareFrom32.py), engine:
//   src/tb_capture_ConvertSquareFrom32.cpp
//
// The callees outside the unit are recording fakes (UPixels, ULogManager, CHField::LoadProcTextures, CTextureCache, COpenGL,
// CDirectDraw, CErrorHandler::Push, EngineAdd_Reinit, glDeleteLists, glGetError); the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "tb_capture.h"
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

using namespace calllog;

// ---- globals of other files, defined here
CDirectDraw* gei_DDraw = nullptr;
CErrorHandler gei_ErrorHandler;
CDisplay gei_Display;
CSystem gei_System;
CTextureSettings gei_TextureSettings;
CTextureCache* gei_TCache = nullptr;
COpenGL* gei_OpenGL = nullptr;
CHField* gei_HField = nullptr;
namespace SamsUtilities {
ULogManager* gLogger = nullptr;
}

CDisplay::CDisplay() {}
CSystem::CSystem() {}
CTextureSettings::CTextureSettings() {}

static int64_t gPixelCount = 0;

static int64_t PixelId(const SamsUtilities::UPixels* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p->mImplementation)); }

// ---- the fakes
namespace SamsUtilities {
UPixels::UPixels(intptr_t format, void* data, int32_t width, int32_t height, int32_t stride, UPixel* palette) {
    mImplementation = reinterpret_cast<UPixelsImplementation*>(static_cast<intptr_t>(++gPixelCount));
    mAlphaCount = 0;
    ev("UPixels", {gPixelCount, static_cast<int64_t>(static_cast<int32_t>(format)), lab(data), width, height, stride,
                   lab(palette)});
}
UPixels::~UPixels() { ev("~UPixels", {PixelId(this)}); }
void UPixels::Convert(UPixels& source, bool dither) { ev("Convert", {PixelId(this), PixelId(&source), dither ? 1 : 0}); }

ULogManager::ULogManager() { ev("ULogManager", {newlab(this)}); }
void ULogManager::SetContext(const char* context, const char* file, int32_t line, uint32_t mask) {
    ev("SetContext", {lab(this), line, mask}, {context, file});
}
void ULogManager::Log(const char* text, ...) { ev("Log", {lab(this)}, {text}); }
}  // namespace SamsUtilities

int32_t CHField::LoadProcTextures(char* name, int32_t size) {
    ev("LPT", {lab(this), size}, {name});
    return static_cast<int32_t>(ret("LPT"));
}

int32_t CTextureCache::Close() {
    ev("TCClose", {lab(this)});
    return static_cast<int32_t>(ret("TCClose"));
}
int32_t CTextureCache::Init(int32_t format) {
    ev("TCInit", {lab(this), format});
    return static_cast<int32_t>(ret("TCInit"));
}

int32_t CDirectDraw::Close() {
    ev("DDClose", {lab(this)});
    return static_cast<int32_t>(ret("DDClose"));
}
int32_t CDirectDraw::Init(int32_t width, int32_t height, int32_t depth, int32_t device, int32_t fullscreen, int32_t videoMemory) {
    ev("DDInit", {lab(this), width, height, depth, device, fullscreen, videoMemory});
    return static_cast<int32_t>(ret("DDInit"));
}

void CErrorHandler::Push(int32_t level, int32_t code) { ev("Push", {lab(this), level, code}); }

bool EngineAdd_Reinit() {
    ev("EAR", {});
    return ret("EAR") != 0;
}

COpenGL::COpenGL() {}
int32_t COpenGL::Close() {
    ev("OGLClose", {lab(this)});
    return static_cast<int32_t>(ret("OGLClose"));
}
int32_t COpenGL::ReInitialize() {
    ev("ReInit", {lab(this)});
    return static_cast<int32_t>(ret("ReInit"));
}
int32_t COpenGL::TempShutdown() { return 0; }
int32_t COpenGL::ClearViewport() { return 0; }
int32_t COpenGL::FlipScreen() { return 0; }
int32_t COpenGL::GetTextureMemory() { return 0; }
int32_t COpenGL::GetVRAMSize() { return 0; }
void* COpenGL::CreateTextureBase() { return nullptr; }

extern "C" {

void glDeleteLists(GLuint list, GLsizei range) { ev("glDeleteLists", {list, static_cast<uint32_t>(range)}); }
GLenum glGetError() {
    ev("glGetError", {});
    return static_cast<GLenum>(ret("glGetError"));
}

uint64_t tpw_cs_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gei_DDraw", &gei_DDraw},
        {"gei_ErrorHandler", &gei_ErrorHandler},
        {"gei_Display", &gei_Display},
        {"gei_System", &gei_System},
        {"gei_TextureSettings", &gei_TextureSettings},
        {"gei_TCache", &gei_TCache},
        {"gei_OpenGL", &gei_OpenGL},
        {"gei_HField", &gei_HField},
        {"gLogger", &SamsUtilities::gLogger},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A COpenGL with its table (the host's virtual call needs one).
uint64_t tpw_cs_new_opengl() {
    void* block = tpw::ArenaAllocZeroed(sizeof(COpenGL) + 16);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (block) COpenGL));
}

void tpw_cs_reset() { gPixelCount = 0; }

int32_t tpw_cs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CSystem, mRenderer));
    out[n++] = static_cast<int64_t>(offsetof(CDisplay, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(CDisplay, mHeight));
    out[n++] = static_cast<int64_t>(offsetof(CDisplay, mBitsPerPixel));
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mFormat));
    out[n++] = static_cast<int64_t>(offsetof(CDirectDraw, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CDirectDraw, mDevice));
    out[n++] = static_cast<int64_t>(sizeof(CSwitch));
    return n;
}

// ---- the functions
void tpw_cs_convert(uint64_t dest, uint64_t source, int32_t size) {
    ConvertSquareFrom32(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(source), size);
}
uint32_t tpw_cs_ptload(uint64_t path, int32_t mode) { return PT_Load(reinterpret_cast<char*>(path), mode); }
int32_t tpw_cs_do(uint64_t self, int32_t toggle) { return reinterpret_cast<CSwitch*>(self)->Do(toggle); }
uint32_t tpw_cs_close(uint64_t self) { return reinterpret_cast<COpenGLPrintf*>(self)->Close() ? 1u : 0u; }

}  // extern "C"
