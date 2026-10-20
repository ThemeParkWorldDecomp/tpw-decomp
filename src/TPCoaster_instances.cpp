// Coaster instances (TPCoaster.cpp on the Mac, unit w186): SetCurrentEditCoaster 0x1003aca0, the pylon interval three 0x10040e10, 0x10040e64 and
// 0x10040eb8, FlagGenerateTrack 0x10040f08, FlagCollisionUpdateTrackAroundBrace 0x10040fa4, XZAngleBetweenPylons 0x10041018,
// GetPotentialPylonPositionArea 0x10041170, DestroyEndPylons 0x10042728, EndTrack 0x100427d0, SetCoasterState 0x10042bb8, IsCoasterType 0x100432dc,
// IsCoaster 0x10043330, FindCoasterInstanceFromEntranceID 0x10043380, SetCoasterCObject 0x10043480, InstanceCoaster 0x100434ec,
// SetCoasterScriptID 0x1004373c and ui_GetNumCoasterTracks 0x100458ac. The globals' places are in include/TPCoaster.h (names invented); every
// callee outside the unit is untranslated.
#include <cmath>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

tag_sCoasterInstance* gCoasterInstances = nullptr;   // 0x102b68b4
int32_t gCoasterInstanceCount = 0;                  // 0x102b68ac
uint32_t gCoasterWord689C = 0;                       // 0x102b689c
uint32_t gCoasterWord6898 = 0;                       // 0x102b6898
uint32_t gCoasterWord6894 = 0;                       // 0x102b6894
uint32_t gCoasterWord6890 = 0;                       // 0x102b6890

namespace {

// The ride anim set's mesh node: its y place (the chain gRideAnimSets[i] -> mesh -> header -> node mF78 -> matrix row 3, entry 1).
float EntranceHeight(uint32_t entrance) { return gRideAnimSets[entrance]->mHeads->mHeader->mF78->mMatrix.m[3][1]; }

// SetCoasterState's unloading: both unloads of each train, then the queue's peeps are kicked out. The train base is read once.
void UnloadTrains(tag_sCoasterInstance* instance) {
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i) {
        UnloadTrain(instance, train, 0);
        UnloadTrain(instance, train, 1);
        ++train;
    }
    KickPeepsFromEntranceQueue(instance);
}

}  // namespace

// 0x1003aca0 (12 bytes).
void SetCurrentEditCoaster(tag_sCoasterInstance* instance) { gCoasterEditInstance = instance; }

// 0x10040e10 (24 bytes): the place of the height between the pylon's two entries (single precision steps).
float GetIntervalForHeight(float height, sPylonControl* pylon) {
    const float below = height - pylon->mFBC;
    const float span = pylon->mFC0 - pylon->mFBC;
    return below / span;
}

// 0x10040e64 (20 bytes).
float GetIntervalForHeightDelta(float height, sPylonControl* pylon) {
    const float span = pylon->mFC0 - pylon->mFBC;
    return height / span;
}

// 0x10040eb8 (20 bytes): fmadds.
float GetHeightForInterval(float interval, sPylonControl* pylon) {
    const float span = pylon->mFC0 - pylon->mFBC;
    return ppc_fmadds(interval, span, pylon->mFBC);
}

// 0x10040f08 (92 bytes).
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag) {
    RecalcTrackParams(instance);
    instance->mFlags |= 1;
    if (flag != 0) instance->mFlags |= 0x1000;
}

// 0x10040fa4 (40 bytes).
void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace) {
    brace->mFlags |= 0x200000;
    if (brace->mNext != nullptr) brace->mNext->mFlags |= 0x200000;
}

// 0x10041018 (288 bytes): the angle at the second point between the directions to the first and the third in the x,z plane. The points are
// converted from the words (fcfid-style conversion to single), the lengths come from a fused multiply-add in double, the clamp lets NaN through.
float XZAngleBetweenPylons(uint32_t x1, uint32_t z1, uint32_t x2, uint32_t z2, uint32_t x3, uint32_t z3) {
    const float dz1 = static_cast<float>(z1) - static_cast<float>(z2);
    const float dx1 = static_cast<float>(x1) - static_cast<float>(x2);
    const float dx2 = static_cast<float>(x3) - static_cast<float>(x2);
    const float dz2 = static_cast<float>(z3) - static_cast<float>(z2);
    const float length1 = static_cast<float>(std::sqrt(ppc_fmadd(dx1, dx1, static_cast<double>(dz1 * dz1))));
    const float dx1n = dx1 / length1;
    const float dz1n = dz1 / length1;
    const float length2 = static_cast<float>(std::sqrt(ppc_fmadd(dx2, dx2, static_cast<double>(dz2 * dz2))));
    const float dx2n = dx2 / length2;
    const float dz2n = dz2 / length2;
    float dot = ppc_fmadds(dx1n, dx2n, dz1n * dz2n);
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;
    return static_cast<float>(std::acos(static_cast<double>(dot)));
}

// 0x10041170 (52 bytes): the four words, each answer from its own global.
void GetPotentialPylonPositionArea(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    *a = gCoasterWord689C;
    *b = gCoasterWord6898;
    *c = gCoasterWord6894;
    *d = gCoasterWord6890;
}

// 0x10042728 (104 bytes): the end pylon braces lead the list; each is deleted (the next is read first). There is no NULL test: the list always
// holds a plain brace after the end pylons.
void DestroyEndPylons(tag_sCoasterInstance* instance) {
    tag_sTrackBrace* brace = instance->mBraces;
    while ((brace->mData->mFlags & 1) != 0) {
        tag_sTrackBrace* next = brace->mNext;
        DeleteTrackBrace(instance, brace);
        brace = next;
    }
    instance->mBlock3C &= ~1u;
}

// 0x100427d0 (228 bytes): the edit coaster is read from its global each time (the callees may change it).
void EndTrack(int32_t flag) {
    gCoasterEditInstance->mBlock3C |= 1;
    tag_sCoasterInstance* edit = gCoasterEditInstance;
    PlaceEndPylons(edit, EntranceHeight(edit->mEntranceID));
    edit = gCoasterEditInstance;
    RecalcTrackParams(edit);
    edit->mFlags |= 1;
    edit->mFlags |= 0x1000;
    if (gActionRecorder.mRecording == 0) SetCoasterEditState(0x10);
    if (flag == 0) CoasterPanelEndTrack();
    SetCoasterState(gCoasterEditInstance, 2);
}

// 0x10042bb8 (1,072 bytes): the low nibble of the state is the running kind (1 building, 2 stopped, 4 and 8 the loading kinds), the bits 0x70
// the mode (0x10 and 0x20 the editor's, 0x40 the test run). Every compare is unsigned.
void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) {
    const uint32_t low = state & 0xf;
    if (low != 0) {
        const uint32_t current = instance->mState & 0xf;
        if (current == 4) {
            if (low == 2) {
                UnloadTrains(instance);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        } else if (current == 2 && low != 4) {
            if (low == 1) {
                for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i)
                    DeactivateCoasterTrain(instance, instance->mTrains + i);
                instance->mFlags &= ~0x800u;
                FlashEndPylons(instance, 2);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        } else if (current == 1 && low != 4) {
            if (low == 2) {
                if ((instance->mState & 0x40) == 0) instance->mF2C = 1.0f;
                sTrainInstance* train = instance->mTrains;
                for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i) {
                    ActivateCoasterTrain(instance, train);
                    ++train;
                }
                FlashEndPylons(instance, 0);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        } else if (current == 8 && low != 4) {
            if (low == 2) {
                UnloadTrains(instance);
            } else if (low == 8) {
                KickPeepsFromEntranceQueue(instance);
            }
        }
        instance->mState &= ~0xfu;
        instance->mState |= low;
    }
    const uint32_t high = state & 0x70;
    if (high != 0) {
        const uint32_t current = instance->mState & 0x70;
        if ((current == 0x20 || current == 0x10) && high == 0x40) {
            instance->mF2C = 0.0f;
        } else if (current == 0x40 && (high == 0x20 || high == 0x10)) {
            instance->mF2C = 1.0f;
            sTrainInstance* train = instance->mTrains;
            for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i) {
                train->mFlags |= 0x80;
                train->mFlags &= ~0x40u;
                ++train;
            }
        }
        instance->mState = (instance->mState & ~0x70u) | high;
    }
}

// 0x100432dc (44 bytes).
int32_t IsCoasterType(uint32_t type) { return (gRideTypes[type]->mFlags & 0x40000) != 0 ? 1 : 0; }

// 0x10043330 (44 bytes).
int32_t IsCoaster(uint32_t instance) { return (gRideAnimSets[instance]->mFlags & 0x40000) != 0 ? 1 : 0; }

// 0x10043380 (56 bytes).
tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance) {
    tag_sCoasterInstance* instance = gCoasterInstances;
    while (instance != nullptr) {
        if (instance->mEntranceID == entrance) return instance;
        instance = instance->mNext;
    }
    return nullptr;
}

// 0x10043480 (64 bytes).
void SetCoasterCObject(uint32_t entrance, void* object) {
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(entrance);
    if (instance != nullptr) instance->mCObject = object;
}

// 0x1004373c (100 bytes): the sounds are restored even when no instance was found (the Mac passes the NULL on).
void SetCoasterScriptID(uint32_t entrance, int32_t id) {
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(entrance);
    if (instance != nullptr) instance->mScriptID = id;
    RestoreCoasterSounds(instance);
}

// 0x100458ac (12 bytes).
int32_t ui_GetNumCoasterTracks() { return gCoasterInstanceCount; }

// 0x100434ec (544 bytes). Without the flag the instance gets an object number (none: the instance is freed and 0 answered). The type is found by
// its ride type number (NULL when there is none: the Mac would crash on the first read). The Mac answers its register 25 when the flag is set
// (undefined); the host answers 0.
uint32_t InstanceCoaster(uint32_t typeNumber, uint32_t entrance, uint32_t x, uint32_t z, int32_t flag, void* object) {
    // The Mac asks for 0x14c bytes; the host's record holds native pointers and is bigger, so it asks for its own size.
    auto* instance = static_cast<tag_sCoasterInstance*>(MReleaseAllocF(sizeof(tag_sCoasterInstance), 1));
    uint32_t id = 0;
    if (flag == 0) {
        id = AllocateObjectInstanceToTable(gObjectTableCount, gObjectTableFirstFree, gObjectTableLowest, 0xff, gObjectTable, instance, 0xffffffffu);
        if (id == 0) {
            MReleaseFreeF(instance);
            return 0;
        }
        instance->mObjectInstanceID = id;
    }
    instance->mNext = gCoasterInstances;
    gCoasterInstances = instance;
    tag_sCoasterType* type = nullptr;
    for (uint32_t i = 0; i < gCoasterTypeCount; ++i) {
        if (gCoasterTypes[i]->mRideTypeNumber == typeNumber) {
            type = gCoasterTypes[i];
            break;
        }
    }
    instance->mType = reinterpret_cast<const uint32_t*>(type);
    instance->mTypeID = typeNumber;
    instance->mEntranceID = entrance;
    instance->mX = x;
    instance->mZ = z;
    instance->mF118 = type->mFA4;
    instance->mF11C = 0.0f;
    instance->mFAC = 3.0f;
    AllocateQueue(&instance->mQueueB0, static_cast<uint32_t>(type->mF2F8));
    AllocateQueue(&instance->mQueueD0, static_cast<uint32_t>(type->mF318) << 1);
    if ((type->mFlags & 8) != 0) instance->mBlock3C |= 8;
    if ((type->mFlags & 4) != 0)
        instance->mUVOffsets = static_cast<float*>(MReleaseAllocF(type->mTextureCount << 2, 1));
    instance->mCObject = object;
    gCoasterEditInstance = instance;
    SetCoasterState(instance, 0x11);
    CoasterSetSpeed(instance, 0x3c, flag);
    if (flag == 0) {
        InstanceCoasterTrains(type, instance, 0);
        SetTrainCapacity(instance, 0, static_cast<uint32_t>(type->mF2F4), 1);
        PlaceEntrancePylons(instance, x, z, EntranceHeight(entrance), 0);
    }
    ++gCoasterInstanceCount;
    gCoasterEditInstance = nullptr;
    return id;
}
