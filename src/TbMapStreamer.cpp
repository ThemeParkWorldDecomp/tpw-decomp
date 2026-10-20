// Sound map registration and in-place readers, sound library 0x1001f6ec..0x100237a4.
#include "TbMapStreamer.h"
#include "TbSoundManager.h"
#include <cstring>

namespace {
const TbGUID kSFXGUID = {0xe9612c00, 0x31d0, 0x11d2, {0xb4, 0x09, 0, 0xb0, 0xc9, 0x93, 0xf2, 3}};
const TbGUID kBankGUID = {0xe9612c01, 0x31d0, 0x11d2, {0xb4, 0x09, 0, 0xa0, 0xc9, 0x93, 0xf2, 3}};
const char* text(const TbDynamicStringTemplate<char>& s) { return s.mData; }
}
TbSoundManager* TbMapStreamer::mpSoundManager = nullptr;
void TbMapStreamer::SetSoundManager(TbSoundManager* manager) { mpSoundManager = manager; }
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
void TbMapStreamer::ObtainCorrectStorage(CStorageContainer& container, const TbDynamicStringTemplate<char>& path) {
    if (!container.mValid) {
        bfl::FileSystem::CDiscFileStorage storage;
        if (static_cast<int32_t>(storage.Open(text(path))) >= 0) {
            container.mStorage = storage;
            container.mValid = 1;
        }
    }
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
void TbMapStreamer::ModifyEventForSense(CEventData* event) {
    if (!(event->mFlags & 4) || !(event->mFlags & 2) || event->mSoundCount != 1) return;
    CSoundData& sound = event->mSounds[0];
    if ((sound.mSampleCount & 0xffff) != 1 || sound.mBranches.mOffset != 0 || sound.mF10 != 0 || sound.mF12 != 0) return;
    event->mFlags = (event->mFlags & 0xfffb) | 8;
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
uint32_t TbMapStreamer::FindCatagory(const char* name) {
    if (name != nullptr && *name != '\0') {
        for (uint32_t i = 0; i < mCatagories.Size(); ++i) {
            if (mCatagories[i] != nullptr && std::strcmp(name, text(mCatagories[i]->mName)) == 0) return i;
        }
    }
    return 0;
}
CEventData* TbEventMapper::MapEvent(CCatagoryData* category, const sound::TbAudioEvent& event) {
    const uint32_t type = event.GetEvent();
    const uint32_t index = event.GetCategory();
    if (index != 0 && type != 0 && category != nullptr) {
        for (uint32_t i = 0; i < category->mEventCount; ++i) {
            CEventData& data = category->mEvents[i];
            if (data.mID == type) return data.mSoundCount == 0 ? nullptr : &data;
        }
    }
    return nullptr;
}
