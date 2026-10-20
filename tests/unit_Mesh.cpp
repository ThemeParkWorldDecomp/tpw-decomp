// Test library for src/tb_mesh.cpp (libtpw_unit_Mesh.dylib; see tests/test_Mesh.py): one
// CMesh over a mesh image the test lays out in the arena, driven by scripts.
//
// Real: CMesh. Fakes that record: the texture object (construction, Load, which also
// gives the object a list of blank entries, Close, its allocation), the link base table,
// the height field, the delayed loads, the loading picture, the engine allocator, the
// small-textures callback, the error handler.
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "Wavelet.h"
#include "platform/arena.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_mesh.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {
// The arrays the test plants (ride base and ride sign lists) get a label for their
// allocation; the PowerPC side labels the same blocks in the same order.
void ObserveAllocation(std::size_t, bool array, void* block) {
    if (array) newlab(block);
}

alignas(16) unsigned char gMeshStore[sizeof(CMesh)];
CMesh* gMesh = reinterpret_cast<CMesh*>(gMeshStore);
tag_sM3D2Header* gHeader;
alignas(16) unsigned char gFieldStore[sizeof(CHField)];

void* FakeMalloc(uint32_t size) {
    ev("Malloc", {size});
    if (!ret("Malloc", 1)) return nullptr;
    void* block = tpw::ArenaAllocZeroed(size + 16);
    newlab(block);
    return block;
}
void FakeFree(void*& block) {
    ev("Free", {lab(block)});
    forget(block);
    tpw::ArenaFree(block);
    block = nullptr;
}
bool FakeSmallTextures() {
    ev("UseSmallTextures");
    return ret("Small", 0) != 0;
}
}  // namespace

TbMallocFn gMalloc = FakeMalloc;
TbFreeFn gFree = FakeFree;
TbUseSmallTexturesFn gUseSmallTextures = FakeSmallTextures;
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }
CSystem::CSystem() {}
CSystem gei_System;
CDisplay::CDisplay() {}
CDisplay gei_Display;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CSplash::~CSplash() {}
CSplash gei_Splash;
int32_t CSplash::InterpolateSplash() {
    ev("Splash");
    return 1;
}
CHField::CHField() {}
CHField::~CHField() {}
CHField* gei_HField;
void CHField::Init(tag_sM3D2Header* header) {
    ev("HField.Init", {header == gHeader});
}
int32_t CHField::Close() {
    ev("HField.Close");
    return 1;
}
void DelayedLoad_LoadTextures() { ev("DelayedLoad"); }

CMapLinkBase::~CMapLinkBase() {}
int32_t RegisterLinkBase(CMapLinkBase* link) {
    ev("RegisterLinkBase", {link == static_cast<CMapLinkBase*>(gMesh)});
    return 1;
}
int32_t FreeLinkBase(CMapLinkBase* link) {
    ev("FreeLinkBase", {link == static_cast<CMapLinkBase*>(gMesh)});
    return 1;
}
int32_t CMesh::Prepare() { return 0; }

// ---- the texture object
static void* gJustAllocated;
void* CsTexture::operator new(size_t size) noexcept {
    ev("new", {static_cast<int64_t>(0x10)});
    if (!ret("New", 1)) return nullptr;
    void* block = tpw::ArenaAlloc(size);
    std::memset(block, 0xa5, size);
    newlab(block);
    gJustAllocated = block;
    return block;
}
void CsTexture::operator delete(void* block) {
    ev("delete", {lab(block)});
    forget(block);
    tpw::ArenaFree(block);
}
// Ride-sign arrays must live in the arena because file records point at them.
#ifndef TPW_RIDESIGN_TEST
void* CsTexture::operator new[](size_t size) noexcept {
    void* block = tpw::ArenaAlloc(size);
    newlab(block);
    return block;
}
void CsTexture::operator delete[](void* block) {
    ev("delete", {lab(block)});
    forget(block);
    tpw::ArenaFree(block);
}
int32_t CsTexture::ReLoad(CsTexture*) { return 1; }
void* CTextureEntry::GetData() { return nullptr; }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("Assert", {line}, {condition, file});
}
#endif
void CsTexture::Reset() {
    mTexture = nullptr;
    mInfo = nullptr;
    mEntries = nullptr;
    mEntryCount = 0;
    mReferences = 0;
    // An object of an array gets its label here (a fresh one: the memory may have held an
    // earlier array's object); a single object has the one its allocation gave it.
    if (this != gJustAllocated) {
        forget(this);
        newlab(this);
    }
    gJustAllocated = nullptr;
    ev("Tex.ctor", {lab(this)});
}
// Records the call and gives the object one blank entry per frame of the record (none
// where the "Entry" queue says 0).
int32_t CsTexture::Load(sTexture* texture, sTextureInfo* info) {
    ev("Tex.Load", {lab(this), texture - gHeader->mTextures.get(), info - gHeader->mTextureInfos.get()});
    const int32_t frames = info->mFrames;
    mEntries = static_cast<CTextureEntry**>(std::calloc(static_cast<size_t>(frames) + 1, sizeof(CTextureEntry*)));
    for (int32_t i = 0; i < frames; ++i) {
        if (!ret("Entry", 1)) continue;
        mEntries[i] = static_cast<CTextureEntry*>(std::calloc(1, sizeof(CTextureEntry)));
        newlab(mEntries[i]);
    }
    mEntryCount = static_cast<int16_t>(frames);
    return static_cast<int32_t>(ret("Tex.Load", 1));
}
int32_t CsTexture::Close() {
    ev("Tex.Close", {lab(this)});
    return static_cast<int32_t>(ret("Tex.Close", 1));
}

extern "C" {
void* tpw_ms_arena_base() { return tpw::ArenaBase(); }
void* tpw_ms_alloc(uint32_t size) { return tpw::ArenaAllocZeroed(size); }
// A block as the engine allocator gives it, with a label (a node's block at 0x74).
void* tpw_ms_block(uint32_t size) {
    void* block = tpw::ArenaAllocZeroed(size + 16);
    newlab(block);
    return block;
}
void tpw_ms_reset(void* header) {
    gHeader = static_cast<tag_sM3D2Header*>(header);
    std::memset(gMeshStore, 0xa5, sizeof gMeshStore);
    gei_System.mFlags = 0;
    gei_Display.mFlags = 0;
    gei_Display.mWidth = 800;
    gei_TextureSettings.mFlags = 0;
    gei_HField = nullptr;
}

int64_t tpw_ms_op(int32_t op, const int32_t* a) {
    switch (op) {
    case 1: new (gMesh) CMesh; return 0;
    case 2: return gMesh->Init(a[0] ? gHeader : nullptr);
    case 3: return gMesh->Close();
    case 4: return gMesh->DestroyBumpmappingNormals();
    case 5: return gMesh->DeleteRideBase();
    case 6: return gMesh->DeleteRideSign();
    case 7: return gMesh->GetData() == nullptr ? 0 : gMesh->GetData() == gHeader ? 1 : -1;
    case 8: return static_cast<CMapLinkBase*>(gMesh)->GetLinkType();
    case 9: gMesh->~CMesh(); return 0;
    case 10:
    case 11: {   // a ride base (10) or sign (11): a[0] records, the flag when a[1], old textures per bit of a[2]
        const int32_t count = a[0];
        auto* old = new SOldCsTexture[static_cast<size_t>(count) + 1];
        auto* textures = new CsTexture[static_cast<size_t>(count) + 1];
        for (int32_t i = 0; i < count; ++i) {
            const int32_t record = i % gHeader->mTextureCount;
            old[i].mWhere = &gHeader->mTextures[record].mTexture;
            // A texture object as a value only: no constructor, no events.
            old[i].mOld = nullptr;
            if ((a[2] >> i & 1) != 0) {
                void* block = tpw::ArenaAllocZeroed(sizeof(CsTexture));
                newlab(block);
                old[i].mOld = static_cast<CsTexture*>(block);
            }
        }
        if (op == 10) {
            gMesh->mRideBaseOld = old;
            gMesh->mRideBaseTextures = textures;
            gMesh->mRideBaseCount = count;
            if (a[1]) gMesh->mFlags |= CMesh::kRideBase;
        } else {
            gMesh->mRideSignOld = old;
            gMesh->mRideSignTextures = textures;
            gMesh->mRideSignCount = count;
            if (a[1]) gMesh->mFlags |= CMesh::kRideSign;
        }
        return 0;
    }
    case 12:
        gei_TextureSettings.mFlags = static_cast<uint32_t>(a[0]);
        gei_Display.mFlags = static_cast<uint32_t>(a[1]);
        gei_Display.mWidth = a[2];
        gei_System.mFlags = static_cast<uint32_t>(a[3]);
        gei_HField = a[4] ? reinterpret_cast<CHField*>(gFieldStore) : nullptr;
        return 0;
    }
    return -99;
}

// The state in the order of PpcSide.state in the Python module. `built`: the object has
// been constructed (its pointers can be followed).
int32_t tpw_ms_state(int64_t* out, int32_t built) {
    int32_t n = 0;
    const CMesh* m = gMesh;
    out[n++] = m->mFlags;
    out[n++] = !built ? -7 : m->mHeader == nullptr ? 0 : m->mHeader == gHeader ? 1 : -1;
    out[n++] = m->mF90;
    out[n++] = m->mF94;
    out[n++] = m->mF98[0];
    out[n++] = m->mF98[32];
    out[n++] = m->mF11C;
    out[n++] = m->mF13C;
    out[n++] = m->mF140;
    out[n++] = built ? m->mRideBaseOld != nullptr : -7;
    out[n++] = built ? m->mRideBaseTextures != nullptr : -7;
    out[n++] = m->mRideBaseCount;
    out[n++] = built ? m->mRideSignOld != nullptr : -7;
    out[n++] = built ? m->mRideSignTextures != nullptr : -7;
    out[n++] = m->mRideSignCount;
    for (int32_t i = 0; i < 16; ++i) out[n++] = m->mMatrix.m[i / 4][i % 4] == (i % 5 == 0 ? 1.0f : 0.0f);
    for (int32_t i = 0; i < 16; ++i) out[n++] = m->mMatrix2.m[i / 4][i % 4] == (i % 5 == 0 ? 1.0f : 0.0f);
    const bool words = built && (m->mFlags & CMesh::kInitialised) != 0 && lab(m->mNodeWords) > 0;
    out[n++] = words ? lab(m->mNodeWords) : -7;
    out[n++] = words ? m->mNodeWordCursor - m->mNodeWords : -7;
    out[n++] = built ? m->mNodeWordCursor == nullptr : -7;
    const tag_sM3D2Header* h = gHeader;
    out[n++] = h->mFlags;
    for (int32_t i = 0; i < h->mTextureCount; ++i) {
        const CsTexture* texture = h->mTextures[i].mTexture;
        out[n++] = lab(texture);
        for (int32_t f = 0; f < 3; ++f) {
            const bool there = texture != nullptr && lab(texture) > 0 && texture->mEntries != nullptr && f < texture->mEntryCount &&
                               texture->mEntries[f] != nullptr;
            out[n++] = there ? static_cast<int64_t>(texture->mEntries[f]->mFlags) : -7;
        }
    }
    for (int32_t i = 0; i < h->mFaceSetCount; ++i) out[n++] = h->mFaceSets[i].mF0C[0];
    for (int32_t i = 0; i < h->mNodeCount; ++i) {
        const tag_sNode& node = h->mNodes[i];
        out[n++] = lab(node.mF74.get());
        const uint32_t* word = node.mMeshWords;
        out[n++] = word == nullptr ? 0 : words ? 1 + (word - m->mNodeWords) : -1;
        out[n++] = word != nullptr && words ? *word : 0;
    }
    out[n++] = gei_TextureSettings.mFlags;
    return n;
}
}
