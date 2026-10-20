// Test library for StartTrainSounds and LayoutStartAllCoasterSounds (unit v5205; libtpw_unit_LayoutStartAllCoasterSounds.dylib,
// tests/test_LayoutStartAllCoasterSounds.py), main app:
//   src/TPConfig_bend_LayoutStartAllCoasterSounds.cpp
//
// The callees (StopTrainSounds, RSSE_GetSoundEventCode, SND_SendIndirectedEvent, SetTrainSoundState) are recording fakes; the globals
// of other files (gCoasterInstances, gSoundBanks, gActionRecorder) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "ActionRecorder.h"

using namespace calllog;

tag_sCoasterInstance* gCoasterInstances;
int32_t gSoundBanks[8];
CActionRecorder gActionRecorder;

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs (a train is named by its mF08 word)
void StopTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) {
    ev("stop", {instance->mScriptID, static_cast<int64_t>(train->mF08)});
}

int32_t RSSE_GetSoundEventCode(int32_t a, int32_t b) {
    ev("rsse", {a, b});
    return static_cast<int32_t>(ret("rsse"));
}

int32_t SND_SendIndirectedEvent(int32_t a, int32_t entity, int32_t category, int32_t b, int32_t p5, int32_t p6, int32_t p7) {
    ev("snd", {a, entity, category, b, p5, p6, p7});
    return static_cast<int32_t>(ret("snd"));
}

void SetTrainSoundState(sTrainInstance* train, uint32_t state, uint32_t value) {
    ev("state", {static_cast<int64_t>(train->mF08), static_cast<int64_t>(state), static_cast<int64_t>(value)});
}

extern "C" {

uint64_t tpw_lsc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"head", &gCoasterInstances},
        {"bank", &gSoundBanks[7]},
        {"rec", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_lsc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host's offsets: instance (script id, next, train count, trains, size), train (id word mF08, size, mF48, mF54, mF64, mF68).
int32_t tpw_lsc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mScriptID));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mNext));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTrainCount));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mTrains));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF08));
    out[n++] = static_cast<int64_t>(sizeof(sTrainInstance));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF48));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF54));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF64));
    out[n++] = static_cast<int64_t>(offsetof(sTrainInstance, mF68));
    return n;
}

void tpw_lsc_start(uint64_t instance, uint64_t train, int32_t flag) {
    StartTrainSounds(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)),
                     reinterpret_cast<sTrainInstance*>(static_cast<uintptr_t>(train)), flag);
}

void tpw_lsc_all() { LayoutStartAllCoasterSounds(); }

}  // extern "C"
