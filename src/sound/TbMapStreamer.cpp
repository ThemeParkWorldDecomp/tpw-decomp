// TbMapStreamer.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbMapStreamer code, named after it.

#include "AudioPlaceHolder.h"
#include "AudioPlaceHolderList.h"
#include "CSprocketsModule.h"
#include "helpers/TbMapStreamerHelpers.h"
#include "PlaceHolderHelpers.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "snd_commands.h"
#include "TbAudioBody.h"
#include "TbAudioSystem.h"
#include "TbError.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundBank.h"   // SamsUtilities::USwapInplace32(uint32_t&), the sound library's file-endian swap
#include "TbSoundCodecs.h"
#include "TbSoundInfo.h"
#include "TbSoundManager.h"
#include "TbSoundPipe.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// PlaceHolderHelpers.cpp: CMapReference and the one-shot sentence children of the sound library (unit w79g4, "sound symbols shared",
// the library's CSprocketsModule.cpp). See include/PlaceHolderHelpers.h for the classes, their vtables and the overrides.
//
// Conventions of this file. The parent of an Element is the sentence placeholder kept in the extra data's mParent; the Mac code
// reads it through the extra data on every use. Virtual calls the Mac makes through the object's vtable are virtual calls here
// (Init calls +0x3c, +0x40 and +0x50, FillSampleInfo +0x4c); the base's non-virtual members are called directly. The sound manager
// is the class static CAudioPlaceHolder::mpSoundManager (0x1003f360). The pointers of the map data that the Mac compares as 32-bit
// addresses are compared as arena offsets (the value a Ptr32 holds).

namespace {

// A pointer into the map data as the Mac's 32-bit address: the arena offset, which is what a Ptr32 field of the data holds.
uint32_t Address(const void* pointer) {
    return pointer != nullptr ? tpw::ArenaOffset(pointer) : 0;
}

}  // namespace

// The compares are the listing's: the event address against the events array (unsigned, an address one past the end matches), then
// the three indexes by signed division (the compiler's multiply and shift for /20 and /42, shift and add-carry for /16), a negative
// index becomes 0 and an index at or past the count becomes count - 1 (signed compares; a sample count is its low 16 bits). The
// first match ends the search, so a one-past-the-end address of one category that is also the first event of the next gives the
// first category.
void CMapReference::GetSoundIds(int32_t& category, int32_t& event, int32_t& sound, int32_t& sample, CAudioPlaceHolder* holder) {
    CResize<CCatagoryContainer*>& containers = TbMapStreamer::Instance()->GetCatagoryDeque();
    category = 0;
    event = 0;
    sound = 0;
    sample = 0;
    const uint32_t sampleAddress = Address(holder->mSample);   // +0xc
    const uint32_t eventAddress = Address(holder->mEvent);     // +4
    const uint32_t soundAddress = Address(holder->mSound);     // +8
    if (sampleAddress == 0 || soundAddress == 0 || eventAddress == 0) {
        sample = 0;
        sound = 0;
        event = 0;
    }
    const int32_t count = static_cast<int32_t>(containers.Size());
    for (int32_t index = 0; index < count; ++index) {
        CCatagoryContainer* container = containers[static_cast<uint32_t>(index)];
        CCatagoryData* data = container != nullptr ? container->mData : nullptr;
        if (data != nullptr && data->mEventCount != 0) {
            const uint32_t events = data->mEvents.mOffset;
            if (eventAddress >= events && eventAddress <= events + data->mEventCount * 0x14) {
                event = static_cast<int32_t>(eventAddress - events) / 0x14;
                if (event < 0) event = 0;
                if (event >= static_cast<int32_t>(container->mData->mEventCount)) event = static_cast<int32_t>(container->mData->mEventCount) - 1;
                // the event record is read through the placeholder's own pointer (r30 holds the address, which is the record)
                const CEventData* eventRecord = holder->mEvent;
                sound = static_cast<int32_t>(soundAddress - eventRecord->mSounds.mOffset) / 0x2a;
                if (sound < 0) sound = 0;
                if (sound >= static_cast<int32_t>(eventRecord->mSoundCount)) sound = static_cast<int32_t>(eventRecord->mSoundCount) - 1;
                const CSoundData* soundRecord = holder->mSound;
                sample = static_cast<int32_t>(sampleAddress - soundRecord->mSamples.mOffset) / 16;
                if (sample < 0) sample = 0;
                const int32_t samples = static_cast<int32_t>(soundRecord->mSampleCount & 0xffff);
                if (sample >= samples) sample = samples - 1;
                category = index;
                return;
            }
        }
        ++category;
    }
    category = 0;
}

CCatagoryContainer* CMapReference::GetSoundPointer(uint32_t category, CAudioPlaceHolder*) {
    if (category < TbMapStreamer::Instance()->GetCatagoryDeque().Size()) {
        return TbMapStreamer::Instance()->GetCatagoryDeque()[category];
    }
    return nullptr;
}

// Sound-library containers and file-record swaps, sound 0x1001f410..0x10022e48.

namespace {
// Files hold little-endian fields. The test build forces swaps to check their exact offsets.
template<class T> void file_swap(T& value) {
#if defined(TPW_SWAP_FILE_WORDS) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    if constexpr (sizeof(T) == 2) value = sound::SamsUtilities::USwap16(value);
    else value = sound::SamsUtilities::USwap32(value);
#endif
}
}

CCatagoryContainer::~CCatagoryContainer() {
    TbMapStreamer::Instance()->UnloadCatagory(this);
    if (mData != nullptr) {
        tpw::ArenaFree(mData);
        mData = nullptr;
    }
    if (mElements != nullptr) {
        mElements->Release();
        mElements = nullptr;
    }
}
void CCatagoryContainer::CElement::Release() {
    if (mNext != nullptr) mNext->Release();
    delete this;
}
bool CCatagoryContainer::Find(uint32_t value) {
    for (CElement* p = mElements; p != nullptr; p = p->mNext) {
        if (p->mValue == value) return true;
    }
    return false;
}
void CCatagoryContainer::Add(uint32_t value) {
    mElements = new (std::nothrow) CElement(mElements, value);
}

// Sound map registration and in-place readers, sound library 0x1001f6ec..0x100237a4.

namespace {
const TbGUID kSFXGUID = {0xe9612c00, 0x31d0, 0x11d2, {0xb4, 0x09, 0, 0xb0, 0xc9, 0x93, 0xf2, 3}};
const TbGUID kBankGUID = {0xe9612c01, 0x31d0, 0x11d2, {0xb4, 0x09, 0, 0xa0, 0xc9, 0x93, 0xf2, 3}};
const char* text(const TbDynamicStringTemplate<char>& s) { return s.mData; }
}
TbMapStreamer* TbMapStreamer::Instance() {
    static TbMapStreamer instance;
    return &instance;
}
TbMapStreamer::TbMapStreamer() : mCurrentCatagory(1) {
    auto* category = new CCatagoryContainer(0);
    category->mData = static_cast<CCatagoryData*>(tpw::ArenaAlloc(sizeof(CCatagoryData)));
    category->mData->mEventCount = 0;
    category->mData->mFlags = 0xd;
    category->mData->mF0C = 0.2f;
    category->mData->mF10 = 2.0f;
    category->mData->mF14 = 0.5f;
    mCatagories[0] = category;
    mNames[0] = "";
    mBankIndices.Clear();
    mBankIndexCount = 0;
}

CCatagoryContainer::CCatagoryContainer(uint32_t index)
    : mData(nullptr), mIndex(index), mElements(nullptr), mName() {}

TbMapStreamer::~TbMapStreamer() {
    mCatagories[0]->Release();
    mCatagories[0] = nullptr;
}
TbError TbMapStreamer::SetPath(const char* path) {
    mStorage.Close();
    CStorageContainer storage;
    {
        TbDynamicStringTemplate<char> name(path);
        ObtainCorrectStorage(storage, name);
    }
    if (!storage.mValid) return TbError(-1);
    mStorage = storage.mStorage;
    return TbError(0);
}
TbError TbMapStreamer::RegisterCatagory(const char* qualified, uint32_t& index) {
    TbDynamicStringTemplate<char> full(qualified);
    const char colon = ':';
    const int32_t split = full.FindReverse(colon, 0xffffffff);
    if (split != -1) {
        TbDynamicStringTemplate<char> path(full.Left(split));
        path += colon;
        TbDynamicStringTemplate<char> name(full.Right(full.Length() - split - 1));
        const int32_t suffix = name.Find("SFX.map", 0);
        if (suffix != -1) {
            name = name.Left(suffix);
            return RegisterCatagory(text(path), text(name), index);
        }
    }
    return TbError(-1);
}
TbError TbMapStreamer::RegisterCatagory(const char* path, const char* name, uint32_t& index) {
    if (static_cast<int32_t>(SetPath(path)) >= 0 && name != nullptr) {
        TbDynamicStringTemplate<char> full(path);
        full += name;
        if (mNames.Find(full, index)) {
            mCatagories[index]->AddRef();
            return TbError(0);
        }
        TbDynamicStringTemplate<char> filename(name);
        filename += "BANK.map";
        mCurrentCatagory = GetNextFreeIndex(mCatagories);
        auto* category = new CCatagoryContainer(mCurrentCatagory);
        category->mName = path;
        int32_t last = -1;
        const char* start = text(category->mName);
        for (const char* p = std::strstr(start, ":data:"); p != nullptr; p = std::strstr(p + 1, ":data:")) {
            last = static_cast<int32_t>(p - start);
        }
        if (last != -1) category->mName = category->mName.Right(category->mName.Length() - last);
        category->mName += name;
        mCatagories[mCurrentCatagory] = category;
        if (static_cast<int32_t>(RegisterBankDataFromStorage(text(filename), path)) >= 0) {
            filename = name;
            filename += "SFX.map";
            if (static_cast<int32_t>(RegisterSFXDataFromStorage(text(filename), index)) >= 0) {
                mBankIndices.Clear();
                mBankIndexCount = 0;
                mNames[index] = full;
                return TbError(0);
            }
        }
        mCatagories[mCurrentCatagory]->Release();
        mCatagories[mCurrentCatagory] = nullptr;
    }
    mBankIndices.Clear();
    mBankIndexCount = 0;
    return TbError(-1);
}
void TbMapStreamer::ObtainCorrectStorage(CStorageContainer& container, const TbDynamicStringTemplate<char>& path) {
    if (!container.mValid) {
        bfl::FileSystem::CDiscFileStorage storage;
        if (static_cast<int32_t>(storage.Open(text(path))) >= 0) {
            container.mStorage = storage;
            container.mValid = 1;
        }
    }
}
void TbMapStreamer::ReadBankDataFromMap(bfl::FileSystem::CFile& file, CBankDataMap& map) {
    const int32_t size = file.GetSize();
    const int32_t position = file.GetPos();
    auto* block = static_cast<char*>(tpw::ArenaAlloc(static_cast<uint32_t>(size - position)));
    file.Read(block, size - position);
    map.mBanks = reinterpret_cast<CBankData*>(block);
    char* next = block + map.mCount * sizeof(CBankData);
    for (uint32_t i = 0; i < map.mCount; ++i) {
        // The Mac sign-extends each char before combining the length, including the low byte.
        uint32_t length = static_cast<uint32_t>(static_cast<int8_t>(next[0])) |
            (static_cast<uint32_t>(static_cast<int8_t>(next[1])) << 8) |
            (static_cast<uint32_t>(static_cast<int8_t>(next[2])) << 16) |
            (static_cast<uint32_t>(static_cast<int8_t>(next[3])) << 24);
        next += 4;
        CBankData& bank = map.mBanks[i];
        bank.mName = next;
        bank.mF00 = 0;
        bank.mF04 = 0;
        next += static_cast<int32_t>(length);
        bank.mName = RemovePath(bank.mName, length);
    }
}
bool TbMapStreamer::BankDoesNotExist(uint32_t& index, const char* name, const TbDynamicStringTemplate<char>& path,
                                    CStorageContainer& storage) {
    TbDynamicStringTemplate<char> existing;
    TbDynamicStringTemplate<char> wanted;
    storage.mValid = 0;
    for (uint32_t i = 0; i < mBanks.Size(); ++i) {
        if (mBanks[i] != nullptr) {
            TbDynamicStringTemplate<char> empty;
            mBanks[i]->mStorage.FullPath(existing, empty);
            wanted = path;
            if (existing == wanted) {
                storage.mValid = 1;
                storage.mStorage = mBanks[i]->mStorage;
            }
            existing += mBanks[i]->mName;
            wanted += name;
            if (existing == wanted) {
                mBankIndices[mBankIndexCount++] = i;
                index = i;
                return false;
            }
        }
    }
    return true;
}
char* TbMapStreamer::RemovePath(char* name, uint32_t length) {
    int32_t i = static_cast<int32_t>(length - 1);
    char* p = name + i;
    while (i != 0) {
        if (*p == '\\') return p + 1;
        --i;
        --p;
    }
    return name;
}
TbError TbMapStreamer::RegisterBankData(bfl::FileSystem::CFile& file, const char* path) {
    CFileHeader header;
    file.Read(&header, sizeof(header));
    header.Byteswap();
    if (header.mGUID == kBankGUID) {
        CBankDataMap map;
        file.Read(&map, sizeof(map));
        map.Byteswap();
        ReadBankDataFromMap(file, map);
        for (uint32_t i = 0; i < map.mCount; ++i) {
            TbDynamicStringTemplate<char> location(path);
            if (map.mBanks[i].mFlags & 0x20) {
                location = mpSoundManager->GetSoundPath();
                location += "Global";
                const char colon = ':';
                location += colon;
            }
            CStorageContainer storage;
            uint32_t index;
            if (!BankDoesNotExist(index, map.mBanks[i].mName, location, storage)) {
                mBanks[index]->AddRef();
            } else {
                auto* bank = new CBankContainer(map.mBanks[i]);
                ObtainCorrectStorage(storage, location);
                if (storage.mValid) {
                    bank->mStorage = storage.mStorage;
                    index = GetNextFreeIndex(mBanks);
                    mBanks[index] = bank;
                    mBankIndices[mBankIndexCount++] = index;
                    mpSoundManager->GetBank(index);
                }
            }
        }
        tpw::ArenaFree(map.mBanks);
    }
    return TbError(0);
}

CBankContainer::CBankContainer(const CBankData& bank)
    : mBank(nullptr), mName(bank.mName), mFlags(bank.mFlags), mStorage() {}
void CBankDataMap::Byteswap() {
    file_swap(mCount);
    file_swap(mBanks.mOffset);
}
bool TbGUID::operator==(const TbGUID& other) const {
    return mData1 == other.mData1 && mData2 == other.mData2 && mData3 == other.mData3 &&
        std::memcmp(mData4, other.mData4, 8) == 0;
}
void CFileHeader::Byteswap() {
    mGUID.Byteswap();
    file_swap(mVersion);
}
void TbGUID::Byteswap() {
    file_swap(mData1);
    file_swap(mData2);
    file_swap(mData3);
}

TbError TbMapStreamer::RegisterBankDataFromStorage(const char* name, const char*) {
    bfl::FileSystem::CFile file;
    if (static_cast<int32_t>(mStorage.OpenFile(file, name, 0x80000001, nullptr)) >= 0) {
        TbDynamicStringTemplate<char> path;
        TbDynamicStringTemplate<char> empty;
        mStorage.FullPath(path, empty);
        RegisterBankData(file, text(path));
        file.Close();
    }
    return TbError(0);
}
bool TbMapStreamer::IsSFXHeaderValid(const CFileHeader& header) {
    mOldVersion = header.mVersion == 0;
    return header.mGUID == kSFXGUID;
}
TbError TbMapStreamer::RegisterSFXData(bfl::FileSystem::CFile& file, uint32_t& index) {
    CFileHeader header;
    file.Read(&header, sizeof(header));
    header.Byteswap();
    if (!IsSFXHeaderValid(header)) return TbError(-1);
    const int32_t size = file.GetSize();
    const int32_t position = file.GetPos();
    CSFXDataMap map;
    file.Read(&map, sizeof(map));
    // The allocation/read request includes the eight bytes of map already consumed.
    map.Byteswap();
    auto* block = static_cast<uint8_t*>(tpw::ArenaAlloc(static_cast<uint32_t>(size - position)));
    file.Read(block, size - position);
    ReadSampleData(block, &mCatagories[mCurrentCatagory]->mData, map.mCount);
    index = mCurrentCatagory;
    return TbError(0);
}

void CSFXDataMap::Byteswap() {
    file_swap(mCount);
    file_swap(mCatagories.mOffset);
}

TbError TbMapStreamer::RegisterSFXDataFromStorage(const char* name, uint32_t& index) {
    bfl::FileSystem::CFile file;
    if (static_cast<int32_t>(mStorage.OpenFile(file, name, 0x80000001, nullptr)) >= 0) {
        const TbError result = RegisterSFXData(file, index);
        file.Close();
        if (static_cast<int32_t>(result) >= 0) return TbError(0);
    }
    return TbError(-1);
}
uint8_t* TbMapStreamer::ReadSampleData(uint8_t* block, CCatagoryData** output, uint32_t count) {
    *output = reinterpret_cast<CCatagoryData*>(block);
    char* next = reinterpret_cast<char*>(*output + count);
    for (uint32_t i = 0; i < count; ++i) next = ReadCatagory(next, *output + i);
    return reinterpret_cast<uint8_t*>(next);
}
void TbMapStreamer::ModifyEventForSense(CEventData* event) {
    if (!(event->mFlags & 4) || !(event->mFlags & 2) || event->mSoundCount != 1) return;
    CSoundData& sound = event->mSounds[0];
    if ((sound.mSampleCount & 0xffff) != 1 || sound.mBranches.mOffset != 0 || sound.mF10 != 0 || sound.mF12 != 0) return;
    event->mFlags = (event->mFlags & 0xfffb) | 8;
}
char* TbMapStreamer::ReadCatagory(char* block, CCatagoryData* category) {
    category->Byteswap();
    category->mEvents = reinterpret_cast<CEventData*>(block);
    sound::SamsUtilities::USwapBlockStruct(category->mEvents.get(), category->mEventCount);
    char* next = block + category->mEventCount * sizeof(CEventData);
    for (uint32_t i = 0; i < category->mEventCount; ++i) {
        next = ReadEvents(next, &category->mEvents[i]);
        ModifyEventForSense(&category->mEvents[i]);
    }
    return next;
}

void CEventData::Byteswap() {
    file_swap(mID);
    file_swap(mSoundCount);
    file_swap(mSounds.mOffset);
    file_swap(mF0C);
    file_swap(mFlags);
    file_swap(mF12);
}
void CCatagoryData::Byteswap() {
    file_swap(mEventCount);
    file_swap(mEvents.mOffset);
    file_swap(mF08);
    file_swap(mFlags);
    file_swap(mF0C);
    file_swap(mF10);
    file_swap(mF14);
}

char* TbMapStreamer::ReadEvents(char* block, CEventData* event) {
    event->mSounds = reinterpret_cast<CSoundData*>(block);
    sound::SamsUtilities::USwapBlockStruct(event->mSounds.get(), event->mSoundCount);
    char* next = block + event->mSoundCount * sizeof(CSoundData);
    for (uint32_t i = 0; i < event->mSoundCount; ++i) next = ReadSounds(next, &event->mSounds[i]);
    uint32_t previous = 0;
    for (uint32_t i = 0; i < event->mSoundCount; ++i) {
        CSoundData& sound = event->mSounds[i];
        if (mOldVersion) {
            const uint32_t current = sound.mF1E;
            sound.mF1E = current - previous;
            previous = current;
        }
        for (uint32_t j = 0; j < sound.mBranchCount; ++j) {
            CSoundBranchData& branch = sound.mBranches[j];
            branch.mSound.mOffset = event->mSounds.mOffset + (branch.mSound.mOffset - 1) *
                static_cast<uint32_t>(sizeof(CSoundData));
        }
    }
    return next;
}

void CSoundData::Byteswap() {
    file_swap(mSampleCount);
    file_swap(mBranchCount);
    file_swap(mSamples.mOffset);
    file_swap(mF10);
    file_swap(mF12);
    file_swap(mF16);
    file_swap(mF18);
    file_swap(mF1A);
    file_swap(mF1C);
    file_swap(mF1E);
    file_swap(mF22);
    file_swap(mBranches.mOffset);
}

char* TbMapStreamer::ReadSounds(char* block, CSoundData* sound) {
    const uint32_t count = sound->mSampleCount & 0xffff;
    sound->mSamples = reinterpret_cast<CSampleData*>(block);
    sound::SamsUtilities::USwapBlockStruct(sound->mSamples.get(), count);
    sound->mBranches = reinterpret_cast<CSoundBranchData*>(block + count * sizeof(CSampleData));
    sound::SamsUtilities::USwapBlockStruct(sound->mBranches.get(), sound->mBranchCount);
    for (uint32_t i = 0; i < count; ++i) {
        CSampleData& sample = sound->mSamples[i];
        if (sample.mBankIndex != 0) {
            sample.mBankIndex = static_cast<uint16_t>(mBankIndices[sample.mBankIndex - 1]);
            if (!mCatagories[mCurrentCatagory]->Find(sample.mBankIndex)) {
                mCatagories[mCurrentCatagory]->Add(sample.mBankIndex);
            }
        }
    }
    return block + count * sizeof(CSampleData) + sound->mBranchCount * sizeof(CSoundBranchData);
}

void CSoundBranchData::Byteswap() {
    file_swap(mSound.mOffset);
    file_swap(mF04);
}

void CSampleData::Byteswap() {
    file_swap(mF00);
    file_swap(mF04);
    file_swap(mF08);
    file_swap(mBankIndex);
    file_swap(mF0E);
}

TbError TbMapStreamer::RemoveCatagory(uint32_t index) {
    if (mCatagories[index] == nullptr) return TbError(-1);
    mCatagories[index]->Release();
    return TbError(0);
}
TbError TbMapStreamer::UnloadCatagory(CCatagoryContainer* category) {
    if (category != nullptr) {
        auto* element = category->mElements;
        while (element != nullptr) {
            const uint32_t index = element->mValue;
            element = element->mNext;
            if (mBanks[index] != nullptr) {
                TbBank* bank = mBanks[index]->mBank;
                if (mBanks[index]->Release() == 0) {
                    if (bank != nullptr) reinterpret_cast<TbRefRelease*>(bank)->Release();
                    mBanks[index] = nullptr;
                }
            }
        }
        if (mCatagories[category->mIndex] == category) {
            mCatagories[category->mIndex] = nullptr;
            mNames[category->mIndex] = "";
            return TbError(0);
        }
    }
    return TbError(-1);
}

// TbMapStreamer: ReShuffleSoundForDroppable 0x1002309c (276) and SaveCatgoryFixupTable 0x100233e0 (576), unit v5047.

// 0x1002309c: the droppable samples go to the end of the list. The count word first loses its high half (<< 16) and then gets
// the low half added back sign-extended, as the listing does (the word ends as (count << 16) + (int16_t)count).
void TbMapStreamer::ReShuffleSoundForDroppable(CSoundData* sound) {
    const uint32_t count = sound->mSampleCount;
    CSampleData* first = sound->mSamples.get();
    sound->mSampleCount = count << 16;
    CSampleData* last = first + (static_cast<int32_t>(count & 0xffff) - 1);
    sound->mSampleCount = sound->mSampleCount + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(count)));
    while (first <= last) {
        if (mpSoundManager->IsBankDroppable(first->mBankIndex)) {
            const CSampleData saved = *first;
            *first = *last;
            *last = saved;
            --last;
            sound->mSampleCount = sound->mSampleCount - 1;
        } else {
            ++first;
        }
    }
}

// Unit v51032: CSprocketsChannelMac::SetDescription, the UReferenceCounter<USoundDescription> copy constructor and destructor,
// TbMapStreamer (CCatFixupTable::Unload, LoadCatgoryFixupTable, SetDroppable) and TbStaticFileBank (destructor, Deregister).

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

// Unit v51113: the small sound-library functions at the edges of CSprocketsModule.cpp (sound 0x10023310 to 0x1002e8fc) and the
// float square roots / cleanup objects of the codec files.

// 0x10023310 (40): the fixup table lives at +4.
void TbMapStreamer::UnloadCatgoryFixupTable() { mFixup.Unload(); }

// 0x10023378 (44): the table is the Mac's +8 word (data), the count the +4 word; an unsigned index test.
uint32_t TbMapStreamer::FixUpCatagoryIndex(uint32_t index) {
    uint32_t* table = mFixup.mData;
    if (table == nullptr || index >= mFixup.mCount) return 0;
    return table[index];
}

// 0x100233e0: the table is written as: position, count (twice, the original's bug), one zero word per category, then per
// category a length byte (0 for none, else the name's length + 1) and the name with ':' turned into '\'; then the byte size
// of everything after the first two words is patched in at the start and the position goes back to the end.
TbError TbMapStreamer::SaveCatgoryFixupTable(bfl::FileSystem::CFile* file) {
    const int32_t start = file->GetPos();
    const uint32_t count = mCatagories.Size();
    uint32_t word = ::SamsUtilities::USwap32<uint32_t>(count);   // the file is little-endian: native words on the host
    file->Write(&word, 4);
    file->Write(&word, 4);
    for (uint32_t i = 0; i < count; ++i) {
        word = 0;
        file->Write(&word, 4);
    }
    for (uint32_t i = 0; i < mCatagories.Size(); ++i) {
        CCatagoryContainer* category = mCatagories[i];
        uint8_t length = 0;
        if (category != nullptr) length = static_cast<uint8_t>(TbMapStreamerFixup::NameLength(category->mName) + 1);
        file->Write(&length, 1);
        if (length != 0) {
            // Room for a host UString (24 bytes, the text at +16 where StringText reads it; the Mac's is 12 bytes, v5L12).
            alignas(SamsUtilities::UString) uint8_t ustring[sizeof(SamsUtilities::UString)];
            TbMapStreamerFixup::StringInit(ustring, TbMapStreamerFixup::NameText(category->mName));
            TbMapStreamerFixup::StringReplace(ustring, ':', '\\', 0);
            file->Write(TbMapStreamerFixup::StringText(ustring), length);
            TbMapStreamerFixup::StringDestroy(ustring, -1);
        }
    }
    const int32_t end = file->GetPos();
    file->Seek(static_cast<uint32_t>(start), 0);
    word = ::SamsUtilities::USwap32<uint32_t>(static_cast<uint32_t>(end - start - 8));
    file->Write(&word, 4);
    file->Seek(static_cast<uint32_t>(end), 0);
    return TbError(0);
}

// Default bodies of the string helpers (the unit test links its own recording fakes instead). The text lives in the
// UString's mText, where the translated operator char* (StringText, src/TbMapStreamer_TbMapStreamer_FixUpCatagoryIndex.cpp)
// reads it; these bodies kept it at +0, so the save wrote from a stray pointer once v51113 landed (v5L12).
namespace TbMapStreamerFixup {
__attribute__((weak)) uint32_t NameLength(const TbDynamicStringTemplate<char>& name) {
    return static_cast<uint32_t>(std::strlen(NameText(name)));
}
__attribute__((weak)) const char* NameText(const TbDynamicStringTemplate<char>& name) { return name.mData; }
__attribute__((weak)) void StringInit(void* ustring, const char* text) {
    char* copy = static_cast<char*>(std::malloc(std::strlen(text) + 1));
    std::strcpy(copy, text);
    Text(ustring) = copy;
}
__attribute__((weak)) void StringReplace(void* ustring, int32_t from, int32_t to, int32_t flag) {
    (void)flag;
    char* text = Text(ustring);
    for (; *text != 0; ++text)
        if (*text == from) *text = static_cast<char>(to);
}
__attribute__((weak)) char* StringText(void* ustring) { return Text(ustring); }
__attribute__((weak)) void StringDestroy(void* ustring, int32_t flag) {
    (void)flag;
    std::free(Text(ustring));
}
}  // namespace TbMapStreamerFixup

// 0x1002370c (68): loads the table, answers 0 (the load's own answer is dropped).
TbError TbMapStreamer::LoadCatgoryFixupTable(bfl::FileSystem::CFile* file) {
    mFixup.Load(*file);
    return TbError(0);
}

uint32_t TbMapStreamer::FindCatagory(const char* name) {
    if (name != nullptr && *name != '\0') {
        for (uint32_t i = 0; i < mCatagories.Size(); ++i) {
            if (mCatagories[i] != nullptr && std::strcmp(name, text(mCatagories[i]->mName)) == 0) return i;
        }
    }
    return 0;
}

// TbMapStreamer::CCatFixupTable::Load, sound 0x1002388c (336 bytes).

// Reads a save game's category fix-up table: the byte size of the block, the entry count, then the block itself (count words,
// then one record per entry: a length byte and a name of that many bytes). Every entry word becomes the live category index
// of its name (0 for a record whose length byte is 0). Backslashes in a name become colons first.
TbError TbMapStreamer::CCatFixupTable::Load(bfl::FileSystem::CFile& file) {
    Unload();
    uint32_t size;
    file.Read(&size, 4);
    ::SamsUtilities::USwapInplace32(size);
    file.Read(&mCount, 4);   // the count sits at Mac +0
    ::SamsUtilities::USwapInplace32(mCount);
    mData = reinterpret_cast<uint32_t*>(::operator new[](size, std::nothrow));   // 0x1002ebfc
    file.Read(mData, size);
    uint8_t* p = reinterpret_cast<uint8_t*>(mData) + mCount * 4;
    for (uint32_t i = 0; i < mCount; ++i) {
        uint8_t length = *p++;
        if (length != 0) {
            char* q = reinterpret_cast<char*>(p);
            char c = *q++;
            while (c != 0) {
                if (c == '\\') q[-1] = ':';
                c = *q++;
            }
            mData[i] = TbMapStreamer::Instance()->FindCatagory(reinterpret_cast<const char*>(p));
            p += length;
        } else {
            mData[i] = 0;
        }
    }
    return TbError(0);
}

// ---- TbMapStreamer

// 0x10023a78 (68): frees the entry array (Mac +4) and clears it; the count is left.
void TbMapStreamer::CCatFixupTable::Unload() {
    if (mData != nullptr) DisposeBuffer_(mData);
    mData = nullptr;
}
