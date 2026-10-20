// TbSoundBank.h: the sound library's sample banks (the Mac's "sound symbols shared" library, Code at 0x10000000, TOC base
// r2 = 0x1003abf0): the Bullfrog .sdt bank reader TbBank / TbFileBank / TbMMFileBank (file TbMMFileBank.cpp in the library),
// the bank description TbSoundBankInfo, the factory CBankFactory (ChannelState.cpp) and the handle Bank::CBank handed to the
// codecs. Unit w72.
//
// A bank file is little-endian (the PC format): u32 count, u32 offsets[count], then per sample a 0x28-byte header and the
// sample's data (an MPEG stream). The Mac swaps every word with SamsUtilities::USwap32 and friends. On a little-endian host
// the swap helpers below do nothing, unless TPW_SWAP_FILE_WORDS is defined (a big-endian host swaps always). The unit test
// does not define it: the emulator reads the file's bytes as big-endian words and swaps them, the host reads the same bytes
// as little-endian words, and the values are compared.
//
// Vtable slots (byte offsets from the vptr, which points past the RTTI pair; the slots hold function descriptors):
//   TbBank      +8 Release, +0xc AddRef (imports of TbRefRelease), +0x10 destructor, +0x14 RegisterBank (pure),
//               +0x18 GetNumberOfSamples, +0x1c RetrieveSample (pure), +0x20 GetSampleLength, +0x24 GetSampleName,
//               +0x28 GetSamplePosition, +0x2c GetHandle, +0x30 IsType (pure), +0x34 IsStreaming, +0x38 Initialise,
//               +0x3c Deregister (pure). Vtables: TbBank 0x1003be84, TbFileBank 0x1003befc, TbMMFileBank 0x1003bf74.
//   CBankFactory (base CAudioFileSystem, 4 bytes: vptr 0x1003d5e0 with one pure slot): +8 GenerateBank, +0xc ConstructBank.
//               Vtable 0x1003c830.
//   Bank::CBank +8 Release, +0xc AddRef, +0x10 destructor. Vtable 0x1003c85c.
// Every function that returns a TbError does so through the hidden r3 buffer on the Mac, so its arguments start at r4
// (RegisterBank, RetrieveSample, Initialise, ConstructBank; Deregister answers nothing).
#pragma once

#include <cstddef>
#include <cstdint>

#include "TbError.h"
#include "TbRefRelease.h"
#include "TbSoundInfo.h"
#include "TbSoundCore.h"
#include "bfl_FileSystem.h"
#include "platform/samsutils.h"

// ---- the swap helpers (SamsUtilities, the sound library's own instances) -----------------------------------------------
// SHARED WITH w70: include/TbMapStreamer.h declares the same SamsUtilities::USwap32 / USwap16 templates (there they always
// swap). Eight instances live in this unit's range: USwap32<unsigned long> 0x10008db4, USwap16<unsigned short> 0x10008fd0
// and 0x1000942c, USwap32<unsigned int> 0x10009020 and 0x1000947c, USwap32<int> 0x10009078 and 0x100094d4,
// USwapBlock32<unsigned long> 0x10009528 (calls the platform's _USwapBlock32) and USwapInplace32<unsigned long> 0x10009588.
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define TB_SOUND_SWAP_FILE_WORDS 1
#endif

namespace SamsUtilities {

// A file's little-endian 32-bit word in the host's order (a byte reversal on the Mac).
template <class T>
inline T USwap32(T value) {
    static_assert(sizeof(T) == 4, "a 32-bit word");
#ifdef TB_SOUND_SWAP_FILE_WORDS
    uint32_t word;
    __builtin_memcpy(&word, &value, 4);
    word = __builtin_bswap32(word);
    __builtin_memcpy(&value, &word, 4);
#endif
    return value;
}

template <class T>
inline T USwap16(T value) {
    static_assert(sizeof(T) == 2, "a 16-bit word");
#ifdef TB_SOUND_SWAP_FILE_WORDS
    uint16_t word;
    __builtin_memcpy(&word, &value, 2);
    word = __builtin_bswap16(word);
    __builtin_memcpy(&value, &word, 2);
#endif
    return value;
}

// USwapInplace32<unsigned long> (0x10009588, 12 bytes: lwz, stwbrx): swaps the word where it is. (A plain overload: the
// platform header declares the general template for the main app's instances.)
inline void USwapInplace32(uint32_t& value) {
    value = USwap32(value);
}

// USwapBlock32<unsigned long> (0x10009528, 36 bytes): the platform's _USwapBlock32 over `count` words.
inline void USwapBlock32(uint32_t* words, int32_t count) {
    _USwapBlock32(words, count);
}

}  // namespace SamsUtilities

// The CodeWarrior runtime's double to unsigned long conversion (0x1002e988, 76 bytes): 0 below zero, 0xffffffff from 2^32 up
// and for a NaN, otherwise the value truncated.
uint32_t CvtDblUl(double value);

// ---- the sample header -------------------------------------------------------------------------------------------------
// The in-memory header of one sample, 0x14 bytes on the Mac (the data field is a Ptr32 there; native here). TbFileBank
// overlays it on the start of the file's own 0x28-byte header (only the first 0x14 bytes of the Mac layout are written, the
// data field is never set), TbMMFileBank keeps an array of them. The file header holds the size at +4, the rate at +0x18, the
// bits at +0x1a, the flags at +0x1b and the words at +0x1c and +0x20 (name at +8..+0x17, the first word is 0x28).
struct TbSampleHeader {
    int32_t mSize;       // +0x00 the sample's size in bytes, from file +4
    uint16_t mRate;      // +0x04 samples per second (0x5622 = 22050), from file +0x18
    uint8_t mBits;       // +0x06 0x10 for 16-bit samples, from file +0x1a
    uint8_t mFlags;      // +0x07 bit 0: stereo, from file +0x1b
    int32_t mField8;     // +0x08 from file +0x1c
    uint32_t mFieldC;    // +0x0c from file +0x20
    void* mData;         // +0x10 TbMMFileBank: a copy of the first min(size, 0x2000) bytes (NULL when that allocation failed)
};

// ---- CRefManager (TbMMFileBank.cpp, 0x10008940; destructor 0x10000ec0) -------------------------------------------------
// Holds one reference on the object the bank belongs to. The Mac constructor calls AddRef without looking at the pointer
// and the destructor calls Release when it is not NULL; the constructor here also tolerates NULL (the Mac would call
// through whatever low memory holds).
class CRefManager {
public:
    explicit CRefManager(TbRefRelease* owner) : mOwner(owner) {   // 0x10008940 (68 bytes)
        if (mOwner != nullptr) mOwner->AddRef();
    }
    ~CRefManager() {                                              // 0x10000ec0 (108 bytes)
        if (mOwner != nullptr) mOwner->Release();
    }
    CRefManager();          // 0x10013824 (12), unit v51075: no owner
    TbRefRelease* mOwner;   // +0
};

// ---- TbBank (abstract, 0x14 bytes; the derived banks are 0x20) ---------------------------------------------------------
class TbBank : public TbRefRelease {
public:
    explicit TbBank(TbRefRelease* owner);                                     // 0x100088a8 (104 bytes)
    ~TbBank() override;                                                       // 0x10008c28 (116 bytes)

    virtual TbError RegisterBank(const char* path, uint32_t size) = 0;        // +0x14
    virtual int32_t GetNumberOfSamples();                                     // +0x18, 0x10008ac0: the count
    virtual TbError RetrieveSample(TbSampleHeader** out, uint32_t index) = 0; // +0x1c, one-based index
    virtual uint32_t GetSampleLength(uint32_t index);                         // +0x20, 0x100089bc (212): milliseconds
    virtual const char* GetSampleName(uint32_t index);                        // +0x24, 0x10008af8: NULL
    virtual uint32_t GetSamplePosition(uint32_t index);                       // +0x28, 0x10008b2c: 0
    virtual bfl::FileSystem::CFile* GetHandle();                              // +0x2c, 0x10008b64: NULL
    virtual bool IsType(uint32_t type) const = 0;                             // +0x30
    virtual bool IsStreaming() const;                                         // +0x34, 0x10008b94: false
    virtual TbError Initialise(TbRefRelease** out);                           // +0x38, 0x10008bc8: *out = this
    virtual void Deregister() = 0;                                            // +0x3c

    // +8: TbFileBank the whole file (mapped by the file), TbMMFileBank the offsets array (count words, host order).
    uint8_t* mData = nullptr;
    uint32_t mCount = 0;          // +0x0c the number of samples
    CRefManager mRefManager;      // +0x10 one reference on the owner
};

// ---- TbFileBank (0x20 bytes) -------------------------------------------------------------------------------------------
// The whole file is mapped by the CFile and the sample headers are converted in place on first use.
class TbFileBank : public TbBank {
public:
    // The binary has no out-of-line constructor for this class; it is modelled on TbMMFileBank's (vtable 0x1003befc).
    TbFileBank(TbRefRelease* owner, bfl::FileSystem::CFile* file);
    ~TbFileBank() override;                                                   // 0x10009144 (136 bytes)

    TbError RegisterBank(const char* path, uint32_t size) override;           // 0x10008cc0 (192)
    TbError RetrieveSample(TbSampleHeader** out, uint32_t index) override;    // 0x10008e94 (248)
    const char* GetSampleName(uint32_t index) override;                       // 0x100090cc (8)
    bool IsType(uint32_t type) const override;                                // 0x10009108 (16): type == 2
    void Deregister() override;                                               // 0x10008e0c (88)

    bfl::FileSystem::CFile mFile;          // +0x14 a copy of the file handle (8 bytes on the Mac)
    TbSampleHeader** mCache = nullptr;     // +0x1c count header pointers, filled as samples are retrieved
};

// ---- TbMMFileBank (0x20 bytes) -----------------------------------------------------------------------------------------
// Reads the offsets and every sample's header at registration and keeps the first 0x2000 bytes of each sample.
class TbMMFileBank : public TbBank {
public:
    TbMMFileBank(TbRefRelease* owner, bfl::FileSystem::CFile* file);          // 0x100140a0 (96)
    ~TbMMFileBank() override;                                                 // 0x10009834 (136)

    TbError RegisterBank(const char* path, uint32_t size) override;           // 0x100091f4 (512)
    TbError RetrieveSample(TbSampleHeader** out, uint32_t index) override;    // 0x100096c8 (112)
    uint32_t GetSamplePosition(uint32_t index) override;                      // 0x100098e8 (20): the offset of the sample
    bfl::FileSystem::CFile* GetHandle() override;                             // 0x100097bc (8): &mFile
    bool IsType(uint32_t type) const override;                                // 0x100097f4 (16): type == 1
    bool IsStreaming() const override;                                        // 0x10009780 (8): true
    void Deregister() override;                                               // 0x100095d0 (200)

    bfl::FileSystem::CFile mFile;          // +0x14 a copy of the file handle
    TbSampleHeader* mHeaders = nullptr;    // +0x1c count headers
};

// ---- TbSoundBankInfo (0x10 bytes) --------------------------------------------------------------------------------------
// What a bank is made from. The constructors write only the fields they name (the others are left as they were).
class TbSoundBankInfo {
public:
    TbSoundBankInfo();                                                        // 0x100099d8 (24): type 0, size 0, path ""
    TbSoundBankInfo(uint32_t type, const char* path);                         // 0x10009a1c (12): type and path
    TbSoundBankInfo(uint32_t type, const char* path, uint32_t size);          // 0x10009a58 (16): type, path and size
    TbSoundBankInfo(uint32_t type, const char* path, bfl::FileSystem::CFile* file);   // 0x100277c4 (16): type, path and file
    bfl::FileSystem::CFile* GetFile() const;                                  // 0x10014150 (8)
    uint32_t GetSize() const;                                                 // 0x10014188 (8)
    const char* GetPath() const;                                              // 0x100143b0 (8)

    uint32_t mType;                     // +0
    uint32_t mSize;                     // +4
    const char* mPath;                  // +8
    bfl::FileSystem::CFile* mFile;      // +0xc
};

// ---- Bank::CBank (0xc bytes) -------------------------------------------------------------------------------------------
namespace Bank {
// What the codecs hold: a reference counted handle on a TbBank. The remaining members of the class (IsType,
// GetNumberOfSamples, GetSampleLength, GetSampleName at 0x1001463c..0x10014810) are in src/TbSoundCore_TbSpeaker_Mono_operator_int.cpp
// (units v51075, v5L21); unit w80b added the three
// the codec factory calls.
class CBank : public TbRefRelease {
public:
    explicit CBank(TbBank* bank);                                             // 0x100143e8 (76)
    ~CBank() override;                                                        // 0x10014464 (124): releases the bank
    operator bfl::FileSystem::CFile*() const;                                 // 0x10014508 (64): the bank's GetHandle, or NULL
    // 0x1001458c (8): the original is `operator TbRefRelease*()`, which answers the bank WITHOUT a test for NULL. A C++ conversion
    // to a base class is never used, so it has a name here.
    TbRefRelease* GetBankAsRefRelease() const;
    bool IsStreaming() const;                                                 // 0x100145cc (64): the bank's, false without a bank
    uint32_t GetSamplePosition(uint32_t index) const;                         // 0x10014720 (64): the bank's, 0 without a bank
    int32_t RetrieveSample(TbSampleHeader** out, uint32_t index) const;       // 0x10014884 (112): 1 on success, else 0
    // Unit v51075: the bank's, 0 (false, NULL) without a bank.
    bool IsType(uint32_t type) const;                                         // 0x1001463c (64): slot +0x30
    uint32_t GetSampleLength(uint32_t index) const;                           // 0x10014798 (64): slot +0x20
    int32_t GetNumberOfSamples() const;                                       // 0x100146a8 (64): slot +0x18 (unit v5L21)
    const char* GetSampleName(uint32_t index) const;                          // 0x10014810 (64): slot +0x24

    TbBank* mBank;   // +8
};
}  // namespace Bank

// ---- the sample info the codecs fill ---------------------------------------------------------------------------------------
// TbSoundSampleInfo (SetBank at 0x1000df50 is in src/TbSoundBank.cpp) is declared in TbSoundInfo.h (unit w77); that header
// does not include this one.

// ---- CAudioComponent and CAudioFileSystem (sound library, other units) -------------------------------------------------
// The audio component (CAudioComponentAccess at +0, TbRefRelease at +4 on the Mac) is unit w79b's: include/TbSoundCore.h. Only the
// TbRefRelease base is used here (ConstructBank hands the bank the component's base, +4, or NULL).

// The audio file system interface the bank factory implements (constructor 0x10027884, 12 bytes: it stores the vptr, vtable
// 0x1003d5e0 with a single pure slot at +8). A later unit's declaration replaces this one.
class CAudioFileSystem {
public:
    CAudioFileSystem() {}
    virtual ~CAudioFileSystem() {}
    virtual Bank::CBank* GenerateBank(const TbSoundBankInfo& info, CAudioComponent* component) = 0;   // +8
};

// ---- CBankFactory (4 bytes on the Mac, vtable 0x1003c830) -------------------------------------------------------------
class CBankFactory : public CAudioFileSystem {
public:
    CBankFactory();                                                           // 0x10027820 (56)
    // +8: makes the bank, registers it and wraps it; NULL when any step fails (the bank is released if its registration fails).
    Bank::CBank* GenerateBank(const TbSoundBankInfo& info, CAudioComponent* component) override;   // 0x100141c0 (316)
    // +0xc: *out = a new TbMMFileBank over the info's file, or NULL when the allocation fails.
    virtual TbError ConstructBank(TbBank*& out, const TbSoundBankInfo& info, CAudioComponent* component);   // 0x10013fb4 (140)
};

// ---- small functions of other classes the unit owns --------------------------------------------------------------------

namespace bfl {
namespace FileSystem {
class CFileStorage;
}
}  // namespace bfl

template <class Char>
class TbDynamicStringTemplate;

class CFileOperation {
public:
    // 0x1001e720 (56): appends "HD.sdt" to `name` (through the string's operator+= virtual, slot 0x30) and answers 1. The
    // storage, the first string and the number are not used. (The original takes `name` as a const reference.)
    static int32_t GetBankPath(bfl::FileSystem::CFileStorage& storage, TbDynamicStringTemplate<char>& path,
                               const TbDynamicStringTemplate<char>& name, int32_t flags);
};

namespace SamsUtilities {
// A debugging watcher over a string (0x10014354, 4 bytes: a bare return in this build).
class UMemoryWatcher {
public:
    explicit UMemoryWatcher(const UString& name);
};
}  // namespace SamsUtilities

// Unit v51032: TbStaticFileBank, a TbFileBank whose Deregister also frees the data at +8 (mData). Mac: vtable 0x1003c6b8.
class TbStaticFileBank : public TbFileBank {
public:
    ~TbStaticFileBank() override;                       // 0x10013350 (120): Deregister, ~TbFileBank
    // The Mac destructor with its hidden delete flag (r4): answers this; this may be NULL; a flag above 0 frees the object (0x1002ea94).
    TbStaticFileBank* Destruct(int16_t deleteFlag);
    void Deregister() override;                         // 0x10013278 (96): frees mData (+8) and mCache (+0x1c), each cleared
    bool IsType(uint32_t type) const override;          // 0x1001330c (16), unit v51075: type == 3
    // Unit v51045: the whole file into mData, mCount from its first word, mCache zeroed (count words); -1 and Deregister when
    // the buffer cannot be had. The Mac answers through the hidden r3 buffer; path and size are not read.
    TbError RegisterBank(const char* path, uint32_t size) override;   // 0x100130e8 (252)
    using TbFileBank::TbFileBank;
};

// 0x100130e8 (252): defined inline so every library holding the vtable links it (USwap32<unsigned long> is a byte swap).
void* AllocBuffer_(uint32_t bytes);   // 0x1002ebfc
inline TbError TbStaticFileBank::RegisterBank(const char*, uint32_t) {
    const int32_t size = mFile.GetSize();
    mData = static_cast<uint8_t*>(AllocBuffer_(static_cast<uint32_t>(size)));
    if (mData == nullptr) {
        Deregister();
        return TbError(-1);
    }
    mFile.Read(mData, static_cast<uint32_t>(size));
    mFile.Close();
    uint32_t first;
    __builtin_memcpy(&first, mData, 4);
    mCount = SamsUtilities::USwap32<uint32_t>(first);
    const uint32_t count = mCount;
    void* cache = AllocBuffer_(count << 2);
    mCache = static_cast<TbSampleHeader**>(__builtin_memset(cache, 0, count << 2));
    return TbError(0);
}

// Unit v51045: the static initialisers of TbSoundSamplePipe.cpp 0x1000f8d4 (68), TbMMFileBank.cpp 0x10009934 (120) and
// StreamableMpeg2.cpp 0x10008760 (140). Stand-ins for the Mac data they register (object, destructor word, record), named by
// their Mac addresses; the two guard bytes are the "already built" flags of the pool's queue and its cleanup object.
extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x1002eefc here
void __sinit_TbSoundSamplePipe_cpp();
void __sinit_TbMMFileBank_cpp();
void __sinit_StreamableMpeg2_cpp();
extern uint32_t gSinit_1003f2a8[1], gSinit_1003f2ac[3], gSinit_1003f284[1], gSinit_1003f288[3];   // objects and records
extern uint32_t gSinitDtor_10033d3c[2], gSinitDtor_10033d34[2], gSinitDtor_1003352c[2], gSinitDtor_100336dc[2];   // TVectors
extern uint32_t gSinit_1003ebe8[1], gSinit_1003ebec[3], gSinit_1003ebf8[3], gSinit_1003ebc8[3];
extern uint8_t gPoolQueueBuilt;     // 0x1003e329
extern uint8_t gPoolCleanupBuilt;   // 0x1003e328
