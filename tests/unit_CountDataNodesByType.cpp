// Test library for the embedded-entry counters, EnableRideHead, bthread_cond and CalculateOrientFromTrackMorph (unit v5962;
// libtpw_unit_CountDataNodesByType.dylib, tests/test_CountDataNodesByType.py), main app:
//   src/game/TPEmbed.cpp, src/game/online/bthread.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The world's tables
// (gThingArray, gTPGlobals) are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"


using namespace calllog;

// ---- globals of other files the unit reads, defined here
// gTPGlobals is only the sine table here: plain zeroed storage under the symbol's name (the real constructor builds strings from files
// this library does not compile).
alignas(16) uint8_t gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
CThingArrayEntry gThingArray[kThingArraySize];

namespace {
void* gCurrent = nullptr;   // the object the bthread_cond wrappers work on
}

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    va_list args;
    va_start(args, format);
    uint32_t a = va_arg(args, uint32_t);
    uint32_t b = va_arg(args, uint32_t);
    va_end(args);
    ev("GAME_ASSERTMSG", {condition, a, b}, {format});
}

int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {lab(block), static_cast<int32_t>(sprite), a, static_cast<int32_t>(acts), fbits(x), fbits(y), fbits(z)});
    return static_cast<int32_t>(ret("ESP_SpawnScript"));
}

uint32_t CPerson::GetHeadSpriteID() {
    ev("GetHeadSpriteID", {static_cast<int64_t>(*reinterpret_cast<uint32_t*>(this))});
    return static_cast<uint32_t>(ret("GetHeadSpriteID"));
}

namespace NS_MacDoze {
InternalHandle* CreateEvent(void* security, int32_t manualReset, int32_t initialState, const char* name) {
    ev("CreateEvent", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(security)), manualReset, initialState,
                       static_cast<int64_t>(reinterpret_cast<uintptr_t>(name))});
    return reinterpret_cast<InternalHandle*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("CreateEvent"))));
}
uint32_t WaitForSingleObject(InternalHandle* handle, uint32_t milliseconds) {
    ev("WaitForSingleObject", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle))), milliseconds});
    return static_cast<uint32_t>(ret("WaitForSingleObject"));
}
int32_t CloseHandle(InternalHandle* handle) {
    ev("CloseHandle", {static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(handle)))});
    return 0;
}
}  // namespace NS_MacDoze

void bthread_callee::OperatorDelete(void* p) { ev("delete", {p == gCurrent ? 1 : 0}); }

void SetMatrixIdentity(sMatrix* matrix) {
    ev("SetMatrixIdentity", {});
    std::memset(matrix, 0, sizeof(sMatrix));
    for (int i = 0; i < 4; i++) matrix->m[i][i] = 1.0f;
}

// The rotation fakes put their two arguments in distinct cells so that CombineMatrices... shows which matrix it was given.
void ZRotateLMatrix(sMatrix* matrix, float c, float s) {
    ev("ZRotateLMatrix", {fbits(c), fbits(s)});
    matrix->m[0][0] = c;
    matrix->m[0][1] = s;
}
void YRotateLMatrix(sMatrix* matrix, float c, float s) {
    ev("YRotateLMatrix", {fbits(c), fbits(s)});
    matrix->m[1][1] = c;
    matrix->m[1][2] = s;
}
void XRotateLMatrix(sMatrix* matrix, float c, float s) {
    ev("XRotateLMatrix", {fbits(c), fbits(s)});
    matrix->m[2][2] = c;
    matrix->m[2][0] = s;
}
void CombineMatricesNoWNoTransDest2nd(sMatrix* first, sMatrix* second) {
    int64_t a[9], b[9];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            uint32_t x, y;
            std::memcpy(&x, &first->m[i][j], 4);
            std::memcpy(&y, &second->m[i][j], 4);
            a[i * 3 + j] = x;
            b[i * 3 + j] = y;
        }
    ev("CombineFirst", {a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]});
    ev("CombineSecond", {b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8]});
}

extern "C" {

uint64_t tpw_cdn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"things", &gThingArray[0]},
        {"block", &gRideHeadSpawnBlock},
        {"tpg", &gTPGlobalsStorage[0]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cdn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_cdn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mF28));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mRecords));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedSet, mF08));
    out[n++] = static_cast<int64_t>(offsetof(sEmbeddedRecord, mF0C));
    out[n++] = static_cast<int64_t>(sizeof(sEmbeddedRecord));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(bthread_cond));
    out[n++] = static_cast<int64_t>(sizeof(sMatrix));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTable));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableScale));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableMask));
    return n;
}

uint32_t tpw_cdn_count(uint64_t set, uint32_t kind) { return CountDataNodesByType(reinterpret_cast<tag_sRideAnimSet*>(set), kind); }
uint32_t tpw_cdn_find(uint64_t set, uint32_t kind, uint32_t n) {
    return static_cast<uint32_t>(FindNthDataNodeIndex(reinterpret_cast<tag_sRideAnimSet*>(set), kind, n));
}
void tpw_cdn_enable(uint64_t mesh, uint32_t index, uint32_t peep) { EnableRideHead(reinterpret_cast<tag_sMesh*>(mesh), index, peep); }
uint64_t tpw_cdn_ctor(uint64_t object) {
    gCurrent = reinterpret_cast<void*>(object);
    return reinterpret_cast<uintptr_t>(new (gCurrent) bthread_cond());
}
uint64_t tpw_cdn_dtor(uint64_t object, int32_t flag) {
    gCurrent = reinterpret_cast<void*>(object);
    return reinterpret_cast<uintptr_t>(bthread_cond::Destroy(reinterpret_cast<bthread_cond*>(object), static_cast<int16_t>(flag)));
}
uint32_t tpw_cdn_wait(uint64_t object, int32_t ms) { return reinterpret_cast<bthread_cond*>(object)->wait(ms) ? 1 : 0; }
void tpw_cdn_orient(uint64_t p0, uint64_t p1, uint64_t normal, uint64_t morph, uint64_t matrix) {
    CalculateOrientFromTrackMorph(reinterpret_cast<s3DPoint*>(p0), reinterpret_cast<s3DPoint*>(p1), reinterpret_cast<s3DPoint*>(normal),
                                  reinterpret_cast<sTrackMorphingParent*>(morph), reinterpret_cast<sMatrix*>(matrix));
}

}  // extern "C"
