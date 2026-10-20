// Test library for the coaster car's bend to the track and a train's sounds and screams (unit w214; libtpw_unit_CarBend.dylib,
// tests/test_CarBend.py), main app:
//   src/TPConfig_bend.cpp   AccumulateBendAngles, RotateNodeVertexPosns, BendCarToTrack, HandleScreams, TickTrainSounds, InterpAngles and
//                           gBendAngleClamps
//
// Every callee outside the unit is a fake that records its arguments (floats as bits, the mesh as a label) and answers from the case's
// queues: GetFirstPersonRideID, GetVectorRotationAroundYAxisFromZ (the angle comes from the queue "Rot"), SOUND_SetParamValue,
// SOUND_MoveSoundEffect, SCREAM_Start, SCREAM_Single, SCREAM_Move, SCREAM_SetLevel, SCREAM_Stop and TPProcessEmbeddedData. The globals of
// other files the unit reads (the engine's sine table in gTPGlobals, gActionRecorder, gRideAnimSets) are defined here, zero at start like
// the Mac's bss. The module builds the world word by word at the Mac's offsets (every block from the arena, so the M3D2 pieces' Ptr32
// fields fit; tpw_cbd_base is the offset those fields count from) and reads it back the same way.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "Advisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- the strings of gTPGlobals are never read
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- globals of other files, defined here (the program's definitions are in files this library does not compile)
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CActionRecorder gActionRecorder;
tag_sRideAnimSet* gRideAnimSets[16];

namespace {

int64_t Bits(float value) { return fbits(value); }
float FromBits(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

template<typename T>
T* Ptr(uint64_t address) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(address));
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
uint32_t GetFirstPersonRideID() {
    ev("GetFirstPersonRideID");
    return static_cast<uint32_t>(ret("GetFirstPersonRideID", 0));
}

// The angle comes from the queue "Rot"; the point's three floats are logged (the original's callers pass a copy on their stack).
float GetVectorRotationAroundYAxisFromZ(s3DPoint& point) {
    ev("GetVectorRotationAroundYAxisFromZ", {CanonBits(point.x), CanonBits(point.y), CanonBits(point.z)});
    return FromBits(ret("Rot", 0));
}

int32_t SOUND_SetParamValue(int32_t handle, uint32_t param, uint32_t value) {
    ev("SOUND_SetParamValue", {handle, param, value});
    return static_cast<int32_t>(ret("SOUND_SetParamValue", 0));
}

int32_t SOUND_MoveSoundEffect(int32_t handle, int32_t x, int32_t y, int32_t z) {
    ev("SOUND_MoveSoundEffect", {handle, x, y, z});
    return static_cast<int32_t>(ret("SOUND_MoveSoundEffect", 0));
}

int32_t SCREAM_Start(int32_t handle, int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) {
    ev("SCREAM_Start", {handle, count, a, b, p, q, r});
    return static_cast<int32_t>(ret("SCREAM_Start", 0));
}

int32_t SCREAM_Single(int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) {
    ev("SCREAM_Single", {count, a, b, p, q, r});
    return static_cast<int32_t>(ret("SCREAM_Single", 0));
}

int32_t SCREAM_Move(int32_t handle, int32_t x, int32_t y, int32_t z) {
    ev("SCREAM_Move", {handle, x, y, z});
    return static_cast<int32_t>(ret("SCREAM_Move", 0));
}

void SCREAM_SetLevel(int32_t handle, int32_t a, int32_t b) { ev("SCREAM_SetLevel", {handle, a, b}); }

void SCREAM_Stop(int32_t handle) { ev("SCREAM_Stop", {handle}); }

void TPProcessEmbeddedData(tag_sMesh* mesh) { ev("TPProcessEmbeddedData", {lab(mesh)}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_cbd_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"table", gTPGlobals.mSinTable},
        {"mask", &gTPGlobals.mSinTableMask},
        {"scale", &gTPGlobals.mSinTableScale},
        {"recording", &gActionRecorder.mRecording},
        {"clamps", gBendAngleClamps},
        {"ridesets", gRideAnimSets},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold), and its release.
uint64_t tpw_cbd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
void tpw_cbd_free(uint64_t block) { tpw::ArenaFree(Ptr<void>(block)); }

// The arena's first byte: a Ptr32 field of the M3D2 pieces holds a block's address minus this.
uint64_t tpw_cbd_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

// The offsets and sizes the module relies on (HOST_LAYOUT): per struct the fields in the module's FIELDS order, then the size.
int32_t tpw_cbd_layout(int64_t* out) {
    int32_t n = 0;
    auto put = [&](size_t value) { out[n++] = static_cast<int64_t>(value); };
    put(offsetof(sCarInstance, mType));
    put(offsetof(sCarInstance, mRide));
    put(offsetof(sCarInstance, mF10) + 0);
    put(offsetof(sCarInstance, mF10) + 4);
    put(offsetof(sCarInstance, mF10) + 8);
    put(offsetof(sCarInstance, mF40));
    put(sizeof(sCarInstance));
    put(offsetof(sTrainInstance, mFlags));
    put(offsetof(sTrainInstance, mF10));
    put(offsetof(sTrainInstance, mFirstPersonCar));
    put(offsetof(sTrainInstance, mF3C) + 0);
    put(offsetof(sTrainInstance, mF3C) + 4);
    put(offsetof(sTrainInstance, mF3C) + 8);
    put(offsetof(sTrainInstance, mF48) + 0);
    put(offsetof(sTrainInstance, mF48) + 4);
    put(offsetof(sTrainInstance, mF48) + 8);
    put(offsetof(sTrainInstance, mF54) + 4);
    put(offsetof(sTrainInstance, mF64));
    put(offsetof(sTrainInstance, mF68));
    put(offsetof(sTrainInstance, mF6C));
    put(offsetof(sTrainInstance, mF70));
    put(offsetof(sTrainInstance, mPosition));
    put(offsetof(sTrainInstance, mF78));
    put(sizeof(sTrainInstance));
    put(offsetof(tag_sCoasterInstance, mType));
    put(offsetof(tag_sCoasterInstance, mState));
    put(offsetof(tag_sCoasterInstance, mSamples));
    put(offsetof(tag_sCoasterInstance, mSampleCount));
    put(offsetof(tag_sCoasterInstance, mFAC));
    put(sizeof(tag_sCoasterInstance));
    put(offsetof(tag_sCoasterType, mF50));
    put(offsetof(tag_sCoasterType, mF58));
    put(offsetof(tag_sCoasterType, mF5C));
    put(sizeof(tag_sCoasterType));
    put(offsetof(sCarType, mNodeIndex));
    put(offsetof(sCarType, mF24));
    put(offsetof(sCarType, mF28));
    put(sizeof(sCarType));
    put(offsetof(sTrackSample, mFlags));
    put(offsetof(sTrackSample, mPos) + offsetof(s3DPoint, x));
    put(offsetof(sTrackSample, mPos) + offsetof(s3DPoint, y));
    put(offsetof(sTrackSample, mPos) + offsetof(s3DPoint, z));
    put(offsetof(sTrackSample, mYaw));
    put(sizeof(sTrackSample));
    put(offsetof(tag_sRideAnimSet, mHeads));
    put(sizeof(tag_sRideAnimSet));
    put(offsetof(tag_sMesh, mHeader));
    put(sizeof(tag_sMesh));
    put(offsetof(tag_sM3D2Header, mNodeCount));
    put(offsetof(tag_sM3D2Header, mNodes));
    put(offsetof(tag_sM3D2Header, mMiniNodes));
    put(offsetof(tag_sM3D2Header, mSelf));
    put(sizeof(tag_sM3D2Header));
    put(offsetof(tag_sNode, mFlags));
    put(offsetof(tag_sNode, mVertexCount));
    put(offsetof(tag_sNode, mVertices));
    put(sizeof(tag_sNode));
    put(offsetof(tag_sMiniNode, mFlags));
    put(sizeof(tag_sMiniNode));
    put(sizeof(sVertexPosnSet));
    return n;
}

// ---- the functions, one wrapper each with C types only (floats as their bits, pointers as uint64_t)
uint32_t tpw_cbd_InterpAngles(uint32_t a, uint32_t b, uint32_t t) {
    return static_cast<uint32_t>(fbits(InterpAngles(FromBits(a), FromBits(b), FromBits(t))));
}

void tpw_cbd_AccumulateBendAngles(uint64_t instance, uint64_t car, uint64_t yaws, uint64_t pitches, uint64_t heights, uint32_t capacity,
                                  uint32_t position, uint64_t centre) {
    AccumulateBendAngles(Ptr<tag_sCoasterInstance>(instance), Ptr<sCarInstance>(car), Ptr<float>(yaws), Ptr<float>(pitches),
                         Ptr<float>(heights), capacity, FromBits(position), Ptr<uint32_t>(centre));
}

void tpw_cbd_RotateNodeVertexPosns(uint64_t node, uint64_t source, uint64_t dest, uint32_t count, uint64_t yaws, uint64_t pitches,
                                   uint64_t heights, uint32_t centre, uint32_t spacing) {
    RotateNodeVertexPosns(Ptr<tag_sNode>(node), Ptr<sVertexPosnSet>(source), Ptr<sVertexPosnSet>(dest), count, Ptr<float>(yaws),
                          Ptr<float>(pitches), Ptr<float>(heights), centre, FromBits(spacing));
}

void tpw_cbd_BendCarToTrack(uint64_t instance, uint64_t car) {
    BendCarToTrack(Ptr<tag_sCoasterInstance>(instance), Ptr<sCarInstance>(car));
}

void tpw_cbd_HandleScreams(uint64_t instance, uint64_t train, int32_t x, int32_t y, int32_t z) {
    HandleScreams(Ptr<tag_sCoasterInstance>(instance), Ptr<sTrainInstance>(train), x, y, z);
}

void tpw_cbd_TickTrainSounds(uint64_t instance, uint64_t train) {
    TickTrainSounds(Ptr<tag_sCoasterInstance>(instance), Ptr<sTrainInstance>(train));
}

}  // extern "C"
