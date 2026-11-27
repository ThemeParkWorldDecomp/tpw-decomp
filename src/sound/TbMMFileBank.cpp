// TbMMFileBank.cpp: name proven by __sinit_TbMMFileBank_cpp in the sound library.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CMpegBase.h"
#include "CSprocketsModule.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbDynamicStringTemplate.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// The sound library's sample banks (file TbMMFileBank.cpp of the Mac's "sound symbols shared" library, with the factory and
// the bank handle from ChannelState.cpp). See include/TbSoundBank.h for the layouts, the vtables and the file format.
//
// Every allocation goes through the nothrow forms of operator new, new[] and delete[] (the library's own allocator pair on
// the Mac: __nw__FUl, __nwa__FUl and __dla__FPv), so that a failed allocation is a NULL result as it is there.

namespace {

uint32_t Load32(const uint8_t* where) {
    uint32_t value;
    std::memcpy(&value, where, 4);
    return value;
}

void Store32(uint8_t* where, uint32_t value) {
    std::memcpy(where, &value, 4);
}

uint16_t Load16(const uint8_t* where) {
    uint16_t value;
    std::memcpy(&value, where, 2);
    return value;
}

void Store16(uint8_t* where, uint16_t value) {
    std::memcpy(where, &value, 2);
}

}  // namespace

// The CodeWarrior runtime's double to unsigned long conversion (0x1002e988, 76 bytes): 0 below zero, 0xffffffff from 2^32 up
// and for a NaN, otherwise the value truncated (through fctiwz, with 2^31 taken off and put back above 2^31).
uint32_t CvtDblUl(double value) {
    if (value < 0.0) return 0;
    if (!(value < 4294967296.0)) return 0xffffffffu;
    const bool low = value < 2147483648.0;
    const double part = low ? value : value - 2147483648.0;
    const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(part));
    return low ? word : word + 0x80000000u;
}

// ---- TbFileBank --------------------------------------------------------------------------------------------------------

TbFileBank::TbFileBank(TbRefRelease* owner, bfl::FileSystem::CFile* file) : TbBank(owner), mFile(*file) {}

// 0x10008cc0 (192 bytes): the file maps itself whole; the count is its first word; the cache of resolved headers starts
// empty. No file image: the registration fails.
TbError TbFileBank::RegisterBank(const char* path, uint32_t size) {
    mData = static_cast<uint8_t*>(mFile.MapFile());
    if (mData == nullptr) {
        Deregister();
        return TbError(-1);
    }
    mCount = SamsUtilities::USwap32(Load32(mData));
    const uint32_t count = mCount;
    mCache = new (std::nothrow) TbSampleHeader*[count];
    // (the original clears the new block without looking at the pointer)
    if (mCache != nullptr) std::memset(mCache, 0, count * sizeof(TbSampleHeader*));
    return TbError(0);
}

// 0x10008e0c (88 bytes): closes the file and frees the cache (the file image is the file's).
void TbFileBank::Deregister() {
    mFile.Close();
    if (mCache != nullptr) {
        delete[] mCache;
        mCache = nullptr;
    }
}

// 0x10008e94 (248 bytes): sample `index` (one-based) is converted in place on first use. The file's header becomes the
// in-memory one: the size moves from +4 to +0, the rate from +0x18 to +4, the two bytes from +0x1b and +0x1a to +7 and +6, the
// words from +0x1c and +0x20 to +8 and +0xc. The offsets table is read from the word after the count, so sample `index`
// is at word `index` (the table has `count` entries after the count word). An index above the count fails.
TbError TbFileBank::RetrieveSample(TbSampleHeader** out, uint32_t index) {
    if (index > mCount) return TbError(-1);
    TbSampleHeader** slot = mCache + static_cast<int32_t>(index - 1);
    if (*slot == nullptr) {
        uint8_t* file = mData + SamsUtilities::USwap32(Load32(mData + index * 4));
        *out = reinterpret_cast<TbSampleHeader*>(file);
        Store32(file + offsetof(TbSampleHeader, mSize), SamsUtilities::USwap32(static_cast<int32_t>(Load32(file + 4))));
        Store32(file + offsetof(TbSampleHeader, mFieldC), SamsUtilities::USwap32(Load32(file + 0x20)));
        Store32(file + offsetof(TbSampleHeader, mField8), SamsUtilities::USwap32(static_cast<int32_t>(Load32(file + 0x1c))));
        Store16(file + offsetof(TbSampleHeader, mRate), SamsUtilities::USwap16(Load16(file + 0x18)));
        file[offsetof(TbSampleHeader, mFlags)] = file[0x1b];
        file[offsetof(TbSampleHeader, mBits)] = file[0x1a];
        *slot = *out;
    } else {
        *out = *slot;
    }
    return TbError(0);
}

// 0x100090cc (8 bytes)
const char* TbFileBank::GetSampleName(uint32_t index) {
    return nullptr;
}

// 0x10009108 (16 bytes)
bool TbFileBank::IsType(uint32_t type) const {
    return type == 2;
}

// 0x10009144 (136 bytes): drops everything (the virtual Deregister), then the file handle and the base.
TbFileBank::~TbFileBank() {
    Deregister();
}

// 0x100091f4 (512 bytes): reads the count (four bytes at the start), the offsets and, for each sample, its 0x28-byte header
// at its offset (converted into the header array) and the first min(size, 0x2000) bytes of its data. A file that does not
// answer the first seek or the four bytes fails (after Deregister). The reads after those are not checked, nor are the
// allocations of the offsets (the original reads into the result without a test); an allocation failure of the header array
// or of a sample's data leaves it out (the registration still succeeds).
TbError TbMMFileBank::RegisterBank(const char* path, uint32_t size) {
    if (mFile.Seek(0, 0) != -1 && mFile.Read(&mCount, 4) == 4) {
        SamsUtilities::USwapInplace32(mCount);
        mData = new (std::nothrow) uint8_t[mCount << 2];
        mFile.Read(mData, mCount << 2);
        SamsUtilities::USwapBlock32(reinterpret_cast<uint32_t*>(mData), static_cast<int32_t>(mCount));
        mHeaders = new (std::nothrow) TbSampleHeader[mCount];
        if (mHeaders != nullptr) {
            const uint32_t* offsets = reinterpret_cast<const uint32_t*>(mData);
            // (the original reads every header into the same uninitialised stack buffer: a short read leaves the rest of
            // the previous sample's header in it)
            uint8_t raw[0x28] = {};
            for (uint32_t i = 0; i < mCount; ++i) {
                mFile.Seek(offsets[i], 0);
                mFile.Read(raw, 0x28);
                TbSampleHeader* header = &mHeaders[i];
                header->mSize = SamsUtilities::USwap32(static_cast<int32_t>(Load32(raw + 4)));
                header->mFieldC = SamsUtilities::USwap32(Load32(raw + 0x20));
                header->mField8 = SamsUtilities::USwap32(static_cast<int32_t>(Load32(raw + 0x1c)));
                header->mRate = SamsUtilities::USwap16(Load16(raw + 0x18));
                header->mFlags = raw[0x1b];
                header->mBits = raw[0x1a];
                int32_t bytes = header->mSize;
                if (bytes > 0x2000) bytes = 0x2000;
                header->mData = new (std::nothrow) uint8_t[static_cast<uint32_t>(bytes)];
                if (header->mData != nullptr) mFile.Read(header->mData, static_cast<uint32_t>(bytes));
            }
        }
        return TbError(0);
    }
    Deregister();
    return TbError(-1);
}

// 0x100095d0 (200 bytes): closes the file, frees the offsets, then the data of every header from the last to the first and
// the header array.
void TbMMFileBank::Deregister() {
    mFile.Close();
    if (mData != nullptr) {
        delete[] mData;
        mData = nullptr;
    }
    if (mHeaders != nullptr) {
        for (uint32_t i = mCount; i != 0; --i) {
            if (mHeaders[i - 1].mData != nullptr) delete[] static_cast<uint8_t*>(mHeaders[i - 1].mData);
        }
        delete[] mHeaders;
        mHeaders = nullptr;
    }
}
// 0x100096c8 (112 bytes): the header of sample `index` (one-based) in the array; an index above the count fails, and so does
// a bank without headers (which also answers NULL).
TbError TbMMFileBank::RetrieveSample(TbSampleHeader** out, uint32_t index) {
    if (index > mCount) return TbError(-1);
    if (mHeaders == nullptr) {
        *out = nullptr;
        return TbError(-1);
    }
    *out = mHeaders + static_cast<int32_t>(index - 1);
    return TbError(0);
}

// 0x10009780 (8 bytes)
bool TbMMFileBank::IsStreaming() const {
    return true;
}

// 0x100097bc (8 bytes)
bfl::FileSystem::CFile* TbMMFileBank::GetHandle() {
    return &mFile;
}

// 0x100097f4 (16 bytes)
bool TbMMFileBank::IsType(uint32_t type) const {
    return type == 1;
}

// 0x10009834 (136 bytes)
TbMMFileBank::~TbMMFileBank() {
    Deregister();
}

// 0x100098e8 (20 bytes): the file offset of sample `index` (one-based, not checked).
uint32_t TbMMFileBank::GetSamplePosition(uint32_t index) {
    return reinterpret_cast<const uint32_t*>(mData)[static_cast<int32_t>(index - 1)];
}

// Unit v51045: TbStaticFileBank::RegisterBank and three static initialisers of the sound library.

extern uint32_t gSinitDtor_10033d3c[2], gSinitDtor_10033d34[2], gSinitDtor_1003352c[2], gSinitDtor_100336dc[2];

extern uint32_t gSinit_1003ebe8[1], gSinit_1003ebec[3], gSinit_1003ebf8[3], gSinit_1003ebc8[3];

extern uint8_t gPoolQueueBuilt;

extern uint8_t gPoolCleanupBuilt;

using Pool = SamsUtilities::MpegFilePlayerPool;

// 0x10009934 (120)
void __sinit_TbMMFileBank_cpp() {
    if (static_cast<int8_t>(gPoolQueueBuilt) == 0) {
        new (&Pool::gPoolQueue) Pool::Queue();
        gPoolQueueBuilt = 1;
    }
    if (static_cast<int8_t>(gPoolCleanupBuilt) == 0) {
        __register_global_object(&Pool::gPoolCleanup, gSinitDtor_1003352c, gSinit_1003ebf8);
        gPoolCleanupBuilt = 1;
    }
}
