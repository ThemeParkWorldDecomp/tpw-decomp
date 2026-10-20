// TbSoundSampleStore.h: the sample store of the sound library ("sound symbols shared", Code at 0x10000000, TOC base
// r2 = 0x1003abf0), unit w79c. The store owns the array of TbSoundPipeChannel that TbSoundCore plays samples through; it finds
// a free channel (or steals the one with the lowest priority), builds the codec chain behind it and answers lookups by the
// handles it gave out. Mac size 0x10, no vtable: +0 mCore, +4 mMaxChannels, +8 mChannelsToUse (the "used" count the
// iterators walk), +0xc the CAutoArrayPtr<TbSoundPipeChannel> (a pointer to the first of mMaxChannels channels of 0x60 bytes).
// Lookups by index do not check the bounds; a handle's index is the top bits of its value (CUniqueHandle::GetIndex).
// Every function that returns a TbError does so through the hidden r3 buffer on the Mac, so its arguments start at r4.
#pragma once
#include <cstdint>

#include "TbError.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include "TbSoundPipeChannel.h"
#include "snd_containers.h"
#include "snd_handles.h"

class TbSoundCore;

class TbSoundSampleStore {
public:
    // The channels from the first up to the used count. Its end is the same sum whether or not the used count is zero, and
    // operator TbSoundPipeChannel*() answers the first channel even then; only operator++ ends a walk.
    class CChannelIterator {
    public:
        explicit CChannelIterator(TbSoundSampleStore* store);    // 0x1000ffa4 (88)
        TbSoundPipeChannel* operator++();                        // 0x1000fed8 (40): the next channel, or NULL at the end
        operator TbSoundPipeChannel*();                          // 0x1000ff44 (8): the current channel
        TbSoundPipeChannel* mCurrent;                            // +0
        TbSoundPipeChannel* mEnd;                                // +4
    };
    // Derives from the iterator and sets the same end again.
    class CUsedChannelIterator : public CChannelIterator {
    public:
        explicit CUsedChannelIterator(TbSoundSampleStore* store);   // 0x10010858 (80)
    };

    TbSoundSampleStore(TbSoundCore* core, uint32_t maxChannels); // 0x1000fa64 (72)
    ~TbSoundSampleStore();                                       // 0x10010054 (100), not virtual; the deleting call passes flag 1
    TbError Init(uint32_t channelsToUse);                        // 0x1000fc60 (164)
    void SetNumberOfChannelsToUse(const TbSoundSystemInfo& info);   // 0x10010568 (92)
    uint32_t GetNumberOfUsedChannels();                          // 0x1000be68 (8)
    uint32_t GetMaxNumberOfChannels();                           // 0x1000beb4 (8)
    TbSoundPipeChannel* operator[](uint32_t index) const;        // 0x10015478 (56)
    TbSoundPipeChannel* operator[](const CUniqueHandle& handle) const;   // 0x100100e8 (136)
    TbSoundPipeChannel* FindChannelAndPipe(const CUniqueHandle& handle, TbSoundSamplePipe*& pipe);   // 0x100101b0 (152)
    bool GetValidChannelIndexFromHandle(const CUniqueHandle& handle, uint32_t& index);   // 0x100102ac (132)
    int32_t ReleaseAllPipes();                                   // 0x1000fe34 (104): how many channels had a pipe
    void StopAllChannels(bool flag);                             // 0x1001038c (116)
    void StopAllChannelsWithVolumeControl(uint32_t mask);        // 0x1001043c (152): the stop is asked with the flag false
    TbSoundPipeChannel* GetFreeChannel(TbSoundSampleInfo& info); // 0x10010660 (256): the lowest priority is stolen with the flag true
    TbSoundSamplePipe* GetPipesFromHandle(void* handle, TbSoundPipeChannel** channel, TbSoundSampleInfo& info);   // 0x10010c90 (172)
    void* CreateNewSample(void* handle, TbSoundSampleInfo& info, uint32_t& index);   // 0x1001093c (248): the channel's handle value or 0
    void* CreateNewControl(void* handle, TbSoundSampleInfo& info);   // 0x10010b6c (212): the control's SetHandle answer or 0

    TbSoundCore* mCore;                              // +0
    uint32_t mMaxChannels;                           // +4
    uint32_t mChannelsToUse;                         // +8
    CAutoArrayPtr<TbSoundPipeChannel> mChannels;     // +0xc: 0x1000fb94 (136) counted constructor, 0x1000faec (104) destructor, 0x1000fd90 (8) conversion
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(TbSoundSampleStore) == 0x10, "the Mac store is 0x10 bytes");
#endif
