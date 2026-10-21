// Test library for the coaster trains (unit w192; libtpw_unit_CoasterTrains.dylib, tests/test_CoasterTrains.py), main app:
//   src/game/rides/BumperRide.cpp, src/game/rides/TPCoaster.cpp   InstanceCoasterTrains, DestroyCoasterTrains, GetCoaster1stPersonCar, SaveTrainsAndCars, LoadTrainsAndCars and the
//                              four swap instances (the plain library does not swap, so that its files compare byte for byte with the original's
//                              little-endian ones; tests/unit_CoasterTrainsSwap.cpp is this file built with TPW_SWAP_FILE_WORDS)
//
// Every callee outside the unit is a fake that records its arguments (objects as labels) and answers from the test's queues. The ones that run
// inside a loop of the unit can change the world (a type's counts, a train's type, a car's ride, the ride set array) to show which words the
// original reads again. LbFile_Read and LbFile_Write serve and collect the case's stream and answer the transferred counts and TbErrors from
// the queues "xfer" and "err". The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "LbFile.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

#if defined(TPW_SWAP_FILE_WORDS)
#define UNIT_SWAPS 1
#endif

// ---- globals of other files, defined here (the program's definitions are in files this library does not compile)
tag_sCoasterInstance* gCoasterInstances;
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances + 1];

namespace {
std::vector<uint8_t> gIn;     // the stream LbFile_Read serves
size_t gInPos = 0;
std::vector<uint8_t> gOut;    // what LbFile_Write got, in order
int32_t gAllocCalls = 0;      // MReleaseAllocF calls since tpw_ctr_begin (the Mac's sizes are told by the call's place)
uint32_t gLastRide = 0;       // the ride the last InstanceRide or RestoreRideInstance answered
tag_sCoasterType* gTypeForFakes = nullptr;
sCarInstance* gCarsBase = nullptr;
int32_t gCarsCount = 0;
tag_sCoasterType* gTypes[2];
}  // namespace

namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) {
    ev("USwapBlock32", {count});
#ifdef UNIT_SWAPS
    for (int32_t i = 0; i < count; ++i) words[i] = __builtin_bswap32(words[i]);
#else
    (void)words;
#endif
}
uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    uint32_t* copy = static_cast<uint32_t*>(std::malloc(4 * (count > 0 ? count : 1)));
    for (int32_t i = 0; i < count; ++i) {
#ifdef UNIT_SWAPS
        copy[i] = __builtin_bswap32(words[i]);
#else
        copy[i] = words[i];
#endif
    }
    ev("USwapCopy32", {count, count > 0 ? static_cast<int64_t>(words[0]) : 0, newlab(copy)});
    return copy;
}
void _USwapDelete(void* copy) {
    ev("USwapDelete", {lab(copy)});
    std::free(copy);
}
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

// ---- the fakes
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    // The Mac's sizes: the trains 0x80 each, the cars 0x60 each (the host structs are bigger); the words and the heads are the same.
    int64_t told = size;
    ++gAllocCalls;
    if (gAllocCalls == 1) told = static_cast<int64_t>(size / sizeof(sTrainInstance)) * 0x80;
    if (gAllocCalls == 3) told = static_cast<int64_t>(size / sizeof(sCarInstance)) * 0x60;
    void* block = tpw::ArenaAllocZeroed(size);
    ev("MReleaseAllocF", {told, flags, newlab(block)});
    return block;
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e, uint32_t f,
                      int32_t g) {
    ev("InstanceRide", {type, flags, static_cast<int64_t>(reinterpret_cast<uintptr_t>(params)), a, static_cast<uint32_t>(b),
                        static_cast<int64_t>(reinterpret_cast<uintptr_t>(c)), static_cast<uint32_t>(d), static_cast<uint32_t>(e), f,
                        static_cast<uint32_t>(g)});
    gLastRide = static_cast<uint32_t>(ret("ride", 0));
    const int64_t n = ret("n", -1);
    if (n >= 0 && gTypeForFakes != nullptr) gTypeForFakes->mF2E8 = static_cast<int32_t>(n);
    const int64_t c2 = ret("c", -1);
    if (c2 >= 0 && gTypeForFakes != nullptr) gTypeForFakes->mF2F4 = static_cast<int32_t>(c2);
    return gLastRide;
}
uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d,
                             int32_t e, int32_t f) {
    ev("RestoreRideInstance", {instance, type, flags, static_cast<int64_t>(reinterpret_cast<uintptr_t>(params)), a,
                               static_cast<int64_t>(reinterpret_cast<uintptr_t>(c)), static_cast<uint32_t>(d), static_cast<uint32_t>(e),
                               static_cast<uint32_t>(f)});
    gLastRide = instance;
    return static_cast<uint32_t>(ret("restore", 0));
}
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {instance}); }
void DisableMeshDraw(tag_sMesh* mesh) {
    ev("DisableMeshDraw1", {lab(mesh)});
    if (ret("clear", 0) != 0) gRideAnimSets[gLastRide] = nullptr;
}
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to) {
    ev("DisableMeshDraw3", {lab(mesh), (static_cast<int64_t>(from.mF00) << 16) | from.mF02, (static_cast<int64_t>(to.mF00) << 16) | to.mF02});
    if (ret("clear", 0) != 0) gRideAnimSets[gLastRide] = nullptr;
}
void OverrideActiveChannels(tag_sRideAnimSet* set) { ev("OverrideActiveChannels", {lab(set)}); }
void DeactivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    ev("DeactivateCoasterTrain", {lab(instance), lab(train)});
    const int64_t retype = ret("retype", -1);
    if (retype >= 0) train->mType = gTypes[retype];
    const int64_t count = ret("ncount", -1);
    if (count >= 0) gTypes[0]->mF2E8 = static_cast<int32_t>(count);
    const int64_t itype = ret("itype", -1);
    if (itype >= 0) instance->mType = reinterpret_cast<const uint32_t*>(gTypes[itype]);
}
void DisableCoasterCarDrawSafe(uint32_t ride) {
    ev("DisableCoasterCarDrawSafe", {ride});
    const int64_t mod = ret("rmod", -1);
    if (mod >= 0)
        for (int32_t i = 0; i < gCarsCount; ++i)
            if (gCarsBase[i].mRide == ride) {
                gCarsBase[i].mRide = static_cast<uint32_t>(mod);
                break;
            }
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    std::memset(buffer, 0, length);
    const size_t left = gInPos < gIn.size() ? gIn.size() - gInPos : 0;
    const size_t n = length < left ? length : left;
    gInPos += length;
    if (n) std::memcpy(buffer, gIn.data() + (gInPos - length), n);
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(ret("xfer", length));
    return TbError(static_cast<int32_t>(ret("err", 0)));
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    gOut.insert(gOut.end(), bytes, bytes + length);
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(ret("xfer", length));
    return TbError(static_cast<int32_t>(ret("err", 0)));
}

extern "C" {

uint64_t tpw_ctr_addr(const char* name) {
    if (std::strcmp(name, "instances") == 0) return reinterpret_cast<uintptr_t>(&gCoasterInstances);
    if (std::strcmp(name, "rideanim") == 0) return reinterpret_cast<uintptr_t>(gRideAnimSets);
    return 0;
}
uint64_t tpw_ctr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

#define OFF(T, m) static_cast<int64_t>(reinterpret_cast<uintptr_t>(&reinterpret_cast<T*>(0x1000)->m) - 0x1000)

// The host's offsets of the fields the module sets and reads, struct by struct (tests/test_CoasterTrains.py FIELDS), each struct's size last.
int32_t tpw_ctr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFF(sCarInstance, mFlags);
    out[n++] = OFF(sCarInstance, mType);
    out[n++] = OFF(sCarInstance, mIndex);
    out[n++] = OFF(sCarInstance, mRide);
    out[n++] = OFF(sCarInstance, mF10[0]);
    out[n++] = OFF(sCarInstance, mF10[1]);
    out[n++] = OFF(sCarInstance, mF10[2]);
    out[n++] = OFF(sCarInstance, mF1C);
    out[n++] = OFF(sCarInstance, mHeadsA);
    out[n++] = OFF(sCarInstance, mHeadsB);
    out[n++] = OFF(sCarInstance, mCurrentA);
    out[n++] = OFF(sCarInstance, mCurrentB);
    out[n++] = OFF(sCarInstance, mCountA);
    out[n++] = OFF(sCarInstance, mCountB);
    out[n++] = OFF(sCarInstance, mF38);
    out[n++] = OFF(sCarInstance, mF3C);
    out[n++] = OFF(sCarInstance, mF40);
    out[n++] = OFF(sCarInstance, mF44);
    out[n++] = OFF(sCarInstance, mF48[0]);
    out[n++] = OFF(sCarInstance, mF48[1]);
    out[n++] = OFF(sCarInstance, mF48[2]);
    out[n++] = OFF(sCarInstance, mF54[0]);
    out[n++] = OFF(sCarInstance, mF54[1]);
    out[n++] = OFF(sCarInstance, mF54[2]);
    out[n++] = static_cast<int64_t>(sizeof(sCarInstance));
    out[n++] = OFF(sTrainInstance, mFlags);
    out[n++] = OFF(sTrainInstance, mType);
    out[n++] = OFF(sTrainInstance, mF08);
    out[n++] = OFF(sTrainInstance, mCarsInUse);
    out[n++] = OFF(sTrainInstance, mF10);
    out[n++] = OFF(sTrainInstance, mF14);
    out[n++] = OFF(sTrainInstance, mF18);
    out[n++] = OFF(sTrainInstance, mF1C);
    out[n++] = OFF(sTrainInstance, mFrontSample);
    out[n++] = OFF(sTrainInstance, mRearSample);
    out[n++] = OFF(sTrainInstance, mCars);
    out[n++] = OFF(sTrainInstance, mCentreCar);
    out[n++] = OFF(sTrainInstance, mF30);
    out[n++] = OFF(sTrainInstance, mFirstPersonCar);
    out[n++] = OFF(sTrainInstance, mF38);
    out[n++] = OFF(sTrainInstance, mF3C[0]);
    out[n++] = OFF(sTrainInstance, mF3C[1]);
    out[n++] = OFF(sTrainInstance, mF3C[2]);
    out[n++] = OFF(sTrainInstance, mF48[0]);
    out[n++] = OFF(sTrainInstance, mF48[1]);
    out[n++] = OFF(sTrainInstance, mF48[2]);
    out[n++] = OFF(sTrainInstance, mF54[0]);
    out[n++] = OFF(sTrainInstance, mF54[1]);
    out[n++] = OFF(sTrainInstance, mF54[2]);
    out[n++] = OFF(sTrainInstance, mF54[3]);
    out[n++] = OFF(sTrainInstance, mF64);
    out[n++] = OFF(sTrainInstance, mF68);
    out[n++] = OFF(sTrainInstance, mF6C);
    out[n++] = OFF(sTrainInstance, mF70);
    out[n++] = OFF(sTrainInstance, mPosition);
    out[n++] = OFF(sTrainInstance, mF78);
    out[n++] = OFF(sTrainInstance, mF7C);
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    out[n++] = OFF(tag_sCoasterInstance, mFlags);
    out[n++] = OFF(tag_sCoasterInstance, mType);
    out[n++] = OFF(tag_sCoasterInstance, mEntranceID);
    out[n++] = OFF(tag_sCoasterInstance, mNext);
    out[n++] = OFF(tag_sCoasterInstance, mLeadTrain);
    out[n++] = OFF(tag_sCoasterInstance, mTrains);
    out[n++] = OFF(tag_sCoasterInstance, mTrainWords);
    out[n++] = OFF(tag_sCoasterInstance, mCars);
    out[n++] = OFF(tag_sCoasterInstance, mHeads);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = OFF(tag_sCoasterType, mF2E8);
    out[n++] = OFF(tag_sCoasterType, mF2F4);
    out[n++] = OFF(tag_sCoasterType, mCarTypeA);
    out[n++] = OFF(tag_sCoasterType, mCarTypeB);
    out[n++] = OFF(tag_sCoasterType, mCarTypeC);
    out[n++] = OFF(tag_sCoasterType, mF318);
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterType));
    out[n++] = OFF(sCarType, mRideType);
    out[n++] = OFF(sCarType, mHeadCount);
    out[n++] = static_cast<int64_t>(sizeof(sCarType));
    out[n++] = OFF(tag_sRideAnimSet, mFlags);
    out[n++] = OFF(tag_sRideAnimSet, mHeads);
    out[n++] = OFF(tag_sRideAnimSet, mFC0);
    out[n++] = OFF(tag_sRideAnimSet, mFC4);
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    return n;
}

// A case starts here: the stream in, the written stream cleared, the counters reset.
void tpw_ctr_begin(const uint8_t* data, int32_t length) {
    gIn.assign(data, data + length);
    gInPos = 0;
    gOut.clear();
    gAllocCalls = 0;
    gLastRide = 0;
}
// The type the InstanceRide fake may change the counts of; the two types DeactivateCoasterTrain's fake may hand out; the cars DisableCoasterCarDrawSafe's
// fake may change.
void tpw_ctr_set_world(uint64_t type, uint64_t type0, uint64_t type1, uint64_t cars, int32_t count) {
    gTypeForFakes = reinterpret_cast<tag_sCoasterType*>(static_cast<uintptr_t>(type));
    gTypes[0] = reinterpret_cast<tag_sCoasterType*>(static_cast<uintptr_t>(type0));
    gTypes[1] = reinterpret_cast<tag_sCoasterType*>(static_cast<uintptr_t>(type1));
    gCarsBase = reinterpret_cast<sCarInstance*>(static_cast<uintptr_t>(cars));
    gCarsCount = count;
}
int32_t tpw_ctr_out_length() { return static_cast<int32_t>(gOut.size()); }
void tpw_ctr_out_data(uint8_t* out) {
    if (!gOut.empty()) std::memcpy(out, gOut.data(), gOut.size());
}

// ---- the functions, one wrapper each with C types only
void tpw_ctr_Instance(uint64_t type, uint64_t instance, int32_t flag) {
    InstanceCoasterTrains(reinterpret_cast<tag_sCoasterType*>(static_cast<uintptr_t>(type)),
                          reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)), flag);
}
void tpw_ctr_Destroy(uint64_t instance) { DestroyCoasterTrains(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance))); }
uint32_t tpw_ctr_First(uint32_t entrance) { return GetCoaster1stPersonCar(entrance); }
int32_t tpw_ctr_Save(uint64_t file, uint64_t instance) {
    return SaveTrainsAndCars(reinterpret_cast<void*>(static_cast<uintptr_t>(file)),
                             reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)));
}
int32_t tpw_ctr_Load(uint64_t file, uint64_t instance) {
    return LoadTrainsAndCars(reinterpret_cast<void*>(static_cast<uintptr_t>(file)),
                             reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)));
}
// The four swaps work on the value at the pointer.
void tpw_ctr_swap16(uint64_t p) { TPCoasterSwap::USwapInplace16UShort(*reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(p))); }
void tpw_ctr_swap32i(uint64_t p) { TPCoasterSwap::USwapInplace32Int(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(p))); }
void tpw_ctr_swap32u(uint64_t p) { TPCoasterSwap::USwapInplace32ULong(*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(p))); }
void tpw_ctr_swapf(uint64_t p) { TPCoasterSwap::USwapInplaceFloat(*reinterpret_cast<float*>(static_cast<uintptr_t>(p))); }

}  // extern "C"
