// Unit v5205: StartTrainSounds (0x10046a18) and LayoutStartAllCoasterSounds (0x10046bc0).
#include "TPCoaster.h"
#include "TPSound.h"
#include "ActionRecorder.h"
#include "ppc_math.h"

namespace {

// The part both functions share, after StopTrainSounds: the four sound words and the two loops, unless the recorder runs.
void StartWords(tag_sCoasterInstance* instance, sTrainInstance* train) {
    if (gActionRecorder.mRecording == 0) {
        train->mF54[0] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 5));
        train->mF54[1] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 6));
        train->mF54[2] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 7));
        train->mF54[3] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 8));
        train->mF64 = SND_SendIndirectedEvent(instance->mScriptID, 0, gSoundBanks[7], 0,
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[0])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[1])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[2])));
        train->mF68 = SND_SendIndirectedEvent(instance->mScriptID, 0, gSoundBanks[7], 1,
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[0])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[1])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[2])));
    }
}

}  // namespace

void StartTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t noState) {
    StopTrainSounds(instance, train);
    StartWords(instance, train);
    if (noState == 0) SetTrainSoundState(train, 2, 100);
}

void LayoutStartAllCoasterSounds() {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        sTrainInstance* train = instance->mTrains;
        for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); i++) {
            StopTrainSounds(instance, train);
            StartWords(instance, train);
            SetTrainSoundState(train, 2, 100);
            train++;
        }
    }
}
