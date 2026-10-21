// Test library for StopTrainSounds and RestoreCoasterSounds (unit v5728; libtpw_unit_StopTrainSounds.dylib, tests/test_StopTrainSounds.py), main app:
//   src/game/rides/CoasterSounds.cpp
//
// The callees outside the unit are recording fakes; gActionRecorder is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "rsse.h"

using namespace calllog;

CActionRecorder gActionRecorder;

void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }

void SCREAM_Stop(int32_t handle) { ev("SCREAM_Stop", {handle}); }

int32_t RSSE_GetSoundEventCode(int32_t a, int32_t b) {
    ev("RSSE_GetSoundEventCode", {a, b});
    return static_cast<int32_t>(ret("RSSE", b, 0));
}

extern "C" {

uint64_t tpw_stsn_addr(const char* name) {
    if (std::strcmp(name, "rec") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gActionRecorder.mRecording));
    return 0;
}

uint64_t tpw_stsn_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// coaster mScriptID, mTrainCount, mTrains; train mF54, stride
int32_t tpw_stsn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mScriptID));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTrainCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTrains));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF54));
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF64) - offsetof(sTrainInstance, mF54));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF70) - offsetof(sTrainInstance, mF54));
    return n;
}

void tpw_stsn_stop(uint64_t instance, uint64_t train) {
    StopTrainSounds(reinterpret_cast<tag_sCoasterInstance*>(instance), reinterpret_cast<sTrainInstance*>(train));
}

void tpw_stsn_restore(uint64_t instance) { RestoreCoasterSounds(reinterpret_cast<tag_sCoasterInstance*>(instance)); }

}  // extern "C"
