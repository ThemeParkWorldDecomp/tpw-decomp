#include <cmath>
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "Advisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

// 0x100471bc (184 bytes). Stops the train's two sound effects (mF64, mF68) and its scream (mF70) unless the action recorder is
// running, then clears the sound words either way.
void StopTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) {
    (void)instance;
    if (train->mF64 != 0 && gActionRecorder.mRecording == 0) SOUND_StopSoundEffect(train->mF64);
    if (train->mF68 != 0 && gActionRecorder.mRecording == 0) SOUND_StopSoundEffect(train->mF68);
    if (train->mF70 != 0 && gActionRecorder.mRecording == 0) SCREAM_Stop(static_cast<int32_t>(train->mF70));
    train->mF54[0] = 0;
    train->mF54[1] = 0;
    train->mF54[2] = 0;
    train->mF54[3] = 0;
    train->mF64 = 0;
    train->mF68 = 0;
    train->mF70 = 0;
    train->mF6C = 0;
}

// 0x100472c4 (220 bytes). Every train's four sound event codes (mF54[0..3]) that are 0 are looked up again with the coaster's
// script id and the kinds 5 to 8.
void RestoreCoasterSounds(tag_sCoasterInstance* instance) {
    if (instance == nullptr) return;
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); i++) {
        if (train->mF54[0] == 0) train->mF54[0] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 5));
        if (train->mF54[1] == 0) train->mF54[1] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 6));
        if (train->mF54[2] == 0) train->mF54[2] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 7));
        if (train->mF54[3] == 0) train->mF54[3] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 8));
        train++;
    }
}
