// Differential library for unit w132: the ride scripts (RSSE_Tick, RSSE_RenderTick, RSSE_Getvar, RSSE_GetScriptPointer, RSSE_SetReverbOnEax), the script
// table's tick, close and sprite list, and the ride object's UpdateTurn, ModelState, CreateRideScript and UpdateEngine. Every callee outside the unit is a
// recording fake. The script records, their variables, the script table and its entries come from the arena (their links are 32-bit offsets), so they
// are made here from the images the test builds; the object, the world's turn word and the ride record are static blocks.
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "ActionRecorder.h"
#include "Clock.h"
#include "Debug.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "LevelInit.h"
#include "MainBalance.h"
#include "RideDB.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// The two CPerson members ModelState calls (declared against their Mac names in src/game/Object.cpp).
void ObjectForceRideAdmittance(CPerson*) __asm__("__ZN7CPerson19ForceRideAdmittanceEv");
void ObjectDismissFromQueue(CPerson*) __asm__("__ZN7CPerson16DismissFromQueueEv");

// The globals the unit's sources use (their definitions are in files that are not part of the unit).
CWorld* gWorld;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[16];
CRideDatabase gRideDatabase;
RSSEState gRSSEState;
int32_t gRSSEF7BE0;
uint32_t gRSSEF7BE4;
uint32_t gScriptInitialised;
int32_t gScriptTableSize;
void* gScriptTable;
CClock gClock;
CActionRecorder gActionRecorder;

namespace {
constexpr int32_t kBlockSize = 0x280;
constexpr int64_t kKeep = 0x5a5a5a5a;   // the queues' "leave the state alone"
alignas(16) unsigned char gObject[0x448 + 0x40];
alignas(16) unsigned char gWorldTail[0x100];
alignas(16) unsigned char gRideBlock[sizeof(RideInfo) + 0x40];
alignas(16) unsigned char gMisc[24][kBlockSize];
char16_t gFileName[0x100];
char gFound[0x420];
std::vector<void*> gArenaBlocks;
std::vector<RSSE_SCRIPT*> gScripts;
tagSCRIPT* gEntries[128];
void* gTableBlock;
int32_t gFieldOffset[32];
int32_t gFieldWidth[32];
int32_t gFieldCount;
int32_t gPiCalls;

CWorld* WorldBase() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gWorldTail) - 0x1da6f0); }

// The object's fields as the case's table lists them: the hash of the object at the call of an object fake.
uint32_t Snap() {
    uint32_t h = 2166136261u;
    for (int32_t i = 0; i < gFieldCount; ++i) {
        uint64_t v = 0;
        std::memcpy(&v, gObject + gFieldOffset[i], static_cast<size_t>(gFieldWidth[i]));
        h = static_cast<uint32_t>((static_cast<uint64_t>(h) ^ v) * 16777619u);
    }
    return h;
}

void* ArenaZero(size_t size) {
    void* p = tpw::ArenaAllocZeroed(size);
    gArenaBlocks.push_back(p);
    return p;
}
RideInfo* Ride() { return reinterpret_cast<RideInfo*>(gRideBlock); }
}  // namespace

// ---- debug output and assertions
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int64_t ints[2] = {0, 0};
    const char* strings[2] = {"", ""};
    int32_t ni = 0, ns = 0;
    for (const char* p = format; *p != 0; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == 'd') {
            const int32_t v = va_arg(args, int);
            if (ni < 2) ints[ni++] = v;
        } else if (*p == 's') {
            const char* s = va_arg(args, const char*);
            if (ns < 2) strings[ns++] = s != nullptr ? s : "(null)";
        }
    }
    va_end(args);
    ev("CTRACE", {level, ints[0], ints[1]}, {format, strings[0], strings[1]});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// ---- the script interpreter and the ticks of the script's things
void ProcessInstruction(RSSE_SCRIPT* script) {
    const int32_t countdown = script->mCountdown;
    const int32_t state = script->mState;
    ++gPiCalls;
    gRSSEF7BE4 = static_cast<uint32_t>(ret("pi_flag", 0));
    const int64_t next = ret("pi_state", kKeep);
    if (next != kKeep) script->mState = static_cast<int32_t>(next);
    if (gPiCalls > 40) script->mState = -1;
    ev("ProcessInstruction", {lab(script), countdown, state, gRSSEF7BE4, script->mState});
}
void ProcessObjects(RSSE_SCRIPT* script) {
    const int32_t state = script->mState;
    const int64_t next = ret("po_state", kKeep);
    if (next != kKeep) script->mState = static_cast<int32_t>(next);
    ev("ProcessObjects", {lab(script), state, script->mCountdown, script->mState});
}
void BOUNCE_Tick(RSSE_SCRIPT* script) { ev("BOUNCE_Tick", {lab(script)}); }
void WALKNODE_Tick(RSSE_SCRIPT* script) { ev("WALKNODE_Tick", {lab(script)}); }
void RSSE_KillScript(int32_t handle, int32_t flag) { ev("RSSE_KillScript", {handle, flag}); }
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) { ev("RSSE_Setvar", {lab(script), index, value}); }
int32_t RSSE_InitScript(const char* path, uint32_t thing, int32_t recording) {
    ev("RSSE_InitScript", {thing >> 16, recording}, {path});
    return static_cast<int32_t>(ret("init", 0));
}
void UpdateRideAnimSpeedAllChannels(tag_sRideAnimSet* set, float speed) { ev("UpdateRideAnimSpeedAllChannels", {lab(set), fbits(speed)}); }
void FLY_SetRunTime(int32_t fly, int32_t time) { ev("FLY_SetRunTime", {fly, time}); }
void FLY_SetRunSpeed(int32_t fly, int32_t speed) { ev("FLY_SetRunSpeed", {fly, speed}); }
void CoasterSetSpeed(uint32_t coaster, uint32_t speed) { ev("CoasterSetSpeed", {coaster, speed}); }
void BMP_SetRidePerformance(int32_t ride, int32_t speed) { ev("BMP_SetRidePerformance", {ride, speed}); }
uint32_t GetFirstPersonRideID() {
    ev("GetFirstPersonRideID");
    return static_cast<uint32_t>(ret("fpr", 0));
}
void SOUND_SetReverbLevel(int32_t level) { ev("SOUND_SetReverbLevel", {level}); }

// ---- the script table
int32_t DoScript(tagSCRIPT* script) {
    ev("DoScript", {lab(script), script->mState, script->mNextTime, script->mPeriod});
    return static_cast<int32_t>(ret("ds", 0));
}
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
void SCR_CloseCheckTable() { ev("SCR_CloseCheckTable"); }
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }
int32_t CClock::Read() {
    ev("Clock");
    return static_cast<int32_t>(ret("clock", 0));
}
void ResetHLSprites() { ev("ResetHLSprites"); }
void DrawHLSprites() { ev("DrawHLSprites"); }
void ESP_AddHLSpriteFromScript(tagSCRIPT* script) { ev("ESP_AddHLSpriteFromScript", {lab(script)}); }

// ---- the ride object's members and the world
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const auto id = static_cast<uint16_t>(ret("id", 0x1234));
    ev("GetThingId", {lab(this), Snap()});
    return {id};
}
int32_t CPerson::InQueueingState() const {
    ev("InQueueingState", {lab(this), Snap()});
    return static_cast<int32_t>(ret("inq", 0));
}
void ObjectForceRideAdmittance(CPerson* person) { ev("ForceRideAdmittance", {lab(person), Snap()}); }
void ObjectDismissFromQueue(CPerson* person) { ev("DismissFromQueue", {lab(person), Snap()}); }
void CBalanceTable::Init() {}
void CBalanceTable::UnInit() {}
void CObject::AddWearAndTear() { ev("AddWearAndTear", {lab(this), Snap()}); }
void CObject::RequestBreakdown() { ev("RequestBreakdown", {lab(this), Snap()}); }
void CObject::SetWornFlag() { ev("SetWornFlag", {lab(this), Snap()}); }
void CObject::ProcessStateFunctioning() { ev("ProcessStateFunctioning", {lab(this), Snap()}); }
void CObject::LetPeopleOff() { ev("LetPeopleOff", {lab(this), Snap()}); }
void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("RemovePersonFromQueue", {lab(this), person.mIndex, Snap()});
}
const char16_t* CObject::GetFileName() const {
    ev("GetFileName", {lab(this), Snap()});
    return gFileName;
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this), Snap()});
    return Ride();
}
void CMap::RemoveRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("RemoveRegionalEffector", {lab(this), effector, cell.mIndex, Snap()});
}
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {this == &gRideDatabase, id.mIndex, Snap()});
    return ret("ri", 1) != 0 ? Ride() : nullptr;
}
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    ev("FindFilePath", {flags, result != nullptr}, {name});
    if (ret("ffp", 1) == 0) return nullptr;
    std::strcpy(gFound, "found:");
    std::strncat(gFound, name, 0x400);
    return gFound;
}
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c, int32_t d, int32_t e, uint32_t f, int32_t g) {
    (void)a;
    ev("InstanceRide", {type, flags, params->mX, params->mZ, params->mRotation, lab(params->mF0C), b, lab(c), d, e, f, g});
    return static_cast<uint32_t>(ret("inst", 0));
}
uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d, int32_t e, int32_t f) {
    ev("RestoreRideInstance", {instance, type, flags, params->mX, params->mZ, params->mRotation, lab(params->mF0C), a, lab(c), d, e, f});
    return static_cast<uint32_t>(ret("rest", 0));
}
void SetRideScriptID(uint32_t instance, uint32_t script) { ev("SetRideScriptID", {instance, script}); }
void HACKRestoreCoasterQueues(uint32_t instance) { ev("HACKRestoreCoasterQueues", {instance}); }

extern "C" {
// Pointers by kind: 0 script i, 1 misc block i, 2 object, 3 world tail, 4 ride record, 5 script table, 6 table entry i, 7 world base.
void* tpw_rs_ptr(int32_t kind, int32_t i) {
    switch (kind) {
    case 0: return i >= 0 && i < static_cast<int32_t>(gScripts.size()) ? static_cast<void*>(gScripts[static_cast<size_t>(i)]) : nullptr;
    case 1: return gMisc[i];
    case 2: return gObject;
    case 3: return gWorldTail;
    case 4: return gRideBlock;
    case 5: return gTableBlock;
    case 6: return gEntries[i];
    default: return WorldBase();
    }
}
int32_t tpw_rs_size(int32_t kind) {
    switch (kind) {
    case 2: return sizeof gObject;
    case 3: return sizeof gWorldTail;
    case 4: return sizeof gRideBlock;
    default: return kBlockSize;
    }
}
void tpw_rs_reset() {
    for (void* b : gArenaBlocks) tpw::ArenaFree(b);
    gArenaBlocks.clear();
    gScripts.clear();
    std::memset(gEntries, 0, sizeof gEntries);
    gTableBlock = nullptr;
    gRSSEState = RSSEState{};
    gRSSEF7BE0 = 0;
    gRSSEF7BE4 = 0;
    gScriptTable = nullptr;
    gScriptTableSize = 0;
    gScriptInitialised = 0;
    gScriptTickTotal = 0;
    gScriptTickTime = 0;
    gPiCalls = 0;
    gWorld = WorldBase();
    gWorldMap = reinterpret_cast<CMap*>(gMisc[0]);
    std::memset(gRideBlock, 0, sizeof gRideBlock);
    std::memset(gFileName, 0, sizeof gFileName);
    for (auto& entry : gThingArray) entry.mThing = nullptr;
    for (auto& entry : gRideAnimSets) entry = nullptr;
    gActionRecorder.mRecording = 0;
}
// The system's globals: 0 initialised, 1 tick count, 2 gRSSEF7BE4, 3 tick total, 4 tick time, 5 table size, 6 table (label), 7 script system on, 8 gRSSEF7BE0.
void tpw_rs_set_globals(const int64_t* in) {
    gRSSEState.mInitialised = static_cast<int32_t>(in[0]);
    gRSSEState.mF04 = static_cast<int32_t>(in[1]);
    gRSSEF7BE4 = static_cast<uint32_t>(in[2]);
    gScriptTickTotal = static_cast<int32_t>(in[3]);
    gScriptTickTime = static_cast<int32_t>(in[4]);
    gScriptInitialised = static_cast<uint32_t>(in[7]);
    gRSSEF7BE0 = static_cast<int32_t>(in[8]);
}
void tpw_rs_get_globals(int64_t* out) {
    out[0] = gRSSEState.mInitialised;
    out[1] = gRSSEState.mF04;
    out[2] = gRSSEF7BE4;
    out[3] = gScriptTickTotal;
    out[4] = gScriptTickTime;
    out[5] = gScriptTableSize;
    out[6] = lab(gScriptTable);
    out[7] = gScriptInitialised;
    out[8] = gRSSEF7BE0;
}
// Appends a script record (the 0xf4-byte image without its links) and its variables to the list.
void* tpw_rs_script(const uint8_t* image, const int32_t* vars, int32_t count) {
    auto* s = static_cast<RSSE_SCRIPT*>(ArenaZero(sizeof(RSSE_SCRIPT)));
    std::memcpy(static_cast<void*>(s), image, sizeof(RSSE_SCRIPT));
    auto* v = static_cast<int32_t*>(ArenaZero(4 * static_cast<size_t>(count > 0 ? count : 1)));
    if (count > 0) std::memcpy(v, vars, 4 * static_cast<size_t>(count));
    s->mVars = v;
    s->mNext = nullptr;
    s->mPrev = nullptr;
    if (gScripts.empty()) {
        gRSSEState.mScripts = s;
    } else {
        gScripts.back()->mNext = s;
        s->mPrev = gScripts.back();
    }
    gScripts.push_back(s);
    return s;
}
// The script table: `size` words (the table is NULL when `null`); then the entries.
void tpw_rs_table(int32_t size, int32_t null) {
    gScriptTableSize = size;
    if (null != 0) {
        gScriptTable = nullptr;
        gTableBlock = nullptr;
    } else {
        gTableBlock = ArenaZero(4 * static_cast<size_t>(size > 0 ? size : 1));
        gScriptTable = gTableBlock;
    }
}
void* tpw_rs_entry(int32_t index, const uint8_t* image) {
    auto* e = static_cast<tagSCRIPT*>(ArenaZero(sizeof(tagSCRIPT)));
    std::memcpy(static_cast<void*>(e), image, sizeof(tagSCRIPT));
    gEntries[index] = e;
    static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[index] = e;
    return e;
}
void tpw_rs_fields(const int32_t* pairs, int32_t n) {
    gFieldCount = n;
    for (int32_t i = 0; i < n; ++i) {
        gFieldOffset[i] = pairs[2 * i];
        gFieldWidth[i] = pairs[2 * i + 1];
    }
}
void tpw_rs_thing(int32_t index, int32_t block) { gThingArray[index].mThing = block < 0 ? nullptr : gMisc[block]; }
void tpw_rs_anim(int32_t index, int32_t block) { gRideAnimSets[index] = block < 0 ? nullptr : reinterpret_cast<tag_sRideAnimSet*>(gMisc[block]); }
void tpw_rs_ride_value(int32_t slot, int32_t value) { Ride()->Value(static_cast<RideBalanceSlot>(slot)).mInteger = value; }
void tpw_rs_ride_shape(int32_t block) { Ride()->Value(kRideSlot_Shape).mShape = block < 0 ? nullptr : gMisc[block]; }
void tpw_rs_ride_folder(const char* text) { std::strncpy(Ride()->mFolder, text, sizeof(Ride()->mFolder) - 1); }
void tpw_rs_ride_number(int32_t number) { Ride()->mRideTypeNumber = static_cast<uint32_t>(number); }
void tpw_rs_file_name(const char* text) {
    size_t i = 0;
    for (; text[i] != 0 && i < 0xff; ++i) gFileName[i] = static_cast<char16_t>(static_cast<uint8_t>(text[i]));
    gFileName[i] = 0;
}
void tpw_rs_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
void tpw_rs_recorder(int32_t value) { gActionRecorder.mRecording = value; }
// op: 0 RSSE_Tick, 1 RSSE_RenderTick, 2 RSSE_Getvar(script a, index b), 3 RSSE_GetScriptPointer(handle a), 4 RSSE_SetReverbOnEax, 5 SCR_ScriptTick, 6 SCR_CloseScriptSystem,
// 7 SCR_BuildScriptSpriteList, 8 CObject::UpdateTurn, 9 ModelState, 10 CreateRideScript, 11 UpdateEngine(flag a). The answer (0 for the void ones; a label for 3).
int64_t tpw_rs_call(int32_t op, int32_t a, int32_t b) {
    CObject* object = reinterpret_cast<CObject*>(gObject);
    switch (op) {
    case 0: return RSSE_Tick();
    case 1: RSSE_RenderTick(); return 0;
    case 2: return RSSE_Getvar(static_cast<RSSE_SCRIPT*>(tpw_rs_ptr(0, a)), b);
    case 3: return lab(RSSE_GetScriptPointer(a));
    case 4: RSSE_SetReverbOnEax(); return 0;
    case 5: SCR_ScriptTick(); return 0;
    case 6: SCR_CloseScriptSystem(); return 0;
    case 7: SCR_BuildScriptSpriteList(); return 0;
    case 8: object->UpdateTurn(); return 0;
    case 9: object->ModelState(); return 0;
    case 10: object->CreateRideScript(); return 0;
    default: object->UpdateEngine(a); return 0;
    }
}
}
