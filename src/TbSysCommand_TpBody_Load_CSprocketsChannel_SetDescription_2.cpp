// Unit v51032: CSprocketsChannelMac::SetDescription, the UReferenceCounter<USoundDescription> copy constructor and destructor,
// TbMapStreamer (CCatFixupTable::Unload, LoadCatgoryFixupTable, SetDroppable) and TbStaticFileBank (destructor, Deregister).
#include <cstdint>
#include <cstring>
#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "snd_commands.h"
#include "CSprocketsModule.h"
#include "platform/samsutils.h"

using namespace SamsUtilities;

// ---- UReferenceCounter<USoundDescription>

// 0x1002ad08 (68): the copy constructor; the object gains a reference when there is one. (The Mac answers this.)
void SamsUtilities::USoundDescriptionCounter_Copy(UReferenceCounter<USoundDescription>* self,
                                                  const UReferenceCounter<USoundDescription>* source) {
    self->mObject = source->mObject;
    if (self->mObject != nullptr) UReferenceCounted_AddReference(reinterpret_cast<UReferenceCounted*>(self->mObject));
}

// 0x1002ae58 (104): the destructor with the hidden delete flag; this may be NULL (nothing happens). The Mac answers this.
void SamsUtilities::USoundDescriptionCounter_Destruct(UReferenceCounter<USoundDescription>* self, int16_t flag) {
    if (self != nullptr) {
        if (self->mObject != nullptr) reinterpret_cast<UReferenceCounted*>(self->mObject)->RemoveReference();
        self->mObject = nullptr;
        if (flag > 0) USoundChannel_Free(self);
    }
}

// ---- CSprocketsChannelMac::SetDescription

// 0x1002a9f8 (332): one buffer of 0x1200 samples a channel, a sound description over it, loaded into the channel.
void CSprocketsChannelMac::SetDescription(TbSampleHeader& header) {
    Cleanup();
    int32_t channels = (header.mFlags & 1) != 0 ? 2 : 1;
    mBufferSize = static_cast<uint32_t>(channels * 0x1200);
    mBuffer = AllocBuffer_(mBufferSize << 1);
    if (mBuffer == nullptr) {
        UDebug("bailed: %s was nil", "mBuffer = new Unsigned16[mBufferSize]");
    } else {
        void* object = UAllocatorMake(0x58);
        if (object != nullptr) {
            USoundBuffer_Construct(object, mBuffer, mBufferSize, header.mBits, static_cast<double>(header.mRate), 0x4e4f4e45,
                                   channels);
        }
        if (object != nullptr) {
            UReferenceCounter<USoundDescription> counter;
            USoundDescriptionCounter_Make(&counter, static_cast<USoundDescription*>(object));
            UCommandLoadSound command(counter);
            SendToChannel(this, reinterpret_cast<UCommand&>(command));
            command.Destruct(-1);
            USoundDescriptionCounter_Destruct(&counter, -1);
            return;
        }
        UDebug("bailed: %s was nil",
               "description = new USoundBuffer(mBuffer, mBufferSize, header.BitRate, (Real) header.SampleRate, kSoundNotCompressed, "
               "channels)");
    }
    UDebug("bailed out");
    Cleanup();
}

// ---- TbMapStreamer

// 0x10023a78 (68): frees the entry array (Mac +4) and clears it; the count is left.
void TbMapStreamer::CCatFixupTable::Unload() {
    if (mData != nullptr) DisposeBuffer_(mData);
    mData = nullptr;
}

// 0x1002370c (68): loads the table, answers 0 (the load's own answer is dropped).
TbError TbMapStreamer::LoadCatgoryFixupTable(bfl::FileSystem::CFile* file) {
    mFixup.Load(*file);
    return TbError(0);
}

// 0x100231fc (224): every sound of every category is re-shuffled (droppable) or its sample count loses the low half.
void TbMapStreamer::SetDroppable(bool droppable) {
    for (uint32_t c = 0; c < mCatagories.Size(); ++c) {
        CCatagoryContainer* container = mCatagories[c];
        if (container != nullptr && container->mData != nullptr) {
            for (uint32_t e = 0; e < container->mData->mEventCount; ++e) {
                CEventData* event = &container->mData->mEvents[e];
                for (uint32_t s = 0; s < event->mSoundCount; ++s) {
                    CSoundData* sound = &event->mSounds[s];
                    if (droppable) {
                        ReShuffleSoundForDroppable(sound);
                    } else {
                        sound->mSampleCount = sound->mSampleCount >> 16;
                    }
                }
            }
        }
    }
}

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
