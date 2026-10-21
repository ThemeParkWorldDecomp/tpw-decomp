// ServiceQueue.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of ServiceQueue code, named after it.

#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundCore.h"
#include "TbSoundPipeChannel.h"
#include "TbSoundSampleMPEG2.h"
#include "TbSoundSampleStore.h"
#include "TbSoundSystemModule.h"
#include <cmath>
#include <cstring>
#include <new>

// TbSoundCore.cpp: the sound library's core (unit w79b, sound symbols shared). See include/TbSoundCore.h for the layouts, the
// vtables and the calling convention. The sample store (unit w79c), the device module (CSprocketsModule, unit p-sound) and the
// command bodies (unit w79d) are other units'.

// ---------------------------------------------------------------- CModuleFactory

TbError CModuleFactory::GenerateModule(TbSoundSystemModule*& module, const TbSoundSystemInfo& info, TbSoundSampleStore*) {
    module = new CSprocketsModule();
    TbError result = module->Init(info);
    if (static_cast<int32_t>(result) >= 0) return TbError(0);
    CloseModule(module);
    return TbError(-1);
}

void CModuleFactory::CloseModule(TbSoundSystemModule*& module) {
    if (module != nullptr) {
        module->Close();
        if (module != nullptr) delete module;
        module = nullptr;
    }
}

TbSoundSystemModule::~TbSoundSystemModule() = default;

// CServiceQueue (StreamableMpeg2.cpp of the Mac's sound library): 0x10007410 constructor, 0x1000726c destructor,
// 0x10007514 Close, 0x10007590 Poll.

CServiceQueue::~CServiceQueue() {
    for (uint32_t i = 0; i < mSlots; i++) USoundEvent_Free(static_cast<void**>(mItems)[i]);
}

CServiceQueue::CServiceQueue(TbSoundPipeChannel* channel, uint32_t last)
    : mChannel(channel), mSlots(last + 1), mLast(last), mItems(mSlots) {
    mFlags = 0;
}

void CServiceQueue::operator delete(void* object) { USoundChannel_Free(object); }

void CServiceQueue::Close() {
    mFlags |= 1;
    USoundEvent_Set(static_cast<void**>(mItems)[mLast]);
}

void CServiceQueue::Poll() {
    mFlags |= 2;
    while ((mFlags & 1) == 0) {
        uint32_t index = USoundQueue_WaitAny(mSlots, mItems, 0, 0xFFFFFFFFu);
        if (index == mLast) break;
        mChannel[index].FillServiceBuffer();   // the Mac steps 0x60 bytes per channel
    }
    mFlags &= ~2u;
}
