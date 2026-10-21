// RSSE_UpgradeRideEffect (0x100d67e0, 688 bytes): the particle effect and the sound of a ride that was upgraded.
#include <cstddef>
#include <cstdint>

#include "Debug.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

uint16_t gRSSEUpgradeSkipThing = 0;   // 0x1029f152 (a halfword in the data; no thing is skipped by default)

namespace {

const char kNotInitialised[] = "RSSE: System not initialised";   // 0x1023f81a
const char kBadScriptId[] = "Bad ScriptID";                      // 0x1023f837
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1023f2ae

}  // namespace

// 0x100d67e0 (688 bytes)
void RSSE_UpgradeRideEffect(int32_t handle) {
    RSSE_SCRIPT* script;
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        GAME_ASSERTMSG(0, kBadScriptId);
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
