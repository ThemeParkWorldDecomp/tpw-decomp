// TimerCodec.cpp: name proven by __sinit_TimerCodec_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CSprocketsModule.h"
#include "helpers/TbSoundSampleWAVHelpers.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "snd_handles.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51032: CSprocketsChannelMac::SetDescription, the UReferenceCounter<USoundDescription> copy constructor and destructor,
// TbMapStreamer (CCatFixupTable::Unload, LoadCatgoryFixupTable, SetDroppable) and TbStaticFileBank (destructor, Deregister).

using namespace SamsUtilities;

// ---- TbStaticFileBank

// 0x10013278 (96)
void TbStaticFileBank::Deregister() {
    if (mData != nullptr) {
        DisposeBuffer_(mData);
        mData = nullptr;
    }
    if (mCache != nullptr) {
        DisposeBuffer_(mCache);
        mCache = nullptr;
    }
}

// Unit v51075: small functions of the sound library ("sound symbols shared"): the speaker module's conversions, the bank handle's
// forwards, the static cleanup objects and their __sinit_*_cpp, and a few tiny helpers.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1002eefc here

bool TbStaticFileBank::IsType(uint32_t type) const { return type == 3; }

// 0x10013350 (120): the Mac sets its vtable, deregisters through slot +0x3c, then ~TbFileBank.
TbStaticFileBank::~TbStaticFileBank() {
    TbStaticFileBank::Deregister();
}

TbStaticFileBank* TbStaticFileBank::Destruct(int16_t deleteFlag) {
    if (this != nullptr) {
        TbStaticFileBank::~TbStaticFileBank();
        if (deleteFlag > 0) USoundChannel_Free(this);
    }
    return this;
}

// The sound library's four small codecs (unit w80, program "sound symbols shared"): _TbSoundSampleWAV and
// _TbSoundSampleWAVFile (TbSoundSampleWAV.cpp, LbMpeg.cpp), _TbSoundCodecLoop (TbSoundCodecLoop.cpp) and TbTimerCodec
// (TimerCodec.cpp), with CMemoryCopy::Copy. See include/TbSoundCodecs.h for the layouts and the slot order.

namespace {

struct TimerPool {
    using Object = TbTimerCodec;
    static Object*& first() { return Object::mFirstTbTimerCodec; }
    static int32_t& current() { return Object::mCurrentTbTimerCodec; }
    static int32_t& low() { return Object::mLowTbTimerCodec; }
    static int32_t& high() { return Object::mHighTbTimerCodec; }
    static int32_t& total() { return Object::mTotalTbTimerCodec; }
    static int32_t& level() { return Object::mLevelTbTimerCodec; }
    static Object*& link(Object* object) { return object->mTimerPoolLink; }
};

}  // namespace

void TbTimerCodec::ClearCache() { PoolClearCache<TimerPool>(); }

// ---------------------------------------------------------------- TbTimerCodec

void* TbTimerCodec::operator new(std::size_t size) { return PoolNew<TimerPool>(size); }
void TbTimerCodec::operator delete(void* object, std::size_t size) { PoolDelete<TimerPool>(object, size); }
void TbTimerCodec::TopUp() { PoolTopUp<TimerPool>(); }
void TbTimerCodec::SetLevel(int32_t level) { mLevelTbTimerCodec = level; }
// The default sample codec constructor (no owner), the format byte, then SetTime through the virtual slot with the
// milliseconds + 0x1200 as the "byte" count (the codec's SetTime halves it for stereo and 16 bits, as for a real sample).
TbTimerCodec::TbTimerCodec(uint8_t format, uint32_t milliseconds) {
    mFormat = format;
    SetTime(false, milliseconds + 0x1200);
}

// Silence: the whole buffer is zeroed and counted as delivered; the answer is whether the time is over.
int32_t TbTimerCodec::GetDataChunk(void* buffer, int32_t size, int32_t& done, bool) {
    done = size;
    std::memset(buffer, 0, static_cast<std::size_t>(done));
    return IsTimeComplete();
}
TbTimerCodec::~TbTimerCodec() {}

uint32_t gCS_TbTimerCodecObject;           // 0x1003f314
uint32_t gCS_TbTimerCodecNode;             // 0x1003f318
void __sinit_TimerCodec_cpp() {
    __register_global_object(&gCS_TbTimerCodecObject, reinterpret_cast<void*>(&_CS_TbTimerCodec::Destruct), &gCS_TbTimerCodecNode);
}
