// Test library for NewPylon and UpdatePylonStackMeshes (unit v5091; libtpw_unit_NewPylon.dylib, tests/test_NewPylon.py), main app:
//   src/game/rides/CoasterPylon.cpp   NewPylon, UpdatePylonStackMeshes and the global gPylonStackSkip
//
// The callees outside the unit are recording fakes (MReleaseAllocF, GetRideRequiredYOffset, FloatToCell, FlagCollisionUpdateTrackAroundBrace,
// AnimateRideSet, DoMeshSkirtDeform); the globals of other files are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sCoasterInstance* gCoasterEditInstance;   // 0x102b68b0
int32_t gHeightfieldHeight;                   // 0x102c6c64
int32_t gHeightfieldWidth;                    // 0x102c6c68
tag_sM3D2Header* gHeightfieldHeader;          // 0x102c6c7c
tag_sRideAnimSet* gRideAnimSets[16];          // 0x102c79a4

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    // the host record is bigger than the Mac's 0x48 bytes: the log shows the Mac's size
    ev("MReleaseAllocF", {static_cast<int64_t>(size == sizeof(sPylonRecord) ? 0x48 : size), static_cast<int64_t>(flags)});
    return tpw::ArenaAllocZeroed(size);
}

float GetRideRequiredYOffset(int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags, int32_t rotation) {
    ev("GetRideRequiredYOffset", {x, z, lab(a), w, h, static_cast<int64_t>(flags), rotation});
    uint32_t bits = static_cast<uint32_t>(ret("Y"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

int32_t FloatToCell(double value) {
    ev("FloatToCell", {CanonBits(static_cast<float>(value))});
    return static_cast<int32_t>(ret("Cell"));
}

void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) { ev("FlagCollision", {lab(brace)}); }

void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags) {
    ev("AnimateRideSet", {lab(set), a, static_cast<int64_t>(flags)});
}

void DoMeshSkirtDeform(tag_sMesh* mesh, tag_sM3D2Header* header, float y, uint32_t a) {
    ev("DoMeshSkirtDeform", {lab(mesh), lab(header), CanonBits(y), static_cast<int64_t>(a)});
}

extern "C" {

uint64_t tpw_np_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"inst", &gCoasterEditInstance}, {"hh", &gHeightfieldHeight}, {"hw", &gHeightfieldWidth},
        {"hdr", &gHeightfieldHeader},    {"sets", gRideAnimSets},     {"skip", &gPylonStackSkip},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_np_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 word holds for an address.
uint32_t tpw_np_off(uint64_t address) { return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

#define SAME_AS_LINK(m, l) static_assert(offsetof(sPylonRecord, m) == offsetof(sLinkPylon, l), #m)

int32_t tpw_np_layout(int64_t* out) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
    SAME_AS_LINK(mControl, mControl);
    SAME_AS_LINK(mY, mY);
    SAME_AS_LINK(mCount, mCount);
    SAME_AS_LINK(mBraces, mBraces);
    int32_t n = 0;
    out[n++] = offsetof(tag_sRideAnimSet, mHeads);
    out[n++] = offsetof(tag_sMesh, mHeader);
    out[n++] = offsetof(tag_sMesh, mF28);
    out[n++] = offsetof(tag_sMesh, mRootNode);
    out[n++] = offsetof(sPylonHeadTable, mEntries);
    out[n++] = sizeof(sPylonHeadEntry);
    out[n++] = offsetof(sPylonHeadEntry, mRec);
    out[n++] = offsetof(sPylonHeadRec, mY);
    out[n++] = offsetof(tag_sTrackBrace, mFlags);
    out[n++] = offsetof(tag_sTrackBrace, mF0C) + (0x50 - 0x0c);
    out[n++] = offsetof(tag_sTrackBrace, mWord70);
    out[n++] = offsetof(tag_sTrackBrace, mData);
    out[n++] = offsetof(sTrackBraceData, mF1C) + 4;
    out[n++] = offsetof(sLinkPylon, mControl);
    out[n++] = offsetof(sLinkPylon, mY);
    out[n++] = offsetof(sLinkPylon, mCount);
    out[n++] = offsetof(sLinkPylon, mBraces);
    out[n++] = offsetof(sPylonControl, mFC4);
    out[n++] = offsetof(sPylonControl, mF18);
    out[n++] = offsetof(sPylonControl, mF1C);
    out[n++] = offsetof(sPylonControl, mF20);
    out[n++] = offsetof(sPylonControl, mF24);
    out[n++] = offsetof(sPylonControl, mF28);
    out[n++] = offsetof(tag_sM3D2Header, mNodeCount);
    out[n++] = offsetof(tag_sM3D2Header, mF46);
    out[n++] = offsetof(tag_sM3D2Header, mHField);
    out[n++] = offsetof(tag_sM3D2Header, mNodes);
    out[n++] = offsetof(tag_sM3D2Header, mMiniNodes);
    out[n++] = offsetof(tag_sM3D2Header, mF78);
    out[n++] = offsetof(sHField, mF18);
    out[n++] = offsetof(tag_sCoasterInstance, mF40);
    out[n++] = offsetof(tag_sCoasterInstance, mPylons);
    out[n++] = offsetof(tag_sCoasterInstance, mPylonTail);
    out[n++] = offsetof(tag_sCoasterInstance, mPreviewPylon);
    out[n++] = offsetof(sPylonRecord, mNumber);
    out[n++] = offsetof(sPylonRecord, mFlags);
    out[n++] = offsetof(sPylonRecord, mIndex);
    out[n++] = offsetof(sPylonRecord, mCellX);
    out[n++] = offsetof(sPylonRecord, mCellZ);
    out[n++] = offsetof(sPylonRecord, mInstance);
    out[n++] = offsetof(sPylonRecord, mControl);
    out[n++] = offsetof(sPylonRecord, mX);
    out[n++] = offsetof(sPylonRecord, mY);
    out[n++] = offsetof(sPylonRecord, mZ);
    out[n++] = offsetof(sPylonRecord, mPrev);
    out[n++] = offsetof(sPylonRecord, mNext);
    out[n++] = sizeof(sPylonRecord);
#pragma clang diagnostic pop
    return n;
}

uint64_t tpw_np_new(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, uint64_t control, uint64_t point) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        NewPylon(x, z, flags, unused, reinterpret_cast<sPylonControl*>(static_cast<uintptr_t>(control)),
                 reinterpret_cast<s3DPoint*>(static_cast<uintptr_t>(point)))));
}

void tpw_np_upd(uint64_t control, uint64_t pylon, int32_t a, int32_t b) {
    UpdatePylonStackMeshes(reinterpret_cast<sPylonControl*>(static_cast<uintptr_t>(control)),
                           reinterpret_cast<tag_sPylon*>(static_cast<uintptr_t>(pylon)), a, b);
}

}  // extern "C"
