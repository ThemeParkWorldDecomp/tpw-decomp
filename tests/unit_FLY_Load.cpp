// Test library for the fly file loader, the mesh restore and the object file loader (unit v5209; libtpw_unit_FLY_Load.dylib,
// tests/test_FLY_Load.py), main app:
//   src/FLY_Load.cpp   FLY_Load, FLY_RestoreMeshes, OBJECT_Load
//
// The callees outside the unit are fakes that record the arguments the real ones read (a text as its length and byte sum) and answer
// from the test's queues; the file functions serve one byte stream set by the test. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gFlyActive;
int32_t gFlyInitialised;
void* gFlySinTable;
void* gFlyBases[100];
int32_t gFlyZoneA, gFlyZoneB, gFlyZoneC, gFlyZoneD;
int32_t gSoundBanks[8];
CThingArrayEntry gThingArray[kThingArraySize];
// gRideDatabase is only an address here (the fakes never look inside).
alignas(16) char gRideDatabaseStorage[64] __asm__("_gRideDatabase");

namespace {

std::vector<uint8_t> gFile;
size_t gFilePos = 0;
void* gRideBlock = nullptr;

int64_t U(uint64_t v) { return static_cast<int64_t>(static_cast<uint32_t>(v)); }
int64_t S(int32_t v) { return static_cast<int64_t>(v); }
// A text as the module's tsig: the length and the sum of its bytes.
void Sig(const char* text, int64_t* len, int64_t* sum) {
    *len = 0;
    *sum = 0;
    for (; text[*len] != 0; ++*len) *sum += static_cast<uint8_t>(text[*len]);
}

}  // namespace

// ---- the fakes
void FLY_Close() { ev("FLY_Close", {}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    int64_t len, sum;
    Sig(format, &len, &sum);
    ev("GAME_ASSERTMSG", {S(condition), len, sum});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {U(reinterpret_cast<uintptr_t>(file)), U(length), transferred != nullptr ? 1 : 0});
    size_t n = gFile.size() - gFilePos < length ? gFile.size() - gFilePos : length;
    if (n != 0) std::memcpy(buffer, gFile.data() + gFilePos, n);
    gFilePos += n;
    if (transferred != nullptr) *transferred = static_cast<uint32_t>(n);
    return TbError(0);
}

TbError LbFile_Open(void*& file, const char* name, uint32_t mode) {
    int64_t len, sum;
    Sig(name, &len, &sum);
    ev("LbFile_Open", {U(mode), len, sum});
    file = ret("LbFile_Open") != 0 ? reinterpret_cast<void*>(0x5678) : nullptr;
    return TbError(0);
}

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    ev("LbFile_Seek", {U(reinterpret_cast<uintptr_t>(file)), S(offset), U(whence)});
    gFilePos += static_cast<size_t>(static_cast<int64_t>(offset));
    return 0;
}

TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {U(reinterpret_cast<uintptr_t>(file))});
    return TbError(0);
}

void* LbMem_Alloc(uint32_t size) {
    ev("LbMem_Alloc", {U(size)});
    return tpw::ArenaAllocZeroed(size);
}

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {S(type), S(a), S(b), S(c)});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {S(entity), S(bank), S(event), S(a), S(b), S(c)});
    return static_cast<int32_t>(ret("SOUND_SendEvent"));
}

int32_t SOUND_SendEventExt(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("SOUND_SendEventExt", {S(entity), S(bank), S(event), S(a), S(b), S(c), S(d)});
    return static_cast<int32_t>(ret("SOUND_SendEventExt"));
}

void SOUND_SetRect(int32_t handle, int32_t a, int32_t b, int32_t c, int32_t d) {
    ev("SOUND_SetRect", {S(handle), S(a), S(b), S(c), S(d)});
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {this == reinterpret_cast<CRideDatabase*>(gRideDatabaseStorage) ? 1 : 0, S(id.mIndex)});
    return static_cast<RideInfo*>(gRideBlock);
}

uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d,
                             int32_t e, int32_t f) {
    ev("RestoreRideInstance", {U(instance), U(type), U(flags), U(reinterpret_cast<uintptr_t>(params)), U(a),
                               U(reinterpret_cast<uintptr_t>(c)), U(static_cast<uint32_t>(d)), U(static_cast<uint32_t>(e)),
                               U(static_cast<uint32_t>(f))});
    return static_cast<uint32_t>(ret("RestoreRideInstance"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_fly_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gFlyActive", &gFlyActive},     {"gFlyInitialised", &gFlyInitialised}, {"gFlySinTable", &gFlySinTable},
        {"gFlyBases", gFlyBases},        {"gFlyZoneA", &gFlyZoneA},             {"gFlyZoneB", &gFlyZoneB},
        {"gFlyZoneC", &gFlyZoneC},       {"gFlyZoneD", &gFlyZoneD},             {"gSoundBanks", gSoundBanks},
        {"gThingArray", gThingArray},    {"gRideBlock", &gRideBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_fly_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The byte stream the file fakes serve.
void tpw_fly_setfile(const uint8_t* data, uint32_t size) {
    gFile.assign(data, data + size);
    gFilePos = 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_fly_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mSubRideTypes));
    out[n++] = static_cast<int64_t>(sizeof(sFlyBase));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

int32_t tpw_fly_load(uint64_t file) { return FLY_Load(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
void tpw_fly_restore() { FLY_RestoreMeshes(); }
void tpw_fly_object(uint64_t path) { OBJECT_Load(reinterpret_cast<char*>(static_cast<uintptr_t>(path))); }

}  // extern "C"
