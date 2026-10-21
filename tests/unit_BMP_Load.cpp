// Test library for the advisor load (unit v5L09; libtpw_unit_BMP_Load.dylib, tests/test_BMP_Load.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_Load
// with the chunk reader of unit v51017 (src/game/rides/BumperRide.cpp) as the original runs it.
//
// The callees outside the unit are recording fakes answering from the test's queues. The file functions share one byte stream
// (the test sets it): BMPFile_Read copies from it (a 4-byte read at a position the test marks fails), LbFile_GetPosition answers
// a base plus the position, LbFile_Seek moves the position. gColList (0x40 bytes here, the Mac's world is 0x3c), gRideAnimSets
// and gRideDatabase are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files, defined here
alignas(16) unsigned char gColListStorage[0x40] __asm__("_gColList");
tag_sRideAnimSet* gRideAnimSets[64];
alignas(16) unsigned char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {
std::vector<uint8_t> gStream;
uint32_t gPosition;
uint32_t gStreamBase;
}  // namespace

namespace MathLib {
int __fpclassifyf(float x) {
    ev("fpclassifyf", {x != x ? 0x7fc00000u : fbits(x)});
    return x != x ? 1 : 4;
}
}  // namespace MathLib

// ---- the fakes
void TRACE(const char* format, ...) {
    char text[160];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("TRACE", {}, {text});
}

TbError BMPFile_Read(void* file, void* buffer, uint32_t size, uint32_t* transferred) {
    ev("BMPFile_Read", {lab(file), static_cast<int64_t>(size)});
    if (size == 4 && ret("fail", gPosition, 0) != 0) return TbError(-1);
    uint32_t left = gPosition < gStream.size() ? static_cast<uint32_t>(gStream.size()) - gPosition : 0;
    uint32_t count = size < left ? size : left;
    if (count != 0) std::memcpy(buffer, gStream.data() + gPosition, count);
    gPosition += count;
    *transferred = count;
    return TbError(0);
}

int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(gStreamBase + gPosition);
}

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    ev("LbFile_Seek", {lab(file), offset, whence});
    gPosition = static_cast<uint32_t>(offset) - gStreamBase;
    return 0;
}

namespace NS_MacDoze {
InternalHandle* GlobalAlloc(uint32_t flags, uint32_t size) {
    ev("GlobalAlloc", {static_cast<int64_t>(flags), static_cast<int64_t>(size)});
    return reinterpret_cast<InternalHandle*>(static_cast<uintptr_t>(ret("GlobalAlloc")));
}

InternalHandle* GlobalFree(InternalHandle* handle) {
    ev("GlobalFree", {lab(handle)});
    return nullptr;
}
}  // namespace NS_MacDoze

int32_t BMP_CreateRide(int32_t kind, int32_t x, int32_t y, int32_t rotation, int32_t id, int32_t f10) {
    ev("BMP_CreateRide", {kind, x, y, rotation, id, f10});
    return static_cast<int32_t>(ret("BMP_CreateRide"));
}

void BMP_SetRidePerformance(int32_t ride, int32_t speed) { ev("BMP_SetRidePerformance", {ride, speed}); }

void BMP_SetRideModelSelection(int32_t ride, int32_t a, int32_t b) { ev("BMP_SetRideModelSelection", {ride, a, b}); }

int32_t BMP_SetRideDuration(int32_t ride, int32_t duration) {
    ev("BMP_SetRideDuration", {ride, duration});
    return static_cast<int32_t>(ret("BMP_SetRideDuration"));
}

int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9) {
    ev("BMP_TrackSectionPlaceSection", {id, x, y, type, f7, f8, f9});
    return static_cast<int32_t>(ret("BMP_TrackSectionPlaceSection"));
}

void BMP_AddPeepToRideLeavingList(int32_t ride, uint32_t peep) { ev("BMP_AddPeepToRideLeavingList", {ride, peep}); }

int32_t BMP_AddPeepToRideJoiningList(int32_t ride, uint32_t peep) {
    ev("BMP_AddPeepToRideJoiningList", {ride, peep});
    return static_cast<int32_t>(ret("BMP_AddPeepToRideJoiningList"));
}

void SetBuoySequence(tagBUMPRIDE* ride) { ev("SetBuoySequence", {lab(ride)}); }

int32_t FindBuoy(tagBUMPRIDE* ride, int32_t x, int32_t y) {
    ev("FindBuoy", {lab(ride), x, y});
    return static_cast<int32_t>(ret("FindBuoy"));
}

tagBUMP* BMP_StartBump(int32_t x, int32_t y, int32_t type) {
    ev("BMP_StartBump", {x, y, type});
    return reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(ret("BMP_StartBump")));
}

tabCOLOBJECT* PointInAnyColObject(int32_t x, int32_t y) {
    ev("PointInAnyColObject", {x, y});
    return reinterpret_cast<tabCOLOBJECT*>(static_cast<uintptr_t>(ret("PointInAnyColObject")));
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(ret("GetRideInfo")));
}

uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d,
                             int32_t e, int32_t f) {
    ev("RestoreRideInstance", {instance, type, flags, lab(params), a, lab(c), d, e, f});
    return static_cast<uint32_t>(ret("RestoreRideInstance"));
}

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID"));
}

extern "C" {

uint64_t tpw_bl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gColListStorage},
        {"sets", gRideAnimSets},
        {"ridedb", gRideDatabaseStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 field holds for an address.
uint32_t tpw_bl_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

void tpw_bl_stream(const uint8_t* data, uint32_t size, uint32_t base) {
    gStream.assign(data, data + size);
    gPosition = 0;
    gStreamBase = base;
}

int32_t tpw_bl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabBMPCHUNK));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, col));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mSubRideTypes));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    return n;
}

int32_t tpw_bl_load(uint64_t file) { return BMP_Load(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

}  // extern "C"
