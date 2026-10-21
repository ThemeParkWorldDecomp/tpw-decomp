// Test library for the CMesh ride base members (unit v5752; libtpw_unit_CMesh_GenerateRideBase.dylib, tests/test_CMesh_GenerateRideBase.py), engine:
//   src/engine/Mesh.cpp   CMesh::CenterMatrix, CMesh::GenerateRideBase (two overloads)
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. Entries are
// identified by their flag word (the test gives each a different marker), arrays of CsTexture by the index of the element.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_mesh.h"
#include "tb_proctexture.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"
#include "tb_texturecache.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CHField* gei_HField;
CTextureCache* gei_TCache;
CErrorHandler gei_ErrorHandler;
CTextureSettings gei_TextureSettings;
CTextureSettings::CTextureSettings() {}

namespace {

CMesh* gMesh = nullptr;
int gNothrowCalls = 0;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t Marker(const CTextureEntry* entry) { return entry != nullptr ? static_cast<int64_t>(entry->mFlags) : 0xFFFFFFFFll; }

int64_t Index(const CsTexture* texture) {
    const intptr_t base = reinterpret_cast<intptr_t>(gMesh->mRideBaseTextures);
    return (reinterpret_cast<intptr_t>(texture) - base) / static_cast<intptr_t>(sizeof(CsTexture));
}

}  // namespace

// ---- the fakes
// The assertion: recorded with the line and a hash (FNV-1a, 32 bits) of the condition and of the file name.
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    auto fnv = [](const char* s) {
        uint32_t h = 2166136261u;
        for (; *s != 0; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
        return static_cast<int64_t>(h);
    };
    ev("assert", {line, fnv(condition), fnv(file)});
}

void CErrorHandler::Push(int32_t level, int32_t code) { ev("Push", {lab(this), level, code}); }

int32_t CProcTextureHandler::CreateProcTexture(CTextureEntry* entry, int32_t x, int32_t y, sTileOverlay* overlay) {
    ev("CreateProcTexture", {lab(this), Marker(entry), x, y, static_cast<int64_t>(reinterpret_cast<uintptr_t>(overlay->mData) & 0xFFFFFFFFu),
                             overlay->mWidth, overlay->mHeight, overlay->mFormat, static_cast<int64_t>(overlay->mFlags)});
    return static_cast<int32_t>(ret("CreateProcTexture"));
}

void* CTextureEntry::GetCorrectedData() {
    ev("GetCorrectedData", {Marker(this)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetCorrectedData"))));
}

CTextureEntry::CTextureEntry() { std::memset(static_cast<void*>(this), 0, sizeof(CTextureEntry)); }
CTextureEntry::~CTextureEntry() {}

int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("Remove", {lab(this), Marker(entry)});
    return 0;
}

void CsTexture::Reset() { std::memset(static_cast<void*>(this), 0, sizeof(CsTexture)); }

int32_t CsTexture::Load(CTextureEntry* entry) {
    ev("Load", {Index(this), Marker(entry)});
    return 1;
}

int32_t CsTexture::Close() {
    ev("Close", {Index(this)});
    return 1;
}

void* CsTexture::operator new[](size_t size) noexcept {
    bool ok = ret("alloc", 1) != 0;
    ev("new[]", {2, static_cast<int64_t>((size - 8) / sizeof(CsTexture)), ok});
    return ok ? tpw::ArenaAllocZeroed(size + 8) : nullptr;
}

void CsTexture::operator delete[](void* block) {
    if (block != nullptr) ev("delete[]", {});
}

// The old-texture array and the entry array of the original come from the C++ allocator: the global nothrow array new.
// The Mac asks for the bytes of the elements plus an 8 byte count in front, always; the host's old array has no count.
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    int64_t kind = 1;
    int64_t count = static_cast<int64_t>(size / sizeof(SOldCsTexture));
    if (gNothrowCalls++ > 0) {      // the first is the old-texture array, every other one an entry
        kind = 3;
        count = 1;
    }
    bool ok = ret("alloc", 1) != 0;
    ev("new[]", {kind, count, ok});
    return ok ? std::calloc(size + 8, 1) : nullptr;     // the test shim's delete[] frees it
}


int32_t CMesh::DeleteRideBase() {
    ev("DeleteRideBase", {0});
    return 1;
}

void CMesh::SetLocalMatrix(int32_t flags, CMatrix& matrix) {
    const float* f = &matrix.m[0][0];
    ev("SetLocalMatrix", {flags, CanonBits(f[0]), CanonBits(f[1]), CanonBits(f[2]), CanonBits(f[3]), CanonBits(f[4]), CanonBits(f[5]),
                          CanonBits(f[6]), CanonBits(f[7])});
    ev("SetLocalMatrix2", {CanonBits(f[8]), CanonBits(f[9]), CanonBits(f[10]), CanonBits(f[11]), CanonBits(f[12]), CanonBits(f[13]),
                           CanonBits(f[14]), CanonBits(f[15])});
}

extern "C" {

uint64_t tpw_gr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"hfield", &gei_HField},
        {"tcache", &gei_TCache},
        {"settings", &gei_TextureSettings},
        {"errh", &gei_ErrorHandler},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena base: a Ptr32 field holds an address minus this.
uint64_t tpw_gr_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The host's offsets and sizes, in the order of L_NAMES of the test module.
int32_t tpw_gr_layout(int64_t* out) {
    alignas(16) static char mesh[0x400];
    alignas(16) static char hfield[0x400];
    CMesh* m = reinterpret_cast<CMesh*>(mesh);
    CHField* h = reinterpret_cast<CHField*>(hfield);
    auto at = [](const void* base, const void* member) {
        return static_cast<int64_t>(reinterpret_cast<const char*>(member) - reinterpret_cast<const char*>(base));
    };
    CTextureEntry* e = reinterpret_cast<CTextureEntry*>(mesh);
    CsTexture* c = reinterpret_cast<CsTexture*>(mesh);
    SOldCsTexture* o = reinterpret_cast<SOldCsTexture*>(mesh);
    int32_t n = 0;
    out[n++] = at(m, &m->mFlags);
    out[n++] = at(m, &m->mHeader);
    out[n++] = at(m, &m->mRideBaseOld);
    out[n++] = at(m, &m->mRideBaseTextures);
    out[n++] = at(m, &m->mRideBaseCount);
    out[n++] = static_cast<int64_t>(sizeof(CsTexture));
    out[n++] = at(c, &c->mEntries);
    out[n++] = at(h, &h->mF00);
    out[n++] = at(h, &h->mProcTextures);
    out[n++] = at(e, &e->mFlags);
    out[n++] = at(e, &e->mWidth);
    out[n++] = at(e, &e->mHeight);
    out[n++] = static_cast<int64_t>(sizeof(SOldCsTexture));
    out[n++] = at(o, &o->mOld);
    out[n++] = at(o, &o->mWhere);
    return n;
}

// 0x1001e644
int32_t tpw_gr_a(uint64_t mesh, uint64_t target, uint64_t source, int32_t x, int32_t y, int32_t flags) {
    gMesh = reinterpret_cast<CMesh*>(mesh);
    return gMesh->GenerateRideBase(reinterpret_cast<CTextureEntry*>(target), reinterpret_cast<CTextureEntry*>(source), x, y, flags);
}

// 0x1001e72c
int32_t tpw_gr_b(uint64_t mesh, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    gMesh = reinterpret_cast<CMesh*>(mesh);
    gNothrowCalls = 0;
    return gMesh->GenerateRideBase(x0, y0, x1, y1);
}

// 0x1001d97c
void tpw_gr_c(uint64_t mesh, int32_t flags, float scale) {
    gMesh = reinterpret_cast<CMesh*>(mesh);
    gMesh->CenterMatrix(flags, scale);
}

}  // extern "C"
