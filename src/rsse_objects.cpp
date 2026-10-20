// The script objects of the ride scripts (main app, rsse.cpp range of the inventory, 0x100cc778..0x100d6bd0; unit w150): the emitters and
// sounds a script starts on its ride's nodes (CreateObject, KillObject, KillLinkedObject, SetObjParam, StartEvent, InitObject,
// ProcessObjects), the program reader and the stack (ExtractData, ReadScript, Push, Pop), the small getters and setters and two
// position getters of a ride set. An object lives in the arena (its links are Ptr32), where the Mac used LbMem_Alloc and LbMem_Free.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "rsse.h"

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
const float kHalf = 0.5f;      // 0x10264fb8

constexpr int32_t kDead = -10000;   // 0xffffd8f0: the state of a script that went wrong

// The embedded record of the ride set's node `index` (the set's mesh keeps the embedded set in mF28; 0x14 bytes a record).
const sEmbeddedRecord& NodeRecord(const tag_sRideAnimSet* set, uint32_t index) {
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(set->mHeads->mF28);
    return embedded->mRecords[index];
}

float ReadFloat(const uint8_t* base, uint32_t offset) {
    float value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
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

// 0x100d6734 (120 bytes)
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
    } else if (script != nullptr) {
        if (index < 0 || index >= script->mVarCount) GAME_ASSERTMSG(0, kBadVariable);
        else script->mVars[index] = value;
    }
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

// 0x100d2e60 (44 bytes): the floats +0x40, +0x44, +0x48 of the node the ride's header calls mF78.
void GetRidePosition(tag_sRideAnimSet* set, float& x, float& y, float& z) {
    const uint8_t* node = reinterpret_cast<const uint8_t*>(set->mHeads->mHeader->mF78.get());
    x = ReadFloat(node, 0x40);
    y = ReadFloat(node, 0x44);
    z = ReadFloat(node, 0x48);
}

// 0x100cc778 (220 bytes): the centre of the ride's bounding box, the height from the node of GetRidePosition. Answers 0 when found.
int32_t GetRideCentrePosition(uint32_t& ride, float& x, float& y, float& z) {
    if (ride != 0) {
        sAlignedBox box;
        if (GetRideMasterBoundArea(ride, &box) != 0) {
            const uint8_t* node = reinterpret_cast<const uint8_t*>(gRideAnimSets[ride]->mHeads->mHeader->mF78.get());
            y = ReadFloat(node, 0x44);
            x = (box.mMin[0] + box.mMax[0]) * kHalf;
            z = (box.mMin[2] + box.mMax[2]) * kHalf;
            return 0;
        }
    }
    z = kZero;
    y = z;
    x = y;
    return 1;
}
