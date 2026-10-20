// TbSoundSampleMPEG2.h: the sound library's MPEG sample codecs (the Mac's "sound symbols shared" library, Code at 0x10000000, TOC
// base r2 = 0x1003abf0), unit w80b. _TbSoundSampleMPEG2 plays a sample held in memory (a TbFileBank's mapped file): it decodes the
// MPEG stream behind mDataStart through a CMpegBase (unit w71a, include/TbMpegBase.h) and hands the decoded bytes out like the WAV
// codec of unit w80 (include/TbSoundCodecs.h) does its own. _TbSoundSampleMPEG2File plays a sample streamed from the bank file
// (a TbMMFileBank): its decoder is a CMpegMacFilePlayerUnpooled (unit w80c, include/CMpegBase.h) that reads through the codec's
// Read and Seek. Both are pooled like the WAV codecs (see the top of include/TbSoundCodecs.h); the factory CCodecFactory
// (include/CCodecFactory.h) makes them.
//
// The Mac layouts (offsets are the Mac's; the host's pointers are native, so the host objects are larger):
//   _TbSoundSampleMPEG2 : TbSoundSampleCodec, 0x40 bytes: +0x2c mFlags (bit 0: the length came from the sample header), +0x30
//       mTotalBytes (the decoded length), +0x34 mMpeg (the decoder, NULL until OpenMpegFile), +0x38 (not used by this code),
//       +0x3c the pool link.
//   _TbSoundSampleMPEG2File : _TbSoundSampleMPEG2, 0x50 bytes: +0x40 the pool link, +0x44 mFile (the bank's file, not owned),
//       +0x48 mStart (the file offset of the sample's data), +0x4c mData (the sample's cached first bytes, the header's data
//       word).
// The vtables (the vptr points at the RTTI pair, the slots start at +8): _TbSoundSampleMPEG2 0x1003c2dc and
// _TbSoundSampleMPEG2File 0x1003bda8 are w80's pipe slot map (include/TbSoundCodecs.h, ending with +0x5c SetTime) with these
// overridden: +0x08 destructor (0x10007a74 / 0x10008458), +0x10 Release (0x1000e618), +0x24 GetDataChunk (0x1000e690), +0x28
// Restart (0x1000e850 / 0x100083a8), +0x2c SetInfo (0x1000e510), +0x38 SetDataStart (0x100084f0), +0x40 SetDataPosition
// (0x1000ec8c, empty), and four new ones: +0x60 SetNewDataPosition (0x1000ec48 / 0x10008410, empty), +0x64 OpenMpegFile (0x1000e8ec /
// 0x10007b0c), +0x68 CloseMpegFile (0x1000eb44 / 0x10008304) and +0x6c GetMpegBlock (0x1000ebe4 / 0x1000828c). Read and Seek of
// the file codec are not virtual: the decoder's file player calls them.
// The static cleanup objects _CS__TbSoundSampleMPEG2 (0x1000ed30) and _CS__TbSoundSampleMPEG2File (0x1000881c) clear the pools at
// exit; src/TbSoundSampleMPEG2.cpp keeps one of each, as src/TbMpegBase.cpp does for CMpegBase.
#pragma once
#include <cstddef>
#include <cstdint>

#include "TbMpegBase.h"
#include "TbSoundPipe.h"
#include "bfl_FileSystem.h"

class _TbSoundSampleMPEG2 : public TbSoundSampleCodec {
public:
    // The pool: statics 0x1003f24c (total), 0x1003f250 (current), 0x1003f254 (low), 0x1003f258 (high), 0x1003f25c (first), level
    // 0x1003c27c (initially 10).
    static _TbSoundSampleMPEG2* mFirst_TbSoundSampleMPEG2;
    static int32_t mCurrent_TbSoundSampleMPEG2;
    static int32_t mLow_TbSoundSampleMPEG2;
    static int32_t mHigh_TbSoundSampleMPEG2;
    static int32_t mTotal_TbSoundSampleMPEG2;
    static int32_t mLevel_TbSoundSampleMPEG2;

    _TbSoundSampleMPEG2(TbSampleHeader* header, TbRefRelease* owner);   // 0x1000e44c (116): the sample codec's constructor, then the vptr
    ~_TbSoundSampleMPEG2() override;                                    // +0x08, 0x10007a74 (104): deleting form frees 0x40 bytes
    void Release() override;                                            // +0x10, 0x1000e618 (68): CloseMpegFile, then the pipe's Release
    // Not timed: decodes min(size, bytes left) through GetMpegBlock and advances the position; at the end the timer is started
    // for the bytes just decoded plus 0x1200, as the WAV codec does. Timed: nothing is decoded, `done` is 0.
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) override;   // +0x24, 0x1000e690 (264)
    void Restart() override;                                            // +0x28, 0x1000e850 (104): position 0, the decoder reset
    TbError SetInfo(const TbSoundSampleInfo& info) override;            // +0x2c, 0x1000e510 (192): OpenMpegFile
    void SetDataStart(void* start) override;                            // +0x38, 0x100084f0 (16): also the decoder's stream
    void SetDataPosition(uint32_t position) override;                   // +0x40, 0x1000ec8c (4): nothing
    virtual void SetNewDataPosition();                                  // +0x60, 0x1000ec48 (4): nothing
    virtual TbError OpenMpegFile();                                     // +0x64, 0x1000e8ec (504)
    virtual void CloseMpegFile();                                       // +0x68, 0x1000eb44 (100)
    virtual int32_t GetMpegBlock(void* buffer, int32_t bytes);          // +0x6c, 0x1000ebe4 (40): CMpegBase::Read

    static void* operator new(std::size_t size);                        // 0x1000e1b4 (156)
    static void operator delete(void* object, std::size_t size);        // 0x1000e284 (112)
    static void TopUp();                                                // 0x1000e328 (176)
    static void SetLevel(int32_t level);                                // 0x1000e40c (12)
    static void ClearCache();                                           // 0x1000e120 (92)

    // Layer I streams are primed after OpenMpegFile (SetInfo) and after Restart by reading 0x300 bytes into these file-level
    // scratch buffers of the Mac (0x1003ef4c and 0x1003ec4c).
    static uint8_t sSetInfoScratch[0x300];
    static uint8_t sRestartScratch[0x300];

    uint32_t mFlags;                           // +0x2c bit 0: mTotalBytes is the header's length word
    uint32_t mTotalBytes;                      // +0x30
    CMpegBase* mMpeg;                          // +0x34
    // (+0x38 is not used by this code: the host class has no member for it)
    _TbSoundSampleMPEG2* mMPEG2PoolLink;       // +0x3c the pool's link while the object is idle

protected:
    // The rest of OpenMpegFile after the decoder opened (both codecs run the same code): the total length from the decoder
    // unless the header gave it, scaled when the decoder's rate differs from the codec's. Answers the TbError.
    TbError FinishOpen();
};

class _TbSoundSampleMPEG2File : public _TbSoundSampleMPEG2 {
public:
    // The pool: statics 0x1003ebd4 (total), 0x1003ebd8 (current), 0x1003ebdc (low), 0x1003ebe0 (high), 0x1003ebe4 (first), level
    // 0x1003bd1c (initially 10).
    static _TbSoundSampleMPEG2File* mFirst_TbSoundSampleMPEG2File;
    static int32_t mCurrent_TbSoundSampleMPEG2File;
    static int32_t mLow_TbSoundSampleMPEG2File;
    static int32_t mHigh_TbSoundSampleMPEG2File;
    static int32_t mTotal_TbSoundSampleMPEG2File;
    static int32_t mLevel_TbSoundSampleMPEG2File;

    // `start` is the file offset of the sample's data (TbMMFileBank passes the sample's offset + 0x28).
    _TbSoundSampleMPEG2File(bfl::FileSystem::CFile* file, TbSampleHeader* header, uint32_t start, TbRefRelease* owner);   // 0x1000798c (120)
    ~_TbSoundSampleMPEG2File() override;                                // +0x08, 0x10008458 (100): deleting form frees 0x50 bytes
    void Restart() override;                                            // +0x28, 0x100083a8 (48): position 0, the decoder reset
    void SetNewDataPosition() override;                                 // +0x60, 0x10008410 (4): nothing
    TbError OpenMpegFile() override;                                    // +0x64, 0x10007b0c (468)
    void CloseMpegFile() override;                                      // +0x68, 0x10008304 (100)
    int32_t GetMpegBlock(void* buffer, int32_t bytes) override;         // +0x6c, 0x1000828c (56): NULL decoder answers 0

    // The decoder's callbacks. Read answers -1 when no byte of the sample is left, else what the file's Read answers for
    // min(count, bytes left); Seek answers the position relative to the start of the sample's data.
    int32_t Read(void* buffer, uint32_t count);                         // 0x100080f4 (152)
    int32_t Seek(int32_t offset);                                       // 0x10008208 (80)

    static void* operator new(std::size_t size);                        // 0x100076e4 (156)
    static void operator delete(void* object, std::size_t size);        // 0x100077b8 (112)
    static void TopUp();                                                // 0x10007860 (176)
    static void SetLevel(int32_t level);                                // 0x10007948 (12)
    static void ClearCache();                                           // 0x1000764c (92)

    _TbSoundSampleMPEG2File* mMPEG2FilePoolLink;   // +0x40 the pool's link while the object is idle
    bfl::FileSystem::CFile* mFile;             // +0x44
    uint32_t mStart;                           // +0x48
    void* mData;                               // +0x4c
};

// The static cleanup objects (destructors that call ClearCache at program exit).
class _CS__TbSoundSampleMPEG2 {
public:
    ~_CS__TbSoundSampleMPEG2();                // 0x1000ed30 (84)
};

class _CS__TbSoundSampleMPEG2File {
public:
    ~_CS__TbSoundSampleMPEG2File();            // 0x1000881c (84)
};

// Unit v5739: CServiceQueue (StreamableMpeg2.cpp). Waits on a set of events and services the pipe channel (an array of 0x60-byte
// TbSoundPipeChannel) whose event fired. Mac layout 0x18 bytes: +0 vptr (0x1003bd10), +4 mChannel, +8 mSlots, +0xc mLast,
// +0x10 mItems (a CAutoArrayPtr, one pointer), +0x14 mFlags (bit 0: stop, bit 1: polling).
class TbSoundPipeChannel;
// Glue imports of the sound library (0x1002edc4: wait for any of the events, answers the index; 0x1002eddc: set an event;
// 0x1002edac: free an event).
uint32_t USoundQueue_WaitAny(uint32_t count, void** events, uint32_t flags, uint32_t timeout);
void USoundEvent_Set(void* event);
void USoundEvent_Free(void* event);
#include "snd_containers.h"
class CServiceQueue {
public:
    CServiceQueue(TbSoundPipeChannel* channel, uint32_t last);   // 0x10007410 (88)
    virtual ~CServiceQueue();                                    // +0x08, 0x1000726c (168), the deleting form frees through 0x1002ea94
    void Close();                                                // 0x10007514 (80)
    void Poll();                                                 // 0x10007590 (144)
    static void operator delete(void* object);

    TbSoundPipeChannel* mChannel;              // +0x04
    uint32_t mSlots;                           // +0x08 mLast + 1
    uint32_t mLast;                            // +0x0c
    CAutoArrayPtr<void*> mItems;               // +0x10
    uint32_t mFlags;                           // +0x14
};
