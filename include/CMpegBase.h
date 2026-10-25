// The sound library's MPEG file player (unit w80c; program "sound symbols shared", source files LbMpeg.cpp and
// StreamableMpeg2.cpp): CMpegMacFilePlayerUnpooled, the decoder that reads its stream from the bank file in two 8 KiB buffers
// through a Mac File Manager parameter block, SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10>, the pool that hands the
// players out, and UInterruptSafeQueue<unsigned char[28004]>, the Mac OS queue the pool keeps its elements in. The base class
// CMpegBase is unit w71a's (include/TbMpegBase.h, which this header includes). The one user is _TbSoundSampleMPEG2File
// (src/sound/StreamableMpeg2.cpp, src/sound/TbSoundSampleMPEG2.cpp, src/game/advisor/AdvisorScoring.cpp, src/sound/TbBank.cpp, src/sound/TbSoundSamplePipe.cpp); the host sound module (platform/sound_module.cpp) tops the pool up.
//
// The Mac player is 0x6d64 bytes; the first 0x24f0 are CMpegBase. The host object keeps the Mac's ORDER of members, with native
// pointers (so not its offsets); the Mac offset of each member is in its comment.
#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>

#include "TbMpegBase.h"
#include "bfl_FileSystem.h"

// The IO part of the Mac File Manager's ParamBlockRec (0x32 bytes): the fields PBReadSync and PBReadAsync take. Open clears it,
// the player sets ioRefNum, ioPosMode and ioPosOffset and RequestBuffer fills the rest per read. The Mac pointers the player
// never uses are plain words here.
struct MacFileIoParams {
    uint32_t qLink;          // 0x00
    int16_t qType;           // 0x04
    int16_t ioTrap;          // 0x06
    uint32_t ioCmdAddr;      // 0x08
    uint32_t ioCompletion;   // 0x0c
    int16_t ioResult;        // 0x10  > 0: a read is in flight (1, ioInProgress); 0 noErr; -39 eofErr; -51 rfNumErr
    uint32_t ioNamePtr;      // 0x12
    int16_t ioVRefNum;       // 0x16
    int16_t ioRefNum;        // 0x18  the file's Mac reference number (bfl::FileSystem::CFile::GetMacFileReference)
    int8_t ioVersNum;        // 0x1a
    int8_t ioPermssn;        // 0x1b
    uint32_t ioMisc;         // 0x1c
    uint8_t* ioBuffer;       // 0x20  the buffer the next read fills
    int32_t ioReqCount;      // 0x24  bytes asked for
    int32_t ioActCount;      // 0x28  bytes read
    int16_t ioPosMode;       // 0x2c  1: fsFromStart
    int32_t ioPosOffset;     // 0x2e  (a word at a 2-byte alignment on the Mac)
};

// The decoder over the bank file (Mac object 0x6d64 bytes in its pooled wrapper). Open takes the file, the file offset of the
// sample's data, its length and `data`, the cached first 0x2000 bytes of the sample that the sample header carries; the later
// bytes come from the file, 0x2000 at a time, into the two buffers in turn. -1 when the stream is not a valid MPEG one.
class CMpegMacFilePlayerUnpooled : public CMpegBase {
public:
    // 0x10007dac (60): CMpegBase's constructor, then the player's vtable; the members after CMpegBase are left as they are (the
    // pool hands out memory that was cleared once). 0x10003a30 (100): the base's destructor (which closes an open decoder), then,
    // for the deleting form, CMpegBase::operator delete with the player's size (so the heap's).
    CMpegMacFilePlayerUnpooled() {}
    ~CMpegMacFilePlayerUnpooled() override {}
    int32_t Open(bfl::FileSystem::CFile* file, int32_t start, int32_t size, char* data);   // 0x100031e4 (356)
    int32_t Close();                                                                       // 0x100039a0 (88)
    void SeekBits(int32_t byte) override;                                                  // 0x100034d4 (180)  slot +0x0c
    int32_t DecodeHeader() override;                                                       // 0x10003420 (116)  slot +0x10
    int32_t GetHeader() override;                                                          // 0x1000339c (72)   slot +0x14
    int32_t NextBuffer();                                                                  // 0x100035c4 (28)
    uint32_t ReadBuffered(void* out, uint32_t bytes);                                      // 0x10003620 (516)
    void RequestBuffer(int32_t buffer, bool sync);                                         // 0x10003868 (164)

    MacFileIoParams mIo;                       // +0x24f0
    uint8_t mBufferSpace[0x4012];              // +0x2522  room for two 0x2000 byte buffers and the 16-byte alignment
    uint8_t* mBuffers[3];                      // +0x6534  buffer 0, buffer 1 (the two read into) and [2], Open's `data`
    int32_t mCurrent;                          // +0x6540  the buffer being decoded: 0, 1, or 2 (`data`) until the first swap
    int32_t mReadPosition;                     // +0x6544  the read position inside it
    uint8_t mFrameSpace[0x810];                // +0x6548  room for the frame buffer (0x800 bytes) and its alignment
    uint8_t* mFrameBuffer;                     // +0x6d58  the aligned frame buffer (CMpegBase::mData points at it too)
    bfl::FileSystem::CFile* mFile;             // +0x6d5c
    int32_t mStart;                            // +0x6d60  the file offset of the sample's data
};

namespace SamsUtilities {

// The Mac OS queue the pool keeps its elements in (QHdr: qFlags +0, qHead +2, qTail +6; the count at +0xc). `Data` only gives
// the size of the data that follows each element: on the Mac unsigned char[28004], the player. Enqueue and Dequeue are the
// File Manager's (InterfaceLib), written out in the source file.
template <class Data>
class UInterruptSafeQueue {
public:
    // qLink +0, qType +4, the data +8 on the Mac; the host element is 16 bytes so that the data keeps the allocator's alignment.
    class alignas(16) Element {
    public:
        Element();                                                 // 0x1002bd2c (16)
        Element* GetNext();                                        // 0x10003da8 (8)
        void* GetData();                                           // 0x10003fe0 (8)
        static Element* GetElementFromDataPointer(void* data);    // 0x10007f88 (8)
        Element* mLink;                                            // +0x00  qLink
        uint32_t mType;                                            // +0x04  qType
    };

    UInterruptSafeQueue();                 // 0x100040ac (24)
    Element* GetHead();                    // 0x10003e08 (8)
    int32_t GetSize();                     // 0x1002bab0 (8)
    void Add(Element* element);            // 0x10007ec0 (68)
    bool Remove(Element* element);         // 0x10003e60 (88)

    uint16_t mFlags;                       // +0x00  qFlags
    Element* mHead;                        // +0x02  qHead
    Element* mTail;                        // +0x06  qTail
    int32_t mCount;                        // +0x0c
};

// The pool of file players (StreamableMpeg2.cpp, CSprocketsModule.cpp): operator new takes an idle player from the queue; on
// the Mac it answers NULL when none is idle (there is no heap fallback: the game keeps the pool full, MaintainPoolLevel, from
// the sound module's Tick); the port's fixes make a new player then (operator new, src/sound/StreamableMpeg2.cpp). Operator
// delete puts the player back. Level 10 is the target the pool is kept at. The members are defined
// for the one instance the game has, <CMpegMacFilePlayerUnpooled, 10> (src/sound/StreamableMpeg2.cpp), as explicit specializations.
template <class T, int Level>
class UPooled : public T {
public:
    using Queue = UInterruptSafeQueue<unsigned char[sizeof(T)]>;   // the Mac's unsigned char[28004]

    // The object whose destructor clears the pool at exit.
    class StaticCleanup {
    public:
        ~StaticCleanup();   // 0x10003b98 (88)
    };

    UPooled();                                                      // 0x10007d1c (56): the player's constructor, then the wrapper's vtable
    ~UPooled() override {}                                          // 0x100086a4 (100): the player's destructor, then (the deleting form) operator delete
    static void* operator new(std::size_t size);                   // 0x10007ffc (160)
    static void operator delete(void* object, std::size_t size);   // 0x10007e20 (68)

    static void ClearPool(uint8_t all);                            // 0x10003c58 (244)
    static bool IsPoolObjectAvailable(const T* player);            // 0x10003f40 (28)
    static void ReportPoolStatistics();                            // 0x10004040 (4)
    static void SetPoolKeepFull(uint8_t flag);                     // 0x1002b5ec (12)
    static void MaintainPoolLevel();                               // 0x1002b964 (232)
    static typename Queue::Element* MakeNewPoolElement();          // 0x1002bb08 (120)

    static Queue gPoolQueue;              // 0x1003e33c (constructed by __sinit_LbMpeg_cpp, 0x10003acc)
    static StaticCleanup gPoolCleanup;    // 0x1003e32a (its destructor registered there)
    static uint8_t gPoolKeepFull;         // 0x1003e338
    static uint32_t gPoolTarget;          // 0x1003943c, initially 10
};

using MpegFilePlayerPool = UPooled<CMpegMacFilePlayerUnpooled, 10>;


}  // namespace SamsUtilities

template <>
SamsUtilities::MpegFilePlayerPool::UPooled();
template <>
void* SamsUtilities::MpegFilePlayerPool::operator new(std::size_t size);
template <>
void SamsUtilities::MpegFilePlayerPool::operator delete(void* object, std::size_t size);
template <>
SamsUtilities::MpegFilePlayerPool::StaticCleanup::~StaticCleanup();
template <>
void SamsUtilities::MpegFilePlayerPool::ClearPool(uint8_t all);
template <>
bool SamsUtilities::MpegFilePlayerPool::IsPoolObjectAvailable(const CMpegMacFilePlayerUnpooled* player);
template <>
void SamsUtilities::MpegFilePlayerPool::ReportPoolStatistics();
template <>
void SamsUtilities::MpegFilePlayerPool::SetPoolKeepFull(uint8_t flag);
template <>
void SamsUtilities::MpegFilePlayerPool::MaintainPoolLevel();
template <>
SamsUtilities::MpegFilePlayerPool::Queue::Element* SamsUtilities::MpegFilePlayerPool::MakeNewPoolElement();
template <>
SamsUtilities::MpegFilePlayerPool::Queue SamsUtilities::MpegFilePlayerPool::gPoolQueue;
template <>
SamsUtilities::MpegFilePlayerPool::StaticCleanup SamsUtilities::MpegFilePlayerPool::gPoolCleanup;
template <>
uint8_t SamsUtilities::MpegFilePlayerPool::gPoolKeepFull;
template <>
uint32_t SamsUtilities::MpegFilePlayerPool::gPoolTarget;

// Unit v5123 (src/sound/CSprocketsModule.cpp)
namespace SamsUtilities {
class UAllocator;
template <class A>
class UManager {
public:
    void Allocate();   // 0x1002bc7c (84): stores a new 8-byte object (or NULL) in mObject
    void* operator->();   // 0x1002bbe4 (64), unit v51107: Allocate when mObject is NULL, then mObject
    void* mObject;     // +0x00
};
void* UAllocatorMake(int32_t size);   // 0x1002eac4, the allocation the sound library does (answer may be 0)
void UAllocatorInit(void* object);    // 0x1002ec8c, initialises the new object
template <>
void UManager<UAllocator>::Allocate();
template <>
void* UManager<UAllocator>::operator->();
}  // namespace SamsUtilities
