// Test library for SOUND_SetRect (unit v5294; libtpw_unit_SOUND_SetRect.dylib, tests/test_SOUND_SetRect.py), main app:
//   src/game/Sound.cpp   SOUND_SetRect
//
// The audio object is a fake whose SystemCommand records the command's words. The sound globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <typeinfo>

#include "calllog.h"
#include "platform/arena.h"
#include "HLSprite.h"
#include "TPSound.h"
#include "TbAudioSystem.h"

using namespace calllog;

TbAudioSystem* gAudioSystem;
uint32_t gSoundWanted;
uint32_t gSoundSystemOn;

// The command's key function (its vtable lives with it); never run here.
TbError TbSysCommand::SetBoundingRectangle::Execute(const TbAudioBody*) const { return TbError(0); }

namespace {

class FakeAudio : public TbAudioSystem {
public:
    TbError Release() override { return TbError(0); }
    TbError SetListenerOrientation(const TbSoundPosition&, const TbSoundPosition&, const TbSoundPosition&,
                                   const TbSoundPosition&) override { return TbError(0); }
    TbError RegisterCatagory(const char*, const char*, uint32_t&) override { return TbError(0); }
    int32_t SendEvent(int32_t, const TbAudioEvent&) override { return 0; }
    int32_t UpdateHandle(int32_t, const TbSoundPosition&, const TbSoundPosition&) override { return 0; }
    int32_t UpdateParameter(int32_t, uint8_t, uint32_t) override { return 0; }
    TbError SystemCommand(const TbSysCommand::Command& command) const override {
        const TbSysCommand::SetBoundingRectangle* c = static_cast<const TbSysCommand::SetBoundingRectangle*>(&command);
        if (c == nullptr) {
            ev("SystemCommand", {0});
        } else {
            ev("SystemCommand", {1, this == gAudioSystem ? 1 : 0, c->mHandle, c->mLeft, c->mTop, c->mRight, c->mBottom});
        }
        return TbError(0);
    }
    TbError Init(const char*, const CAudioConfig&) override { return TbError(0); }
    TbError UpdateDistortion(void*, float) override { return TbError(0); }
    TbError UpdateLowPass(void*, float) override { return TbError(0); }
    TbError StopHandle(void*) const override { return TbError(0); }
    TbError DeregisterCatagoryBanks(uint32_t) const override { return TbError(0); }
    TbError CreateSystemChain(const CAudioConfig&) override { return TbError(0); }
};

FakeAudio gFake;

}  // namespace

extern "C" {

uint64_t tpw_sr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"wanted", &gSoundWanted},
        {"on", &gSoundSystemOn},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(uint32_t));
    return n;
}

// The audio object is always present (the original would read through a NULL one).
void tpw_sr_setrect(int32_t handle, int32_t a, int32_t b, int32_t c, int32_t d) {
    gAudioSystem = &gFake;
    SOUND_SetRect(handle, a, b, c, d);
}

}  // extern "C"
