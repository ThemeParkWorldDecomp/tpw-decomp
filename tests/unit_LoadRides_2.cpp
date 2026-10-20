// Test library for LoadRides (unit v5890; libtpw_unit_LoadRides_2.dylib, tests/test_LoadRides_2.py), main app:
//   src/RestoreAnimTime_LoadRides_2.cpp   LoadRides, gLoadRidesTbErrorZero, gLoadRidesShape
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: LbFile_Read
// (answers from a file image the test sets), MReleaseAllocF, MReleaseFreeF, TickRideAnims, GetRideInfo, InstanceRide (answers an instance
// number; the test has put the ride sets in gRideAnimSets), ERRORBox, CTRACE, UDebug, ReactivateHoardings, RestoreAnimTime,
// OutputAnimTimeStats, StaticifyCreateOnlyAnimNodes, OverrideActiveChannels, GetNumActiveChannels. The globals of other files the unit
// reads (the ride counts, gRideTypes, gRideAnimSets, the database object) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "LbFile.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPEngine.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
uint32_t gRideCountA;
uint32_t gRideCountB;
uint32_t gRideFirstFree;
uint32_t gRideTypeCount;
tag_sRideAnimSet* gRideTypes[kMaxRideTypes + 2];
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances + 2];
CRideDatabase gRideDatabase;

namespace {

constexpr int kInstances = 4;

uint8_t gFile[1 << 16];
uint32_t gFileLength = 0;
uint32_t gFilePos = 0;
uint8_t* gBlock = nullptr;
RideInfo* gInfos[2] = {nullptr, nullptr};

int64_t SlotOf(const tag_sRideAnimSet* set) {
    for (int i = 1; i <= kInstances; i++)
        if (gRideAnimSets[i] == set) return i;
    return 0;
}

}  // namespace

// ---- the fakes
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    uint32_t left = gFileLength - gFilePos;
    uint32_t take = length < left ? length : left;
    std::memcpy(buffer, gFile + gFilePos, take);
    gFilePos += take;
    if (transferred != nullptr) *transferred = take;
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), length, transferred != nullptr ? 1 : 0, take});
    return TbError(static_cast<int32_t>(ret("LbFile_Read")));
}

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags});
    gBlock = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(size + 0x400));
    return gBlock;
}

void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {block == gBlock ? 1 : 0});
    return nullptr;
}

void TickRideAnims() { ev("TickRideAnims", {}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    int64_t k = ret("GetRideInfo");
    return k >= 1 && k <= 2 ? gInfos[k - 1] : nullptr;
}

uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e,
                      uint32_t f, int32_t g) {
    ev("InstanceRide", {type, flags, params->mX, params->mZ, params->mRotation, a, b, reinterpret_cast<intptr_t>(c)});
    int64_t shape = reinterpret_cast<intptr_t>(params->mF0C);
    int64_t s0 = 0, s1 = 0;
    if (params->mF0C == gLoadRidesShape) {
        shape = -1;
        s0 = gLoadRidesShape[0];
        s1 = gLoadRidesShape[1];
    }
    ev("InstanceRide2", {d, e, f, g, shape, s0, s1});
    return static_cast<uint32_t>(ret("InstanceRide"));
}

void ERRORBox(const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* name = va_arg(args, const char*);
    va_end(args);
    ev("ERRORBox", {}, {format, name});
}

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {level}, {format}); }

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

void ReactivateHoardings(tag_sRideAnimSet* set) { ev("ReactivateHoardings", {SlotOf(set)}); }

bool RestoreAnimTime(tag_sRideAnimSet* set, sAnimTimeControl* control, sAnimTimeControlSave* save) {
    ev("RestoreAnimTime", {SlotOf(set), control - static_cast<sAnimTimeControl*>(set->mTimeControls),
                           reinterpret_cast<uint8_t*>(save) - gBlock});
    uint32_t w[11];
    std::memcpy(w, save, sizeof(w));
    ev("RestoreAnimTimeSave", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8], w[9], w[10]});
    return ret("RestoreAnimTime") != 0;
}

void OutputAnimTimeStats(sAnimTimeControl* control, tag_sRideAnimSet* set) {
    ev("OutputAnimTimeStats", {control - static_cast<sAnimTimeControl*>(set->mTimeControls), SlotOf(set)});
}

void StaticifyCreateOnlyAnimNodes(tag_sRideAnimSet* set) { ev("StaticifyCreateOnlyAnimNodes", {SlotOf(set)}); }
void OverrideActiveChannels(tag_sRideAnimSet* set) { ev("OverrideActiveChannels", {SlotOf(set)}); }
void GetNumActiveChannels(tag_sRideAnimSet* set) { ev("GetNumActiveChannels", {SlotOf(set)}); }

extern "C" {

uint64_t tpw_ld_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"db", &gRideDatabase},
        {"shape", gLoadRidesShape},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ld_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ld_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControl));
    out[n++] = static_cast<int64_t>(sizeof(sAnimTimeControlSave));
    return n;
}

// A ride set with its mesh, header, node arrays (Mac strides), embedded set and time controls.
uint64_t tpw_ld_make_set(int32_t allNodes, int32_t nodes, int32_t embedded, int32_t hasEmbed, int32_t timeCount, const char* name,
                         uint32_t flags, uint32_t f0c, uint32_t levelBits, uint32_t ff8) {
    auto* set = static_cast<tag_sRideAnimSet*>(tpw::ArenaAllocZeroed(sizeof(tag_sRideAnimSet)));
    auto* mesh = static_cast<tag_sMesh*>(tpw::ArenaAllocZeroed(sizeof(tag_sMesh)));
    auto* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    set->mHeads = mesh;
    mesh->mHeader = header;
    set->mFlags = flags;
    set->mF0C = static_cast<uint16_t>(f0c);
    std::memcpy(&set->mHoardingLevel, &levelBits, 4);
    set->mFF8 = ff8;
    set->mTimeCount = static_cast<uint16_t>(timeCount);
    set->mTimeControls = tpw::ArenaAllocZeroed((timeCount > 0 ? timeCount : 1) * sizeof(sAnimTimeControl));
    header->mAllNodeCount = static_cast<uint16_t>(allNodes);
    header->mNodeCount = static_cast<uint16_t>(nodes);
    header->mEmbeddedCount = static_cast<uint16_t>(embedded);
    std::strncpy(header->mName, name, 0x13);
    header->mNodes.set(static_cast<tag_sNode*>(tpw::ArenaAllocZeroed((nodes > 0 ? nodes : 1) * 0xa0)));
    int32_t minis = allNodes - nodes;
    header->mMiniNodes.set(static_cast<tag_sMiniNode*>(tpw::ArenaAllocZeroed((minis > 0 ? minis : 1) * 0x58)));
    if (hasEmbed != 0) {
        auto* es = static_cast<sEmbeddedSet*>(tpw::ArenaAllocZeroed(sizeof(sEmbeddedSet)));
        es->mRecords = static_cast<sEmbeddedRecord*>(tpw::ArenaAllocZeroed((embedded > 0 ? embedded : 1) * sizeof(sEmbeddedRecord)));
        mesh->mF28 = es;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(set));
}

static uint32_t* NodeFlagsAt(tag_sRideAnimSet* set, int32_t i) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    if (i < header->mNodeCount) return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mNodes.get()) + i * 0xa0);
    return reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(header->mMiniNodes.get()) + (i - header->mNodeCount) * 0x58);
}

void tpw_ld_node_flags(uint64_t setAddress, int32_t i, uint32_t value) {
    *NodeFlagsAt(reinterpret_cast<tag_sRideAnimSet*>(setAddress), i) = value;
}

// i < 0: the set's two words; else the record i.
void tpw_ld_embed(uint64_t setAddress, int32_t i, uint32_t a, uint32_t b) {
    auto* set = reinterpret_cast<tag_sRideAnimSet*>(setAddress);
    auto* es = static_cast<sEmbeddedSet*>(set->mHeads->mF28);
    if (i < 0) {
        es->mFlags = a;
        es->mF08 = b;
    } else {
        es->mRecords[i].mFlags = a;
        es->mRecords[i].mF0C = b;
    }
}

void tpw_ld_control(uint64_t setAddress, int32_t i, uint32_t f04) {
    auto* set = reinterpret_cast<tag_sRideAnimSet*>(setAddress);
    static_cast<sAnimTimeControl*>(set->mTimeControls)[i].mF04 = f04;
}

void tpw_ld_install(int32_t kind, int32_t index, uint64_t setAddress) {
    auto* set = reinterpret_cast<tag_sRideAnimSet*>(setAddress);
    if (kind == 0) gRideAnimSets[index] = set; else gRideTypes[index] = set;
}

void tpw_ld_globals(uint32_t a, uint32_t b, uint32_t first, uint32_t typeCount, int32_t zero, int32_t s0, int32_t s1) {
    std::memset(gRideAnimSets, 0, sizeof(gRideAnimSets));
    std::memset(gRideTypes, 0, sizeof(gRideTypes));
    gRideCountA = a;
    gRideCountB = b;
    gRideFirstFree = first;
    gRideTypeCount = typeCount;
    gLoadRidesTbErrorZero = zero;
    gLoadRidesShape[0] = s0;
    gLoadRidesShape[1] = s1;
    gInfos[0] = gInfos[1] = nullptr;
}

void tpw_ld_info(int32_t k, uint64_t shape) {
    auto* info = static_cast<RideInfo*>(tpw::ArenaAllocZeroed(sizeof(RideInfo)));
    info->Value(kRideSlot_Shape).mShape = reinterpret_cast<void*>(static_cast<uintptr_t>(shape));
    gInfos[k - 1] = info;
}

void tpw_ld_file(const uint8_t* data, uint32_t length) {
    std::memcpy(gFile, data, length);
    gFileLength = length;
    gFilePos = 0;
    gBlock = nullptr;
}

// The words the call may change: the set's flags, f0c, level; the node flags; the embedded words; the controls' f04.
uint32_t tpw_ld_dump(uint64_t setAddress, uint32_t* out) {
    auto* set = reinterpret_cast<tag_sRideAnimSet*>(setAddress);
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t n = 0;
    out[n++] = set->mFlags;
    out[n++] = set->mF0C;
    std::memcpy(&out[n++], &set->mHoardingLevel, 4);
    for (int32_t i = 0; i < header->mAllNodeCount; i++) out[n++] = *NodeFlagsAt(set, i);
    auto* es = static_cast<sEmbeddedSet*>(set->mHeads->mF28);
    if (es != nullptr) {
        out[n++] = es->mFlags;
        out[n++] = es->mF08;
        for (int32_t i = 0; i < header->mEmbeddedCount; i++) {
            out[n++] = es->mRecords[i].mFlags;
            out[n++] = es->mRecords[i].mF0C;
        }
    }
    for (int32_t i = 0; i < set->mTimeCount; i++) out[n++] = static_cast<sAnimTimeControl*>(set->mTimeControls)[i].mF04;
    return n;
}

void tpw_ld_globals_out(uint32_t* out) {
    out[0] = gRideCountA;
    out[1] = gRideCountB;
    out[2] = gRideFirstFree;
    out[3] = static_cast<uint32_t>(gLoadRidesShape[0]);
    out[4] = static_cast<uint32_t>(gLoadRidesShape[1]);
}

int32_t tpw_ld_run(uint64_t file) { return LoadRides(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

}  // extern "C"
