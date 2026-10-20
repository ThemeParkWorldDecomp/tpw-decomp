// TbMapStreamer and its in-place map records, sound library 0x1001f410..0x10023cd8.
#pragma once
#include <cstddef>
#include <cstdint>
#include "Ptr32.h"
#include "TbRefRelease.h"
#include "TbSoundPosition.h"
#include "bfl_FileSystem.h"
#include "snd_containers.h"

struct CEventData;
struct CSoundData;
struct CSampleData;
struct CSoundBranchData;

#pragma pack(push, 1)
// GUID: u32 at 0, u16 at 4 and 6, eight unchanged bytes at 8.
struct TbGUID {
    uint32_t mData1;
    uint16_t mData2;
    uint16_t mData3;
    uint8_t mData4[8];
    bool operator==(const TbGUID&) const;   // 0x10021ae4
    void Byteswap();                       // 0x10021be4
};
struct CFileHeader {
    CFileHeader() : mVersion(1) {}          // 0x10021c90; GUID untouched
    void Byteswap();                       // 0x10021b80
    TbGUID mGUID;                          // +0
    uint32_t mVersion;                     // +0x10
};
struct CBankData {
    uint32_t mF00;                         // +0, zeroed by reader
    uint8_t mF04;                          // +4, zeroed by reader
    Ptr32<char> mName;                     // +5
    uint8_t mFlags;                        // +9
    uint8_t mF0A;                          // +0xa, untouched
};
struct CBankDataMap {
    void Byteswap();                       // 0x100219c8
    Ptr32<CBankData> mBanks;               // +0
    uint32_t mCount;                       // +4
};
struct CCatagoryData {
    void Byteswap();                       // 0x1002268c
    uint32_t mEventCount;                  // +0
    Ptr32<CEventData> mEvents;             // +4
    uint16_t mF08;
    uint16_t mFlags;                       // +0xa
    float mF0C;
    float mF10;
    float mF14;
};
struct CSFXDataMap {
    void Byteswap();                       // 0x10022060
    Ptr32<CCatagoryData> mCatagories;
    uint32_t mCount;
};
struct CEventData {
    void Byteswap();                       // 0x1002259c
    uint32_t mID;                          // +0
    uint32_t mSoundCount;                  // +4
    Ptr32<CSoundData> mSounds;             // +8
    uint32_t mF0C;
    uint16_t mFlags;                       // +0x10
    uint16_t mF12;
};
struct CSoundData {
    void Byteswap();                       // 0x10022970
    uint32_t mSampleCount;                 // +0, only low 16 bits count
    uint32_t mBranchCount;                 // +4
    Ptr32<CSampleData> mSamples;           // +8
    uint8_t mF0C[4];                       // +0xc, not swapped
    uint16_t mF10;
    uint16_t mF12;
    uint8_t mF14[2];                       // +0x14, not swapped
    uint16_t mF16;
    uint16_t mF18;
    uint16_t mF1A;
    uint16_t mF1C;
    uint32_t mF1E;                         // +0x1e, old-version cumulative value
    float mF22;
    Ptr32<CSoundBranchData> mBranches;     // +0x26
};
struct CSampleData {
    void Byteswap();                       // 0x10022e00
    uint32_t mF00;
    uint32_t mF04;
    uint32_t mF08;
    uint16_t mBankIndex;                   // +0xc
    uint16_t mF0E;
};
struct CSoundBranchData {
    void Byteswap();                       // 0x10022cec
    Ptr32<CSoundData> mSound;              // +0, file holds 1-based index
    uint16_t mF04;
    uint8_t mF06[2];                       // +6, not swapped
};
#pragma pack(pop)
static_assert(sizeof(TbGUID) == 16);
static_assert(sizeof(CFileHeader) == 20);
static_assert(sizeof(CBankData) == 11);
static_assert(sizeof(CBankDataMap) == 8);
static_assert(sizeof(CSFXDataMap) == 8);
static_assert(sizeof(CCatagoryData) == 24);
static_assert(sizeof(CEventData) == 20);
static_assert(sizeof(CSoundData) == 42);
static_assert(sizeof(CSampleData) == 16);
static_assert(sizeof(CSoundBranchData) == 8);
static_assert(offsetof(CBankData, mName) == 5);
static_assert(offsetof(CBankData, mFlags) == 9);
static_assert(offsetof(CCatagoryData, mEvents) == 4);
static_assert(offsetof(CEventData, mSounds) == 8);
static_assert(offsetof(CEventData, mFlags) == 0x10);
static_assert(offsetof(CSoundData, mF1E) == 0x1e);
static_assert(offsetof(CSoundData, mF22) == 0x22);
static_assert(offsetof(CSoundData, mBranches) == 0x26);
static_assert(offsetof(CSampleData, mBankIndex) == 0xc);

// These sound-library inline bit swaps are distinct from the platform's file-endian helpers.
namespace sound::SamsUtilities {
// The library emits the same bit permutation for integers, pointers and floats.
template<class T> T USwap32(T value) {
    static_assert(sizeof(T) == 4);
    uint32_t word;
    __builtin_memcpy(&word, &value, 4);
    word = __builtin_bswap32(word);
    __builtin_memcpy(&value, &word, 4);
    return value;
}
template<class T> T USwap16(T value) {
    static_assert(sizeof(T) == 2);
    uint16_t word;
    __builtin_memcpy(&word, &value, 2);
    word = __builtin_bswap16(word);
    __builtin_memcpy(&value, &word, 2);
    return value;
}
template<class T> void USwapInplace32(T& value) { value = USwap32(value); }
template<class T> void USwapInplace16(T& value) { value = USwap16(value); }
template<class T> void USwapBlock32(T* words, int32_t count) {
    for (int32_t i = 0; i < count; ++i) USwapInplace32(words[i]);
}
inline float USwapFloat(float value) { return USwap32(value); }
inline void USwapInplaceFloat(float& value) { USwapInplace32(value); }
template<class T> void USwapBlockStruct(T* data, int32_t count) {
    while (count-- != 0) (data++)->Byteswap();
}
}  // namespace sound::SamsUtilities

class TbBank;
class TbSoundManager;
class CCatagoryContainer : public TbRefRelease {
public:
    struct CElement {
        CElement(CElement* next, uint32_t value) : mNext(next), mValue(value) {} // 0x1001f684
        void Release();                            // 0x1001f50c, recursively tail first
        CElement* mNext;                           // Mac +0
        uint32_t mValue;                           // Mac +4
    };
    explicit CCatagoryContainer(uint32_t index);    // 0x1001fc30
    ~CCatagoryContainer() override;                // 0x1001f410
    bool Find(uint32_t value);                     // 0x1001f590
    void Add(uint32_t value);                      // 0x1001f5f0
    CCatagoryData* mData;                          // Mac +8
    uint32_t mIndex;                               // Mac +0xc
    CElement* mElements;                           // Mac +0x10
    TbDynamicStringTemplate<char> mName;           // Mac +0x14
};
class CBankContainer : public TbRefRelease {
public:
    explicit CBankContainer(const CBankData&);      // 0x10021914
    ~CBankContainer() override {}                   // Unit v5573: +0x08, 0x10023afc (136); inline so every library gets the vtable
    TbBank* mBank;                                 // Mac +8
    TbDynamicStringTemplate<char> mName;           // Mac +0xc
    uint32_t mFlags;                               // Mac +0x20, whole word stores byte
    bfl::FileSystem::CFileStorage mStorage;        // Mac +0x24
};
struct CStorageContainer {
    CStorageContainer() { mValid = 0; }            // 0x100204ac; storage constructed before flag is cleared
    ~CStorageContainer() {}                        // 0x1002041c
    uint8_t mValid;                                // Mac +0
    bfl::FileSystem::CFileStorage mStorage;        // Mac +4
};

class TbMapStreamer {
public:
    struct CCatFixupTable {
        CCatFixupTable() : mData(nullptr), mCount(0) {} // 0x100201c0
        uint32_t* mData;                           // Mac +0
        uint32_t mCount;                           // Mac +4
        // Unit v5234. NOTE: the listing of Load shows the count at Mac +0 and the data pointer at Mac +4 (the order of the two
        // comments above is swapped); the code uses the names by meaning: mCount is the count, mData the entry array.
        TbError Load(bfl::FileSystem::CFile& file);  // 0x1002388c (336), answers 0
        void Unload();                               // 0x10023a78 (68), NOT translated yet (a stand-in)
    };
    TbMapStreamer();                              // 0x1001f784
    static TbMapStreamer* Instance();              // 0x1001f6ec
    virtual ~TbMapStreamer();                      // 0x1002020c, only virtual
    void SetSoundManager(TbSoundManager*);         // 0x10016760
    CResize<CCatagoryContainer*>& GetCatagoryDeque() { return mCatagories; } // 0x10017274
    CResize<CBankContainer*>& GetBankDeque() { return mBanks; } // 0x100273cc
    TbError SetPath(const char*);                  // 0x1002030c
    TbError RegisterCatagory(const char*, const char*, uint32_t&); // 0x1002071c
    TbError RegisterCatagory(const char*, uint32_t&); // 0x1002051c
    void ObtainCorrectStorage(CStorageContainer&, const TbDynamicStringTemplate<char>&); // 0x10020b4c
    void ReadBankDataFromMap(bfl::FileSystem::CFile&, CBankDataMap&); // 0x10020d74
    bool BankDoesNotExist(uint32_t&, const char*, const TbDynamicStringTemplate<char>&, CStorageContainer&); // 0x10020f0c
    char* RemovePath(char*, uint32_t);              // 0x100215f8
    TbError RegisterBankData(bfl::FileSystem::CFile&, const char*); // 0x10021668
    TbError RegisterBankDataFromStorage(const char*, const char*); // 0x10021cc4
    bool IsSFXHeaderValid(const CFileHeader&);       // 0x10021e30
    TbError RegisterSFXData(bfl::FileSystem::CFile&, uint32_t&); // 0x10021ecc
    TbError RegisterSFXDataFromStorage(const char*, uint32_t&); // 0x1002213c
    uint8_t* ReadSampleData(uint8_t*, CCatagoryData**, uint32_t); // 0x10022290
    void ModifyEventForSense(CEventData*);          // 0x10022358
    char* ReadCatagory(char*, CCatagoryData*);       // 0x10022410
    char* ReadEvents(char*, CEventData*);            // 0x10022788
    char* ReadSounds(char*, CSoundData*);            // 0x10022b10
    TbError RemoveCatagory(uint32_t);              // 0x10022e78
    TbError UnloadCatagory(CCatagoryContainer*);    // 0x10022f1c
    uint32_t FindCatagory(const char*);             // 0x100237a4
    // 0x10023378 (44): maps a category index read from a save file to the live one. NOT translated yet (a stand-in); called by
    // CAudioPlaceHolder::Load.
    uint32_t FixUpCatagoryIndex(uint32_t index);
    // The category fix-up table of a save game, written and read after the list's header (unit w79g1: CAudioPlaceHolderList::Save
    // and Load). NOT translated yet (stand-ins). Save and Load answer a TbError through the hidden r3 buffer (this in r4, the file in
    // r5; the list drops the answers); Unload has only this.
    TbError SaveCatgoryFixupTable(bfl::FileSystem::CFile* file);   // 0x100233e0 (576)
    TbError LoadCatgoryFixupTable(bfl::FileSystem::CFile* file);   // 0x1002370c (68)
    void UnloadCatgoryFixupTable();                                // 0x10023310 (40)
    // Unit v5047: 0x1002309c (276): moves every sample whose bank is droppable to the end of the sound's sample list (a
    // partition from both ends) and leaves the count in the low half of mSampleCount sign-extended-added (see the source).
    void ReShuffleSoundForDroppable(CSoundData* sound);
    // 0x100231fc (224): applies the droppable level to every sound of every category. NOT translated yet (a stand-in); called by
    // TbSoundManager::SetDroppable.
    void SetDroppable(bool droppable);
    static TbSoundManager* mpSoundManager;          // sound 0x1003f3b4
    CCatFixupTable mFixup;                         // Mac +4
    CResize<TbDynamicStringTemplate<char>> mNames;   // Mac +0xc
    bfl::FileSystem::CFileStorage mStorage;        // Mac +0x14
    CResize<CCatagoryContainer*> mCatagories;       // Mac +0x1c
    CResize<CBankContainer*> mBanks;                // Mac +0x24
    CResize<uint32_t> mBankIndices;                // Mac +0x2c
    uint32_t mBankIndexCount;                      // Mac +0x34
    uint32_t mCurrentCatagory;                     // Mac +0x38
    uint8_t mOldVersion;                           // Mac +0x3c, constructor leaves it untouched
};

// Unit v5047: the in-binary callees of SaveCatgoryFixupTable that no header names (default definitions are weak in
// src/TbMapStreamer_TbMapStreamer_ReShuffleSoundForDroppable.cpp; the unit test defines recording fakes).
namespace TbMapStreamerFixup {
uint32_t NameLength(const TbDynamicStringTemplate<char>& name);   // 0x1002efa4 (the string's length)
const char* NameText(const TbDynamicStringTemplate<char>& name);  // 0x10016bd8 (operator const char*)
void StringInit(void* ustring, const char* text);                 // 0x1002ee6c (SamsUtilities::UString(const char*), 12 bytes)
void StringReplace(void* ustring, int32_t from, int32_t to, int32_t flag);   // 0x1002f0c4
char* StringText(void* ustring);                                  // 0x10023674 (operator char*)
void StringDestroy(void* ustring, int32_t flag);                  // 0x1002ee84 (destructor, flag -1)
}  // namespace TbMapStreamerFixup

// The sound library's event interface and its own concrete record (unit w79g4). Main's include/TbAudioSystem.h holds the
// game's class of the same name, so this one lives in namespace sound; the layout is the same (vtable, category, event, a
// position, a float: 0x1c bytes on the Mac), the game's object is passed to the library as one of these.
// The sound library's vtable is 0x1003cd8c (the constructor's `addi r0, r2, 0x219c`): +8 GetType, +0xc GetCategory, +0x10 GetEvent,
// +0x14 GetPosition; no destructor slot. The functions are inline: several units derive from this class (the map streamer's and
// the audio body's test events) and link without the unit that owns them.
namespace sound {
class TbAudioEvent {
public:
    // The host's default for the classes that derive from this one and set their own members (no function of the library).
    TbAudioEvent() : mCategory(0), mEvent(0), mPosition(0.0f, 0.0f, 0.0f), mW(0.0f) {}
    // 0x1001ee64 (80): stores the vtable, the two words and a copy of the position (TbSoundPosition 0x1000cb08), and the float
    // constant 0x100332a8 (0.0f) at +0x18.
    TbAudioEvent(uint32_t category, uint32_t event, const TbSoundPosition& position)
        : mCategory(category), mEvent(event), mPosition(position), mW(0.0f) {}
    virtual uint32_t GetType() const { return 0; }                         // sound vtable +8, 0x1001c8a0 (8): 0
    virtual uint32_t GetCategory() const { return mCategory; }             // sound vtable +0xc, 0x10017238 (8)
    virtual uint32_t GetEvent() const { return mEvent; }                   // sound vtable +0x10, 0x1001eef4 (8)
    virtual const TbSoundPosition& GetPosition() const { return mPosition; }   // sound vtable +0x14, 0x1001ef2c (8): the address of +0xc

    uint32_t mCategory;        // +4
    uint32_t mEvent;           // +8
    TbSoundPosition mPosition; // +0xc
    float mW;                  // +0x18
};
}  // namespace sound
namespace TbEventMapper {
CEventData* MapEvent(CCatagoryData*, const sound::TbAudioEvent&); // 0x10026d24, r3 unused, data r4, event r5
}
