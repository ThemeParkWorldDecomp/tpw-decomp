// TbSoundSampleCodec.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbSoundSampleCodec code, named after it.

#include "AudioPlaceHolder.h"
#include "LbTime.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleCallback.h"
#include <new>

// The sample callback pipes of the sound library (unit w79i, program "sound symbols shared"): TbSoundUserCallback (UserCallback.cpp),
// _TbSoundSampleCallback and CAutoDetach (TbSoundSampleCallback.cpp), _TbSoundSampleCompletedCallback (TbSoundSamplePipe.cpp, its
// constructor and GetDataChunk in LbMpeg.cpp) and the three cleanup objects. See include/TbSoundSampleCallback.h for the layouts
// and the vtables.

_CS__TbSoundSampleCallback::~_CS__TbSoundSampleCallback() { _TbSoundSampleCallback::ClearCache(); }

// The sound library's sample pipe base (unit w78, program "sound symbols shared"): TbSoundSamplePipe (LbMpeg.cpp and
// TbSoundSamplePipe.cpp), TbSoundSPCodec (LbMpeg.cpp, StreamableMpeg2.cpp, TbSoundCodecLoop.cpp, TimerCodec.cpp) and
// TbSoundSampleCodec (StreamableMpeg2.cpp, TbSoundSampleCallback.cpp, TbSoundSampleMPEG2.cpp, TimerCodec.cpp). See
// include/TbSoundPipe.h for the layouts and the slot order.

// ---------------------------------------------------------------- the sample codec

// The data start is the header + 0x28 (the file's own 0x28-byte header, converted in place, is followed by the sample's data;
// the sample store hands the codec of a non-streaming bank that record). For a streaming bank (TbMMFileBank) the record is
// one of the bank's 0x14-byte headers and +0x28 is a bogus address two records on: the streaming codecs (unit w80) read the
// data through the file instead, and this base class never dereferences it.
TbSoundSampleCodec::TbSoundSampleCodec(TbSampleHeader* header, TbRefRelease* owner) : mRefManager(owner) {
    mDataSize = static_cast<uint32_t>(header->mSize);
    mDataStart = reinterpret_cast<const uint8_t*>(header) + 0x28;
    mTime = 0;
    mDataPosition = 0;
    SetSampleFormatFromHeader(header);
}

// The Mac's CRefManager::CRefManager() (0x10013824, 12 bytes: stores 0 at +0) is another unit's function; the host uses the
// existing constructor with no owner, which does the same and makes no AddRef call.
TbSoundSampleCodec::TbSoundSampleCodec() : mRefManager(nullptr) {
    mDataPosition = 0;
}

// The pipe's own SetInfo goes down the chain; a negative result is -1, otherwise the codec jumps to the end of the sample
// when the second flags word of the info (+0xc) has 0x4000.
TbError TbSoundSampleCodec::SetInfo(const TbSoundSampleInfo& info) {
    TbError result = TbSoundSamplePipe::SetInfo(info);
    if (static_cast<int32_t>(result) < 0) return TbError(-1);
    if ((info.mField0C & 0x4000) != 0) mDataPosition = mDataSize;
    return TbError(0);
}

// The format byte: 0x40 stereo or 0x20, | 2 for 16 bits (header +6 == 0x10) or 1; then the virtual SetSampleRate with the
// header's rate (+4) adds the rate bit.
void TbSoundSampleCodec::SetSampleFormatFromHeader(TbSampleHeader* header) {
    if ((header->mFlags & 1) != 0) {
        mFormat = 0x40;
    } else {
        mFormat = 0x20;
    }
    if (header->mBits == 0x10) {
        mFormat = static_cast<uint8_t>(mFormat | 2);
    } else {
        mFormat = static_cast<uint8_t>(mFormat | 1);
    }
    SetSampleRate(header->mRate);
}

// Clears bits 4, 8 and 0x10, then sets 8 for 22050, 0x10 for 44100 and 4 for any other rate.
void TbSoundSampleCodec::SetSampleRate(uint16_t rate) {
    mFormat = static_cast<uint8_t>(mFormat & 0xfb);
    mFormat = static_cast<uint8_t>(mFormat & 0xf7);
    mFormat = static_cast<uint8_t>(mFormat & 0xef);
    if (rate == 0x5622) {
        mFormat = static_cast<uint8_t>(mFormat | 8);
        return;
    }
    if (rate == 0xac44) {
        mFormat = static_cast<uint8_t>(mFormat | 0x10);
        return;
    }
    mFormat = static_cast<uint8_t>(mFormat | 4);
}

uint32_t TbSoundSampleCodec::GetSampleRate() {
    if ((mFormat & 8) != 0) return 0x5622;
    if ((mFormat & 0x10) != 0) return 0xac44;
    return 0x2b11;
}

uint32_t TbSoundSampleCodec::GetBitRate() {
    return (mFormat & 2) != 0 ? 16 : 8;
}

uint32_t TbSoundSampleCodec::GetChannels() {
    return (mFormat & 0x40) != 0 ? 2 : 1;
}

// Not stopped: the bytes become samples (halved for stereo, halved again for 16 bits), the time is the sample's length in
// milliseconds at the virtual GetSampleRate (an unsigned division: the Mac's divwu leaves a zero divisor undefined, the
// emulator answers the dividend and so does this) plus 200 and the clock.
bool TbSoundSampleCodec::SetTime(bool stopped, uint32_t bytes) {
    if (stopped) return false;
    if ((mFormat & 0x40) != 0) bytes >>= 1;
    if ((mFormat & 2) != 0) bytes >>= 1;
    uint32_t rate = GetSampleRate();
    uint32_t product = bytes * 1000u;
    uint32_t span = rate == 0 ? product : product / rate;
    uint32_t time = span + 200;
    mTime = time + LbTime_GetClock();
    return true;
}

bool TbSoundSampleCodec::IsTimeComplete() {
    uint32_t now = LbTime_GetClock();
    return now >= mTime;
}

void TbSoundSampleCodec::SetDataPosition(uint32_t position) {
    mDataPosition = position;
}
uint32_t TbSoundSampleCodec::Timed() {
    return mTime;
}
