// Unit v51113: the small sound-library functions at the edges of CSprocketsModule.cpp (sound 0x10023310 to 0x1002e8fc) and the
// float square roots / cleanup objects of the codec files.
#include "TbMapStreamer.h"
#include "TbSoundManager.h"
#include <cstring>
#include <cmath>

#include "CSprocketsModule.h"
#include "TbAudioBody.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipe.h"
#include "platform/samsutils.h"

// 0x10023378 (44): the table is the Mac's +8 word (data), the count the +4 word; an unsigned index test.
uint32_t TbMapStreamer::FixUpCatagoryIndex(uint32_t index) {
    uint32_t* table = mFixup.mData;
    if (table == nullptr || index >= mFixup.mCount) return 0;
    return table[index];
}

// 0x10023310 (40): the fixup table lives at +4.
void TbMapStreamer::UnloadCatgoryFixupTable() { mFixup.Unload(); }

namespace TbMapStreamerFixup {
// 0x10023674 (8): UString::operator char* answers the text word (+8).
char* StringText(void* ustring) { return static_cast<SamsUtilities::UString*>(ustring)->mText; }
}  // namespace TbMapStreamerFixup

namespace SamsUtilities {
// 0x1002c5fc (56): UCommand's constructor with `on`, then the vtable word 0x1003e010 at +4.
UCommandPlay::UCommandPlay(uint32_t on) {
    UCommand_ConstructOn(this, on);
    const uint32_t vtable = 0x1003e010;
    std::memcpy(mStorage + 4, &vtable, 4);
}
}  // namespace SamsUtilities

using namespace SamsUtilities;

// Lead 2026-10-20 (v51113): RestartChannel and RestartAll are weak: platform/sound_module.cpp holds the host versions the
// launcher links (as v51053 did for SetSpeedOfSound and SetListenerVelocity), the unit test library takes these.
// 0x1002c51c (168): the twin of PauseChannel with a play command.
__attribute__((weak)) TbError CSprocketsModule::RestartChannel(uint32_t channel) {
    UCommandPlay command(1);
    CSprocketsChannel* ch = Channels()[channel];
    if (ch == nullptr) return TbError(-1);
    UStatus status = SendToChannel(ch, command);
    return TbError(status.IfOk(TbLegacyError{0}, TbLegacyError{-1}));
}

// 0x1002c748 (164): the twin of PauseAll with a play command.
__attribute__((weak)) TbError CSprocketsModule::RestartAll() {
    UCommandPlay command(1);
    for (uint32_t i = 0; i < mBuffers; i++) {
        CSprocketsChannel* ch = Channels()[i];
        if (ch != nullptr) SendToChannel(ch, command);
    }
    return TbError(0);
}

// ---- TpBody (0x1002e3bc to 0x1002e8fc)
void TpBody::Tick() { reinterpret_cast<CSprocketsModule*>(this)->Tick(); }   // 0x1002e404
TbError TpBody::Init(const char*) { return TbError(0); }                      // 0x1002e3bc
TbError TpBody::Close() { return TbError(0); }                                // 0x1002e44c
TbError TpBody::SetFades(uint32_t) const { return TbError(0); }               // 0x1002e494
TbError TpBody::SetCatagoryReverb(uint32_t) const { return TbError(0); }      // 0x1002e8a4

// 0x1002e4e0 (100)
TbError TpBody::DeregisterCatagoryBanks(uint32_t category) const {
    if (category == 0) return TbError(-1);
    return TbMapStreamer::Instance()->RemoveCatagory(category);
}

// 0x1002e8fc (104)
TpBody* TpBody::Destruct(TpBody* self, int16_t deleteFlag) {
    if (self != nullptr) {
        reinterpret_cast<TbAudioBody*>(self)->TbAudioBody::~TbAudioBody();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

// ---- the cleanup objects
_CS_TbSoundSamplePipe* _CS_TbSoundSamplePipe::Destruct(_CS_TbSoundSamplePipe* self, int16_t deleteFlag) {   // 0x1000f9dc
    if (self != nullptr) {
        TbSoundSamplePipe::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

_CS__TbSoundCodecLoop* _CS__TbSoundCodecLoop::Destruct(_CS__TbSoundCodecLoop* self, int16_t deleteFlag) {   // 0x1000b590
    if (self != nullptr) {
        _TbSoundCodecLoop::ClearCache();
        if (deleteFlag > 0) USoundChannel_Free(self);
    }
    return self;
}

// ---- 0x1000c70c and 0x1000c6cc: the runtime's double sqrt (glue 0x1002ecec), rounded to float
namespace tbstd {
float sqrtf(float x) { return static_cast<float>(::sqrt(static_cast<double>(x))); }
float sqrtOfFloat(float x) { return sqrtf(x); }
}  // namespace tbstd
