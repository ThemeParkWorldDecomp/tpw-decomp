// CServiceQueue (StreamableMpeg2.cpp of the Mac's sound library): 0x10007410 constructor, 0x1000726c destructor,
// 0x10007514 Close, 0x10007590 Poll.
#include "TbSoundSampleMPEG2.h"
#include <new>
#include "CMpegBase.h"
#include "TbSoundBank.h"
#include "CSprocketsModule.h"
#include "TbSoundPipeChannel.h"

CServiceQueue::CServiceQueue(TbSoundPipeChannel* channel, uint32_t last)
    : mChannel(channel), mSlots(last + 1), mLast(last), mItems(mSlots) {
    mFlags = 0;
}

CServiceQueue::~CServiceQueue() {
    for (uint32_t i = 0; i < mSlots; i++) USoundEvent_Free(static_cast<void**>(mItems)[i]);
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
