// Test library for the peep tables of a ride script (unit w149; libtpw_unit_RidePeeps.dylib, tests/test_RidePeeps.py), main app:
//   src/game/rides/RideEffects.cpp   WALKNODE_Tick, Trigger, Get, Float, Release; BOUNCE_Put, Get, ForceGet, Tick; LIMBO_Put, Get, ForceGet; GetNodePos
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues: the clock, the
// assertion, DrawPeep, SetRideSpritePosnFocus, EnableRideHead, DisableRideHead, FindDataNodeIndexByID, PAR_InitEmitter, CPeep::DrawMySpriteAt,
// GetRideMasterBoundArea, GetRideFromID, GetDataNodeWorldMatrix and NormaliseVector. GetNodePos is the real one. The globals of other files the
// unit reads (gRideAnimSets, gThingArray, gTPGlobals, gClock) are defined here, zero at start like the Mac's bss. The ride sets, their meshes
// and embedded records carry native pointers on the host, so the library builds them (tpw_rp_ride); the script record and its tables are
// Mac-sized arena blocks the test fills.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "calllog.h"
#include "Clock.h"
#include "Debug.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
tag_sRideAnimSet* gRideAnimSets[16];
CThingArrayEntry gThingArray[kThingArraySize];
// gTPGlobals and gClock as raw storage under the symbols' names (nothing constructs them; the unit reads a few words).
#ifdef __APPLE__
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
extern unsigned char gClockStorage[sizeof(CClock)] __asm__("_gClock");
#else
extern unsigned char gGlobalsStorage[sizeof(TPGlobals)] __asm__("gTPGlobals");
extern unsigned char gClockStorage[sizeof(CClock)] __asm__("gClock");
#endif
alignas(16) unsigned char gGlobalsStorage[sizeof(TPGlobals)] = {};
alignas(16) unsigned char gClockStorage[sizeof(CClock)] = {};

namespace {

// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float BitsFloat(int64_t bits) {
    const uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

tag_sRideAnimSet* gCell;      // what GetRideFromID answers a pointer to
float gMatrix[16];            // what GetDataNodeWorldMatrix answers

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stubs (pointers as lab(p))
int32_t CClock::Read() {
    ev("Clock");
    return static_cast<int32_t>(ret("clock", 0));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    if (std::strstr(format, "%s") != nullptr) {
        va_list args;
        va_start(args, format);
        const char* name = va_arg(args, const char*);
        va_end(args);
        ev("GAME_ASSERTMSG", {condition}, {format, name});
    } else {
        ev("GAME_ASSERTMSG", {condition}, {format});
    }
}

void DrawPeep(RSSE_SCRIPT* script, RSSE_WALK* walk, int32_t from, int32_t to, int32_t mode, int32_t fraction) {
    ev("DrawPeep", {lab(script), lab(walk), from, to, mode, fraction});
}

void SetRideSpritePosnFocus(tag_sMesh* mesh) { ev("SetRideSpritePosnFocus", {lab(mesh)}); }

void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) {
    ev("EnableRideHead", {lab(mesh), static_cast<int32_t>(index), static_cast<int32_t>(peep)});
}

void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {lab(mesh), static_cast<int32_t>(index)}); }

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), static_cast<int32_t>(mask), static_cast<int32_t>(id)});
    return static_cast<uint32_t>(ret(mask == 0x80 ? "findHead" : "findWalk", 0));
}

uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return 0;
}

void CPeep::DrawMySpriteAt(float x, float y, float z, int32_t frame, CDelayedScript::ScriptIdx script) {
    ev("DrawMySpriteAt", {lab(this), CanonBits(x), CanonBits(y), CanonBits(z), frame, static_cast<int32_t>(script)});
}

int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    ev("GetRideMasterBoundArea", {static_cast<int32_t>(ride)});
    const int32_t answer = static_cast<int32_t>(ret("bound", 0));
    if (answer != 0) {
        const char* keys[6] = {"box0", "box1", "box2", "box3", "box4", "box5"};
        for (int i = 0; i < 3; ++i) {
            box->mMin[i] = BitsFloat(ret(keys[i], 0));
            box->mMax[i] = BitsFloat(ret(keys[3 + i], 0));
        }
    }
    return answer;
}

tag_sRideAnimSet** GetRideFromID(uint32_t& id) {
    ev("GetRideFromID", {static_cast<int32_t>(id)});
    gCell = gRideAnimSets[id];
    return &gCell;
}

float* GetDataNodeWorldMatrix(tag_sMesh*& mesh, uint32_t& index) {
    ev("GetDataNodeWorldMatrix", {lab(mesh), static_cast<int32_t>(index)});
    const int64_t changed = ret("newIndex", -1);
    if (changed != -1) index = static_cast<uint32_t>(changed);
    if (ret("matrix", 0) == 0) return nullptr;
    for (int i = 0; i < 16; ++i) {
        char key[8];
        std::snprintf(key, sizeof key, "m%d", i);
        gMatrix[i] = BitsFloat(ret(key, 0));
    }
    return gMatrix;
}

void NormaliseVector(s3DPoint* vector) {
    ev("NormaliseVector", {CanonBits(vector->x), CanonBits(vector->y), CanonBits(vector->z)});
    vector->x = BitsFloat(ret("nv0", 0));
    vector->y = BitsFloat(ret("nv1", 0));
    vector->z = BitsFloat(ret("nv2", 0));
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_rp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRideAnimSets", &gRideAnimSets},
        {"gThingArray", &gThingArray},
        {"gTPGlobals", &gTPGlobals},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_rp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset of an arena block: what a Ptr32 field holds.
uint32_t tpw_rp_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_rp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_WALK));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_BOUNCE));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_LIMBO));
    out[n++] = static_cast<int64_t>(sizeof(JCPOINT3D));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableMask));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTableScale));
    out[n++] = static_cast<int64_t>(offsetof(TPGlobals, mSinTable));
    out[n++] = static_cast<int64_t>(sizeof(TPGlobals::mSinTable));
    return n;
}

// Builds ride set `k` (in gRideAnimSets) with `n` embedded records: flags[i], and a 16-float block for those with has[i] set; rootY is the
// root node's matrix height (m[3][1]). Answers the set; tpw_rp_mesh answers its mesh.
uint64_t tpw_rp_ride(int32_t k, int32_t n, const uint32_t* flags, const uint8_t* has, const float* blocks, float rootY) {
    auto* set = static_cast<tag_sRideAnimSet*>(std::calloc(1, sizeof(tag_sRideAnimSet)));
    auto* mesh = static_cast<tag_sMesh*>(std::calloc(1, sizeof(tag_sMesh)));
    auto* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    auto* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
    auto* embedded = static_cast<sEmbeddedSet*>(std::calloc(1, sizeof(sEmbeddedSet)));
    auto* records = static_cast<sEmbeddedRecord*>(std::calloc(static_cast<size_t>(n > 0 ? n : 1), sizeof(sEmbeddedRecord)));
    node->mMatrix.m[3][1] = rootY;
    header->mF78 = node;
    for (int32_t i = 0; i < n; ++i) {
        records[i].mFlags = flags[i];
        if (has[i] != 0) {
            auto* block = static_cast<uint8_t*>(std::calloc(1, 0x40));
            std::memcpy(block, blocks + 16 * i, 0x40);
            records[i].mBlock = block;
        }
    }
    embedded->mRecords = records;
    mesh->mHeader = header;
    mesh->mF28 = embedded;
    set->mHeads = mesh;
    gRideAnimSets[k] = set;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(set));
}
uint64_t tpw_rp_mesh(int32_t k) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRideAnimSets[k]->mHeads)); }
void tpw_rp_ride_clear(int32_t k) { gRideAnimSets[k] = nullptr; }

// Puts a native object in row `index` of gThingArray (0 clears it).
void tpw_rp_thing(int32_t index, uint64_t object) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(object)); }

// ---- the functions, one wrapper each with C types only (pointers as uint64_t)
static RSSE_SCRIPT* S(uint64_t p) { return reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(p)); }
int32_t tpw_rp_getnodepos(uint64_t script, uint64_t pos, uint64_t dir, int32_t node, int32_t mask) {
    return GetNodePos(S(script), reinterpret_cast<JCPOINT3D*>(static_cast<uintptr_t>(pos)), reinterpret_cast<JCPOINT3D*>(static_cast<uintptr_t>(dir)), node, mask);
}
void tpw_rp_trigger(uint64_t script, uint32_t peep, int32_t from, int32_t to, int32_t outFrom, int32_t outTo, int32_t kind, int32_t last) {
    WALKNODE_Trigger(S(script), peep, from, to, outFrom, outTo, kind, last);
}
uint32_t tpw_rp_walkget(uint64_t script) { return WALKNODE_Get(S(script)); }
void tpw_rp_walkfloat(uint64_t script, int32_t duration, int32_t a, int32_t b) { WALKNODE_Float(S(script), duration, a, b); }
void tpw_rp_walkrelease(uint64_t script, uint32_t peep) { WALKNODE_Release(S(script), peep); }
void tpw_rp_walktick(uint64_t script) { WALKNODE_Tick(S(script)); }
int32_t tpw_rp_bounceput(uint64_t script, uint32_t peep, int32_t time) { return BOUNCE_Put(S(script), peep, time); }
uint32_t tpw_rp_bounceget(uint64_t script) { return BOUNCE_Get(S(script)); }
uint32_t tpw_rp_bounceforceget(uint64_t script) { return BOUNCE_ForceGet(S(script)); }
void tpw_rp_bouncetick(uint64_t script) { BOUNCE_Tick(S(script)); }
int32_t tpw_rp_limboput(uint64_t script, uint32_t peep, int32_t time) { return LIMBO_Put(S(script), peep, time); }
uint32_t tpw_rp_limboget(uint64_t script) { return LIMBO_Get(S(script)); }
uint32_t tpw_rp_limboforceget(uint64_t script) { return LIMBO_ForceGet(S(script)); }

}  // extern "C"
