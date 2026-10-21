// Test entry points for the texture pages and the OpenGL draw primitive's state (library
// unit_TexturePages: tb_texturecache.cpp, tb_opengl_state.cpp, tb_drawprimitive.cpp). A
// test runs a script of operations on a texture cache, four texture pages of its own, a
// draw primitive and a handful of texture entries; after each operation it reads their
// state as a flat list of numbers, in the order tests/test_TexturePages.py reads the
// original's. Every OpenGL call is a fake that records its arguments.
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void observe_allocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER observe_allocation
// A whole cache is about 900 pages, each a texture and a link with a label of its own.
#define TPW_CALLLOG_LABELS 8192
#define TPW_CALLLOG_QUEUE_VALUES 512
#include "calllog.cpp"

#include "platform/opengl.h"
#include "tb_ddraw.h"
#include "tb_error.h"
#include "tb_loader_dependencies.h"
#include "tb_printf_opengl.h"
#include "tb_profiler.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"

using namespace calllog;

namespace {
int gQuiet = 0;
struct Quiet {
    Quiet() { ++gQuiet; }
    ~Quiet() { --gQuiet; }
};
bool recording() { return on() && gQuiet == 0; }

// A script that creates a whole cache makes hundreds of pages: more OpenGL calls than the
// call log holds. Such a script runs "in bulk": the OpenGL fakes and operator new still
// work (names, labels, the error queue) but record nothing.
int gBulk = 0;

// Every operator new of the code under test is an event with a new label; the object
// starts filled with 0xa5, as the PowerPC side's does.
void observe_allocation(std::size_t size, bool array, void* block) {
    if (!recording()) return;
    Quiet quiet;
    if (!array) std::memset(block, 0xa5, size);
    const int32_t label = newlab(block);
    if (!gBulk) ev(array ? "new[]" : "new", {label});
}

#define GLEV(...)                 \
    do {                          \
        if (!gBulk) ev(__VA_ARGS__); \
    } while (0)

char gUploadBuffer;      // stands for the shared upload buffer (label 5)
char gGLInstance;        // stands for the UOpenGL instance (label 6)
uint32_t gNextName;      // the names glGenTextures hands out
}  // namespace

// The "New" queue makes an allocation fail: of a texture page or of the cache itself, the
// two the original checks for NULL. (It writes through a page link's pointer unchecked.)
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (recording() && (size == sizeof(COpenGLTexture) || size == sizeof(CTextureCache)) && !ret("New", 1)) return nullptr;
    return operator new(size);
}

// ---- OpenGL -------------------------------------------------------------------------------
extern "C" {
GLenum glGetError() {
    Quiet quiet;
    GLEV("glGetError");
    return static_cast<GLenum>(ret("glGetError", 0));
}
void glGenTextures(GLsizei n, GLuint* textures) {
    Quiet quiet;
    GLEV("glGenTextures", {n});
    *textures = gNextName++;
}
void glDeleteTextures(GLsizei n, const GLuint* textures) {
    Quiet quiet;
    GLEV("glDeleteTextures", {n, *textures});
}
void glBindTexture(GLenum target, GLuint texture) {
    Quiet quiet;
    GLEV("glBindTexture", {target, texture});
}
void glTexEnvf(GLenum target, GLenum pname, GLfloat param) {
    Quiet quiet;
    GLEV("glTexEnvf", {target, pname, fbits(param)});
}
void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    Quiet quiet;
    GLEV("glTexParameteri", {target, pname, param});
}
void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const void* pixels) {
    Quiet quiet;
    GLEV("glTexImage2D", {target, level, internalFormat, width, height, border, format, type, lab(pixels)});
}
void glTexSubImage2D(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
                     GLenum type, const void* pixels) {
    Quiet quiet;
    GLEV("glTexSubImage2D", {target, level, x, y, width, height, format, type, lab(pixels)});
}
GLint gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format,
                        GLenum type, const void* data) {
    Quiet quiet;
    GLEV("gluBuild2DMipmaps", {target, internalFormat, width, height, format, type, lab(data)});
    return 0;
}
void glPixelStorei(GLenum pname, GLint param) {
    Quiet quiet;
    GLEV("glPixelStorei", {pname, param});
}
void glDisable(GLenum cap) {
    Quiet quiet;
    GLEV("glDisable", {cap});
}
void glEnable(GLenum cap) {
    Quiet quiet;
    GLEV("glEnable", {cap});
}
void glShadeModel(GLenum mode) {
    Quiet quiet;
    GLEV("glShadeModel", {mode});
}
void glHint(GLenum target, GLenum mode) {
    Quiet quiet;
    GLEV("glHint", {target, mode});
}
void glAlphaFunc(GLenum func, GLfloat ref) {
    Quiet quiet;
    GLEV("glAlphaFunc", {func, fbits(ref)});
}
void glBlendFunc(GLenum source, GLenum dest) {
    Quiet quiet;
    GLEV("glBlendFunc", {source, dest});
}
void glFlush() {
    Quiet quiet;
    GLEV("glFlush");
}
void glDepthMask(uint8_t flag) {
    Quiet quiet;
    GLEV("glDepthMask", {flag});
}
void glClearColor(GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
    Quiet quiet;
    GLEV("glClearColor", {fbits(a), fbits(b), fbits(c), fbits(d)});
}
}  // extern "C"

SamsUtilities::UOpenGL* SamsUtilities::UOpenGL::GetCurrentInstance() {
    Quiet quiet;
    GLEV("GetCurrentInstance");
    return reinterpret_cast<UOpenGL*>(&gGLInstance);
}
void SamsUtilities::UOpenGL::Swap() {
    Quiet quiet;
    GLEV("Swap", {lab(this)});
}

// ---- The engine around the unit -------------------------------------------------------------
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) {
    Quiet quiet;
    ev("Error", {level, code});
}

CSystem::CSystem() {}
CSystem gei_System;
CDisplay::CDisplay() {}
CDisplay gei_Display;
void CDisplay::SetFogColor(uint32_t argb) {
    Quiet quiet;
    ev("SetFogColor", {argb});
}
void CDisplay::SetFogSpace(float nearDistance, float farDistance) {
    Quiet quiet;
    ev("SetFogSpace", {fbits(nearDistance), fbits(farDistance)});
}
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CDirectDraw::CDirectDraw() {}
CDirectDraw::~CDirectDraw() {}
CDirectDraw gDirectDraw;
CDirectDraw* gei_DDraw = &gDirectDraw;
CProfiler gProfiler;
int32_t CProfiler::GetClockCycleCount10h() {
    Quiet quiet;
    ev("Clock");
    return static_cast<int32_t>(ret("Clock", 0));
}
void* gOpenGLTextureData = nullptr;
uint32_t gOpenGLBlendModes[12];
alignas(8) char gOpenGLStore[64];
COpenGL* gei_OpenGL = reinterpret_cast<COpenGL*>(gOpenGLStore);
int32_t COpenGL::MaxTextures(int32_t size, int32_t format) {
    Quiet quiet;
    ev("MaxTextures", {size, format});
    return static_cast<int32_t>(ret("MaxTextures", 0));
}
// The engine's own (tb_resource_texture.cpp, tested there).
int32_t ConvertSurfaceSizeToFlag(int32_t size) {
    for (int32_t flag = 0; flag < 9; ++flag) {
        if (size == 256 >> flag) return flag;
    }
    return -1;
}

// The renderer the cache asks for pages: the real COpenGL::CreateTextureBase behind the
// interface (it does not use its object).
namespace {
class FakeRenderSystem : public CRenderSystem {
public:
    int32_t ClearViewport() override { return 0; }
    int32_t FlipScreen() override { return 0; }
    int32_t TempShutdown() override { return 0; }
    int32_t ReInitialize() override { return 0; }
    int32_t GetTextureMemory() override { return 0; }
    int32_t GetVRAMSize() override { return 0; }
    void* CreateTextureBase() override { return gei_OpenGL->COpenGL::CreateTextureBase(); }
};
FakeRenderSystem gRenderSystem;
}  // namespace

// Members of the unit's classes that are still to do, or belong to another file.
int32_t COpenGLTexture::Download(CTextureEntry* entry, int32_t x, int32_t y) {
    Quiet quiet;
    ev("Download", {lab(entry), x, y});
    return 1;
}
COpenGLDrawPrimitive::COpenGLDrawPrimitive() {}   // tb_printf_opengl.cpp's; the test sets the fields
COpenGLDrawPrimitive::~COpenGLDrawPrimitive() {}   // src/engine/DrawPrimitiveOpenGL.cpp's
int32_t COpenGLDrawPrimitive::Execute(SPrimitive*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawTestPoly() { return 0; }
int32_t COpenGLDrawPrimitive::DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawEdgeBox() { return 0; }
int32_t COpenGLDrawPrimitive::DrawPoly(XZType*, int32_t, int32_t, float) { return 0; }
int32_t COpenGLDrawPrimitive::DrawProfileBox() { return 0; }
float COpenGLDrawPrimitive::GetPixelFillRate() { return 0.0f; }
int32_t COpenGLDrawPrimitive::DrawWiredframedBoundingBox(SVertex*, int32_t, int32_t) { return 0; }
int32_t COpenGLDrawPrimitive::GetPixel(int32_t, int32_t, uint32_t*) { return 0; }
int32_t COpenGLDrawPrimitive::DrawDetectPoly(float, int32_t, int32_t, float, float, float, float) { return 0; }
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    Quiet quiet;
    ev("assert", {line}, {condition, file});
}

extern int32_t gDrawPrimitiveNvidia;

namespace {

enum { kTextures = 4, kEntries = 16 };
CTextureBase* gTextures[kTextures];
COpenGLDrawPrimitive* gDraw;
alignas(16) unsigned char gDrawStore[sizeof(COpenGLDrawPrimitive) + 16];
CTextureEntry* gEntries[kEntries];
bool gPageAlive[kEntries];   // whether a planted entry's page still exists
STexturePlace* gPlaces[kEntries];

int64_t* gOut;
int32_t gCount, gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOut[gCount] = value;
    ++gCount;
}
uint32_t bits(float value) {
    uint32_t b;
    std::memcpy(&b, &value, 4);
    return b;
}
float from_bits(uint32_t b) {
    float value;
    std::memcpy(&value, &b, 4);
    return value;
}

void TextureState(CTextureBase* base) {
    auto* texture = dynamic_cast<COpenGLTexture*>(base);
    put(base->mFlags);
    put(base->mWidth);
    put(base->mHeight);
    put(base->mPitch);
    put(texture->mTexture);
    put(texture->mMipmaps);
    put(texture->mBytesPerPixel);
    put(texture->mState);
    put(texture->mMinFilter);
}

int32_t EntryIndex(const CTextureEntry* entry) {
    if (entry == nullptr) return 0;
    for (int32_t i = 0; i < kEntries; ++i) {
        if (gEntries[i] == entry) return i + 1;
    }
    return -1;
}

void CacheState() {
    const CTextureCache* cache = gei_TCache;
    put(lab(cache));
    if (cache == nullptr) return;
    for (int32_t size = 0; size < CTextureCache::kSizes; ++size) {
        const int32_t count = cache->mPageCount[size];
        put(count);
        for (int32_t page = 0; page < count && page < CTextureCache::kPagesPerSize; ++page) {
            const CTextureLink* link = cache->mPages[size][page];
            put(lab(link));
            if (link == nullptr || lab(link) <= 0) continue;
            put(link->mF08);
            put(link->mF0C);
            put(link->mF14);
            put(link->mTexture != nullptr);
            if (link->mTexture != nullptr) TextureState(link->mTexture);
            int32_t guard = 0;
            for (const CTextureEntry* entry = link->mEntries; entry != nullptr && guard < 32; entry = entry->mNext, ++guard) {
                put(EntryIndex(entry));
            }
            put(-7);
        }
    }
    for (int32_t value : cache->mF1830) put(value);
    put(cache->mF1860);
    for (float value : cache->mShare) put(bits(value));
    for (int32_t value : cache->mWanted) put(value);
    for (int32_t value : cache->mF1894) put(value);
    put(reinterpret_cast<intptr_t>(cache->mF18AC));
    put(cache->mF18B0);
    put(cache->mFlags);
    put(cache->mF18B8);
    put(cache->mF18BC);
    put(cache->mSizeMask);
    put(cache->mMipmaps);
    put(cache->mLargestSize);
    put(cache->mUITextureSize);
    put(cache->mF18DC);
    put(cache->mF18E0);
    put(bits(cache->mF18E8));
    put(cache->mF1908);
}

}  // namespace

extern "C" {

void tpw_tp_bulk(int32_t bulk) { gBulk = bulk; }
void* tpw_tp_upload_buffer() { return &gUploadBuffer; }
void* tpw_tp_gl_instance() { return &gGLInstance; }

// Recording must be off. Frees what the last script left and puts the globals back.
void tpw_tp_reset() {
    Quiet quiet;
    gei_System.mRenderSystem = &gRenderSystem;
    if (gei_TCache != nullptr) {
        // The planted entries are the test's own: off the pages before those go.
        for (int32_t size = 0; size < CTextureCache::kSizes; ++size) {
            for (int32_t page = 0; page < gei_TCache->mPageCount[size]; ++page) {
                if (gei_TCache->mPages[size][page] != nullptr) gei_TCache->mPages[size][page]->mEntries = nullptr;
            }
        }
        TCache_Close();
    }
    for (CTextureBase*& texture : gTextures) {
        delete texture;
        texture = nullptr;
    }
    for (int32_t i = 0; i < kEntries; ++i) {
        std::free(gEntries[i]);
        std::free(gPlaces[i]);
        gEntries[i] = nullptr;
        gPlaces[i] = nullptr;
        gPageAlive[i] = false;
    }
    if (gDraw != nullptr) gDraw->~COpenGLDrawPrimitive();
    gDraw = nullptr;
    gDrawPrimitiveNvidia = 0;
    gNextName = 1001;
    gOpenGLTextureData = &gUploadBuffer;
    gei_System.mRenderer = 3;
    gei_Display.mFlags = 0;
    gei_TextureSettings.mFlags = 0;
    gei_TextureSettings.mFormat = 4;
    gei_TextureSettings.mTextureMemoryLimit = 0;
    gei_TextureSettings.mTextureMemory = 0;
    gDirectDraw.mDeviceInfo.vendorID = 0;
    gDirectDraw.mDeviceInfo.deviceID = 0;
    gDirectDraw.mHardwareCaps.dwVidMemTotal = 0;
    for (uint32_t i = 0; i < 12; ++i) gOpenGLBlendModes[i] = 0x7000 + i;
    for (int32_t& value : gFrequencyTable) value = 0;
}

int64_t tpw_tp_op(int32_t op, const int32_t* a) {
    CTextureCache* cache = gei_TCache;
    // The operations that can create or delete pages: a planted entry's page may be gone.
    if (op == 1 || op == 2 || op == 6 || (op >= 8 && op <= 11)) {
        for (bool& alive : gPageAlive) alive = false;
    }
    switch (op) {
    case 1: return TCache_Init(a[0]);
    case 2: return TCache_Close();
    case 3:
        if (cache == nullptr) return -9;
        cache->Flush(a[0]);
        return 0;
    case 4: {   // put a[2] new entries on page a[1] of size a[0]; their fields from a[3]
        if (cache == nullptr) return -9;
        if (a[1] >= cache->mPageCount[a[0]]) return -8;
        CTextureLink* link = cache->mPages[a[0]][a[1]];
        uint32_t seed = static_cast<uint32_t>(a[3]);
        int32_t made = 0;
        for (int32_t i = 0; i < kEntries && made < a[2]; ++i) {
            if (gEntries[i] != nullptr) continue;
            Quiet quiet;
            auto* entry = static_cast<CTextureEntry*>(std::calloc(1, sizeof(CTextureEntry)));
            auto* place = static_cast<STexturePlace*>(std::malloc(sizeof(STexturePlace)));
            std::memset(place, 0x5a, sizeof *place);
            seed = seed * 1103515245u + 12345u;
            entry->mPlace = place;
            entry->mFlags = (seed >> 4) & 0x00ffffff;
            // a[4] bit 0: every entry is one the selective flush takes.
            if (a[4] & 1) entry->mFlags |= 0x200000;
            if (a[4] & 2) entry->mFlags &= ~0x200000u;
            entry->mWidth = static_cast<int16_t>(1 + ((seed >> 8) & 0xff));
            entry->mHeight = static_cast<int16_t>(1 + ((seed >> 16) & 0xff));
            entry->mCacheWidth = 0x1111;
            entry->mCacheHeight = 0x2222;
            entry->mPriority = static_cast<float>((seed >> 20) & 0x3ff) / 512.0f;
            entry->mSortKey = 0x33333333;
            entry->mPage = link;
            entry->mNext = link->mEntries;
            link->mEntries = entry;
            gEntries[i] = entry;
            gPlaces[i] = place;
            gPageAlive[i] = true;
            ++made;
        }
        return made;
    }
    case 5:
        if (cache == nullptr) return -9;
        cache->SetUITextureSize();
        return 0;
    case 6:
        if (cache == nullptr) return -9;
        return cache->AutoDetectPageCreation();
    case 7: {   // a cache made by its constructor alone
        if (cache != nullptr) return -9;
        void* block = ::operator new(sizeof(CTextureCache));
        gei_TCache = new (block) CTextureCache;
        return 0;
    }
    case 8: {
        if (cache == nullptr) return -9;
        int32_t count = a[1];
        const int32_t result = cache->CreateSurfaces(a[0], count, a[2], a[3]);
        return result * 1000 + count;
    }
    case 9:
        if (cache == nullptr) return -9;
        return cache->AutoCreateSurfaces();
    case 10:
        if (cache == nullptr) return -9;
        return cache->Init(a[0]);
    case 11:
        if (cache == nullptr) return -9;
        return cache->Close();
    case 12:   // the fields the game or the detection would have set
        if (cache == nullptr) return -9;
        cache->mF1908 = a[0];
        cache->mFlags = static_cast<uint32_t>(a[1]);
        for (int32_t i = 0; i < 6; ++i) cache->mWanted[i] = a[2 + i];
        cache->mSizeMask = a[8];
        return 0;
    case 13:
        gei_TextureSettings.mFlags = static_cast<uint32_t>(a[0]);
        gei_TextureSettings.mFormat = a[1];
        gei_TextureSettings.mTextureMemoryLimit = a[2];
        gei_System.mRenderer = a[3];
        gei_Display.mFlags = static_cast<uint32_t>(a[4]);
        gDirectDraw.mDeviceInfo.vendorID = static_cast<uint32_t>(a[5]);
        gDirectDraw.mDeviceInfo.deviceID = static_cast<uint32_t>(a[6]);
        gDirectDraw.mHardwareCaps.dwVidMemTotal = static_cast<uint32_t>(a[7]);
        gOpenGLTextureData = a[8] ? &gUploadBuffer : nullptr;
        return 0;
    case 14: InitFrequencyTable(); return 0;
    case 16: {   // Remove: entry a[0] (0: none), after giving it the flags a[1] and the cache a[2]
        if (cache == nullptr) return -9;
        CTextureEntry* entry = nullptr;
        if (a[0] > 0) {
            entry = gEntries[a[0] - 1];
            if (entry == nullptr) return -8;
            if ((a[1] & CTextureEntry::kOnPage) != 0 && !gPageAlive[a[0] - 1]) return -8;
            entry->mFlags = static_cast<uint32_t>(a[1]);
        }
        cache->mFlags = static_cast<uint32_t>(a[2]);
        return cache->Remove(entry);
    }
    case 15: {   // as if page a[1] of size a[0] had been used: its texture's flags
        if (cache == nullptr) return -9;
        if (a[1] >= cache->mPageCount[a[0]]) return -8;
        cache->mPages[a[0]][a[1]]->mTexture->mFlags = static_cast<uint32_t>(a[2]);
        return 0;
    }
    // ---- texture pages of the test's own
    case 20:
        if (gTextures[a[0]] != nullptr) return -9;
        gTextures[a[0]] = static_cast<CTextureBase*>(gei_OpenGL->COpenGL::CreateTextureBase());
        return gTextures[a[0]] != nullptr;
    case 50: return gei_OpenGL->COpenGL::FlipScreen();
    case 51: return gei_OpenGL->COpenGL::GetVRAMSize();
    case 52: return gei_OpenGL->COpenGL::GetTextureMemory();
    }
    if (op >= 21 && op <= 34) {
        CTextureBase* base = gTextures[a[0]];
        if (base == nullptr) return -9;
        auto* texture = dynamic_cast<COpenGLTexture*>(base);
        switch (op) {
        case 21: return base->Create(a[1], a[2], a[3], a[4]);
        case 22: return base->Destroy();
        case 23: texture->Select(a[1]); return 0;
        case 24: texture->SetState(a[1]); return 0;
        case 25: return base->LockSurface();
        case 26: return base->UnlockSurface();
        case 27: {
            const double speed = base->TestUploadSpeed();
            int64_t raw;
            std::memcpy(&raw, &speed, 8);
            return raw;
        }
        case 28: return base->MipmapsUsed();
        case 29: return base->GetPixelCount();
        case 30: base->GetTexture(); return 0;
        case 31:
            // The draw primitive may still name the page; its memory can be handed out
            // again, so the test forgets it there first.
            if (gDraw != nullptr && gDraw->mTexture == texture) gDraw->mTexture = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
            delete base;
            gTextures[a[0]] = nullptr;
            return 0;
        case 32: base->SetFlags(a[1]); return 0;
        case 33:
            switch (a[1]) {
            case 0: return base->GetFlags();
            case 1: return base->IsPositionLocked();
            case 2: return base->IsWrapping();
            case 3: return base->IsUsingMaxCompression();
            case 4: base->PositionLock(); return 0;
            default: base->PositionUnlock(); return 0;
            }
        case 34:   // as if the page had been used: flags and state the test chooses
            base->mFlags = static_cast<uint32_t>(a[1]);
            texture->mState = a[2];
            texture->mMinFilter = a[3];
            texture->mMipmaps = a[4];
            return 0;
        }
    }
    if (op == 40) {   // the draw primitive: the base's real constructor under a blank derived one
        if (gDraw != nullptr) return -9;
        std::memset(gDrawStore, 0xa5, sizeof gDrawStore);
        gDraw = new (gDrawStore) COpenGLDrawPrimitive;
        gDraw->mTexture = reinterpret_cast<void*>(static_cast<intptr_t>(-1));
        gDraw->mPasses = 0;
        gDraw->mF10 = 0;
        return 0;
    }
    if (op >= 41 && op <= 49) {
        if (gDraw == nullptr) return -9;
        switch (op) {
        case 41: return gDraw->Init();
        case 42: return gDraw->SetFogParameters(from_bits(a[0]), from_bits(a[1]), static_cast<uint32_t>(a[2]));
        case 43: return gDraw->SetBlendingMode(a[0]);
        case 44: {
            void* texture = nullptr;
            if (a[1] >= 0) {
                if (gTextures[a[1]] == nullptr) return -9;
                texture = dynamic_cast<COpenGLTexture*>(gTextures[a[1]]);
            }
            return gDraw->SetTexture(a[0], texture);
        }
        case 45: return gDraw->ExecuteShadow(nullptr);
        case 46: return gDraw->BeginDraw();
        case 47: return gDraw->EndDraw();
        case 48: return gDraw->SupportsOnePassMultiTexture();
        case 49: gDraw->mFields[4] = static_cast<uint32_t>(a[0]); return 0;
        }
    }
    return -99;
}

int32_t tpw_tp_state(int64_t* out, int32_t capacity) {
    Quiet quiet;
    gOut = out;
    gCount = 0;
    gCapacity = capacity;
    CacheState();
    for (CTextureBase* texture : gTextures) {
        put(texture != nullptr);
        if (texture != nullptr) TextureState(texture);
    }
    put(gDraw != nullptr);
    if (gDraw != nullptr) {
        for (uint32_t field : gDraw->mFields) put(field);
        int64_t current = -2;
        if (gDraw->mTexture == nullptr) current = 0;
        else if (gDraw->mTexture == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) current = -1;
        else {
            for (int32_t i = 0; i < kTextures; ++i) {
                if (gTextures[i] != nullptr && dynamic_cast<COpenGLTexture*>(gTextures[i]) == gDraw->mTexture) current = 10 + i;
            }
        }
        put(current);
        put(gDraw->mPasses);
        put(gDraw->mF10);
    }
    put(gDrawPrimitiveNvidia);
    for (int32_t i = 0; i < kEntries; ++i) {
        const CTextureEntry* entry = gEntries[i];
        put(entry != nullptr);
        if (entry == nullptr) continue;
        put(entry->mFlags);
        put(entry->mCacheWidth);
        put(entry->mCacheHeight);
        put(entry->mSortKey);
        put(EntryIndex(entry->mNext));
        for (int32_t w = 0; w < 6; ++w) {
            uint32_t word;
            std::memcpy(&word, reinterpret_cast<const uint8_t*>(gPlaces[i]) + 4 * w, 4);
            put(word);
        }
    }
    put(gei_TextureSettings.mFlags);
    put(gei_TextureSettings.mFormat);
    put(gei_TextureSettings.mTextureMemory);
    put(gei_Display.mFlags);
    for (int32_t value : gFrequencyTable) put(value);
    return gCount;
}

}  // extern "C"
