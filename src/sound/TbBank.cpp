// TbBank.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbBank code, named after it.

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "snd_handles.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleMPEG2.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cstddef>
#include <cstring>
#include <new>

// The sound library's MPEG sample codecs (unit w80b, program "sound symbols shared"): _TbSoundSampleMPEG2
// (TbSoundSampleMPEG2.cpp) and _TbSoundSampleMPEG2File (StreamableMpeg2.cpp). See include/TbSoundSampleMPEG2.h for the layouts
// and the slot map.

_CS__TbSoundSampleMPEG2File::~_CS__TbSoundSampleMPEG2File() { _TbSoundSampleMPEG2File::ClearCache(); }

namespace {
// The static objects the Mac's initialiser and finaliser run.
_CS__TbSoundSampleMPEG2 sCleanupMPEG2;
_CS__TbSoundSampleMPEG2File sCleanupMPEG2File;
}  // namespace

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

uint32_t CvtDblUl(double value);

// ---- TbBank (0x14 bytes) -----------------------------------------------------------------------------------------------

// 0x100088a8 (104 bytes): the TbRefRelease base (one reference), the vtable, no data yet, one reference on the owner.
TbBank::TbBank(TbRefRelease* owner) : mRefManager(owner) {}

// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.

CRefManager::CRefManager() : mOwner(nullptr) {}

// 0x100089bc (212 bytes): the length of the sample in milliseconds. The bank's RetrieveSample answers the header (an error
// answers 0); the size in bytes is halved for stereo and halved again for 16-bit samples, divided by the rate and scaled by
// 1000, all in single precision, then converted by the runtime's double to unsigned long.
uint32_t TbBank::GetSampleLength(uint32_t index) {
    TbSampleHeader scratch = {};
    TbSampleHeader* header = &scratch;
    const TbError result = RetrieveSample(&header, index);
    if (static_cast<int32_t>(result) < 0) return 0;
    float length = static_cast<float>(header->mSize);
    if ((header->mFlags & 1) != 0) length = length * 0.5f;
    if (header->mBits == 0x10) length = length * 0.5f;
    const float rate = static_cast<float>(header->mRate);
    const float quotient = length / rate;
    return CvtDblUl(static_cast<double>(1000.0f * quotient));
}

// 0x10008ac0 (8 bytes)
int32_t TbBank::GetNumberOfSamples() {
    return static_cast<int32_t>(mCount);
}

// 0x10008af8 (8 bytes)
const char* TbBank::GetSampleName(uint32_t index) {
    return nullptr;
}

// 0x10008b2c (8 bytes)
uint32_t TbBank::GetSamplePosition(uint32_t index) {
    return 0;
}

// 0x10008b64 (8 bytes)
bfl::FileSystem::CFile* TbBank::GetHandle() {
    return nullptr;
}

// 0x10008b94 (8 bytes)
bool TbBank::IsStreaming() const {
    return false;
}

// 0x10008bc8 (40 bytes): hands out the bank itself.
TbError TbBank::Initialise(TbRefRelease** out) {
    *out = this;
    return TbError(0);
}
// 0x10008c28 (116 bytes): releases the owner (the CRefManager's destructor) and the base.
TbBank::~TbBank() {}
