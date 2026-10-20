// TbSoundPipeChannel.h: the head of a sample's pipe chain in the sound library ("sound symbols shared", Code at 0x10000000,
// TOC base r2 = 0x1003abf0), unit w79c (w79a declared it with two accessors; the rest is here). Mac size 0x60, vtable
// 0x1003bfd0. A channel is a TbSoundSamplePipe whose mNext is the codec behind it; the sample store owns an array of them and
// the device module drains the 0x1200-byte service buffer. Every access to the chain goes through the channel's lock.
//
// Mac layout (the host's pointers are native, so the host object is larger; the offsets here are the Mac's):
//   +0x00 TbSoundSamplePipe base (0x14 bytes: vptr, pool link, +8 mNext, +0xc mPrevious, +0x10 base mHandle)
//   +0x14 mInUse (byte)  +0x18 mVolume  +0x1c mSampleFlags  +0x20 mFlags (bit 2: the channel carries a sample)
//   +0x24 mBufferSize  +0x28 mControlMask (bit i: the volume control's value i scales the volume)  +0x2c mField2C
//   +0x30 mHandle (the channel's own CUniqueHandle, distinct from the base's)  +0x34 mServiceResult
//   +0x38 mVolumeControl (a copy of the caller's pointer and count)  +0x40 mSampleHeader (the priority is its +8 word)
//   +0x54 mServiceBuffer (the 0x1220-byte allocation)  +0x58 mAlignedBuffer (32-byte aligned inside it)  +0x5c mLock
// The constructor does not set mSampleFlags (+0x1c), mField2C (+0x2c), mServiceResult (+0x34) or the header words but +8.
// Vtable slots: the inherited ones plus +0x08 destructor, +0x0c Attatch, +0x24 GetDataChunk, +0x2c SetInfo and the new +0x3c
// FindPipe(const CUniqueHandle&) (the base's FindPipe(void*) stays at +0x18).
// Every function that returns a TbError does so through the hidden r3 buffer on the Mac, so its arguments start at r4.
#pragma once
#include <cstdint>

#include "TbSoundPipe.h"
#include "snd_containers.h"
#include "snd_handles.h"

class TbSoundPipeChannel : public TbSoundSamplePipe {
public:
    TbSoundPipeChannel();                                        // 0x10009a9c (156)
    ~TbSoundPipeChannel() override;                              // +0x08, 0x1000ab10 (128), the deleting form frees through the pipe's pool delete with 0x60
    TbSoundSamplePipe* Attatch(TbSoundSamplePipe* pipe) override;   // +0x0c, 0x1000a6a0 (108): the base's, under the lock
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) override;   // +0x24, 0x1000a4ac (256)
    TbError SetInfo(const TbSoundSampleInfo& info) override;     // +0x2c, 0x1000a278 (152)
    using TbSoundSamplePipe::FindPipe;
    virtual TbSoundSamplePipe* FindPipe(const CUniqueHandle& handle);   // +0x3c, 0x1000a5e8 (116)

    uint32_t GetBufferSize() const { return mBufferSize; }       // 0x1001245c (8)
    void SetBufferSize(uint32_t size) { mBufferSize = size; }    // 0x10000f54 (8)
    uint32_t GetNextHandle();                                    // 0x10000bf8 (36): ++mHandle, the new value is in r3 (the listing says void)
    void SetSampleHeader(const TbSampleHeader& header);          // 0x10000f98 (52): keeps the priority word
    void SetPriority(uint32_t priority);                         // 0x10009b68 (8)
    uint32_t GetPriority();                                      // 0x1000c19c (8)
    void SetVolume(uint32_t volume);                             // 0x1000a238 (8)
    uint32_t GetActualVolume() const;                            // 0x1000c1dc (8)
    uint32_t GetSampleFlags();                                   // 0x1002c024 (8)
    TbSampleHeader* GetSampleHeader();                           // 0x1002cf24 (8): the address of +0x40
    uint32_t ControlledBy(uint32_t mask) const;                  // 0x10010520 (12)
    void* GetHandle();                                           // 0x100107ac (36): mHandle's value (r3)
    uint32_t GetIndex();                                         // 0x1000a19c (36)
    void SetIndex(uint16_t index);                               // 0x1000fd34 (40)
    TbError SetPipeInfo(void* handle, TbSoundSampleInfo& info);  // 0x10009e18 (208)
    void GetChannelInfo(uint32_t& used, uint32_t& priority);     // 0x10009f68 (144)
    void* InitChannelInfo(TbSoundSampleInfo& info, uint32_t& index);   // 0x1000a038 (220): answers mHandle's value
    static bool StreamerCallback(void* buffer, uint32_t size, void* pipe);   // 0x1000a358 (112): pipe is the third argument
    uint32_t FillServiceBuffer();                                // 0x1000a408 (104)
    TbSoundSamplePipe* GetChannelUsed();                         // 0x1000a754 (80): mNext
    bool ReleasePipe(bool flag);                                 // 0x1000a7e0 (208)
    uint32_t GetVolume();                                        // 0x1000a9b4 (224)

    uint8_t mInUse;                      // Mac +0x14, alignment supplies +0x16..17
    uint8_t mStreaming;                  // Mac +0x15: CCodecFactory::CreateCodec stores the bank's IsStreaming here (unit w80b)
    uint32_t mVolume;                    // +0x18
    uint32_t mSampleFlags;               // +0x1c
    uint32_t mFlags;                     // +0x20
    uint32_t mBufferSize;                // +0x24
    uint32_t mControlMask;               // +0x28
    uint32_t mField2C;                   // +0x2c
    CUniqueHandle mHandle;               // +0x30 (hides the base's mHandle)
    uint32_t mServiceResult;             // +0x34
    TbSoundVolumeControl mVolumeControl; // +0x38, 8 bytes on the Mac
    TbSampleHeader mSampleHeader;        // +0x40, 0x14 bytes; the priority is mField8 (+0x48)
    CAutoArrayPtr<unsigned char> mServiceBuffer;   // +0x54, owns the 0x1220-byte allocation
    unsigned char* mAlignedBuffer;       // +0x58
    CAutoCriticalSection mLock;          // +0x5c
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(TbSoundPipeChannel) == 0x60, "the Mac channel is 0x60 bytes");
#endif
