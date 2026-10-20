// Unit v5967: AbortMovePylon 0x10057df4, SingleHFieldTextureChange 0x1005b840, PlacePylon (4 arguments) 0x10052a30,
// IsCoasterUseable 0x10052954, IsTrackValid 0x100528d8, IsTrackClosed (void*) 0x10052850, DecCoasterConst2 0x10052628.
#include <cstdint>
#include <new>
#include "Debug.h"
#include "FS_Online.h"
#include "HFieldTextures.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "online.h"
#include "ActionRecorder.h"
#include "MainWindowCallback.h"
#include "keytable_actions.h"
#include "ui_main.h"

// 0x10052780 (36 bytes, stand-in): the instance overload of IsTrackClosed.
int32_t IsTrackClosed(tag_sCoasterInstance* instance);

namespace {
// The entrance id of a ride object sits at +0x1c.
uint32_t EntranceOf(const void* object) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(object) + 0x1c); }
}  // namespace

// 0x10057df4
void AbortMovePylon() {
    bool generate = false;
    gCoasterWord16BC = 0;
    if (gCoasterInputFlags & 0x100) {
        gCoasterInputFlags &= ~0x100u;
        if (gPylonMoveMark2 != nullptr) {
            tag_sTrackBrace* end = gPylonMoveMark1;
            tag_sTrackBrace* walk;
            if (end->mNumber > gPylonMoveMark2->mNumber) {
                walk = gPylonMoveMark2;
            } else {
                walk = end;
                end = gPylonMoveMark2;
            }
            while (walk != end) {
                walk->mFlags &= 0xFEFFBFFFu;
                walk = walk->mPrev;
            }
            generate = true;
        }
        gPylonMoveMark1 = nullptr;
        gPylonMoveMark2 = nullptr;
        gExtrudeTextureKind = 0;
        if (generate) FlagGenerateTrack(gCoasterEditInstance, 0);
        if (gPylonMoveBrace->mData == gPylonMoveOrigData) gPylonMoveOrigData = nullptr;
        MoveBrace(gCoasterEditInstance, gPylonMoveBrace, gPylonMoveOrigData, static_cast<uint32_t>(gPylonMoveX),
                  static_cast<uint32_t>(gPylonMoveZ), nullptr, nullptr, 1, 0);
        LLCC_AbortPylonMove();
        LockPointedAtRide(0);
        gPylonMoveBrace = nullptr;
        gPylonMoveOrigData = nullptr;
        if (gActionRecorder.mRecording != 0) {
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0;
        }
    }
}

// 0x1005b840
void SingleHFieldTextureChange(int32_t x, int32_t y, const CCellBase::CELL_IndexType* tile) {
    tag_sM3D2Header* header = gHeightfieldHeader;
    if (header != nullptr && (header->mFlags & 2)) {
        SetHeightfieldTexture(header, &gExtraTextureTable, x, y, static_cast<int32_t>(tile->mKind), static_cast<int32_t>(tile->mIndex),
                              static_cast<int32_t>(tile->mRotation));
        TPForce3DRefresh(1);
    }
}

// 0x10052a30: the answer is whatever the 7-argument PlacePylon left in r3.
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, uint32_t* meshSlot) {
    tag_sPylon* pylon;
    tag_sTrackBrace* brace;
    int32_t result = PlacePylon(x, z, flag, &pylon, &brace, 0xff, 0);
    if (meshSlot != nullptr) {
        if (pylon != nullptr)
            *meshSlot = brace->mWord70;
        else
            *meshSlot = 0;
    }
    return result;
}

// 0x10052954
int32_t IsCoasterUseable(CObject* object) {
    if (object == nullptr) return 0;
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(EntranceOf(object));
    if (instance == nullptr) return 0;
    bool answer = false;
    bool closed = false;
    if (*reinterpret_cast<const uint32_t*>(&instance->mF140[0]) == 0 && IsTrackClosed(instance) != 0) closed = true;
    if (closed && instance != gCoasterEditInstance) answer = true;
    return answer;
}

// 0x100528d8
int32_t IsTrackValid(void* coaster) {
    if (coaster == nullptr) return 0;
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(EntranceOf(coaster));
    if (instance == nullptr) return 0;
    return *reinterpret_cast<const uint32_t*>(&instance->mF140[0]) == 0 ? 1 : 0;
}

// 0x10052850
int32_t IsTrackClosed(void* coaster) {
    if (coaster == nullptr) return 0;
    tag_sCoasterInstance* instance = FindCoasterInstanceFromEntranceID(EntranceOf(coaster));
    if (instance == nullptr) return 0;
    uint32_t flags = instance->mBlock3C;
    if ((flags & 1) && !(flags & 2)) return 1;
    return 0;
}

// 0x10052628
int32_t DecCoasterConst2() {
    if (gCoasterEditInstance != nullptr && gCoasterEditInstance->mType != nullptr) {
        const float* type = reinterpret_cast<const float*>(gCoasterEditInstance->mType);
        CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(type[2]), static_cast<double>(type[3]));
        FlagGenerateTrack(gCoasterEditInstance, 1);
    }
    return 1;
}
