// rsse.cpp: name proven by __sinit_rsse_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_System.h"
#include "helpers/rsseHelpers.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "LevelInit.h"
#include "Light.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

// The script objects of the ride scripts (main app, rsse.cpp range of the inventory, 0x100cc778..0x100d6bd0; unit w150): the emitters and
// sounds a script starts on its ride's nodes (CreateObject, KillObject, KillLinkedObject, SetObjParam, StartEvent, InitObject,
// ProcessObjects), the program reader and the stack (ExtractData, ReadScript, Push, Pop), the small getters and setters and two
// position getters of a ride set. An object lives in the arena (its links are Ptr32), where the Mac used LbMem_Alloc and LbMem_Free.

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte; the first two end in a newline).
const char kMalloc[] = "RSSE Malloc %ld\n";                                                      // +0x3ca
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x400
const char kCantKill[] = "RSSE: Unknown object type - can't kill it !";                         // +0x5a6
const char kCantSet[] = "RSSE: Unknown object type - can't set parameter on it !";              // +0x5d2
const char kNoSoundBank[] = "RSSE: Tried to trigger a custom effect with no registered custom sound bank";   // +0x60a
const char kCantInit[] = "RSSE: Unknown object type - can't initialise it !";                   // +0x656
const char kUnknownType[] = "RSSE: Unknown object type";                                        // +0x688
const char kReadOutside[] = "RSSE: Tried to read outside script memory";                        // +0x6a2
const char kStackError[] = "RSSE: Stack Error";                                                 // +0x6cc
const char kNotInitialised[] = "RSSE: System not initialised";                                  // +0x96c
const char kBadVariable[] = "RSSE: Bad variable reference";                                     // +0x996

const float k1024 = 1024.0f;   // 0x10264fac
const float kOne = 1.0f;       // 0x10264fb0
const float kZero = 0.0f;      // 0x10264fb4
constexpr int32_t kDead = -10000;   // 0xffffd8f0: the state of a script that went wrong

// The embedded record of the ride set's node `index` (the set's mesh keeps the embedded set in mF28; 0x14 bytes a record).
const sEmbeddedRecord& NodeRecord(const tag_sRideAnimSet* set, uint32_t index) {
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(set->mHeads->mF28);
    return embedded->mRecords[index];
}

// The position (floats at +0x30, +0x34, +0x38 of the record's 0x40-byte block) of the object's node, 0.0 each when it has no block.
void NodePosition(const RSSE_SCRIPT* script, const RSSE_OBJECT* object, float& x, float& y, float& z) {
    const uint8_t* block = NodeRecord(gRideAnimSets[script->mRideAnim], object->mNodeIndex).mBlock;
    if (block != nullptr) {
        x = ReadFloat(block, 0x30);
        y = ReadFloat(block, 0x34);
        z = ReadFloat(block, 0x38);
    } else {
        z = kZero;
        y = z;
        x = y;
    }
}

// The ride record of the script's thing (the thing's object type, the halfword at +0xe; the Mac passes it in the high half of a word).
// The row 0 of the thing table must be empty: the assertion comes first.
RideInfo* ScriptRideInfo(const RSSE_SCRIPT* script) {
    const uint16_t row = script->mThingId;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    const CObject* thing = static_cast<const CObject*>(gThingArray[row].mThing);
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{thing->mObjectType});
}

}  // namespace

// 0x100cece4 (48 bytes)
int32_t ExtractData(RSSE_SCRIPT* script, int32_t word) {
    if ((static_cast<uint32_t>(word) & 0xff000000u) == 0x40000000u) return script->mVars[static_cast<uint32_t>(word) ^ 0x40000000u];
    return static_cast<int16_t>(word);
}

// 0x100ced44 (164 bytes)
void KillLinkedObject(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t fade) {
    (void)script;
    const int32_t type = object->mType;
    if (type == 1 || type == 2) {
        PAR_SetEmitterLife(object->mHandle, -2);
    } else if (type >= 3 && type < 11) {
        if (fade != 0) SOUND_FadeSoundEffect(object->mHandle);
        else SOUND_StopSoundEffect(object->mHandle);
    } else {
        GAME_ASSERTMSG(0, kCantKill);
    }
}

// 0x100cee2c (128 bytes): the new object is pushed at the head of the list. Its fields are left as the allocator gave them.
RSSE_OBJECT* CreateObject(RSSE_SCRIPT* script) {
    CTRACE(8, kMalloc, 0x1c);
    RSSE_OBJECT* object = static_cast<RSSE_OBJECT*>(tpw::ArenaAlloc(sizeof(RSSE_OBJECT)));   // the Mac's LbMem_Alloc(0x1c), unchecked
    object->mNext = script->mObjects;
    object->mPrev = nullptr;
    if (script->mObjects.get() != nullptr) script->mObjects->mPrev = object;
    script->mObjects = object;
    gRSSEF7BE0 = gRSSEF7BE0 + 1;
    return object;
}

// 0x100ceedc (152 bytes)
RSSE_OBJECT* KillObject(RSSE_SCRIPT* script, RSSE_OBJECT* object) {
    RSSE_OBJECT* next = nullptr;
    if (object != nullptr) {
        if (object->mPrev.get() != nullptr) object->mPrev->mNext = object->mNext;
        if (object->mNext.get() != nullptr) object->mNext->mPrev = object->mPrev;
        RSSE_OBJECT* head = script->mObjects;
        next = object->mNext;
        if (object == head) script->mObjects = next;
        tpw::ArenaFree(object);   // the Mac's LbMem_Free
        gRSSEF7BE0 = gRSSEF7BE0 - 1;
    }
    return next;
}

// 0x100cefb0 (116 bytes): void in the original (r3 is not set on any path).
void SetObjParam(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t param, int32_t value) {
    (void)script;
    const int32_t type = object->mType;
    if (type >= 3 && type < 11) {
        object->mHandle = SOUND_SetParamValue(object->mHandle, param, value);
    } else if (type < 1 || type > 2) {
        GAME_ASSERTMSG(0, kCantSet);
    }
}

// 0x100cf064 (1,584 bytes): the original is a switch through the jump table 0x1029ef74 on the unsigned type.
int32_t StartEvent(RSSE_SCRIPT* script, int32_t type, int32_t node, int32_t a, int32_t b) {
    JCPOINT3D pos = {kZero, kZero, kZero};
    JCPOINT3D dir = {kZero, kZero, kZero};
    // The sound banks of the types 3 to 9 (gSoundBanks 0x102f7c94 + 4 * index).
    int32_t soundBank = 0;
    switch (static_cast<uint32_t>(type)) {
    case 1:
    case 2: {
        // An emitter; type 2 also asks for the direction and starts it with a launch vector.
        if (GetNodePos(script, &pos, type == 2 ? &dir : nullptr, node, 0x100) == 0) return 0;
        if ((a & 0x8000) != 0) {
            RideInfo* info = ScriptRideInfo(script);
            a = static_cast<int32_t>(info->GetCustomParticleEffect(a & 0x7fff));
        }
        const int32_t x = ppc_fctiwz(k1024 * pos.x);
        const int32_t y = ppc_fctiwz(k1024 * pos.y);
        const int32_t z = ppc_fctiwz(k1024 * pos.z);
        int32_t handle;
        if (type == 1) {
            handle = static_cast<int32_t>(PAR_InitEmitter(a, x, y, z));
        } else {
            // The direction is not scaled here: GetNodePos gives it times 1024 already.
            const int32_t vx = ppc_fctiwz(dir.x);
            const int32_t vy = ppc_fctiwz(dir.y);
            const int32_t vz = ppc_fctiwz(dir.z);
            handle = static_cast<int32_t>(PAR_InitEmitterLaunch(a, x, y, z, vx, vy, vz));
        }
        if (b != 1000) PAR_SetEmitterScale(handle, b);
        return handle;
    }
    case 3: soundBank = gSoundBanks[7]; break;
    case 4: soundBank = gSoundBanks[6]; break;
    case 5: soundBank = gSoundBanks[2]; break;
    case 6: soundBank = gSoundBanks[1]; break;
    case 7: soundBank = gSoundBanks[4]; break;
    case 8: soundBank = gSoundBanks[0]; break;
    case 9: soundBank = gSoundBanks[3]; break;
    case 10: {
        // The ride's own sound bank.
        RideInfo* info = ScriptRideInfo(script);
        soundBank = info->GetCustomSoundBank();
        if (soundBank == 0) {
            GAME_ASSERTMSG(0, kNoSoundBank);
            return 0;
        }
        break;
    }
    default:
        GAME_ASSERTMSG(0, kCantInit);
        return 0;
    }
    if (GetNodePos(script, &pos, nullptr, node, 0x200) == 0) return 0;
    return SOUND_SendEvent(0, soundBank, a, ppc_fctiwz(pos.x), ppc_fctiwz(pos.y), ppc_fctiwz(pos.z));
}

// The world's load (main app of SimTheme Park 2.04b3, World.cpp; unit w207): CWorld::Load, the two LOADVAR instances it needs for its own
// enums (CWorld::WorldState 0x101415b4 and CThing::ThingModel 0x101416f0) and CThing::ForceThingId (Thing.cpp). The matching save is
// src/World_save.cpp (unit w205).
//
// The load reads what Save wrote: a version word, 25 fields of the world, the control manager, the macro layer and the map, then the used list
// of the thing table. It starts by resetting the table (InitThingHeap). The used list is rebuilt from the head's index and each entry's next
// index, and for every used entry the load MAKES the thing of the entry's kind in place (operator new of the kind's size, the part of the
// class's constructor the Mac inlines here, the kind byte, the entry's index as the thing's id) before the thing's own Serialise reads it.
// The Mac's operator new is the game heap, so the host takes every thing's block from the arena (unit v5L31): the lists keep a thing's
// address in a 32-bit word.
// Every callee answers 0 for success; the first nonzero answer ends Load with 0 at once (nothing is called after it, and a thing that was
// made is not entered in its row), and a load that read everything relinks the free list, switches the sprite drawing by the world state
// and answers 1. The strings are at 0x10247498 + the offset in the comment.

// The int instance's body for the two four byte enums the world loads.
template<> int32_t LOADVAR<CWorld::WorldState>(void* file, CWorld::WorldState* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x101415b4, 236 bytes
}
template<> int32_t LOADVAR<CThing::ThingModel>(void* file, CThing::ThingModel* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x101416f0, 236 bytes
}

// 0x100cf6c8 (380 bytes)
void InitObject(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t a, int32_t b, int32_t e) {
    object->mHandle = StartEvent(script, object->mType, object->mNodeId, a, b);
    const int32_t type = object->mType;
    if (type == 1 || type == 2) {
        object->mNodeIndex = FindDataNodeIndexByID(gRideAnimSets[script->mRideAnim], 0x100, object->mNodeId);
    } else if (type >= 3 && type < 11) {
        object->mNodeIndex = FindDataNodeIndexByID(gRideAnimSets[script->mRideAnim], 0x200, object->mNodeId);
    } else {
        GAME_ASSERTMSG(0, kCantInit);
    }
    if (object->mHandle != 0) {
        object->mF18 = e;
    } else if (object != nullptr) {
        // No event: the object goes again (KillObject's steps, inline).
        if (object->mPrev.get() != nullptr) object->mPrev->mNext = object->mNext;
        if (object->mNext.get() != nullptr) object->mNext->mPrev = object->mPrev;
        RSSE_OBJECT* head = script->mObjects;
        RSSE_OBJECT* next = object->mNext;
        if (object == head) script->mObjects = next;
        tpw::ArenaFree(object);   // the Mac's LbMem_Free
        gRSSEF7BE0 = gRSSEF7BE0 - 1;
    }
}

// 0x100cf884 (832 bytes)
void ProcessObjects(RSSE_SCRIPT* script) {
    for (RSSE_OBJECT* object = script->mObjects; object != nullptr; object = object->mNext) {
        if (object->mNodeId < 0) continue;
        const int32_t type = object->mType;
        if (type == 1) {
            float x, y, z;
            NodePosition(script, object, x, y, z);
            const int32_t px = ppc_fctiwz(x);
            const int32_t py = ppc_fctiwz(y);
            const int32_t pz = ppc_fctiwz(z);
            PAR_SetEmitterPos(object->mHandle, static_cast<int32_t>(static_cast<uint32_t>(px) << 10),
                              static_cast<int32_t>(static_cast<uint32_t>(py) << 10), static_cast<int32_t>(static_cast<uint32_t>(pz) << 10));
        } else if (type == 2) {
            float x, y, z;
            NodePosition(script, object, x, y, z);
            uint32_t rideId = script->mRideAnim;
            const int32_t px = ppc_fctiwz(x);
            const int32_t py = ppc_fctiwz(y);
            const int32_t pz = ppc_fctiwz(z);
            tag_sRideAnimSet* set = *GetRideFromID(rideId);
            const float* matrix = GetDataNodeWorldMatrix(set->mHeads, object->mNodeIndex);
            s3DPoint direction;
            if (matrix != nullptr) {
                // The record is read again after the call, as the original does.
                if ((NodeRecord(set, object->mNodeIndex).mFlags & 0x10) != 0) {
                    direction.x = -matrix[8];
                    direction.y = -matrix[9];
                    direction.z = -matrix[10];
                } else {
                    direction.x = matrix[8];
                    direction.y = matrix[9];
                    direction.z = matrix[10];
                }
                NormaliseVector(&direction);
            } else {
                direction.y = kZero;
                direction.x = direction.y;
                direction.z = kOne;
            }
            PAR_SetEmitterPos(object->mHandle, static_cast<int32_t>(static_cast<uint32_t>(px) << 10),
                              static_cast<int32_t>(static_cast<uint32_t>(py) << 10), static_cast<int32_t>(static_cast<uint32_t>(pz) << 10));
            const int32_t vx = ppc_fctiwz(k1024 * direction.x);
            const int32_t vy = ppc_fctiwz(k1024 * direction.y);
            const int32_t vz = ppc_fctiwz(k1024 * direction.z);
            PAR_SetEmitterLaunchVector(object->mHandle, vx, vy, vz);
        } else if (type >= 3 && type < 11) {
            // A sound moves on its script's turn only: one tick in eight.
            if ((script->mHandle & 7) == (gRSSEState.mF04 & 7)) {
                float x, y, z;
                NodePosition(script, object, x, y, z);
                SOUND_MoveSoundEffect(object->mHandle, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
            }
        } else {
            GAME_ASSERTMSG(0, kUnknownType);
        }
    }
}

// 0x100cfbf8 (116 bytes): mState is the program counter.
uint32_t ReadScript(RSSE_SCRIPT* script) {
    const int32_t pc = script->mState;
    if (pc < 0 || pc >= script->mProgramWords) {
        GAME_ASSERTMSG(0, kReadOutside);
        script->mState = kDead;
        return 0;
    }
    script->mState = pc + 1;
    return script->mCode[pc];
}

// 0x100cfc9c (124 bytes)
void Push(RSSE_SCRIPT* script, int32_t value) {
    const int32_t index = script->mStackIndex;
    if (script->mStack.get() == nullptr || index < 0 || index >= script->mStackSize) {
        GAME_ASSERTMSG(0, kStackError);
        script->mState = kDead;
        return;
    }
    script->mStack[index] = value;
    script->mStackIndex = script->mStackIndex - 1;
}

// 0x100cfd44 (136 bytes): the index may be -1 (empty) and the last slot is never popped from.
int32_t Pop(RSSE_SCRIPT* script) {
    const int32_t index = script->mStackIndex;
    if (script->mStack.get() == nullptr || index < -1 || index >= script->mStackSize - 1) {
        GAME_ASSERTMSG(0, kStackError);
        script->mState = kDead;
        return 0;
    }
    script->mStackIndex = index + 1;
    return script->mStack[script->mStackIndex];
}

// Unit v5882: ProcessInstruction 0x100cfdf4 (12,340 bytes), the ride script interpreter of the original's rsse.cpp: one instruction of a
// script's program per call. The program's words come from ReadScript (the program counter is mState); an instruction is 0x80000000 + its
// number (0 to 0x69, a jump table), its operands follow. An operand word with the top byte 0x40 names a variable (mVars[low bits]), 0x20 a
// program address (a jump), 0x10 an offset into mSaveBlock (a text) or a value for mF74[0]; ExtractData turns an operand into a value.
// Most instructions leave their result in mCommandAnswer (0x48) and copy it into a variable when the destination word names one.

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte).
const char kHeapError[] = "RSSE: Heap Error";                                                    // +0x6de
const char kRemoteVariable[] = "RSSE: Illegal variable reference to remote script\n";            // +0x6ef
const char kBadParticleNode[] = "RSSE: Bad particle node in %s";                                 // +0x722
const char kUnknownInstruction[] = "RSSE: Unknown instruction";                                  // +0x740
const char kMissingParameter[] = "RSSE: Bad instruction - missing parameter ?";                  // +0x75a

const int32_t kDead_2 = -10000;   // 0xffffd8f0: the state of a killed script

// The fields inside RSSE_SCRIPT's byte arrays, by Mac offset (the word at 0xb4 in mFB4, 0xbc in mFB9, 0xc4 in mFC2; the halves of mFDC).
uint32_t Word(const RSSE_SCRIPT* script, size_t offset) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(script) + offset, 4);
    return value;
}

void SetWord(RSSE_SCRIPT* script, size_t offset, uint32_t value) {
    std::memcpy(reinterpret_cast<uint8_t*>(script) + offset, &value, 4);
}

int16_t Half(const RSSE_SCRIPT* script, size_t offset) {
    int16_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(script) + offset, 2);
    return value;
}

void SetHalf(RSSE_SCRIPT* script, size_t offset, int16_t value) {
    std::memcpy(reinterpret_cast<uint8_t*>(script) + offset, &value, 2);
}

bool IsVariable(uint32_t word) { return (word & 0xff000000u) == 0x40000000u; }

// The answer copied into the variable the destination word names (any other word: nothing).
void StoreAnswer(RSSE_SCRIPT* script, uint32_t word) {
    if (IsVariable(word)) script->mVars[word ^ 0x40000000u] = script->mCommandAnswer;
}

// A jump: the word's low bits when its top byte is 0x20, else the script dies.
void Jump(RSSE_SCRIPT* script, uint32_t word) {
    if ((word & 0xff000000u) == 0x20000000u)
        script->mState = static_cast<int32_t>(word ^ 0x20000000u);
    else
        script->mState = kDead_2;
}

int32_t Operand(RSSE_SCRIPT* script) { return ExtractData(script, static_cast<int32_t>(ReadScript(script))); }

// SetCurRideAnim on the script's ride (a local copy of mRideAnim goes to GetRideFromID); no ride: 0.
uint32_t PlayAnim(RSSE_SCRIPT* script, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    uint32_t id = script->mRideAnim;
    if (id == 0) return 0;
    return SetCurRideAnimDuration(*GetRideFromID(id), a, anim, loop, speed, b);
}

// GetCurrentAnim on the script's ride into *kind; no ride: 0 and *kind untouched.
uint32_t CurrentAnim(RSSE_SCRIPT* script, uint32_t* kind, uint32_t control) {
    uint32_t id = script->mRideAnim;
    if (id == 0) return 0;
    return GetCurrentAnim(*GetRideFromID(id), kind, nullptr, control);
}

// The animation's length less 300, at least 300 (signed).
void ClampAnswer(RSSE_SCRIPT* script, uint32_t length) {
    script->mCommandAnswer = static_cast<int32_t>(length - 300u);
    if (script->mCommandAnswer < 300) script->mCommandAnswer = 300;
}

// The answer in ticks at the script's speed.
int32_t Scaled(int32_t value, float speed) { return ppc_fctiwz(static_cast<float>(value) / speed); }

// The wait of ops 17, 24, 44 and 45: answers true when a wait runs (mFA0[0] set): done when the clock reached it, else the
// instruction runs again (the program counter goes back `back` words) and the tick ends.
bool Waiting(RSSE_SCRIPT* script, int32_t back) {
    if (script->mFA0[0] == 0) return false;
    if (static_cast<uint32_t>(gClock.Read()) >= script->mFA0[0]) {
        script->mFA0[0] = 0;
    } else {
        script->mState -= back;
        script->mCountdown = 0;
    }
    return true;
}

// The light of the ride's data node with the id `node` (FindDataNodeIndexByID's index, mask 0x20000).
sNodeLightParams* Light(RSSE_SCRIPT* script, int32_t node) {
    uint32_t id = script->mRideAnim;
    const uint32_t index = FindDataNodeIndexByID(*GetRideFromID(id), 0x20000, static_cast<uint32_t>(node));
    return GetLightsourceHandle(script->mRideAnim, index);
}

// The script name and the text at the save block's offset (ops 63 and 64): the path of a linked script.
void LinkedPath(RSSE_SCRIPT* script, uint32_t word, char* path) {
    processinstruction_callee::StrCpy(path, script->mName.get());
    processinstruction_callee::StrCat(path, reinterpret_cast<const char*>(script->mSaveBlock.get() + (word ^ 0x10000000u)));
}

// Ops 66 and 68: a variable of the script `handle` set (an index past its count: nothing; a negative one is written).
void SetRemote(RSSE_SCRIPT* script, int32_t handle, int32_t index, int32_t value) {
    if (handle == 0) return;
    RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);   // no NULL check in the original
    if (index >= other->mVarCount) return;
    other->mVars[index] = value;
    script->mCommandAnswer = value;
}

// Ops 67 and 69: a variable of the script `handle` read into the variable `word` names.
void GetRemote(RSSE_SCRIPT* script, int32_t handle, uint32_t word, int32_t index) {
    if (!IsVariable(word)) return;
    if (handle == 0) return;
    RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);
    if (index >= other->mVarCount) return;
    script->mCommandAnswer = other->mVars[index];
    script->mVars[word ^ 0x40000000u] = script->mCommandAnswer;
}

// Ops 97 to 102: a word of the local time (struct tm, `field` longs in; op 98 adds 1 to the month).
void TimeField(RSSE_SCRIPT* script, int32_t field, int32_t add) {
    int32_t now;
    processinstruction_callee::Time(&now);
    const int32_t* tm = processinstruction_callee::LocalTime(&now);
    script->mCommandAnswer = tm[field] + add;
    const uint32_t word = ReadScript(script);
    StoreAnswer(script, word);
}

}  // namespace

void ProcessInstruction(RSSE_SCRIPT* script) {
    using namespace processinstruction_callee;
    RSSE_SCRIPT* const s = script;
    const float speed = 0.5f + static_cast<float>(static_cast<int32_t>(s->mSpeed)) / 100.0f;
    const uint32_t op = ReadScript(s);
    if (s->mState < 0) return;
    if ((op & 0xff000000u) != 0x80000000u) {
        GAME_ASSERTMSG(0, kMissingParameter);
        s->mState = kDead_2;
        return;
    }
    const uint32_t n = op ^ 0x80000000u;
    if (n > 0x69) {
        GAME_ASSERTMSG(0, kUnknownInstruction);
        s->mState = kDead_2;
        return;
    }
    switch (n) {
    case 0:
        break;
    case 1:
        gRSSEF7BE4 = 1;
        break;
    case 2:
        gRSSEF7BE4 = 0;
        s->mCountdown = 0;
        break;
    case 3: {   // set a variable
        const uint32_t w = ReadScript(s);
        if (!IsVariable(w)) return;
        const int32_t v = Operand(s);
        s->mVars[w ^ 0x40000000u] = v;
        s->mCommandAnswer = s->mVars[w ^ 0x40000000u];
        break;
    }
    case 4:
        s->mCommandAnswer = static_cast<int32_t>(ReadScript(s));
        break;
    case 5: {   // a difference
        const uint32_t w = ReadScript(s);
        const uint32_t b = ReadScript(s);
        const uint32_t c = ReadScript(s);
        const int32_t x = ExtractData(s, static_cast<int32_t>(b));
        s->mCommandAnswer = x - ExtractData(s, static_cast<int32_t>(c));
        StoreAnswer(s, w);
        break;
    }
    case 6:
        s->mCountdown = 0;
        break;
    case 7: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = gClock.Read();
        StoreAnswer(s, w);
        break;
    }
    case 8:
    case 9: {   // an object at a node (8: the second coordinate 1000)
        RSSE_OBJECT* object = CreateObject(s);
        object->mType = Operand(s);
        object->mNodeId = Operand(s);
        const int32_t x = Operand(s);
        const int32_t y = Operand(s);
        if (n == 8) {
            InitObject(s, object, x, 1000, y);
        } else {
            const int32_t z = Operand(s);
            InitObject(s, object, x, y, z);
        }
        break;
    }
    case 10:
    case 11: {   // the objects of a group killed (11: faded)
        const int32_t group = Operand(s);
        RSSE_OBJECT* object = s->mObjects.get();
        while (object != nullptr) {
            if (group == object->mF18) {
                KillLinkedObject(s, object, n == 11 ? 1 : 0);
                RSSE_OBJECT* next = object->mNext.get();
                KillObject(s, object);
                object = next;
            } else {
                object = object->mNext.get();
            }
        }
        break;
    }
    case 12: {
        const int32_t group = Operand(s);
        const int32_t param = Operand(s);
        const int32_t value = Operand(s);
        for (RSSE_OBJECT* object = s->mObjects.get(); object != nullptr; object = object->mNext.get())
            if (group == object->mF18) SetObjParam(s, object, param, value);
        break;
    }
    case 13: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        StartEvent(s, a, b, c, 1000);
        break;
    }
    case 14: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        const int32_t d = Operand(s);
        StartEvent(s, a, b, c, d);
        break;
    }
    case 15: {
        uint32_t id = s->mRideAnim;
        if (id == 0) return;
        FlushDeferredAnim(*GetRideFromID(id), 0);
        break;
    }
    case 16: {   // play an animation; the answer is its length
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t w = ReadScript(s);
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, 0));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        s->mFA0[2] = 0xffff;
        break;
    }
    case 17: {   // play an animation and wait for it
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        if (Waiting(s, 3)) return;
        s->mAnimFactor = 1000;
        s->mFA0[0] = PlayAnim(s, a, b, 0, speed, 0) - 300u;
        s->mFA0[0] = static_cast<uint32_t>(ppc_fctiwz(static_cast<float>(s->mFA0[0]) / speed));
        if (s->mFA0[0] < 300u) s->mFA0[0] = 300;
        s->mFA0[0] += static_cast<uint32_t>(gClock.Read());
        s->mState -= 3;
        s->mCountdown = 0;
        s->mFA0[1] = 0;
        s->mFA0[2] = 0xffff;
        break;
    }
    case 18: {   // loop an animation (once per key)
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t key = static_cast<uint32_t>(a) + (static_cast<uint32_t>(b) << 16);
        if (s->mFA0[2] == key) return;
        s->mAnimFactor = 1000;
        PlayAnim(s, a, b, 1, speed, 0);
        s->mFA0[1] = 0;
        s->mFA0[2] = key;
        break;
    }
    case 19: {   // play an animation and wait until it is no longer the current one
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        uint32_t w = ReadScript(s);
        if (static_cast<int32_t>(Word(s, 0xbc)) != 0) {
            CurrentAnim(s, &w, 0);
            if (static_cast<int32_t>(w + 1) != static_cast<int32_t>(Word(s, 0xbc))) {
                s->mState -= 4;
                s->mCountdown = 0;
            } else {
                SetWord(s, 0xbc, 0);
            }
            return;
        }
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, 0));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        s->mFA0[2] = 0xffff;
        SetWord(s, 0xbc, static_cast<uint32_t>(a + 1));
        s->mState -= 4;
        break;
    }
    case 20: {   // the current animation (-1 when it ended)
        const uint32_t w = ReadScript(s);
        if (CurrentAnim(s, reinterpret_cast<uint32_t*>(&s->mCommandAnswer), 0) & 4) s->mCommandAnswer = -1;
        StoreAnswer(s, w);
        break;
    }
    case 21: {   // play an animation at a factor (per thousand)
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t w = ReadScript(s);
        const int32_t factor = Operand(s);
        s->mAnimFactor = static_cast<int16_t>(factor);
        ClampAnswer(s, PlayAnim(s, a, b, 0, (speed * static_cast<float>(factor)) / 1000.0f, 0));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>((s->mCommandAnswer * 1000) / factor + clock);
        s->mFA0[2] = 0xffff;
        break;
    }
    case 22: {
        const int32_t flag = Operand(s);
        uint32_t id = s->mRideAnim;
        if (id == 0) return;
        FlushDeferredAnim(*GetRideFromID(id), static_cast<uint32_t>(flag));
        break;
    }
    case 23: {   // 16 with a control word
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t w = ReadScript(s);
        const int32_t c = Operand(s);
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, c));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        break;
    }
    case 24: {   // 17 with a raw control word, unscaled
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t c = ReadScript(s);
        if (Waiting(s, 3)) return;
        s->mAnimFactor = 1000;
        s->mFA0[0] = PlayAnim(s, a, b, 0, speed, c) - 300u;
        if (s->mFA0[0] < 300u) s->mFA0[0] = 300;
        s->mFA0[0] += static_cast<uint32_t>(gClock.Read());
        s->mState -= 3;
        s->mCountdown = 0;
        s->mFA0[1] = 0;
        break;
    }
    case 25: {   // 18 with a raw control word, no key
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t c = ReadScript(s);
        s->mAnimFactor = 1000;
        PlayAnim(s, a, b, 1, speed, c);
        s->mFA0[1] = 0;
        break;
    }
    case 26: {   // 19 with a control word
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        uint32_t w = ReadScript(s);
        const int32_t c = Operand(s);
        if (static_cast<int32_t>(Word(s, 0xbc)) != 0) {
            CurrentAnim(s, &w, static_cast<uint32_t>(c));
            if (static_cast<int32_t>(w + 1) != static_cast<int32_t>(Word(s, 0xbc))) {
                s->mState -= 4;
                s->mCountdown = 0;
            } else {
                SetWord(s, 0xbc, 0);
            }
            return;
        }
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, c));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        SetWord(s, 0xbc, static_cast<uint32_t>(a + 1));
        s->mState -= 4;
        break;
    }
    case 27: {   // 20 with a control word
        const uint32_t w = ReadScript(s);
        const int32_t c = Operand(s);
        if (CurrentAnim(s, reinterpret_cast<uint32_t*>(&s->mCommandAnswer), static_cast<uint32_t>(c)) & 4) s->mCommandAnswer = -1;
        StoreAnswer(s, w);
        break;
    }
    case 28: {   // a random number 0 to the operand (a raw halfword)
        const uint32_t w = ReadScript(s);
        const int32_t top = static_cast<int16_t>(ReadScript(s));
        const int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand() >> 1));
        const int32_t range = top + 1;
        s->mCommandAnswer = Labs(r - (r / range) * range);
        StoreAnswer(s, w);
        break;
    }
    case 29: {   // call
        const uint32_t w = ReadScript(s);
        Push(s, static_cast<int32_t>(static_cast<uint32_t>(s->mState) | 0x20000000u));
        if ((w & 0xff000000u) == 0x20000000u) s->mState = static_cast<int32_t>(w ^ 0x20000000u);
        break;
    }
    case 30: {   // return
        const uint32_t w = static_cast<uint32_t>(Pop(s));
        if ((w & 0xff000000u) == 0x20000000u) s->mState = static_cast<int32_t>(w ^ 0x20000000u);
        break;
    }
    case 31:
        Jump(s, ReadScript(s));
        break;
    case 32: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer != 0) return;
        Jump(s, w);
        break;
    }
    case 33: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer == 0) return;
        Jump(s, w);
        break;
    }
    case 34: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer >= 0) return;
        Jump(s, w);
        break;
    }
    case 35: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer <= 0) return;
        Jump(s, w);
        break;
    }
    case 36:
        ReadScript(s);
        break;
    case 37: {
        const uint32_t w = ReadScript(s);
        if ((w & 0xff000000u) == 0x10000000u) s->mF74[0] = w ^ 0x10000000u;
        break;
    }
    case 38: {
        const uint32_t w = ReadScript(s);
        if (IsVariable(w)) s->mCommandAnswer = s->mVars[w ^ 0x40000000u];
        break;
    }
    case 39: {
        const uint32_t w = ReadScript(s);
        const int32_t d = Operand(s);
        if (IsVariable(w)) s->mCommandAnswer = s->mVars[w ^ 0x40000000u] - d;
        break;
    }
    case 40: {
        const uint32_t w = ReadScript(s);
        const int32_t d = ExtractData(s, static_cast<int32_t>(w));
        Push(s, d);
        s->mCommandAnswer = d;
        break;
    }
    case 41: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = Pop(s);
        StoreAnswer(s, w);
        break;
    }
    case 42: {   // a push on the heap (mF44 counts up, unlike Push)
        const uint32_t w = ReadScript(s);
        if (s->mStack.get() != nullptr && static_cast<int32_t>(s->mF44) >= 0 && static_cast<int32_t>(s->mF44) < s->mStackSize) {
            const int32_t d = ExtractData(s, static_cast<int32_t>(w));
            s->mStack[s->mF44] = d;
            s->mCommandAnswer = d;
            s->mF44 += 1;
        } else {
            GAME_ASSERTMSG(0, kHeapError);
        }
        break;
    }
    case 43: {   // a pop from the heap (the bound tests mStackIndex, not mF44)
        const uint32_t w = ReadScript(s);
        if (s->mStack.get() != nullptr && static_cast<int32_t>(s->mF44) > 0 && s->mStackIndex <= s->mStackSize) {
            s->mF44 -= 1;
            s->mCommandAnswer = s->mStack[s->mF44];
            StoreAnswer(s, w);
        } else {
            GAME_ASSERTMSG(0, kHeapError);
        }
        break;
    }
    case 44: {   // wait, scaled by the speed
        if (s->mFA0[0] != 0) {
            ReadScript(s);
            Waiting(s, 2);
            return;
        }
        const int32_t d = Operand(s);
        s->mFA0[0] = static_cast<uint32_t>(Scaled(d, speed) + gClock.Read());
        s->mState -= 2;
        s->mCountdown = 0;
        break;
    }
    case 45: {   // wait, unscaled
        if (s->mFA0[0] != 0) {
            ReadScript(s);
            Waiting(s, 2);
            return;
        }
        const int32_t clock = gClock.Read();
        s->mFA0[0] = static_cast<uint32_t>(clock + Operand(s));
        s->mState -= 2;
        s->mCountdown = 0;
        break;
    }
    case 46: {   // wait for the animation's end (mFA0[1])
        if (s->mFA0[1] == 0) return;
        if (static_cast<uint32_t>(gClock.Read()) >= s->mFA0[1]) {
            s->mFA0[1] = 0;
        } else {
            s->mState -= 1;
            s->mCountdown = 0;
        }
        break;
    }
    case 47: {
        const uint32_t w = ReadScript(s);
        const uint32_t v = ReadScript(s);
        if (!IsVariable(w)) return;
        s->mVars[w ^ 0x40000000u] += ExtractData(s, static_cast<int32_t>(v));
        s->mCommandAnswer = s->mVars[w ^ 0x40000000u];
        break;
    }
    case 48:
    case 49:
    case 50: {   // product, quotient, remainder (a zero divisor answers 0)
        const uint32_t w = ReadScript(s);
        const int32_t x = Operand(s);
        const int32_t y = Operand(s);
        if (n == 48)
            s->mCommandAnswer = x * y;
        else if (n == 49)
            s->mCommandAnswer = y != 0 ? x / y : 0;
        else
            s->mCommandAnswer = y != 0 ? x % y : 0;
        StoreAnswer(s, w);
        break;
    }
    case 51:
        s->mEveryFrame = static_cast<uint8_t>(ReadScript(s));
        break;
    case 52:
        s->mState = kDead_2;
        break;
    case 53:
        ExecuteTourSpecificCommand(s);
        break;
    case 54:
        ExecuteBumperSpecificCommand(s);
        break;
    case 55:
        ExecuteCoasterSpecificCommand(s);
        break;
    case 56: {   // a peep on a free head of the ride (a random free slot of mSaveWords)
        const int32_t v = Operand(s);
        int32_t* slots = s->mSaveWords.get();
        if (slots == nullptr) return;
        bool free = false;
        for (int32_t i = 0; i < s->mSaveWordCount; ++i)
            if (slots[i] == 0) free = true;
        if (!free) return;
        int32_t i;
        do {
            const int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand() >> 1));
            i = r - (r / s->mSaveWordCount) * s->mSaveWordCount;
        } while (s->mSaveWords[i] != 0);
        s->mSaveWords[i] = v;
        uint32_t id = s->mRideAnim;
        const uint32_t node = FindDataNodeIndexByID(*GetRideFromID(id), 0x80, static_cast<uint32_t>(i + 1));
        if (node == 0xffffffffu) return;
        uint32_t id2 = s->mRideAnim;
        EnableRideHead((*GetRideFromID(id2))->mHeads, node, static_cast<uint32_t>(v));
        break;
    }
    case 57: {   // the peep's heads taken off
        const int32_t v = Operand(s);
        if (s->mSaveWords.get() == nullptr) return;
        for (int32_t i = 0; i < s->mSaveWordCount; ++i) {
            if (v == s->mSaveWords[i]) {
                uint32_t id = s->mRideAnim;
                const uint32_t node = FindDataNodeIndexByID(*GetRideFromID(id), 0x80, static_cast<uint32_t>(i + 1));
                if (node != 0xffffffffu) {
                    uint32_t id2 = s->mRideAnim;
                    DisableRideHead((*GetRideFromID(id2))->mHeads, node);
                }
                s->mSaveWords[i] = 0;
            }
        }
        break;
    }
    case 58: {
        const int32_t peep = Operand(s);
        const int32_t time = Operand(s);
        s->mCommandAnswer = LIMBO_Put(s, static_cast<uint32_t>(peep), time);
        break;
    }
    case 59: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(LIMBO_Get(s));
        StoreAnswer(s, w);
        break;
    }
    case 60: {
        const uint32_t w = ReadScript(s);
        if (!IsVariable(w)) return;
        s->mCommandAnswer = static_cast<int32_t>(LIMBO_ForceGet(s));
        s->mVars[w ^ 0x40000000u] = s->mCommandAnswer;
        break;
    }
    case 61: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mLimboUsed;
        StoreAnswer(s, w);
        break;
    }
    case 62: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mLimboCount - s->mLimboUsed;
        StoreAnswer(s, w);
        break;
    }
    case 63: {   // a linked script started (it shares the thing, the fly ride and the ride)
        const uint32_t w = ReadScript(s);
        if ((w & 0xff000000u) != 0x10000000u) return;
        char path[256];
        LinkedPath(s, w, path);
        // The thing argument (a tag_IDX_TYPE word) is never set: its id half is 0.
        s->mLinked = RSSE_InitScript(path, 0, 0);
        if (s->mLinked == 0) return;
        RSSE_SCRIPT* other = RSSE_GetScriptPointer(s->mLinked);   // no NULL check in the original
        other->mF10 = s->mHandle;
        other->mThingId = s->mThingId;
        other->mFlyHandle = s->mFlyHandle;
        SetWord(other, 0xb4, Word(s, 0xb4));
        other->mRideAnim = s->mRideAnim;
        break;
    }
    case 64: {   // a second script started
        const uint32_t w = ReadScript(s);
        if ((w & 0xff000000u) != 0x10000000u) return;
        char path[256];
        LinkedPath(s, w, path);
        s->mF14 = static_cast<uint32_t>(RSSE_InitScript(path, 0, 0));
        break;
    }
    case 65:
        if (s->mLinked != 0) RSSE_KillScript(s->mLinked, 0);
        s->mLinked = 0;
        break;
    case 66: {
        const int32_t index = Operand(s);
        const int32_t value = Operand(s);
        SetRemote(s, s->mLinked, index, value);
        break;
    }
    case 67: {
        const uint32_t w = ReadScript(s);
        const int32_t index = Operand(s);
        GetRemote(s, s->mLinked, w, index);
        break;
    }
    case 68: {
        const int32_t index = Operand(s);
        const int32_t value = Operand(s);
        SetRemote(s, s->mF10, index, value);
        break;
    }
    case 69: {
        const uint32_t w = ReadScript(s);
        const int32_t index = Operand(s);
        GetRemote(s, s->mF10, w, index);
        break;
    }
    case 70:
        s->mBounceBase = static_cast<int32_t>(ReadScript(s));
        break;
    case 71:
        s->mBounceHeight = static_cast<int16_t>(Operand(s));
        break;
    case 72: {
        const int32_t peep = Operand(s);
        const int32_t time = Operand(s);
        s->mCommandAnswer = BOUNCE_Put(s, static_cast<uint32_t>(peep), time);
        break;
    }
    case 73: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(BOUNCE_Get(s));
        StoreAnswer(s, w);
        break;
    }
    case 74: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(BOUNCE_ForceGet(s));
        StoreAnswer(s, w);
        break;
    }
    case 75: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mBounceUsed;
        StoreAnswer(s, w);
        break;
    }
    case 76: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        const int32_t d = Operand(s);
        const int32_t e = Operand(s);
        const int32_t f = Operand(s);
        const int32_t g = Operand(s);
        WALKNODE_Trigger(s, static_cast<uint32_t>(a), b, c, d, e, f, g);
        break;
    }
    case 77:
        WALKNODE_Release(s, static_cast<uint32_t>(Operand(s)));
        break;
    case 78: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(WALKNODE_Get(s));
        StoreAnswer(s, w);
        break;
    }
    case 79: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        WALKNODE_Float(s, a * 1000, b, c);
        break;
    }
    case 80: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mFloatDuration;
        StoreAnswer(s, w);
        break;
    }
    case 81: {   // the float's duration made 1000 with the same progress (unsigned)
        if (s->mFloatDuration == 0) return;
        const int32_t first = gClock.Read();
        const int32_t second = gClock.Read();
        s->mFloatStart = second - static_cast<int32_t>(static_cast<uint32_t>((first - s->mFloatStart) * 1000) /
                                                       static_cast<uint32_t>(s->mFloatDuration));
        s->mFloatDuration = 1000;
        break;
    }
    case 82:
        EnableLight(Light(s, Operand(s)));
        break;
    case 83:
        DisableLight(Light(s, Operand(s)));
        break;
    case 84: {
        const int32_t node = Operand(s);
        const int32_t level = Operand(s);
        SetLightColourRelative(Light(s, node), static_cast<float>(level) / 100.0f);
        break;
    }
    case 85: {
        const int32_t node = Operand(s);
        const int32_t red = Operand(s);
        const int32_t green = Operand(s);
        const int32_t blue = Operand(s);
        sNodeLightParams* light = Light(s, node);
        SetLightColourExplicit(light, static_cast<float>(red) / 100.0f, static_cast<float>(green) / 100.0f,
                               static_cast<float>(blue) / 100.0f);
        break;
    }
    case 86: {   // the screams started at the ride's centre
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        float x, y, z;
        GetRideCentrePosition(s->mRideAnim, x, y, z);
        s->mCommandAnswer = SCREAM_Start(static_cast<int32_t>(s->mFCC[1]), a, b, s->mSpeed, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        s->mFCC[1] = static_cast<uint32_t>(s->mCommandAnswer);
        break;
    }
    case 87:
        if (s->mFCC[1] != 0) SOUND_FadeSoundEffect(static_cast<int32_t>(s->mFCC[1]));
        s->mFCC[1] = 0;
        break;
    case 88: {   // one scream at the ride's centre (a negative kind: a sad one)
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        float x, y, z;
        GetRideCentrePosition(s->mRideAnim, x, y, z);
        if (b < 0)
            SCREAM_SingleSad(a, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        else
            SCREAM_Single(a, b, s->mSpeed, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        break;
    }
    case 89: {   // the screams' level; the original keeps the callee's r3 as the handle
        const int32_t level = Operand(s);
        s->mFCC[1] = static_cast<uint32_t>(SCREAM_SetLevel(static_cast<int32_t>(s->mFCC[1]), level, s->mSpeed));
        break;
    }
    case 90: {   // a random script of the same kind (the same text at its mF74[0] offset): its handle
        const uint32_t w = ReadScript(s);
        const uint32_t v = ReadScript(s);
        int32_t count = 0;
        s->mCommandAnswer = 0;
        if ((w & 0xff000000u) != 0x10000000u) return;
        if (s->mSaveBlock.get() == nullptr) return;
        const char* name = reinterpret_cast<const char*>(s->mSaveBlock.get() + (w ^ 0x10000000u));
        for (RSSE_SCRIPT* other = gRSSEState.mScripts; other != nullptr; other = other->mNext.get())
            if (other->mSaveBlock.get() != nullptr && static_cast<int32_t>(other->mF74[0]) >= 0 &&
                StrCmp(name, reinterpret_cast<const char*>(other->mSaveBlock.get() + other->mF74[0])) == 0)
                ++count;
        if (count == 0) return;
        int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand() >> 1));
        r = Labs(r - (r / count) * count);
        const int32_t pick = r + 1;
        count = 0;
        for (RSSE_SCRIPT* other = gRSSEState.mScripts; other != nullptr; other = other->mNext.get()) {
            if (other->mSaveBlock.get() != nullptr && static_cast<int32_t>(other->mF74[0]) >= 0 &&
                StrCmp(name, reinterpret_cast<const char*>(other->mSaveBlock.get() + other->mF74[0])) == 0) {
                ++count;
                if (count == pick) {
                    s->mCommandAnswer = other->mHandle;
                    if (v & 0x40000000u) s->mVars[v ^ 0x40000000u] = s->mCommandAnswer;   // only bit 30 is tested
                }
            }
        }
        break;
    }
    case 91: {   // a variable of any script (0 when there is none)
        const uint32_t w = ReadScript(s);
        const int32_t handle = Operand(s);
        const int32_t index = Operand(s);
        s->mCommandAnswer = 0;
        RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);
        if (other != nullptr && index >= 0 && index < other->mVarCount) s->mCommandAnswer = other->mVars[index];
        StoreAnswer(s, w);
        break;
    }
    case 92: {   // a variable of any script set
        const int32_t handle = Operand(s);
        const int32_t index = Operand(s);
        const int32_t value = Operand(s);
        RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);
        if (other == nullptr) return;
        if (index >= 0 && index < other->mVarCount) {
            other->mVars[index] = value;
            s->mCommandAnswer = value;
        } else {
            GAME_ASSERTMSG(0, kRemoteVariable);
        }
        break;
    }
    case 93: {   // the repair effect and its sounds (off: the scream of variable 5)
        const int32_t on = Operand(s);
        TriggerRepairParticleEffect(s, on);
        float x, y, z;
        if (on != 0) {
            GetRidePosition(*GetRideFromID(s->mRideAnim), x, y, z);
            SOUND_SendEvent(0, gSoundBanks[2], 0x6f, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
            SOUND_SendEvent(0, gSoundBanks[2], 0xd3, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        } else {
            if (s->mVarCount <= 5) return;
            const int32_t kind = s->mVars[5];
            GetRidePosition(*GetRideFromID(s->mRideAnim), x, y, z);
            SCREAM_Single(kind, 1, 1, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        }
        break;
    }
    case 94: {
        const uint32_t w = ReadScript(s);
        Operand(s);
        s->mCommandAnswer = 0;
        StoreAnswer(s, w);
        break;
    }
    case 95: {   // a timer (the word at 0xc4) set
        const int32_t d = Operand(s);
        SetWord(s, 0xc4, static_cast<uint32_t>(d + gClock.Read()));
        break;
    }
    case 96: {   // what is left of it, at least 0
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(Word(s, 0xc4)) - gClock.Read();
        if (s->mCommandAnswer < 0) s->mCommandAnswer = 0;
        StoreAnswer(s, w);
        break;
    }
    case 97:
        TimeField(s, 5, 0);   // tm_year
        break;
    case 98:
        TimeField(s, 4, 1);   // tm_mon + 1
        break;
    case 99:
        TimeField(s, 3, 0);   // tm_mday
        break;
    case 100:
        TimeField(s, 2, 0);   // tm_hour
        break;
    case 101:
        TimeField(s, 1, 0);   // tm_min
        break;
    case 102:
        TimeField(s, 0, 0);   // tm_sec
        break;
    case 103:
        s->mReverb = Operand(s);
        break;
    case 104: {
        const int32_t level = Operand(s);
        s->mFB9[0] = static_cast<uint8_t>(level);
        SND_DipMusic(level);
        break;
    }
    case 105: {   // two particle nodes (the halves at 0xdc and 0xde), checked; two more operands read and unused
        SetHalf(s, 0xdc, static_cast<int16_t>(Operand(s)));
        SetHalf(s, 0xde, static_cast<int16_t>(Operand(s)));
        Operand(s);
        Operand(s);
        JCPOINT3D position;
        GAME_ASSERTMSG(GetNodePos(s, &position, nullptr, Half(s, 0xdc), 0x100), kBadParticleNode, s->mName.get());
        GAME_ASSERTMSG(GetNodePos(s, &position, nullptr, Half(s, 0xde), 0x100), kBadParticleNode, s->mName.get());
        break;
    }
    }
}

// 0x100d2e60 (44 bytes): the floats +0x40, +0x44, +0x48 of the node the ride's header calls mF78.
void GetRidePosition(tag_sRideAnimSet* set, float& x, float& y, float& z) {
    const uint8_t* node = reinterpret_cast<const uint8_t*>(set->mHeads->mHeader->mF78.get());
    x = ReadFloat(node, 0x40);
    y = ReadFloat(node, 0x44);
    z = ReadFloat(node, 0x48);
}

// The life of a ride script (main app, rsse.cpp range of the inventory; unit w169): RSSE_InitScript reads a script file into a new
// RSSE_SCRIPT record (the tables, the program, the saved block, the name), puts it at the head of the list and starts the ride's
// effect emitter and sound; KillScript gives one record and everything it owns back; RSSE_KillScript starts the ride's closing effect,
// kills the script and the scripts linked to it. The record and its tables live in the arena (the Ptr32 links), where the Mac used
// LbMem_Alloc and LbMem_Free; the callees outside the unit (the file reads, the particle and sound calls, the ride database) are not here.

namespace {

const char kMallocZero[] = "RSSE: Tried to MALLOC 0 - Stoopid !\n";                                  // +0x3db
const char kModeRead[] = "rb";                                                                       // +0x822
const char kBadVersion[] = "RSSE: Script & Script interpreter are different versions";               // +0x825
const char kZeroLength[] = "RSSE: Zero length script !\n";                                           // +0x85e
const char kNotScript[] = "RSSE: Not a RSSE script file !";                                          // +0x87a
const char kCannotLoad[] = "RSSE: Cannot load Script file";                                          // +0x899
const char kNeedInit[] = "RSSE: Need to initialise before you can start a script\n";                 // +0x8b7
const char kCantKillSystem[] = "RSSE: Cant kill a script - system is not initialised\n";             // +0x8ef
const char kCantKillObject[] = "RSSE: Unknown object type - can't kill it !";                        // +0x5a6

const float k512 = 512.0f;     // 0x10264f5c
const float k256 = 256.0f;     // 0x10264f58
const float kHalf = 0.5f;      // 0x10264fb8

constexpr uint32_t kScriptTag = 0x45535352;   // "RSSE" read big-endian

// The original's inline malloc: the trace, a size of 0 is an assertion and NULL, else LbMem_Alloc (the arena here; the result is unchecked).
void* ScriptMalloc(uint32_t size) {
    CTRACE(8, kMalloc, size);
    if (size == 0) {
        GAME_ASSERTMSG(0, kMallocZero);
        return nullptr;
    }
    return tpw::ArenaAlloc(size);
}

void ReadWord(bfl::FileSystem::CFile* file, void* dest) {
    TPFile_Read(dest, 4, 1, file);
    SamsUtilities::_USwapBlock32(static_cast<uint32_t*>(dest), 1);
}

// The ride record of the script's thing (the thing's object type, the halfword at +0xe). The row 0 of the thing table must be empty.
RideInfo* ThingRideInfo(uint16_t row) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    const CObject* thing = static_cast<const CObject*>(gThingArray[row].mThing);
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{thing->mObjectType});
}

}  // namespace

// 0x100d2ecc (1,004 bytes): `flag` bit 2 (value 4) also turns the script's ride heads off. Answers the script that followed it in the list.
RSSE_SCRIPT* KillScript(RSSE_SCRIPT* script, int32_t flag) {
    RSSE_SCRIPT* next = nullptr;
    if (script == nullptr) return next;
    if (script->mFB9[0] != 0) SND_DipMusic(0);
    if (script->mRideAnim != 0) FLY_RemoveBase(script->mFlyHandle);
    if (script->mFCC[0] != 0) {
        PAR_SetEmitterLife(static_cast<int32_t>(script->mFCC[0]), -2);
        script->mFCC[0] = 0;
    }
    if (script->mFCC[1] != 0) SOUND_FadeSoundEffect(static_cast<int32_t>(script->mFCC[1]));
    if (script->mSaveWords.get() != nullptr) {
        for (int32_t i = 0; i < script->mSaveWordCount; i = i + 1) {
            if (script->mSaveWords[i] != 0) {
                if ((static_cast<uint32_t>(flag) & 4u) != 0) {
                    const uint32_t index = FindDataNodeIndexByID(gRideAnimSets[script->mRideAnim], 0x80, static_cast<uint32_t>(i + 1));
                    if (index != 0xffffffffu) DisableRideHead(gRideAnimSets[script->mRideAnim]->mHeads, index);
                }
                script->mSaveWords[i] = 0;
            }
        }
    }
    if (script->mWalk.get() != nullptr) {
        for (int32_t i = 0; i < script->mWalkCount; i = i + 1) {
            RSSE_WALK* walk = &script->mWalk[i];
            if (walk->mKind == 4 && walk->mState == 2) {
                if ((static_cast<uint32_t>(flag) & 4u) != 0) {
                    const uint32_t index = FindDataNodeIndexByID(gRideAnimSets[script->mRideAnim], 0x80, static_cast<uint32_t>(static_cast<int32_t>(walk->mTo)));
                    if (index != 0xffffffffu) DisableRideHead(gRideAnimSets[script->mRideAnim]->mHeads, index);
                }
                walk->mState = 0;
            }
        }
    }
    gRSSEState.mScriptCount = gRSSEState.mScriptCount - 1;
    if (script->mPrev.get() != nullptr) script->mPrev->mNext = script->mNext;
    if (script->mNext.get() != nullptr) script->mNext->mPrev = script->mPrev;
    if (script->mCode.get() != nullptr) tpw::ArenaFree(script->mCode.get());
    if (script->mVars.get() != nullptr) tpw::ArenaFree(script->mVars.get());
    if (script->mStack.get() != nullptr) tpw::ArenaFree(script->mStack.get());
    if (script->mLimbo.get() != nullptr) tpw::ArenaFree(script->mLimbo.get());
    if (script->mBounce.get() != nullptr) tpw::ArenaFree(script->mBounce.get());
    if (script->mWalk.get() != nullptr) tpw::ArenaFree(script->mWalk.get());
    if (script->mSaveWords.get() != nullptr) tpw::ArenaFree(script->mSaveWords.get());
    if (script->mSaveBlock.get() != nullptr) tpw::ArenaFree(script->mSaveBlock.get());
    if (script->mName.get() != nullptr) tpw::ArenaFree(script->mName.get());

    // The script's objects: the emitters and sounds stop, each record goes (KillObject inline).
    RSSE_OBJECT* object = script->mObjects;
    while (object != nullptr) {
        const int32_t type = object->mType;
        if (type == 1 || type == 2) {
            PAR_SetEmitterLife(object->mHandle, -2);
        } else if (type >= 3 && type < 11) {
            SOUND_StopSoundEffect(object->mHandle);
        } else {
            GAME_ASSERTMSG(0, kCantKillObject);
        }
        if (object->mPrev.get() != nullptr) object->mPrev->mNext = object->mNext;
        if (object->mNext.get() != nullptr) object->mNext->mPrev = object->mPrev;
        RSSE_OBJECT* following = object->mNext;
        if (object == script->mObjects.get()) script->mObjects = following;
        tpw::ArenaFree(object);
        gRSSEF7BE0 = gRSSEF7BE0 - 1;
        object = following;
    }
    next = script->mNext;
    if (script == gRSSEState.mScripts) gRSSEState.mScripts = next;
    tpw::ArenaFree(script);
    return next;
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3): RSSE_Init (0x100d32e8, 212 bytes) and SetAnimationDetail (0x100c7c68,
// 12 bytes). The script system starts here: its state words are set, and a checksum of the script language's op table is
// made (RSSE_InitScript compares a script file's header with it).

// The script ops, as pairs {name, argument count as a digit} (initialised data at 0x1029ec1c: 212 char pointers, 106 ops, ended by a
// pointer to an empty text). Nothing else in the translated code reads it yet. Its characters are all ASCII, so the signed char
// sum below equals an unsigned one.
static const char* const kRSSEOpTable[] = {
    "NOP", "0",
    "CRIT_LOCK", "0",
    "CRIT_UNLOCK", "0",
    "COPY", "2",
    "SETLV", "1",
    "SUB", "3",
    "ENDSLICE", "0",
    "GETTIME", "1",
    "ADDOBJ", "4",
    "ADDOBJ_EXT", "5",
    "KILLOBJ", "1",
    "FADEOBJ", "1",
    "SETOBJPARAM", "3",
    "EVENT", "3",
    "EVENT_EXT", "4",
    "FLUSHANIM", "0",
    "TRIGANIM", "3",
    "WAITANIM", "2",
    "LOOPANIM", "2",
    "TRIGWAITANIM", "3",
    "GETANIM", "1",
    "TRIGANIMSPEED", "4",
    "FLUSHANIM_CH", "1",
    "TRIGANIM_CH", "4",
    "WAITANIM_CH", "3",
    "LOOPANIM_CH", "3",
    "TRIGWAITANIM_CH", "4",
    "GETANIM_CH", "2",
    "RAND", "2",
    "JSR", "1",
    "RETURN", "0",
    "BRANCH", "1",
    "BRANCH_Z", "1",
    "BRANCH_NZ", "1",
    "BRANCH_NV", "1",
    "BRANCH_PV", "1",
    "DBGMSG", "1",
    "NAME", "1",
    "TEST", "1",
    "CMP", "2",
    "PUSH", "1",
    "POP", "1",
    "HUSH", "1",
    "HOP", "1",
    "WAIT", "1",
    "WAITABS", "1",
    "WAIT4ANIM", "0",
    "ADD", "2",
    "MULT", "3",
    "DIV", "3",
    "MOD", "3",
    "TURBO", "1",
    "END", "0",
    "TOUR", "2",
    "BUMP", "2",
    "COAST", "2",
    "ADDHEAD", "1",
    "DELHEAD", "1",
    "LIMBO", "2",
    "UNLIMBO", "1",
    "FORCEUNLIMBO", "1",
    "INLIMBO", "1",
    "LIMBOSPACE", "1",
    "SPAWNCHILD", "1",
    "SPAWNSOUND", "1",
    "REMOVECHILD", "0",
    "SETVARINCHILD", "2",
    "GETVARINCHILD", "2",
    "SETVARINPARENT", "2",
    "GETVARINPARENT", "2",
    "BOUNCESETNODE", "1",
    "BOUNCESETBASE", "1",
    "BOUNCE", "2",
    "UNBOUNCE", "1",
    "FORCEUNBOUNCE", "1",
    "BOUNCING", "1",
    "WALKON", "7",
    "WALKOFF", "1",
    "WALKGET", "1",
    "WALKST_FLOAT", "3",
    "WALKFLOATSTAT", "1",
    "WALKFLOATSTOP", "0",
    "ENABLELIGHT", "1",
    "DISABLELIGHT", "1",
    "SETLIGHT", "2",
    "COLOURLIGHT", "4",
    "STARTSCREAM", "2",
    "STOPSCREAM", "0",
    "SINGLESCREAM", "2",
    "SCREAMLEVEL", "1",
    "FINDSCRIPTRAND", "2",
    "GETREMOTEVAR", "3",
    "SETREMOTEVAR", "3",
    "REPAIREFFECT", "1",
    "GETCUSTPTCLCODE", "2",
    "SETTIMER", "1",
    "GETTIMER", "1",
    "YEAR", "1",
    "MONTH", "1",
    "DAY", "1",
    "HOUR", "1",
    "MIN", "1",
    "SEC", "1",
    "SETREVERB", "1",
    "DIPMUSIC", "1",
    "SPARK", "4",
    "",
};

uint32_t gRSSEScriptChecksum;   // 0x102f7be8
uint32_t gRSSEF7BE4;            // 0x102f7be4

// The first call starts the system: a flag, two words, the list of live scripts (empty) and the checksum (the sum of every
// character of every text of the op table, as signed chars). A second call (before RSSE_Close) only asserts.
void RSSE_Init() {
    if (gRSSEState.mInitialised == 0) {
        gRSSEState.mInitialised = 1;
        gRSSEState.mF08 = 1;
        gRSSEState.mScripts = nullptr;
        gRSSEState.mScriptCount = 0;
        gRSSEState.mF04 = 0;
        gRSSEScriptChecksum = 0;
        gRSSEF7BE4 = 0;
        for (int32_t entry = 0; kRSSEOpTable[entry][0] != '\0'; ++entry) {
            for (uint32_t i = 0; i < std::strlen(kRSSEOpTable[entry]); ++i) {
                gRSSEScriptChecksum += static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(kRSSEOpTable[entry][i])));
            }
        }
    } else {
        GAME_ASSERTMSG(0, "RSSE: Tried to Initialise twice - doh !");
    }
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w132: the ride script system's tick (RSSE_Tick, 0x100d33dc, 632 bytes), the
// render tick (RSSE_RenderTick, 0x100d3674, 108 bytes), the two list lookups (RSSE_Getvar 0x100d6c58, 124 bytes, and RSSE_GetScriptPointer
// 0x100d668c, 124 bytes) and the reverb (RSSE_SetReverbOnEax, 0x100d6e7c, 140 bytes). The scripts are a doubly linked list headed by
// gRSSEState.mScripts. The instruction interpreter, the object and walk-node ticks, the kill and the fly, coaster and animation setters
// are untranslated (rsse.h declares them; the launcher answers zero).

// The texts sit in the program's string pool at 0x1023eeae + the offset in the comment.
namespace {
constexpr const char* kNeedInit_2 = "RSSE: Need to initialise before you can tick\n";       // +0x7ae
constexpr const char* kNotInitialised_2 = "RSSE: System not initialised";                    // +0x96c
constexpr const char* kBadScriptId = "Bad ScriptID";                                        // +0x989
constexpr const char* kBadVariable_2 = "RSSE: Bad variable reference";                        // +0x996
constexpr const char* kRowZero_2 = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x400

uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}
}  // namespace

// Without the system on it asserts and does nothing. Else the tick count goes up by one and every script of the list is visited, in order:
// a script runs when its every-frame byte is set or its handle's low three bits equal the tick count's (so the others run every eighth tick).
// Running a script: instructions (the countdown is re-read from the record each time and only counts down when ProcessInstruction left
// gRSSEF7BE4 at 0), then, each when its handle is set, the ride's animation speed, the fly's time and speed, the coaster's speed, the ride
// object's performance and the linked script's speed copy. Every script then gets ProcessObjects, and a negative state kills it (the next
// link is read first). Answers gRSSEF7BE0 whatever happened.
int32_t RSSE_Tick() {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNeedInit_2);
    } else {
        gRSSEState.mF04 = gRSSEState.mF04 + 1;
        RSSE_SCRIPT* script = gRSSEState.mScripts;
        while (script != nullptr) {
            if (script->mEveryFrame != 0 || (script->mHandle & 7) == (gRSSEState.mF04 & 7)) {
                if (script->mCode != 0) {
                    gRSSEF7BE4 = 0;
                    script->mCountdown = script->mInstructions;
                    while (script->mCountdown > 0 && script->mState >= 0) {
                        ProcessInstruction(script);
                        if (gRSSEF7BE4 == 0) script->mCountdown = script->mCountdown - 1;
                    }
                }
                if (script->mRideAnim != 0) {
                    const float speed = static_cast<float>(script->mSpeed);
                    const float factor = static_cast<float>(script->mAnimFactor);
                    UpdateRideAnimSpeedAllChannels(gRideAnimSets[script->mRideAnim], ((0.5f + speed / 100.0f) * factor) / 1000.0f);
                }
                if (script->mFlyHandle != 0) {
                    if (script->mVarCount > 3) {
                        const int32_t time = static_cast<int32_t>(static_cast<uint32_t>(script->mVars[3]) * 1000u);
                        FLY_SetRunTime(script->mFlyHandle, time);
                    }
                    FLY_SetRunSpeed(script->mFlyHandle, script->mSpeed);
                }
                if (script->mCoaster != 0) CoasterSetSpeed(script->mCoaster, static_cast<uint32_t>(static_cast<int32_t>(script->mSpeed)));
                if (script->mThingId != 0) {
                    const uint16_t id = script->mThingId;
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero_2);
                    void* thing = gThingArray[id].mThing;
                    if (thing != nullptr) {
                        const uint32_t ride = WordAt(thing, 0x24);
                        if (ride != 0) BMP_SetRidePerformance(static_cast<int32_t>(ride), script->mSpeed);
                    }
                }
                if (script->mLinked != 0) {
                    const int16_t speed = script->mSpeed;
                    RSSE_GetScriptPointer(script->mLinked)->mSpeed = speed;
                }
            }
            ProcessObjects(script);
            if (script->mState < 0) {
                RSSE_SCRIPT* next = script->mNext;
                RSSE_KillScript(script->mHandle, 0);
                script = next;
            } else {
                script = script->mNext;
            }
        }
    }
    return gRSSEF7BE0;
}

// Without the system on it asserts (the same text as the tick) and does nothing; else every script gets its bounce tick, then its walk-node tick.
void RSSE_RenderTick() {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNeedInit_2);
    } else {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
            BOUNCE_Tick(script);
            WALKNODE_Tick(script);
        }
    }
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3): the script system's shutdown. RSSE_Init (0x100d32e8, untranslated)
// sets the state; the state and the malloc count are defined here with the first function that uses them.

RSSEState gRSSEState = {};        // 0x102f7bec
int32_t gRSSEMallocCount = 0;     // 0x102f7bdc

// Kills every live script (KillScript answers the next one), forgets the list, marks the system closed (also when it was not
// open) and complains when the scripts left malloced blocks behind (the assert only reports).
void RSSE_Close() {
    if (gRSSEState.mInitialised != 0) {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = KillScript(script, 0)) {
        }
        gRSSEState.mScripts = nullptr;
    }
    gRSSEState.mInitialised = 0;
    if (gRSSEMallocCount != 0) GAME_ASSERTMSG(0, "RSSE: Arrooga - Something malloced but not freed - bad boy - fix it !");
}

// 0x100d37b4 (2,756 bytes): the thing id is the high halfword of `thing`. Answers the new script's handle, 0 when none was made (the
// stream ending early is not checked: a short read leaves the rest of the field as it was, zero).
int32_t RSSE_InitScript(const char* path, uint32_t thing, int32_t recording) {
    int32_t handle = 0;
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNeedInit);
        return handle;
    }
    bfl::FileSystem::CFile* file = TPFile_Open(path, kModeRead);
    if (file == nullptr) {
        GAME_ASSERTMSG(0, kCannotLoad);
        return handle;
    }
    uint32_t word;
    ReadWord(file, &word);
    if (word != kScriptTag) {
        GAME_ASSERTMSG(0, kNotScript);
        TPFile_Close(file);
        return handle;
    }
    ReadWord(file, &word);
    if (word != gRSSEScriptChecksum) GAME_ASSERTMSG(0, kBadVersion);

    CTRACE(8, kMalloc, static_cast<uint32_t>(sizeof(RSSE_SCRIPT)));
    RSSE_SCRIPT* script = static_cast<RSSE_SCRIPT*>(tpw::ArenaAlloc(sizeof(RSSE_SCRIPT)));
    std::memset(script, 0, sizeof(RSSE_SCRIPT));
    gRSSEState.mScriptCount = gRSSEState.mScriptCount + 1;
    script->mNext = gRSSEState.mScripts;
    script->mPrev = nullptr;
    if (gRSSEState.mScripts != nullptr) gRSSEState.mScripts->mPrev = script;
    gRSSEState.mScripts = script;

    ReadWord(file, &script->mVarCount);
    if (script->mVarCount != 0) {
        script->mVars = static_cast<int32_t*>(ScriptMalloc(static_cast<uint32_t>(script->mVarCount) << 2));
        std::memset(script->mVars.get(), 0, static_cast<uint32_t>(script->mVarCount) << 2);
    }
    ReadWord(file, &script->mStackSize);
    if (script->mStackSize != 0) script->mStack = static_cast<int32_t*>(ScriptMalloc(static_cast<uint32_t>(script->mStackSize) << 2));
    ReadWord(file, &script->mInstructions);
    ReadWord(file, &script->mLimboCount);
    if (script->mLimboCount != 0) {
        script->mLimbo = static_cast<RSSE_LIMBO*>(ScriptMalloc(static_cast<uint32_t>(script->mLimboCount) << 3));
        std::memset(script->mLimbo.get(), 0, static_cast<uint32_t>(script->mLimboCount) << 3);
    }
    ReadWord(file, &script->mBounceCount);
    if (script->mBounceCount != 0) {
        script->mBounce = static_cast<RSSE_BOUNCE*>(ScriptMalloc(static_cast<uint32_t>(script->mBounceCount) << 4));
        std::memset(script->mBounce.get(), 0, static_cast<uint32_t>(script->mBounceCount) << 4);
    }
    ReadWord(file, &script->mWalkCount);
    if (script->mWalkCount != 0) {
        script->mWalk = static_cast<RSSE_WALK*>(ScriptMalloc(static_cast<uint32_t>(script->mWalkCount) << 5));
        std::memset(script->mWalk.get(), 0, static_cast<uint32_t>(script->mWalkCount) << 5);
    }
    // Four words the original reads and drops.
    ReadWord(file, &word);
    ReadWord(file, &word);
    ReadWord(file, &word);
    ReadWord(file, &word);

    ReadWord(file, &script->mProgramWords);
    if (script->mProgramWords == 0) {
        GAME_ASSERTMSG(0, kZeroLength);
    } else {
        script->mCode = static_cast<uint32_t*>(ScriptMalloc(static_cast<uint32_t>(script->mProgramWords) << 2));
        TPFile_Read(script->mCode.get(), 4, static_cast<uint32_t>(script->mProgramWords), file);
        SamsUtilities::_USwapBlock32(script->mCode.get(), script->mProgramWords);
    }
    ReadWord(file, &script->mSaveBlockBytes);
    if (static_cast<uint32_t>(script->mSaveBlockBytes) != 0) {
        script->mSaveBlock = static_cast<uint8_t*>(ScriptMalloc(static_cast<uint32_t>(script->mSaveBlockBytes)));
        TPFile_Read(script->mSaveBlock.get(), static_cast<uint32_t>(script->mSaveBlockBytes), 1, file);
    }

    script->mStackIndex = script->mStackSize - 1;
    script->mHandle = gRSSEState.mF08;
    gRSSEState.mF08 = gRSSEState.mF08 + 1;
    script->mSpeed = 50;
    script->mBounceBase = 1;
    script->mF74[0] = 0xffffffffu;
    script->mFloatA = 1000;
    script->mFloatB = 1000;
    script->mFA0[2] = 0xffff;
    script->mReverb = -1;
    script->mFCC[2] = 0;
    script->mFCC[1] = 0;
    script->mAnimFactor = 1000;
    script->mCoaster = 0;
    script->mFE6[0] = 0xff;
    script->mFE6[1] = 0xff;

    // The name is the path up to and including its last ':' (the path's first character alone when there is none).
    char directory[258];
    std::strcpy(directory, path);
    size_t length = std::strlen(directory);
    while (static_cast<int32_t>(length) > 0 && directory[length] != ':') length = length - 1;
    directory[length + 1] = '\0';
    const uint32_t nameSize = static_cast<uint32_t>(std::strlen(directory)) + 1;
    script->mName = static_cast<char*>(ScriptMalloc(nameSize));
    std::strcpy(script->mName.get(), directory);

    script->mThingId = static_cast<uint16_t>(thing >> 16);
    handle = script->mHandle;
    if (script->mThingId != 0) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        script->mRideAnim = static_cast<const CObject*>(gThingArray[script->mThingId].mThing)->mF1C;
    }
    if (script->mThingId != 0 && script->mRideAnim != 0) {
        // The heads of the ride: its nodes with the mask 0x80, numbered from 1.
        int32_t heads = 0;
        while (FindDataNodeIndexByID(gRideAnimSets[script->mRideAnim], 0x80, static_cast<uint32_t>(heads + 1)) != 0xffffffffu) heads = heads + 1;
        if (heads != 0) {
            const uint32_t bytes = static_cast<uint32_t>(heads) << 2;
            script->mSaveWords = static_cast<int32_t*>(ScriptMalloc(bytes));
            std::memset(script->mSaveWords.get(), 0, bytes);
        }
        script->mSaveWordCount = heads;
    }
    if (recording == 0 && script->mThingId != 0) {
        const int32_t effect = ThingRideInfo(script->mThingId)->Value(kRideSlot_CreateParticleEffect).mInteger;
        if (effect != 0) {
            sAlignedBox box;
            if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
                const float x = (box.mMin[0] + box.mMax[0]) * kHalf;
                const float z = (box.mMin[2] + box.mMax[2]) * kHalf;
                const uint8_t* node = reinterpret_cast<const uint8_t*>(gRideAnimSets[script->mRideAnim]->mHeads->mHeader->mF78.get());
                const float y = ReadFloat(node, 0x44);
                const int32_t emitter = static_cast<int32_t>(PAR_InitEmitter(effect, ppc_fctiwz(k1024 * x), ppc_fctiwz(k1024 * y),
                                                                              ppc_fctiwz(k1024 * z)));
                sAlignedBox area;
                if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &area) != 0) {
                    const float dy = area.mMax[1] - area.mMin[1];
                    const float dx = area.mMax[0] - area.mMin[0];
                    const float dz = area.mMax[2] - area.mMin[2];
                    PAR_SetEmitterRandomPos(emitter, ppc_fctiwz(k512 * dx), ppc_fctiwz(k256 * dy), ppc_fctiwz(k512 * dz));
                    script->mFCC[2] = static_cast<uint32_t>(SOUND_AddObject(x, y, z, (dx + dz) * kHalf, dy));
                }
            }
        }
    }
    TPFile_Close(file);
    return handle;
}

// 0x100d42d0 (696 bytes): `flag` bit 1 (value 2) starts the ride's closing effect at its centre. The scripts linked to this one die with it.
void RSSE_KillScript(int32_t handle, int32_t flag) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kCantKillSystem);
        return;
    }
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(handle);
    if (script == nullptr) return;
    if (script->mThingId != 0) {
        const int32_t effect = ThingRideInfo(script->mThingId)->Value(kRideSlot_DestroyParticleEffect).mInteger;
        if (effect != 0 && (static_cast<uint32_t>(flag) & 2u) != 0) {
            float x = kZero;
            float y = kZero;
            float z = kZero;
            sAlignedBox box;
            if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
                x = (box.mMin[0] + box.mMax[0]) * kHalf;
                z = (box.mMin[2] + box.mMax[2]) * kHalf;
                const uint8_t* node = reinterpret_cast<const uint8_t*>(gRideAnimSets[script->mRideAnim]->mHeads->mHeader->mF78.get());
                y = ReadFloat(node, 0x44);
            }
            const int32_t emitter = static_cast<int32_t>(PAR_InitEmitter(effect, ppc_fctiwz(k1024 * x), ppc_fctiwz(k1024 * y),
                                                                          ppc_fctiwz(k1024 * z)));
            sAlignedBox area;
            if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &area) != 0) {
                const float dy = area.mMax[1] - area.mMin[1];
                const float dx = area.mMax[0] - area.mMin[0];
                const float dz = area.mMax[2] - area.mMin[2];
                PAR_SetEmitterRandomPos(emitter, ppc_fctiwz(k512 * dx), ppc_fctiwz(k256 * dy), ppc_fctiwz(k512 * dz));
            }
        }
        if (script->mFCC[2] != 0) {
            SOUND_DelObject(script->mFCC[2]);
            script->mFCC[2] = 0;
        }
    }
    if (script->mF14 != 0) KillScript(RSSE_GetScriptPointer(static_cast<int32_t>(script->mF14)), flag);
    if (script->mLinked != 0) KillScript(RSSE_GetScriptPointer(script->mLinked), flag);
    if (script->mF10 != 0) RSSE_GetScriptPointer(script->mF10)->mLinked = 0;
    KillScript(script, flag);
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w166: RSSE_Save, the ride scripts' save, and rsse.cpp's own instances of the
// SamsUtilities swap templates it uses.
//
// The file holds "RSSE", 0x14, the state's five words and, when the system is initialised, "PAD_" five times, the script count, 0xf4 and then
// each script of the list: its 0xf4 byte record (a swapped copy), the tables as a byte count and the table (program, stack, variables, a raw
// byte block, limbo, bounce, walk, a word table, the name) and "OBJ ", the object count, 0x1c and the script's objects (0x1c byte records).
// The file's words are little-endian. The Mac swaps every word (the main app's swap templates swap unconditionally), a little-endian host
// writes the same bytes by not swapping: the swaps are real only where the host's order differs from the file's (or when a test asks).
//
// Host design: the record's pointer words are the Ptr32 words (arena offsets; 0 for NULL), written like the Mac's addresses (the file holds
// addresses, which nothing reads back). The state's fifth word, the list head, is a native pointer on the host: its Ptr32 encoding is written.
// The thing id at +0xac is swapped by tag_IDX_TYPE::Byteswap, which ALWAYS swaps (it is an exchange of two bytes): it is called only where
// the other swaps are real, else it would put the big-endian image at +0xac of a little-endian host's file.
//
// Unit w167 adds RSSE_Load and RSSE_WALK::Byteswap here, the save's mirror: the same swaps, applied to what was read.

struct IDXTYPEUniqueNameThingId;

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define RSSE_SWAP_FILE_WORDS 1
#endif

namespace RsseSwap {

namespace {
uint32_t Swap32(uint32_t v) {
#ifdef RSSE_SWAP_FILE_WORDS
    return __builtin_bswap32(v);
#else
    return v;
#endif
}
uint16_t Swap16(uint16_t v) {
#ifdef RSSE_SWAP_FILE_WORDS
    return __builtin_bswap16(v);
#else
    return v;
#endif
}
}  // namespace

// 0x100d5324 (12 bytes)
void USwapInplace32Int(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }
// 0x100d5368 (12 bytes)
void USwapInplace32UInt(uint32_t& word) { word = Swap32(word); }
// 0x100d53b0 (40 bytes): a pointer field as its 32-bit word.
uint32_t USwap32ObjectPtr(uint32_t value) { return Swap32(value); }
// 0x100d5424 (12 bytes)
void USwapInplace16UShort(uint16_t& value) { value = Swap16(value); }
// 0x100d546c (12 bytes)
void USwapInplace32ULong(uint32_t& word) { word = Swap32(word); }
// 0x100d54b4 (12 bytes)
void USwapInplace16Short(int16_t& value) { value = static_cast<int16_t>(Swap16(static_cast<uint16_t>(value))); }
// 0x100d54f8 (40 bytes)
uint32_t USwap32CharPtr(uint32_t value) { return Swap32(value); }
// 0x100d5554 (40 bytes)
uint32_t USwap32WalkPtr(uint32_t value) { return Swap32(value); }
// 0x100d55c0 (40 bytes)
uint32_t USwap32LongPtr(uint32_t value) { return Swap32(value); }
// 0x100d561c (12 bytes)
void USwapInplace32Long(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }
// 0x100d5660 (40 bytes)
uint32_t USwap32VoidPtr(uint32_t value) { return Swap32(value); }

}  // namespace RsseSwap

namespace {

// The fields of a record held as bytes (the record and the walk and object copies are not read through their types), swapped in place.
template <class T, class Function>
void SwapAt(uint8_t* record, uint32_t offset, Function swap) {
    T value;
    std::memcpy(&value, record + offset, sizeof value);
    swap(value);
    std::memcpy(record + offset, &value, sizeof value);
}
void Long(uint8_t* r, uint32_t o) { SwapAt<int32_t>(r, o, RsseSwap::USwapInplace32Long); }
void ULong(uint8_t* r, uint32_t o) { SwapAt<uint32_t>(r, o, RsseSwap::USwapInplace32ULong); }
void UInt(uint8_t* r, uint32_t o) { SwapAt<uint32_t>(r, o, RsseSwap::USwapInplace32UInt); }
void Int(uint8_t* r, uint32_t o) { SwapAt<int32_t>(r, o, RsseSwap::USwapInplace32Int); }
void Short(uint8_t* r, uint32_t o) { SwapAt<int16_t>(r, o, RsseSwap::USwapInplace16Short); }
void UShort(uint8_t* r, uint32_t o) { SwapAt<uint16_t>(r, o, RsseSwap::USwapInplace16UShort); }
void Pointer(uint8_t* r, uint32_t o, uint32_t (*swap)(uint32_t)) {
    uint32_t value;
    std::memcpy(&value, r + o, sizeof value);
    value = swap(value);
    std::memcpy(r + o, &value, sizeof value);
}

// One swapped word to the file: a _USwapCopy32 copy of it, written (the count in the original's place for the bytes done), deleted.
void WriteWord(void* file, uint32_t word) {
    uint32_t transferred;
    uint32_t* copy = SamsUtilities::_USwapCopy32(&word, 1);
    LbFile_Write(file, copy, 4, &transferred);
    SamsUtilities::_USwapDelete(copy);
}

// A table of `words` words behind a byte count: the count, then the swapped copy of the table when the count is not 0.
void WriteTable(void* file, uint32_t bytes, const void* table, int32_t words) {
    WriteWord(file, bytes);
    if (bytes != 0) {
        uint32_t* copy = SamsUtilities::_USwapCopy32(static_cast<const uint32_t*>(table), words);
        LbFile_Write(file, copy, bytes, nullptr);
        SamsUtilities::_USwapDelete(copy);
    }
}

// The walk table: records of 32 bytes, each copied and swapped field by field (the word at +0x1c is copied and not swapped).
void WriteWalkTable(void* file, const RSSE_SCRIPT* script) {
    WriteWord(file, static_cast<uint32_t>(script->mWalkCount));
    if (script->mWalkCount != 0) {
        uint8_t* copy = static_cast<uint8_t*>(SamsUtilities::_USwapAllocate(static_cast<uint32_t>(script->mWalkCount) << 5));
        for (int32_t i = 0; i < script->mWalkCount; ++i) {
            uint8_t* record = copy + i * 0x20;
            std::memcpy(record, reinterpret_cast<const uint8_t*>(script->mWalk.get()) + i * 0x20, 0x20);
            Short(record, 0);
            Short(record, 2);
            Short(record, 4);
            Short(record, 6);
            Long(record, 8);
            Long(record, 0xc);
            Long(record, 0x10);
            Short(record, 0x14);
            Short(record, 0x16);
            Short(record, 0x18);
            Short(record, 0x1a);
        }
        LbFile_Write(file, copy, static_cast<uint32_t>(script->mWalkCount) << 5, nullptr);
        SamsUtilities::_USwapDelete(copy);
    }
}

// The script record's fields swapped in place, in the original's order and widths (+0xb8 to +0xbb, +0xae and +0xe8 to +0xf3 stay). The save swaps
// a copy before it writes it, the load swaps the record after it read it (the swaps are their own inverses).
void SwapRecord(uint8_t* r) {
    using namespace RsseSwap;
    Pointer(r, 0x00, USwap32VoidPtr);
    Pointer(r, 0x04, USwap32VoidPtr);
    for (uint32_t o = 0x08; o <= 0x14; o += 4) Long(r, o);
    for (uint32_t o = 0x18; o <= 0x28; o += 4) Pointer(r, o, USwap32LongPtr);
    Pointer(r, 0x2c, USwap32WalkPtr);
    Pointer(r, 0x30, USwap32LongPtr);
    Pointer(r, 0x34, USwap32CharPtr);
    Pointer(r, 0x38, USwap32CharPtr);
    for (uint32_t o = 0x3c; o <= 0x68; o += 4) Long(r, o);
    Short(r, 0x6c);
    Short(r, 0x6e);
    for (uint32_t o = 0x70; o <= 0x7c; o += 4) Long(r, o);
    ULong(r, 0x80);
    ULong(r, 0x84);
    UShort(r, 0x88);
    UShort(r, 0x8a);
    for (uint32_t o = 0x8c; o <= 0x9c; o += 4) Long(r, o);
    ULong(r, 0xa0);
    ULong(r, 0xa4);
    ULong(r, 0xa8);
#ifdef RSSE_SWAP_FILE_WORDS
    SwapAt<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(r, 0xac, [](tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) { id.Byteswap(); });
#endif
    Pointer(r, 0xb0, USwap32ObjectPtr);
    UInt(r, 0xb4);
    Int(r, 0xbc);
    Short(r, 0xc0);
    Short(r, 0xc2);
    ULong(r, 0xc4);
    ULong(r, 0xc8);
    for (uint32_t o = 0xcc; o <= 0xd8; o += 4) Long(r, o);
    Short(r, 0xdc);
    Short(r, 0xde);
    ULong(r, 0xe0);
    Short(r, 0xe4);
    Short(r, 0xe6);
}

// The script's record: a copy with its fields swapped, written.
void WriteRecord(void* file, const RSSE_SCRIPT* script) {
    RSSE_SCRIPT local;
    std::memcpy(static_cast<void*>(&local), script, sizeof local);
    SwapRecord(reinterpret_cast<uint8_t*>(&local));
    LbFile_Write(file, &local, 0xf4, nullptr);
}

// An object's fields swapped in place: its two links, its four words and its unsigned word.
void SwapObject(uint8_t* object) {
    using namespace RsseSwap;
    Pointer(object, 0x00, USwap32VoidPtr);
    Pointer(object, 0x04, USwap32VoidPtr);
    Long(object, 0x08);
    Long(object, 0x0c);
    Long(object, 0x10);
    ULong(object, 0x14);
    Long(object, 0x18);
}

// One object of the script's list: a swapped copy, written.
void WriteObject(void* file, const RSSE_OBJECT* object) {
    uint8_t local[0x1c];
    std::memcpy(local, object, sizeof local);
    SwapObject(local);
    LbFile_Write(file, local, 0x1c, nullptr);
}

}  // namespace

// 0x100d45b0 (3,408 bytes): writes the script system to a file; answers 0. The state's words are swapped by hand into a local and written in
// one piece (the list head as its Ptr32 word); nothing else is written while the system is off.
int32_t RSSE_Save(void* file) {
    WriteWord(file, 0x45535352);   // "RSSE"
    WriteWord(file, 0x14);         // the state's size
    // The state's words, swapped by hand (inline in the original) into a local; the fifth is the list head as its Ptr32 word.
    uint32_t state[5];
    state[0] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mInitialised));
    state[1] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mF04));
    state[2] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mF08));
    state[3] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mScriptCount));
    state[4] = RsseSwap::USwap32VoidPtr(gRSSEState.mScripts != nullptr ? tpw::ArenaOffset(gRSSEState.mScripts) : 0);
    LbFile_Write(file, state, 0x14, nullptr);
    if (gRSSEState.mInitialised != 0) {
        for (int32_t i = 0; i < 5; ++i) WriteWord(file, 0x5f444150);   // "PAD_" five times
        uint32_t count = 0;
        for (const RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) ++count;
        WriteWord(file, count);
        WriteWord(file, 0xf4);   // the record size
        for (const RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
            WriteRecord(file, script);
            WriteTable(file, static_cast<uint32_t>(script->mProgramWords) << 2, script->mCode.get(), script->mProgramWords);
            WriteTable(file, static_cast<uint32_t>(script->mStackSize) << 2, script->mStack.get(), script->mStackSize);
            WriteTable(file, static_cast<uint32_t>(script->mVarCount) << 2, script->mVars.get(), script->mVarCount);
            // The +0x34 block: its size is the word at +0x90 itself (written swapped), the block raw.
            WriteWord(file, static_cast<uint32_t>(script->mSaveBlockBytes));
            if (script->mSaveBlockBytes != 0) LbFile_Write(file, script->mSaveBlock.get(), static_cast<uint32_t>(script->mSaveBlockBytes), nullptr);
            WriteTable(file, static_cast<uint32_t>(script->mLimboCount) << 3, script->mLimbo.get(), script->mLimboCount << 1);
            WriteTable(file, static_cast<uint32_t>(script->mBounceCount) << 4, script->mBounce.get(), script->mBounceCount << 2);
            WriteWalkTable(file, script);
            WriteTable(file, static_cast<uint32_t>(script->mSaveWordCount) << 2, script->mSaveWords.get(), script->mSaveWordCount);
            // The name, with its terminator, raw.
            const uint32_t nameBytes = static_cast<uint32_t>(std::strlen(script->mName.get())) + 1;
            WriteWord(file, nameBytes);
            if (nameBytes != 0) LbFile_Write(file, script->mName.get(), nameBytes, nullptr);
            WriteWord(file, 0x204a424f);   // "OBJ "
            uint32_t objects = 0;
            for (const RSSE_OBJECT* object = script->mObjects; object != nullptr; object = object->mNext) ++objects;
            WriteWord(file, objects);
            WriteWord(file, 0x1c);   // the object size
            for (const RSSE_OBJECT* object = script->mObjects; object != nullptr; object = object->mNext) WriteObject(file, object);
        }
    }
    return 0;
}

// 0x100d65a8 (184 bytes): the walk record's fields swapped in place, always (the load calls it only where the host's order differs from the
// file's): the halfwords at +0, +2, +4, +6, the words at +8, +0xc, +0x10, the halfwords at +0x14, +0x16, +0x18, +0x1a. The word at +0x1c stays.
void RSSE_WALK::Byteswap() {
    mFrom = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mFrom)));
    mTo = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mTo)));
    mOutFrom = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mOutFrom)));
    mOutTo = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mOutTo)));
    mStart = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(mStart)));
    mEnd = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(mEnd)));
    mPeep = __builtin_bswap32(mPeep);
    mDirection = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mDirection)));
    mKind = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mKind)));
    mState = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mState)));
    mLast = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mLast)));
}

namespace {

const char kMallocNothing[] = "RSSE: Tried to MALLOC 0 - Stoopid !\n";                                  // +0x3db
const char kNotFreed[] = "RSSE: Arrooga - Something malloced but not freed - bad boy - fix it !";      // +0x7dc
const char kNoObjects[] = "RSSE: Load Fail - Object list missing";                                      // +0x925
const char kNoHeader[] = "RSSE: Load Fail - Missing header";                                            // +0x94b

// The original reads a count or a tag: four bytes, the transferred count asked for, then _USwapBlock32 on the word (a no-op on a
// little-endian host, which the file's order is).
uint32_t ReadWord_2(void* file) {
    uint32_t word = 0;
    uint32_t transferred;
    LbFile_Read(file, &word, 4, &transferred);
    SamsUtilities::_USwapBlock32(&word, 1);
    return word;
}

// `bytes` bytes of the file into a destination of `capacity` bytes. The Mac reads them all into the destination (a larger size overflows it);
// the host keeps what fits.
void ReadInto(void* file, void* destination, uint32_t capacity, uint32_t bytes) {
    if (bytes <= capacity) {
        LbFile_Read(file, destination, bytes, nullptr);
    } else {
        std::vector<uint8_t> over(bytes);
        LbFile_Read(file, over.data(), bytes, nullptr);
        std::memcpy(destination, over.data(), capacity);
    }
}

// RSSE's malloc: the trace, then LbMem_Alloc (the arena here: the links and tables are Ptr32); 0 asserts and answers NULL.
void* Allocate(uint32_t size) {
    CTRACE(8, kMalloc, size);
    if (size == 0) {
        GAME_ASSERTMSG(0, kMallocNothing);
        return nullptr;
    }
    return tpw::ArenaAlloc(size);
}

// A table: its byte count was read; the block (when the count is not 0) is read raw. Answers the block.
void* ReadBlock(void* file, uint32_t bytes) {
    void* block = nullptr;
    if (bytes != 0) {
        block = Allocate(bytes);
        LbFile_Read(file, block, bytes, nullptr);
    }
    return block;
}

}  // namespace

// 0x100d56bc (3,784 bytes)
int32_t RSSE_Load(void* file) {
    if (gRSSEState.mInitialised != 0) {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr;) script = KillScript(script, 0);
        gRSSEState.mScripts = nullptr;
        gRSSEState.mInitialised = 0;
        if (gRSSEMallocCount != 0) GAME_ASSERTMSG(0, kNotFreed);
    }
    RSSE_Init();
    if (ReadWord_2(file) != 0x45535352) {   // "RSSE"
        GAME_ASSERTMSG(0, kNoHeader);
        return 1;
    }
    // The state: read straight over gRSSEState on the Mac, whose list head is a word there. Here the five words are a local image.
    const uint32_t stateSize = ReadWord_2(file);
    uint32_t image[5] = {static_cast<uint32_t>(gRSSEState.mInitialised), static_cast<uint32_t>(gRSSEState.mF04),
                         static_cast<uint32_t>(gRSSEState.mF08), static_cast<uint32_t>(gRSSEState.mScriptCount), 0};
    ReadInto(file, image, sizeof image, stateSize);
    gRSSEState.mInitialised = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[0]));
    gRSSEState.mF04 = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[1]));
    gRSSEState.mF08 = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[2]));
    gRSSEState.mScriptCount = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[3]));
    gRSSEState.mScripts = nullptr;
    for (int32_t i = 0; i < 5; ++i) ReadWord_2(file);   // the five "PAD_" words
    const int32_t scriptCount = static_cast<int32_t>(ReadWord_2(file));
    const uint32_t recordSize = ReadWord_2(file);
    for (int32_t s = 0; s < scriptCount; ++s) {
        RSSE_SCRIPT* script = static_cast<RSSE_SCRIPT*>(Allocate(0xf4));
        std::memset(static_cast<void*>(script), 0, 0xf4);
        gRSSEState.mScriptCount += 1;
        script->mNext = gRSSEState.mScripts;
        script->mPrev = nullptr;
        if (gRSSEState.mScripts != nullptr) gRSSEState.mScripts->mPrev = script;
        gRSSEState.mScripts = script;
        // The record is read over the links; they are put back after the swaps.
        uint32_t links[2];
        std::memcpy(links, static_cast<void*>(script), 8);
        ReadInto(file, script, 0xf4, recordSize);
        SwapRecord(reinterpret_cast<uint8_t*>(script));
        std::memcpy(static_cast<void*>(script), links, 8);

        // The program, the stack and the variables: a byte count, the block, then the word count and the block swapped (always).
        script->mCode = nullptr;
        uint32_t bytes = ReadWord_2(file);
        script->mCode = static_cast<uint32_t*>(ReadBlock(file, bytes));
        script->mProgramWords = static_cast<int32_t>(bytes >> 2);
        SamsUtilities::_USwapBlock32(script->mCode.get(), script->mProgramWords);

        script->mStack = nullptr;
        bytes = ReadWord_2(file);
        script->mStack = static_cast<int32_t*>(ReadBlock(file, bytes));
        script->mStackSize = static_cast<int32_t>(bytes >> 2);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mStack.get()), script->mStackSize);

        script->mVars = nullptr;
        bytes = ReadWord_2(file);
        script->mVars = static_cast<int32_t*>(ReadBlock(file, bytes));
        script->mVarCount = static_cast<int32_t>(bytes >> 2);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mVars.get()), script->mVarCount);

        // The +0x34 block: its size goes straight into the field at +0x90.
        script->mSaveBlockBytes = static_cast<int32_t>(ReadWord_2(file));
        script->mSaveBlock = nullptr;
        script->mSaveBlock = static_cast<uint8_t*>(ReadBlock(file, static_cast<uint32_t>(script->mSaveBlockBytes)));

        // The limbo table: the field holds the byte count, then the record count (8 bytes each; swapped as two words each).
        script->mLimboCount = static_cast<int32_t>(ReadWord_2(file));
        script->mLimbo = nullptr;
        if (script->mLimboCount != 0) {
            script->mLimbo = static_cast<RSSE_LIMBO*>(ReadBlock(file, static_cast<uint32_t>(script->mLimboCount)));
            script->mLimboCount = static_cast<int32_t>(static_cast<uint32_t>(script->mLimboCount) >> 3);
            SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mLimbo.get()), script->mLimboCount << 1);
        }

        // The bounce table: 16 bytes a record.
        script->mBounceCount = static_cast<int32_t>(ReadWord_2(file));
        if (script->mBounceCount != 0) {
            script->mBounce = static_cast<RSSE_BOUNCE*>(ReadBlock(file, static_cast<uint32_t>(script->mBounceCount)));
            script->mBounceCount = static_cast<int32_t>(static_cast<uint32_t>(script->mBounceCount) >> 4);
            SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mBounce.get()), script->mBounceCount << 2);
        } else {
            script->mBounce = nullptr;
        }

        // The walk table: 32 bytes a record, each swapped by RSSE_WALK::Byteswap where the host's order differs from the file's.
        script->mWalkCount = static_cast<int32_t>(ReadWord_2(file));
        script->mWalk = nullptr;
        if (script->mWalkCount != 0) {
            const uint32_t walkBytes = static_cast<uint32_t>(script->mWalkCount) << 5;
            script->mWalk = static_cast<RSSE_WALK*>(ReadBlock(file, walkBytes));
#ifdef RSSE_SWAP_FILE_WORDS
            for (int32_t i = 0; i < script->mWalkCount; ++i) script->mWalk.get()[i].Byteswap();
#endif
        }

        // The +0x30 words: the byte count in the field, then (outside the test) the word count and the swap.
        script->mSaveWordCount = static_cast<int32_t>(ReadWord_2(file));
        script->mSaveWords = nullptr;
        if (script->mSaveWordCount != 0) {
            script->mSaveWords = static_cast<int32_t*>(ReadBlock(file, static_cast<uint32_t>(script->mSaveWordCount)));
        }
        script->mSaveWordCount = static_cast<int32_t>(static_cast<uint32_t>(script->mSaveWordCount) >> 2);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mSaveWords.get()), script->mSaveWordCount);

        // The name: its length with the terminator, then the characters (a length of 0 leaves the word the record held).
        const uint32_t nameBytes = ReadWord_2(file);
        if (nameBytes != 0) script->mName = static_cast<char*>(ReadBlock(file, nameBytes));

        // The sound: a handle in the record at +0xd4 asks for the sound again, at the middle of the ride's bound area.
        if (script->mFCC[2] != 0) {
            sAlignedBox box;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            bool found = false;
            if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
                const uint8_t* node = reinterpret_cast<const uint8_t*>(gRideAnimSets[script->mRideAnim]->mHeads->mHeader->mF78.get());
                std::memcpy(&y, node + 0x44, sizeof y);
                x = (box.mMin[0] + box.mMax[0]) * kHalf;
                z = (box.mMin[2] + box.mMax[2]) * kHalf;
                found = true;
            }
            if (found && script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
                const float dx = box.mMax[0] - box.mMin[0];
                const float dy = box.mMax[1] - box.mMin[1];
                const float dz = box.mMax[2] - box.mMin[2];
                script->mFCC[2] = static_cast<uint32_t>(SOUND_AddObject(x, y, z, (dx + dz) * kHalf, dy));
            }
        }

        // The objects.
        script->mFE6[0] = 0xff;
        script->mFE6[1] = 0xff;
        script->mObjects = nullptr;
        if (ReadWord_2(file) != 0x204a424f) {   // "OBJ "
            GAME_ASSERTMSG(0, kNoObjects);
            return 1;
        }
        const int32_t objectCount = static_cast<int32_t>(ReadWord_2(file));
        const uint32_t objectSize = ReadWord_2(file);
        for (int32_t o = 0; o < objectCount; ++o) {
            RSSE_OBJECT* object = static_cast<RSSE_OBJECT*>(Allocate(0x1c));
            object->mNext = script->mObjects;
            object->mPrev = nullptr;
            if (script->mObjects != nullptr) script->mObjects->mPrev = object;
            script->mObjects = object;
            gRSSEF7BE0 += 1;
            uint32_t objectLinks[2];
            std::memcpy(objectLinks, static_cast<void*>(object), 8);
            ReadInto(file, object, 0x1c, objectSize);
            SwapObject(reinterpret_cast<uint8_t*>(object));
            std::memcpy(static_cast<void*>(object), objectLinks, 8);
        }
    }
    return 0;
}

// The first script of the list whose handle is `handle`, NULL when there is none. Without the system on it asserts twice ("not initialised",
// then "Bad ScriptID") and answers NULL.
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t handle) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised_2);
        GAME_ASSERTMSG(0, kBadScriptId);
        return nullptr;
    }
    for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
        if (script->mHandle == handle) return script;
    }
    return nullptr;
}

// 0x100d6734 (120 bytes)
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
    } else if (script != nullptr) {
        if (index < 0 || index >= script->mVarCount) GAME_ASSERTMSG(0, kBadVariable);
        else script->mVars[index] = value;
    }
}

// RSSE_UpgradeRideEffect (0x100d67e0, 688 bytes): the particle effect and the sound of a ride that was upgraded.

uint16_t gRSSEUpgradeSkipThing = 0;   // 0x1029f152 (a halfword in the data; no thing is skipped by default)

namespace {

const char kBadScriptId_2[] = "Bad ScriptID";                      // 0x1023f837
}  // namespace

// 0x100d67e0 (688 bytes)
void RSSE_UpgradeRideEffect(int32_t handle) {
    RSSE_SCRIPT* script;
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        GAME_ASSERTMSG(0, kBadScriptId_2);
        script = nullptr;
    } else {
        script = gRSSEState.mScripts;
        while (script != nullptr && script->mHandle != handle) script = script->mNext;
    }
    // The original reads the script without a NULL check.
    if (script->mThingId == gRSSEUpgradeSkipThing) return;

    const uint16_t thingId = script->mThingId;
    bool skip = false;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    const CObject* thing = static_cast<const CObject*>(gThingArray[thingId].mThing);
    RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{thing->mObjectType});
    // The UpgradeParticleEffect slot (the Mac's +0x6c) by name: the host's record is laid out wider (a vtable and 8-byte slots).
    const int32_t emitterType = info->Value(kRideSlot_UpgradeParticleEffect).mInteger;
    if (emitterType == 0) return;

    float x;
    float y;
    float z;
    sAlignedBox box;
    if (script->mRideAnim == 0 || GetRideMasterBoundArea(script->mRideAnim, &box) == 0) {
        skip = true;
        x = 0.0f;
        y = 0.0f;
        z = 0.0f;
    } else {
        x = (box.mMin[0] + box.mMax[0]) * 0.5f;
        z = (box.mMin[2] + box.mMax[2]) * 0.5f;
        const tag_sM3D2Header* header = gRideAnimSets[script->mRideAnim]->mHeads->mHeader;
        const tag_sNode* node = header->mF78;
        y = *reinterpret_cast<const float*>(reinterpret_cast<const char*>(node) + 0x44);
    }
    if (skip) return;

    const uint32_t emitter = PAR_InitEmitter(emitterType, ppc_fctiwz(1024.0f * x), ppc_fctiwz(1024.0f * y), ppc_fctiwz(1024.0f * z));

    float ex;
    float ey;
    float ez;
    sAlignedBox area;
    bool noArea = false;
    if (script->mRideAnim == 0 || GetRideMasterBoundArea(script->mRideAnim, &area) == 0) {
        noArea = true;
        ex = 0.0f;
        ey = 0.0f;
        ez = 0.0f;
    } else {
        ex = area.mMax[0] - area.mMin[0];
        ey = area.mMax[1] - area.mMin[1];
        ez = area.mMax[2] - area.mMin[2];
    }
    if (!noArea) {
        PAR_SetEmitterRandomPos(static_cast<int32_t>(emitter), ppc_fctiwz(512.0f * ex), ppc_fctiwz(256.0f * ey), ppc_fctiwz(512.0f * ez));
    }
    SOUND_SendEvent(0, gSoundBanks[2], 0xb8, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
}

// 0x100d6ac0 (80 bytes)
void RSSE_SetRideSpeed(RSSE_SCRIPT* script, int32_t speed) {
    if (gRSSEState.mInitialised == 0) GAME_ASSERTMSG(0, kNotInitialised);
    else if (script != nullptr) script->mSpeed = static_cast<int16_t>(speed);
}

// 0x100d6b48 (80 bytes)
int32_t RSSE_GetRideSpeed(RSSE_SCRIPT* script) {
    if (gRSSEState.mInitialised == 0) GAME_ASSERTMSG(0, kNotInitialised);
    else if (script != nullptr) return script->mSpeed;
    return 0;
}

// 0x100d6bd0 (80 bytes)
int32_t RSSE_GetTourBaseID(RSSE_SCRIPT* script) {
    if (gRSSEState.mInitialised == 0) GAME_ASSERTMSG(0, kNotInitialised);
    else if (script != nullptr) return script->mFlyHandle;
    return 0;
}

// Variable `index` of the script. Without the system on it asserts and answers 0; a NULL script answers 0 quietly; an index outside 0 to the
// script's count (signed) asserts and answers 0.
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised_2);
        return 0;
    }
    if (script == nullptr) return 0;
    if (index >= 0 && index < script->mVarCount) return script->mVars[index];
    GAME_ASSERTMSG(0, kBadVariable_2);
    return 0;
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w171: a ride script's screams (SCREAM_Start 0x100ccb30, SCREAM_SetLevel
// 0x100cccc0, SCREAM_Move 0x100ccd3c, SCREAM_Stop 0x100ccd98, SCREAM_Single 0x100ccde8, SCREAM_SingleSad 0x100cd098), the repair particle
// effect (TriggerRepairParticleEffect 0x100cd190), the drawing of a walking peep (DrawPeep 0x100cd388) and two getters (RSSE_GetSoundEventCode
// 0x100d6d04, RSSE_GetCoasterHandle 0x100d6f34). The screams are sound events of the bank gSoundBanks[1]; the handle is the event's answer.

namespace {

const char kBadSoundVariable[] = "RSSE: Bad Sound Variable Script variable reference";          // +0x9b3
const char kNoSoundScript[] = "RSSE: no sound script is present";                               // +0x9e6

// The first script of the list whose handle is `handle`: RSSE_GetScriptPointer inlined (the same code as src/rsse_ride_tick.cpp).
RSSE_SCRIPT* FindScript(int32_t handle) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        GAME_ASSERTMSG(0, kBadScriptId_2);
        return nullptr;
    }
    for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
        if (script->mHandle == handle) return script;
    }
    return nullptr;
}

}  // namespace

// 0x100d6d04 (328 bytes): variable `index` of the sound script that the script `handle` points to (its word +0x14 is that script's handle).
// Asserts and answers 0 for a system that is not on, for an index outside the variables; no script of that handle answers 0 quietly for the
// first lookup and with the assertion "no sound script is present" for the second.
int32_t RSSE_GetSoundEventCode(int32_t handle, int32_t index) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        return 0;
    }
    RSSE_SCRIPT* script = nullptr;
    for (RSSE_SCRIPT* s = gRSSEState.mScripts; s != nullptr; s = s->mNext) {
        if (s->mHandle == handle) {
            script = s;
            break;
        }
    }
    if (script == nullptr) return 0;
    const int32_t soundHandle = static_cast<int32_t>(script->mF14);
    RSSE_SCRIPT* sound = nullptr;
    for (RSSE_SCRIPT* s = gRSSEState.mScripts; s != nullptr; s = s->mNext) {
        if (s->mHandle == soundHandle) {
            sound = s;
            break;
        }
    }
    if (sound == nullptr) {
        GAME_ASSERTMSG(0, kNoSoundScript);
        return 0;
    }
    if (index >= 0 && index < sound->mVarCount) return sound->mVars[index];
    GAME_ASSERTMSG(0, kBadSoundVariable);
    return 0;
}

// The reverb level for the first person camera's ride: the first script of the list that belongs to that ride (its ride number is the id), has
// mF10 at 0 and a level of its own sets it (a negative level sets nothing and ends the search). No such script, or no ride: the balance
// sheet's default level.
void RSSE_SetReverbOnEax() {
    const uint32_t ride = GetFirstPersonRideID();
    if (ride != 0) {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
            if (script->mRideAnim == ride && script->mF10 == 0 && script->mReverb != 0) {
                if (script->mReverb >= 0) SOUND_SetReverbLevel(script->mReverb);
                return;
            }
        }
    }
    SOUND_SetReverbLevel(gMainBalance.mValues[kMainBalanceDefaultReverb].mInteger);
}

// 0x100d6f34 (140 bytes): the coaster handle of the script, 0 when there is none.
uint32_t RSSE_GetCoasterHandle(int32_t handle) {
    RSSE_SCRIPT* script = FindScript(handle);
    return script != nullptr ? script->mCoaster : 0;
}

// A ride script's ride-specific commands (main app, rsse.cpp range of the inventory; unit w172): ExecuteTourSpecificCommand 0x100d6fec (2,240 bytes),
// ExecuteBumperSpecificCommand 0x100d78ec (2,008), ExecuteCoasterSpecificCommand 0x100d8104 (1,192) and __sinit_rsse_cpp 0x100d85f0 (28).
// Each dispatcher reads the command word with ReadScript (the original inlines it), switches on it as an unsigned number (a jump table; 0 and
// every number above the bound go to the default, an assertion) and each case reads its operand and calls one ride function. The original inlines
// ExtractData too; here both are called. The answer of some commands goes into the script's word +0x48 (mCommandAnswer).

namespace {

const char kNoNode[] = "RSSE: Cant create a tour ride (no node)";                                 // +0xa07
const char kUnknownTour[] = "RSSE: Unknown tour ride command";                                    // +0xa2f
const char kUnknownBumper[] = "RSSE: Unknown bumper ride command";                                // +0xa4f

// The inlined test "the word names a variable": the top byte is 0x40. (Not ExtractData: the index is the whole word with that byte cleared.)
bool NamesVariable(uint32_t word) { return (word & 0xff000000u) == 0x40000000u; }
uint32_t VariableIndex(uint32_t word) { return word ^ 0x40000000u; }

// The row 0 of the thing table must be free; the dispatchers of the bumper and the coaster check it first.
void CheckRowZero() { GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero); }

// The halfword at +0xe6 (mFE6) as a host-order int16_t (the bumper's track direction; the header keeps it as two bytes).
int16_t ReadTrackDirection(const RSSE_SCRIPT* script) {
    int16_t value;
    std::memcpy(&value, script->mFE6, sizeof(value));
    return value;
}
void WriteTrackDirection(RSSE_SCRIPT* script, int16_t value) { std::memcpy(script->mFE6, &value, sizeof(value)); }

// Tour command 1: creates the fly base of the ride at its node 0x63 (the first embedded entry with flag 0x800).
void CreateTour(RSSE_SCRIPT* script) {
    const int32_t a = Operand(script);
    tag_sRideAnimSet* set = gRideAnimSets[script->mRideAnim];
    const uint32_t node = FindDataNodeIndexByID(set, 0x800, 0x63);
    if (node == 0xffffffffu) {
        GAME_ASSERTMSG(0, kNoNode);
        return;
    }
    set = gRideAnimSets[script->mRideAnim];
    tag_sMesh* mesh = set->mHeads;
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(mesh->mF28);
    const uint8_t* block = embedded->mRecords[node].mBlock;
    float x = 0.0f;   // 0x10264fb4
    float y = 0.0f;
    float z = 0.0f;
    if (block != nullptr) {
        std::memcpy(&x, block + 0x30, sizeof(x));
        std::memcpy(&y, block + 0x34, sizeof(y));
        std::memcpy(&z, block + 0x38, sizeof(z));
    }
    // angle = (mF10 << 12) / 360: the shift wraps, then the compiler's multiply by 0xb60b60b7 (mulhw), add, srawi 8 and the sign bit.
    const int32_t shifted = static_cast<int32_t>(static_cast<uint32_t>(mesh->mF10) << 12);
    const int32_t high = static_cast<int32_t>((static_cast<int64_t>(static_cast<int32_t>(0xb60b60b7u)) * shifted) >> 32);
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(high) + static_cast<uint32_t>(shifted));
    const int32_t scaled = sum >> 8;
    const int32_t angle = static_cast<int32_t>(static_cast<uint32_t>(scaled) + (static_cast<uint32_t>(scaled) >> 31));
    // The thing id word at +0xac: the id in the high halfword, the halfword at +0xae in the low one (host order halfwords).
    uint16_t low;
    std::memcpy(&low, script->mFAE, sizeof(low));
    const uint32_t thingWord = (static_cast<uint32_t>(script->mThingId) << 16) | low;
    // fctiwz truncates (saturating), then mulli 300 wraps.
    const int32_t px = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(x)) * 300u);
    const int32_t py = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(y)) * 300u);
    const int32_t pz = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(z)) * 300u);
    script->mFlyHandle = FLY_CreateBase(thingWord, script->mHandle, px, py, pz, angle, a);
}

}  // namespace

// 0x100d6fec (2,240 bytes)
void ExecuteTourSpecificCommand(RSSE_SCRIPT* script) {
    const uint32_t command = ReadScript(script);
    switch (command) {
        case 1:
            CreateTour(script);
            break;
        case 2:
            FLY_RemoveBase(script->mFlyHandle);
            break;
        case 3: {
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) script->mCommandAnswer = FLY_AddPeep(script->mFlyHandle, script->mVars[VariableIndex(word)]);
            break;
        }
        case 4:
        case 16: {
            // 4 extracts a peep, 16 asks whether the ride runs: the answer goes into the variable, then into the command answer.
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) {
                const uint32_t index = VariableIndex(word);
                const int32_t answer = command == 4 ? FLY_ExtractPeep(script->mFlyHandle) : FLY_RideRunning(script->mFlyHandle);
                script->mVars[index] = answer;
                script->mCommandAnswer = script->mVars[index];
            }
            break;
        }
        case 5:
            FLY_SetNumberOfCars(script->mFlyHandle, Operand(script));
            break;
        case 8: {
            const int32_t a = Operand(script);
            FLY_SetRunTime(script->mFlyHandle, static_cast<int32_t>(static_cast<uint32_t>(a) * 1000u));
            break;
        }
        case 9:
            FLY_SetRepairState(script->mFlyHandle, Operand(script));
            break;
        case 10:   // the original tests the word for a variable and ignores the result
            ReadScript(script);
            script->mCommandAnswer = FLY_GetCarInBase(script->mFlyHandle);
            break;
        case 11:
            ReadScript(script);
            script->mCommandAnswer = FLY_LaunchCar(script->mFlyHandle);
            break;
        case 12:
            FLY_SetRunway(script->mFlyHandle, Operand(script));
            break;
        case 14:
            FLY_SetOpenClosed(script->mFlyHandle, Operand(script));
            break;
        case 15:
            ReadScript(script);
            script->mCommandAnswer = FLY_GetBrokenStat(script->mFlyHandle);
            break;
        case 17:
            FLY_SetCarSize(script->mFlyHandle, Operand(script));
            break;
        case 18:
            FLY_SetMotionScale(script->mFlyHandle, Operand(script));
            break;
        default:   // 0, 6, 7, 13 and everything above 18
            GAME_ASSERTMSG(0, kUnknownTour);
            break;
    }
}

// 0x100d78ec (2,008 bytes)
void ExecuteBumperSpecificCommand(RSSE_SCRIPT* script) {
    CheckRowZero();
    CObject* object = static_cast<CObject*>(gThingArray[script->mThingId].mThing);
    const uint32_t command = ReadScript(script);
    switch (command) {
        case 1: {
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) script->mCommandAnswer = BMP_AddPeepToRideJoiningList(object->mF24, static_cast<uint32_t>(script->mVars[VariableIndex(word)]));
            break;
        }
        case 2: {
            const uint32_t word = ReadScript(script);
            if (NamesVariable(word)) {
                const uint32_t index = VariableIndex(word);
                script->mVars[index] = BMP_PollForPeeps(object->mF24);
                script->mCommandAnswer = script->mVars[index];
            }
            break;
        }
        case 3:
            ReadScript(script);
            BMP_BeginRide(object->mF24);
            break;
        case 4:
            ReadScript(script);
            script->mCommandAnswer = BMP_LAUNCH(object->mF24, 0);
            break;
        case 5:
            ReadScript(script);
            script->mCommandAnswer = object->mF28;
            break;
        case 6:
            ReadScript(script);
            BMP_CloseRide(object->mF24);
            break;
        case 7:
            ReadScript(script);
            BMP_OpenRide(object->mF24);
            break;
        case 8:
            if (Operand(script) != 0) {
                BMP_BreakRide(object->mF24);
            } else {
                BMP_FixRide(object->mF24);
            }
            break;
        case 9:
            if (Operand(script) != 0) {
                BMP_RideWorn(object->mF24);
            } else {
                BMP_FixRide(object->mF24);
            }
            break;
        case 10:
            ReadScript(script);
            BMP_HaltRide(object->mF24);
            break;
        case 11: {
            // The answer is stored first; a variable operand then gets it too.
            const uint32_t word = ReadScript(script);
            script->mCommandAnswer = BMP_GetCarsOnRide(object->mF24);
            if (NamesVariable(word)) script->mVars[VariableIndex(word)] = script->mCommandAnswer;
            break;
        }
        case 12:
            ReadScript(script);
            script->mCommandAnswer = BMP_AddJoiningListToBump(object->mF24);
            break;
        case 13: {
            const int32_t a = Operand(script);
            BMP_SetRideDuration(object->mF24, static_cast<int32_t>(static_cast<uint32_t>(a) * 30u));
            script->mCommandAnswer = a;
            break;
        }
        case 14: {
            const int32_t a = Operand(script);
            BMP_SetRideDuration(object->mF24, static_cast<int32_t>(0u - static_cast<uint32_t>(a)));
            script->mCommandAnswer = a;
            break;
        }
        case 16:
            ReadScript(script);
            script->mCommandAnswer = BMP_REMOVEBUMP(object->mF24, 1);
            break;
        case 17: {
            const int32_t a = Operand(script);
            if (static_cast<int32_t>(ReadTrackDirection(script)) != a) {
                WriteTrackDirection(script, static_cast<int16_t>(a));
                // The flag is inverted: a nonzero value passes false (the original's way round).
                const bool flag = a == 0;
                LLCC_SetTrackAnim(object, flag);
                LLCC_SetTrackAnimReverse(object, flag);
            }
            break;
        }
        default:   // 0, 15 and everything above 17
            GAME_ASSERTMSG(0, kUnknownBumper);
            break;
    }
}

// 0x100d8104 (1,192 bytes)
void ExecuteCoasterSpecificCommand(RSSE_SCRIPT* script) {
    CheckRowZero();
    const uint32_t command = ReadScript(script);
    switch (command) {
        case 1: {
            const int32_t a = Operand(script);
            CoasterAddPeep(script->mCoaster, static_cast<uint32_t>(a));
            break;
        }
        case 2:
        case 3: {
            // The answer is stored first; a variable operand then gets it too.
            const uint32_t word = ReadScript(script);
            script->mCommandAnswer = static_cast<int32_t>(command == 2 ? CoasterGetQueueSpace(script->mCoaster) : CoasterGetPeep(script->mCoaster));
            if (NamesVariable(word)) script->mVars[VariableIndex(word)] = script->mCommandAnswer;
            break;
        }
        case 4:
            CoasterSetRepairState(script->mCoaster, static_cast<uint32_t>(Operand(script)));
            break;
        case 5:
            CoasterSetOpenClosedState(script->mCoaster, Operand(script));
            break;
        case 6:
            CoasterSetCapacity(script->mCoaster, Operand(script));
            break;
        case 7:   // the original tests the word for a variable and does nothing with it
            ReadScript(script);
            break;
        case 8:
            ReadScript(script);
            script->mCoaster = static_cast<uint32_t>(GetCoasterInstanceID(script->mHandle));
            if (script->mCoaster != 0) CoasterSetSpeed(script->mCoaster, static_cast<uint32_t>(static_cast<int32_t>(script->mSpeed)));
            break;
        default:   // 0 and everything above 8; the text is the bumper's (the original's copy and paste)
            GAME_ASSERTMSG(0, kUnknownBumper);
            break;
    }
}

// __sinit_rsse_cpp 0x100d85f0 (28 bytes): two words of the file's BSS, set at start-up. Their use is not known; the names are mine (the addresses
// are their names).
uint32_t gRsseWord102F7C00 = 0x10000;   // 0x102f7c00
uint32_t gRsseWord102F7C04 = 0;         // 0x102f7c04

// The particle system's globals in the main app (the rsse.cpp range: 0x102e49f4 to 0x102f7bd0, see include/rsse.h). Kept
// apart from the functions (src/rsse_particles_init.cpp) so that a unit that only reads the system's records, such as
// the flying meshes' particle count, can carry them without the functions' callees.

PARState gParState;                 // 0x102edd68
PARLibrary gParLibrary;             // 0x102e5208
int32_t gParSinTable[512];          // 0x102e49f4
uint32_t gParLibraryWordB;          // 0x102e51f4
uint32_t gParLibraryWordA;          // 0x102e51f8
int32_t gParSystemRunning;          // 0x102e51fc
int32_t gParDrawingListBusy;        // 0x102e5200
PTCL* gParDrawingList;              // 0x102e5204

// 0x102edd94 on the Mac: the emitters of the state, indexed as far as 126 (see include/rsse.h).
PAREmitter (&gParEmitters)[126] = *reinterpret_cast<PAREmitter(*)[126]>(&gParState.mEmitters[0]);
