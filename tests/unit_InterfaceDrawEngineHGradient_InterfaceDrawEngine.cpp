// Test library for the horizontal gradient's constructor (unit v5080; libtpw_unit_InterfaceDrawEngineHGradient_InterfaceDrawEngine.dylib,
// tests/test_InterfaceDrawEngineHGradient_InterfaceDrawEngine.py), main app:
//   src/game/ui/InterfaceDrawEngine.cpp   the constructor
//
// The callees outside the unit (the base constructor, DMLoadTextures, DMCreateMesh, DMFreeTextures, SetMatrixIdentity, CalcVerts, CalcUVs) are
// fakes that record what the real ones read. DMCreateMesh hands out blocks the test allocated (gBlocks), as the real one would make them.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"

using namespace calllog;

namespace {
uint64_t gBlocks[8];   // vertex sets, points, uv sets, faces, mesh, matrix, face sets, textures
}

// ---- the fakes
InterfaceDrawObject::InterfaceDrawObject(void* data) { ev("InterfaceDrawObject", {lab(this), lab(data)}); }

// Only the vtables need these; the constructor never calls them (the base's CoolRect members are built by the base constructor's fake).
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
void InterfaceDrawEngineHGradient::Change(uint32_t, DOChange*) {}
void InterfaceDrawEngineHGradient::Show(int16_t) {}  // translated by v51115 in another file; the vtable here needs it
InterfaceDrawEngineHGradient::~InterfaceDrawEngineHGradient() {}  // translated by v51129 in another file; the key function, it emits the vtable here

int32_t DMLoadTextures(sDTextureSet* set) {
    ev("DMLoadTextures", {lab(set)});
    return static_cast<int32_t>(ret("DMLoadTextures"));
}

int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags) {
    ev("DMCreateMesh", {lab(set), lab(textures), lab(name), static_cast<int64_t>(flags)});
    int32_t r = static_cast<int32_t>(ret("DMCreateMesh"));
    if (r != 0) {
        set->mVertexSets = reinterpret_cast<sVertexPosnSet*>(gBlocks[0]);
        set->mPoints = reinterpret_cast<s3DPoint*>(gBlocks[1]);
        set->mUVSets = reinterpret_cast<sUVSet*>(gBlocks[2]);
        set->mFaces = reinterpret_cast<sFace*>(gBlocks[3]);
        set->mMesh = reinterpret_cast<tag_sMesh*>(gBlocks[4]);
        set->mEntries[0].mMatrix = reinterpret_cast<sMatrix*>(gBlocks[5]);
        set->mEntries[0].mFaceSets = reinterpret_cast<sFaceSet*>(gBlocks[6]);
        textures->mTextures = reinterpret_cast<sTexture*>(gBlocks[7]);
    }
    return r;
}

void DMFreeTextures(sDTextureSet* set) { ev("DMFreeTextures", {lab(set)}); }

void SetMatrixIdentity(sMatrix* matrix) { ev("SetMatrixIdentity", {lab(matrix)}); }

void InterfaceDrawEngineHGradient::CalcVerts() { ev("CalcVerts", {lab(this), static_cast<int64_t>(fbits(mFraction))}); }

void InterfaceDrawEngineHGradient::CalcUVs() { ev("CalcUVs", {lab(this), static_cast<int64_t>(fbits(mFraction))}); }

extern "C" {

uint64_t tpw_hg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_hg_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

void tpw_hg_setblocks(const uint64_t* blocks) { std::memcpy(gBlocks, blocks, sizeof(gBlocks)); }

// The host addresses of the object, its texture set, its mesh set and its entry.
void tpw_hg_addrs(uint64_t obj, uint64_t* out) {
    auto* o = reinterpret_cast<InterfaceDrawEngineHGradient*>(obj);
    out[0] = obj;
    out[1] = reinterpret_cast<uintptr_t>(&o->mTextures);
    out[2] = reinterpret_cast<uintptr_t>(&o->mMeshSet);
    out[3] = reinterpret_cast<uintptr_t>(&o->mEntry);
}

uint64_t tpw_hg_ctor(uint64_t obj, uint64_t name) {
    // junk in the texture set, which the constructor must clear
    std::memset(reinterpret_cast<char*>(obj) + offsetof(InterfaceDrawEngineHGradient, mTextures), 0xA5, sizeof(sDTextureSet));
    auto* o = new (reinterpret_cast<void*>(obj)) InterfaceDrawEngineHGradient(reinterpret_cast<void*>(name));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(o));
}

// What the constructor writes, as 40 values (pointers raw; the module maps them); see the module's dump_ppc for the Mac's order.
int32_t tpw_hg_dump(uint64_t obj, int64_t* out) {
    auto* o = reinterpret_cast<InterfaceDrawEngineHGradient*>(obj);
    int n = 0;
    auto P = [&](const void* p) { out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); };
    P(o->mData);
    out[n++] = o->mEntry.mFlags;
    out[n++] = o->mEntry.mFaceCount;
    out[n++] = o->mEntry.mVertexCount;
    out[n++] = o->mEntry.mUVCount;
    out[n++] = o->mEntry.mFaceSetCount;
    out[n++] = o->mEntry.mPointCount;
    for (int i = 0; i < 6; ++i) out[n++] = fbits(o->mBox[i]);
    for (int i = 0; i < 6; ++i) out[n++] = fbits(o->mEntry.mBox[i]);
    out[n++] = o->mMeshSet.mF00;
    out[n++] = o->mMeshSet.mCount;
    out[n++] = o->mMeshSet.mEntries == &o->mEntry;
    out[n++] = o->mMeshSet.mMatrix == &o->mMatrix;
    out[n++] = o->mTextures.mFlags;
    out[n++] = o->mTextures.mCount;
    P(o->mTextures.mFolder);
    P(o->mTextures.mSharedFolder);
    out[n++] = o->mTextures.mEntries == &o->mTexEntry;
    P(o->mTextures.mInfos);
    P(o->mTextures.mTextures);
    out[n++] = o->mTexEntry.mFlags;
    P(o->mTexEntry.mName);
    out[n++] = fbits(o->mFraction);
    P(o->mMeshSet.mVertexSets);
    P(o->mMeshSet.mPoints);
    P(o->mMeshSet.mUVSets);
    P(o->mMeshSet.mFaces);
    P(o->mMeshSet.mMesh);
    P(o->mEntry.mMatrix);
    P(o->mEntry.mFaceSets);
    return n;
}

}  // extern "C"
