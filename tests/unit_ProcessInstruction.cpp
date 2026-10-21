// Test library for the ride script interpreter (unit v5882; libtpw_unit_ProcessInstruction.dylib, tests/test_ProcessInstruction.py), main app:
//   src/game/rides/rsse.cpp   ProcessInstruction 0x100cfdf4
//
// Every callee outside the unit is a fake that records the arguments the real one reads (pointers as labels, floats as bits) and answers
// from the test's queues: the script's program words (ReadScript), the operands' values (ExtractData), the clock, the random numbers, the
// ride animation calls, the objects, limbo, bounce and walk-node calls, the lights, the screams and sounds, the linked scripts, the C
// library imports (labs, strcpy, strcat, strcmp, time, localtime). The globals of other files the unit reads, defined here, zero at
// start like the Mac's bss; the ride set, its mesh, a light and a struct tm are this library's own blocks.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "CFixedTime.h"
#include "CVariableTime.h"
#include "Clock.h"
#include "Debug.h"
#include "LevelInit.h"
#include "Light.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
uint32_t gRSSEF7BE4;
RSSEState gRSSEState;
CClock gClock;
CWorld* gGameWorld;
// gClock's construction needs these constructors; the program's are in files this library does not compile (the fake clock never
// reads what they set).
CFixedTime::CFixedTime() : mLastClock(0), mMilliseconds(0.0) {}
CVariableTime::CVariableTime() : mFixedTimeStorage(), mLastClock(0), mMilliseconds(0.0), mRate(1.0) {}
int32_t gSoundBanks[8];

namespace {

alignas(16) uint8_t gSetStorage[sizeof(tag_sRideAnimSet)];
alignas(16) uint8_t gMeshStorage[sizeof(tag_sMesh)];
alignas(16) uint8_t gLightStorage[sizeof(sNodeLightParams)];
int32_t gTm[9];
tag_sRideAnimSet* gSlot;
RSSE_SCRIPT* gOthers[3];
RSSE_OBJECT* gNewObject;

// The ride set GetRideFromID's slot holds (its mesh is the head mesh EnableRideHead gets).
tag_sRideAnimSet* Set() {
    tag_sRideAnimSet* set = reinterpret_cast<tag_sRideAnimSet*>(gSetStorage);
    set->mHeads = reinterpret_cast<tag_sMesh*>(gMeshStorage);
    return set;
}

uint32_t U(int64_t value) { return static_cast<uint32_t>(value); }
int32_t I(int64_t value) { return static_cast<int32_t>(value); }

float BitsFloat(int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
uint32_t ReadScript(RSSE_SCRIPT* script) {
    ev("ReadScript", {lab(script)});
    return U(ret("ReadScript"));
}

int32_t ExtractData(RSSE_SCRIPT* script, int32_t word) {
    ev("ExtractData", {lab(script), word});
    return I(ret("ExtractData"));
}

int32_t CClock::Read() {
    ev("Read", {lab(this)});
    return I(ret("Read"));
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return U(ret("GetRand"));
}

RSSE_OBJECT* CreateObject(RSSE_SCRIPT* script) {
    ev("CreateObject", {lab(script)});
    return gNewObject;
}

void InitObject(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t a, int32_t b, int32_t e) {
    ev("InitObject", {lab(script), lab(object), a, b, e});
}

void KillLinkedObject(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t fade) { ev("KillLinkedObject", {lab(script), lab(object), fade}); }

RSSE_OBJECT* KillObject(RSSE_SCRIPT* script, RSSE_OBJECT* object) {
    ev("KillObject", {lab(script), lab(object)});
    return nullptr;
}

void SetObjParam(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t param, int32_t value) {
    ev("SetObjParam", {lab(script), lab(object), param, value});
}

int32_t StartEvent(RSSE_SCRIPT* script, int32_t type, int32_t node, int32_t a, int32_t b) {
    ev("StartEvent", {lab(script), type, node, a, b});
    return I(ret("StartEvent"));
}

tag_sRideAnimSet** GetRideFromID(uint32_t& id) {
    ev("GetRideFromID", {id});
    gSlot = Set();
    return &gSlot;
}

void FlushDeferredAnim(tag_sRideAnimSet* set, uint32_t flag) { ev("FlushDeferredAnim", {lab(set), flag}); }

uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    ev("SetCurRideAnim", {lab(set), a, anim, loop, fbits(speed), b});
    return U(ret("SetCurRideAnim"));
}

void Push(RSSE_SCRIPT* script, int32_t value) { ev("Push", {lab(script), value}); }

int32_t Pop(RSSE_SCRIPT* script) {
    ev("Pop", {lab(script)});
    return I(ret("Pop"));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    if (std::strstr(format, "%s") != nullptr) {
        va_list args;
        va_start(args, format);
        const char* text = va_arg(args, const char*);
        va_end(args);
        ev("GAME_ASSERTMSG", {condition}, {format, text});
    } else {
        ev("GAME_ASSERTMSG", {condition}, {format});
    }
}

void ExecuteTourSpecificCommand(RSSE_SCRIPT* script) { ev("ExecuteTourSpecificCommand", {lab(script)}); }
void ExecuteBumperSpecificCommand(RSSE_SCRIPT* script) { ev("ExecuteBumperSpecificCommand", {lab(script)}); }
void ExecuteCoasterSpecificCommand(RSSE_SCRIPT* script) { ev("ExecuteCoasterSpecificCommand", {lab(script)}); }

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return U(ret("FindDataNodeIndexByID"));
}

void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) { ev("EnableRideHead", {lab(mesh), index, peep}); }
void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {lab(mesh), index}); }

int32_t LIMBO_Put(RSSE_SCRIPT* script, uint32_t peep, int32_t time) {
    ev("LIMBO_Put", {lab(script), peep, time});
    return I(ret("LIMBO_Put"));
}

uint32_t LIMBO_Get(RSSE_SCRIPT* script) {
    ev("LIMBO_Get", {lab(script)});
    return U(ret("LIMBO_Get"));
}

uint32_t LIMBO_ForceGet(RSSE_SCRIPT* script) {
    ev("LIMBO_ForceGet", {lab(script)});
    return U(ret("LIMBO_ForceGet"));
}

int32_t RSSE_InitScript(const char* path, uint32_t thing, int32_t recording) {
    ev("RSSE_InitScript", {thing >> 16, recording}, {path});
    return I(ret("RSSE_InitScript"));
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {script});
    const int32_t which = I(ret("RSSE_GetScriptPointer"));
    return which >= 1 && which <= 3 ? gOthers[which - 1] : nullptr;
}

void RSSE_KillScript(int32_t handle, int32_t flag) { ev("RSSE_KillScript", {handle, flag}); }

int32_t BOUNCE_Put(RSSE_SCRIPT* script, uint32_t peep, int32_t time) {
    ev("BOUNCE_Put", {lab(script), peep, time});
    return I(ret("BOUNCE_Put"));
}

uint32_t BOUNCE_Get(RSSE_SCRIPT* script) {
    ev("BOUNCE_Get", {lab(script)});
    return U(ret("BOUNCE_Get"));
}

uint32_t BOUNCE_ForceGet(RSSE_SCRIPT* script) {
    ev("BOUNCE_ForceGet", {lab(script)});
    return U(ret("BOUNCE_ForceGet"));
}

void WALKNODE_Trigger(RSSE_SCRIPT* script, uint32_t peep, int32_t from, int32_t to, int32_t outFrom, int32_t outTo, int32_t kind, int32_t last) {
    ev("WALKNODE_Trigger", {lab(script), peep, from, to, outFrom, outTo, kind, last});
}

void WALKNODE_Release(RSSE_SCRIPT* script, uint32_t peep) { ev("WALKNODE_Release", {lab(script), peep}); }

uint32_t WALKNODE_Get(RSSE_SCRIPT* script) {
    ev("WALKNODE_Get", {lab(script)});
    return U(ret("WALKNODE_Get"));
}

void WALKNODE_Float(RSSE_SCRIPT* script, int32_t duration, int32_t a, int32_t b) { ev("WALKNODE_Float", {lab(script), duration, a, b}); }

sNodeLightParams* GetLightsourceHandle(uint32_t ride, uint32_t index) {
    ev("GetLightsourceHandle", {ride, index});
    return ret("GetLightsourceHandle") != 0 ? reinterpret_cast<sNodeLightParams*>(gLightStorage) : nullptr;
}

void EnableLight(sNodeLightParams* params) { ev("EnableLight", {lab(params)}); }
void DisableLight(sNodeLightParams* params) { ev("DisableLight", {lab(params)}); }
void SetLightColourRelative(sNodeLightParams* params, float scale) { ev("SetLightColourRelative", {lab(params), fbits(scale)}); }
void SetLightColourExplicit(sNodeLightParams* params, float red, float green, float blue) {
    ev("SetLightColourExplicit", {lab(params), fbits(red), fbits(green), fbits(blue)});
}

int32_t GetRideCentrePosition(uint32_t& ride, float& x, float& y, float& z) {
    ev("GetRideCentrePosition", {ride});
    x = BitsFloat(ret("GRCP.x"));
    y = BitsFloat(ret("GRCP.y"));
    z = BitsFloat(ret("GRCP.z"));
    return I(ret("GetRideCentrePosition"));
}

void GetRidePosition(tag_sRideAnimSet* set, float& x, float& y, float& z) {
    ev("GetRidePosition", {lab(set)});
    x = BitsFloat(ret("GRP.x"));
    y = BitsFloat(ret("GRP.y"));
    z = BitsFloat(ret("GRP.z"));
}

int32_t SCREAM_Start(int32_t handle, int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) {
    ev("SCREAM_Start", {handle, count, a, b, p, q, r});
    return I(ret("SCREAM_Start"));
}

void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }

int32_t SCREAM_SingleSad(int32_t count, int32_t p, int32_t q, int32_t r) {
    ev("SCREAM_SingleSad", {count, p, q, r});
    return I(ret("SCREAM_SingleSad"));
}

int32_t SCREAM_Single(int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) {
    ev("SCREAM_Single", {count, a, b, p, q, r});
    return I(ret("SCREAM_Single"));
}

void TriggerRepairParticleEffect(RSSE_SCRIPT* script, int32_t on) { ev("TriggerRepairParticleEffect", {lab(script), on}); }

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return I(ret("SOUND_SendEvent"));
}

void SND_DipMusic(int32_t level) { ev("SND_DipMusic", {level}); }

int32_t GetNodePos(RSSE_SCRIPT* script, JCPOINT3D* pos, JCPOINT3D* dir, int32_t node, int32_t mask) {
    (void)pos;
    ev("GetNodePos", {lab(script), lab(dir), node, mask});
    return I(ret("GetNodePos"));
}

uint32_t GetCurrentAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control) {
    ev("GetCurrentAnim", {lab(set), lab(index), control});
    *kind = U(ret("GetCurrentAnim.kind"));
    return U(ret("GetCurrentAnim"));
}

int32_t SCREAM_SetLevel(int32_t handle, int32_t a, int32_t b) {
    ev("SCREAM_SetLevel", {handle, a, b});
    return I(ret("SCREAM_SetLevel"));
}

namespace processinstruction_callee {

int32_t Labs(int32_t value) {
    ev("labs", {value});
    return value < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(value)) : value;
}

char* StrCpy(char* to, const char* from) {
    ev("strcpy", {}, {from});
    return std::strcpy(to, from);
}

char* StrCat(char* to, const char* from) {
    ev("strcat", {}, {to, from});
    return std::strcat(to, from);
}

int32_t StrCmp(const char* a, const char* b) {
    ev("strcmp", {}, {a, b});
    const int32_t d = std::strcmp(a, b);
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

int32_t Time(int32_t* out) {
    ev("time", {});
    const int32_t now = I(ret("time"));
    if (out != nullptr) *out = now;
    return now;
}

const int32_t* LocalTime(const int32_t* when) {
    ev("localtime", {*when});
    return gTm;
}

}  // namespace processinstruction_callee

extern "C" {

// The host address of a global or of one of this library's blocks by the module's name for it.
uint64_t tpw_pi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRSSEF7BE4", &gRSSEF7BE4}, {"gRSSEState", &gRSSEState}, {"gGameWorld", &gGameWorld},
        {"gClock", &gClock},         {"gSoundBanks", gSoundBanks}, {"set", Set()},
        {"mesh", gMeshStorage},      {"light", gLightStorage},    {"tm", gTm},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_pi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena's base: a Ptr32 field holds a block's address less this.
uint64_t tpw_pi_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The three scripts RSSE_GetScriptPointer answers (its queue's 1 to 3) and the object CreateObject answers.
void tpw_pi_blocks(uint64_t a, uint64_t b, uint64_t c, uint64_t object) {
    gOthers[0] = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(a));
    gOthers[1] = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(b));
    gOthers[2] = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(c));
    gNewObject = reinterpret_cast<RSSE_OBJECT*>(static_cast<uintptr_t>(object));
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_pi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_OBJECT));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mAnimFactor));
    out[n++] = static_cast<int64_t>(offsetof(RSSEState, mScripts));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    return n;
}

// ---- the function
void tpw_pi_run(uint64_t script) { ProcessInstruction(reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script))); }

}  // extern "C"
