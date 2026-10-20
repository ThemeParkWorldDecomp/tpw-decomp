// The life of a ride script (main app, rsse.cpp range of the inventory; unit w169): RSSE_InitScript reads a script file into a new
// RSSE_SCRIPT record (the tables, the program, the saved block, the name), puts it at the head of the list and starts the ride's
// effect emitter and sound; KillScript gives one record and everything it owns back; RSSE_KillScript starts the ride's closing effect,
// kills the script and the scripts linked to it. The record and its tables live in the arena (the Ptr32 links), where the Mac used
// LbMem_Alloc and LbMem_Free; the callees outside the unit (the file reads, the particle and sound calls, the ride database) are not here.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "HLSprite.h"
#include "LevelInit.h"
#include "RideDB.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte; some end in a newline).
const char kMalloc[] = "RSSE Malloc %ld\n";                                                          // +0x3ca
const char kMallocZero[] = "RSSE: Tried to MALLOC 0 - Stoopid !\n";                                  // +0x3db
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";       // +0x400
const char kModeRead[] = "rb";                                                                       // +0x822
const char kBadVersion[] = "RSSE: Script & Script interpreter are different versions";               // +0x825
const char kZeroLength[] = "RSSE: Zero length script !\n";                                           // +0x85e
const char kNotScript[] = "RSSE: Not a RSSE script file !";                                          // +0x87a
const char kCannotLoad[] = "RSSE: Cannot load Script file";                                          // +0x899
const char kNeedInit[] = "RSSE: Need to initialise before you can start a script\n";                 // +0x8b7
const char kCantKillSystem[] = "RSSE: Cant kill a script - system is not initialised\n";             // +0x8ef
const char kCantKillObject[] = "RSSE: Unknown object type - can't kill it !";                        // +0x5a6

const float k1024 = 1024.0f;   // 0x10264fac
const float k512 = 512.0f;     // 0x10264f5c
const float k256 = 256.0f;     // 0x10264f58
const float kZero = 0.0f;      // 0x10264fb4
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

float ReadFloat(const uint8_t* base, uint32_t offset) {
    float value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

// The ride record of the script's thing (the thing's object type, the halfword at +0xe). The row 0 of the thing table must be empty.
RideInfo* ThingRideInfo(uint16_t row) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    const CObject* thing = static_cast<const CObject*>(gThingArray[row].mThing);
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{thing->mObjectType});
}

}  // namespace

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
