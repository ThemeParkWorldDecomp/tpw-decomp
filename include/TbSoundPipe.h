// TbSoundPipe.h: the sound library's sample pipe base (the Mac's "sound symbols shared" library, Code at 0x10000000, TOC base
// r2 = 0x1003abf0), unit w78. A sample played by the runtime flows through a chain of pipes: the channel at the head, codecs
// behind it. TbSoundSamplePipe is the chain link (attach, detach, find by handle, forward the calls down the chain) and owns the
// pooled allocation of the pipe objects; TbSoundSPCodec forwards the codec interface to the codec behind it (GetCodec);
// TbSoundSampleCodec is the base of every codec of the sound library (MPEG2, WAV, loop and timer: unit w80): the sample's
// size, data start, position, the format byte and the completion timer.
//
// The Mac layouts (the host's pointers are native, so the host objects are larger; the offsets here are the Mac's):
//   TbSoundSamplePipe 0x14 bytes: +0 vptr, +4 the pool link (only an idle object in the pool uses it), +8 mNext (the pipe behind
//       this one), +0xc mPrevious, +0x10 mHandle. TbSoundSPCodec has no members of its own (0x14 bytes).
//   TbSoundSampleCodec 0x2c bytes: +0x14 CRefManager (the owner's reference), +0x18 mDataSize, +0x1c mDataStart, +0x20 mTime,
//       +0x24 mDataPosition, +0x28 mFormat.
//
// The vtables (the vptr points at the RTTI pair, the slots start at +8 and hold function descriptors): TbSoundSamplePipe
// 0x1003c35c, TbSoundSPCodec 0x1003c560 and TbSoundSampleCodec 0x1003c21c. The slot order is the declaration order below:
//   +0x08 destructor, +0x0c Attatch, +0x10 Release, +0x14 Detatch, +0x18 FindPipe, +0x1c GetInfo, +0x20 GetSampleRate,
//   +0x24 GetDataChunk, +0x28 Restart, +0x2c SetInfo, +0x30 SetHandle, +0x34 Completed, +0x38 SetDataStart  (the pipe)
//   +0x3c GetDataStart, +0x40 SetDataPosition, +0x44 GetDataPosition, +0x48 GetDataSize, +0x4c SetSampleRate, +0x50 GetBitRate,
//   +0x54 GetChannels, +0x58 GetCodec  (TbSoundSPCodec adds these)
//   +0x5c SetTime  (TbSoundSampleCodec adds this one)
// The Mac destructors take a hidden flag in r4 (a halfword, deleted when positive: the object frees itself through operator
// delete with its own size, 0x14 for the pipe and the SP codec, 0x2c for the sample codec). The host mirrors that with the
// virtual destructor and the class's operator delete: `delete this` is the Mac's destructor call with flag 1, and the sizes
// of the three host classes tell the pooled ones from the others (the pipe and the SP codec are the same size, the sample
// codec is larger), as 0x14 and 0x2c do on the Mac.
// Every function that returns a TbError does so through the hidden r3 buffer on the Mac, so its arguments start at r4.
#pragma once
#include <cstddef>
#include <cstdint>

#include "TbError.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"

class TbSoundSPCodec;

class TbSoundSamplePipe {
public:
    // The pool's statics (the POOLED pattern, like CMpegBase's, include/TbMpegBase.h): the Mac keeps them in its data section.
    static TbSoundSamplePipe* mFirstTbSoundSamplePipe;   // 0x1003f2a4  the list of idle objects (linked through +4)
    static int32_t mCurrentTbSoundSamplePipe;            // 0x1003f298  how many are in it
    static int32_t mLowTbSoundSamplePipe;                // 0x1003f29c  the fewest it held after a take
    static int32_t mHighTbSoundSamplePipe;               // 0x1003f2a0  the most it ever held
    static int32_t mTotalTbSoundSamplePipe;              // 0x1003f294  objects taken from the list
    static int32_t mLevelTbSoundSamplePipe;              // 0x1003c34c  the level TopUp fills up to (initially 10)

    TbSoundSamplePipe();                                         // 0x10000344 (28): clears mNext, mPrevious and mHandle only
    virtual ~TbSoundSamplePipe();                                // +0x08, 0x10000248 (80)
    virtual TbSoundSamplePipe* Attatch(TbSoundSamplePipe* pipe); // +0x0c, 0x1000f440 (96)
    virtual void Release();                                      // +0x10, 0x1000f588 (108)
    virtual void Detatch();                                      // +0x14, 0x1000f834 (108)
    virtual TbSoundSamplePipe* FindPipe(void* handle);           // +0x18, 0x1000f7b0 (80)
    virtual void GetInfo(TbSoundSampleInfo& info);               // +0x1c, 0x1000f734 (56)
    virtual uint32_t GetSampleRate();                            // +0x20, 0x1000118c (64)
    virtual int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag);   // +0x24, 0x10001148 (8): -1
    virtual void Restart();                                      // +0x28, 0x10001204 (4): empty
    virtual TbError SetInfo(const TbSoundSampleInfo& info);      // +0x2c, 0x1000f694 (88)
    virtual void* SetHandle(void* handle);                       // +0x30, 0x10000bb8 (12)
    virtual void Completed(bool done);                           // +0x34, 0x1000f628 (56)
    virtual void SetDataStart(void* start);                      // +0x38, 0x1000123c (56)

    // Not virtual: asks `replacement` to attach this pipe behind it. `unused` is the Mac's second argument (never read).
    TbSoundSamplePipe* AttachReplace(TbSoundSamplePipe* replacement, TbSoundSamplePipe* unused);   // 0x1000f4e4 (68)

    static void* operator new(std::size_t size);                 // 0x1000ee48 (156)
    static void operator delete(void* object, std::size_t size); // 0x1000ef14 (112)
    static void TopUp();                                         // 0x1000efb8 (176)
    static void SetLevel(int32_t level);                         // 0x1000f098 (12)
    static void ClearCache();                                    // 0x1000edb8 (92)

    TbSoundSamplePipe* mPoolLink;     // +0x04 the pool's link while the object is idle; nothing else touches it
    TbSoundSamplePipe* mNext;         // +0x08 the pipe attached behind this one
    TbSoundSamplePipe* mPrevious;     // +0x0c the pipe this one is attached behind
    void* mHandle;                    // +0x10
};

// The adapter that forwards the codec interface to the codec behind it: every function but GetCodec is
// "this->GetCodec()->Same(arguments)" with the codec taken through the virtual slot +0x58 and NOT tested for NULL (the Mac
// would call through a NULL vtable). No members of its own.
class TbSoundSPCodec : public TbSoundSamplePipe {
public:
    TbSoundSPCodec();                                            // 0x10000644 (56)
    ~TbSoundSPCodec() override;                                  // +0x08, 0x100005b0 (104)

    void GetInfo(TbSoundSampleInfo& info) override;              // +0x1c, 0x1000b4a0 (76)
    uint32_t GetSampleRate() override;                           // +0x20, 0x1000b350 (60)
    void Restart() override;                                     // +0x28, 0x1000aff4 (60)
    TbError SetInfo(const TbSoundSampleInfo& info) override;     // +0x2c, 0x10011cd4 (100)

    virtual const uint8_t* GetDataStart();                       // +0x3c, 0x1000b0f4 (60)
    virtual void SetDataPosition(uint32_t position);             // +0x40, 0x1000b164 (76)
    virtual uint32_t GetDataPosition();                          // +0x44, 0x1000b1e8 (60)
    virtual uint32_t GetDataSize();                              // +0x48, 0x1000b25c (60)
    virtual void SetSampleRate(uint16_t rate);                   // +0x4c, 0x1000b2cc (76)
    virtual uint32_t GetBitRate();                               // +0x50, 0x1000b3c0 (60)
    virtual uint32_t GetChannels();                              // +0x54, 0x1000b430 (60)
    virtual TbSoundSPCodec* GetCodec();                          // +0x58, 0x1000866c (8): mNext
};

class TbSoundSampleCodec : public TbSoundSPCodec {
public:
    TbSoundSampleCodec(TbSampleHeader* header, TbRefRelease* owner);   // 0x1000d694 (144)
    TbSoundSampleCodec();                                        // 0x100137a4 (80): sets only mRefManager (to no owner) and mDataPosition
    ~TbSoundSampleCodec() override;                              // +0x08, 0x10000e1c (116)

    void GetInfo(TbSoundSampleInfo& info) override;              // +0x1c, 0x10008600 (36): the pipe's, down the chain
    uint32_t GetSampleRate() override;                           // +0x20, 0x1000d9b8 (48)
    TbError SetInfo(const TbSoundSampleInfo& info) override;     // +0x2c, 0x1000d774 (140)
    const uint8_t* GetDataStart() override;                      // +0x3c, 0x10008580 (8)
    void SetDataPosition(uint32_t position) override;            // +0x40, 0x1000dc1c (8)
    uint32_t GetDataPosition() override;                         // +0x44, 0x1000853c (8)
    uint32_t GetDataSize() override;                             // +0x48, 0x100085c0 (8)
    void SetSampleRate(uint16_t rate) override;                  // +0x4c, 0x1000d914 (104)
    uint32_t GetBitRate() override;                              // +0x50, 0x1000da20 (32)
    uint32_t GetChannels() override;                             // +0x54, 0x1000da78 (32)
    virtual bool SetTime(bool stopped, uint32_t bytes);          // +0x5c, 0x1000dad0 (148)

    uint8_t GetSampleFormat() const;                             // 0x1000d4c8 (8)
    void SetSampleFormatFromHeader(TbSampleHeader* header);      // 0x1000d848 (120)
    bool IsTimeComplete();                                       // 0x1000db98 (72)
    uint32_t Timed();                                            // 0x1000e818 (8)

    CRefManager mRefManager;          // +0x14 one reference on the owner
    uint32_t mDataSize;               // +0x18 the header's size word
    const uint8_t* mDataStart;        // +0x1c the header + 0x28 (see the constructor)
    uint32_t mTime;                   // +0x20 the millisecond clock at which the sample is over
    uint32_t mDataPosition;           // +0x24
    uint8_t mFormat;                  // +0x28 0x20 mono or 0x40 stereo; |1 8-bit or |2 16-bit; 4 (11025 Hz by default), 8 (22050)
                                      //       or 0x10 (44100): the sample rate, set by SetSampleRate
};

static_assert(sizeof(TbSoundSPCodec) == sizeof(TbSoundSamplePipe), "the SP codec adds no members: it is pooled like the pipe");
static_assert(sizeof(TbSoundSampleCodec) != sizeof(TbSoundSamplePipe), "the sample codec is not pooled");
