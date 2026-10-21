// TPResource.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TPResource code, named after it.

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "LbFile.h"
#include "lobby.h"
#include "OptionsBalance.h"
#include "platform/interfacelib.h"
#include "Serialise.h"
#include "SoundBalance.h"
#include "TbITable.h"
#include "Text.h"
#include "TPCamera.h"
#include "TPGlobals.h"
#include "TPMovie.h"
#include "TPResource.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TPString.h"
#include "ui_ErrorDisplay.h"
#include "ui_parkStatus.h"
#include <new>

// TPSettings.cpp (decompiled from the main app of SimTheme Park 2.04b3). PARTIAL: the
// resources (CTPBaseResource and its subclasses, CTPResourceManager, QualifyFilename and the
// list instances, 0x1015f464..0x10165a70), CPlayerSettings (0x10165ad0..0x10168374, with
// the LOADVAR/SAVEVAR instances for its types) and __sinit_TPSettings_cpp (0x101683d0). The
// ride database's resources (CTPItemListResource and CTPItemResource: 0x10161148, 0x101612cc,
// 0x101621f4..0x10162470 and 0x10163494..0x10164574; their list: 0x10164610..0x101647dc and
// 0x10164d9c) are untranslated.

// __sinit_TPSettings_cpp, in construction order: the strings TPMain.cpp's gTPMainLanguages
// are (each file has its own copies), then gPlayerSettings, whose constructor runs Init.
TbDynamicStringTemplate<char> gTPSettingsLanguages[16] = {   // 0x10380804, 20-byte slots
    TbDynamicStringTemplate<char>("English"),
    TbDynamicStringTemplate<char>("French"),
    TbDynamicStringTemplate<char>("German"),
    TbDynamicStringTemplate<char>("Spanish"),
    TbDynamicStringTemplate<char>("Swedish"),
    TbDynamicStringTemplate<char>("Italian"),
    TbDynamicStringTemplate<char>("Dutch"),
    TbDynamicStringTemplate<char>("Polish"),
    TbDynamicStringTemplate<char>("Danish"),
    TbDynamicStringTemplate<char>("Japanese"),
    TbDynamicStringTemplate<char>("TraditionalChinese"),
    TbDynamicStringTemplate<char>("SimplifiedChinese"),
    TbDynamicStringTemplate<char>("Thai"),
    TbDynamicStringTemplate<char>("Korean"),
    TbDynamicStringTemplate<char>("Hebrew"),
    TbDynamicStringTemplate<char>("American")
};
TbDynamicStringTemplate<char> gTPSettingsTitle("Theme Park World");    // 0x103807e4
TbDynamicStringTemplate<char> gTPSettingsLanguage("Language");         // 0x103807c4
// ------------------------------------------------------------------ resources

namespace {

constexpr uint32_t kOpenForReading = 0x80000001;   // the mode resources open files with

// Inline (QualifyFilename's name and result): drops the separators `path` starts with.
void RemoveLeadingSeparators(TbDynamicStringTemplate<char>& path) {
    const TbDynamicStringTemplate<char>& separator = gTPGlobals.mSeparator;
    while (path.Left(static_cast<uint32_t>(separator.Length())) == separator)
        path = path.Right(static_cast<uint32_t>(path.Length() - separator.Length()));
}

}  // namespace

CTPBaseResource::~CTPBaseResource() {
    delete mSearchPattern;
    mSearchPattern = nullptr;
    if (mStorages != nullptr) {
        while (mStorages->GetCount() != 0) delete mStorages->RemoveHead();
        delete mStorages;
        mStorages = nullptr;
    }
}

// Each Add*Storage keeps the storage only if it opens (the CFile is the original's, unused).
void CTPBaseResource::AddDiscStorage(const TbDynamicStringTemplate<char>& path) {
    bfl::FileSystem::CDiscFileStorage* storage = new bfl::FileSystem::CDiscFileStorage;
    if (storage == nullptr) return;
    if (storage->Open(path.mData).Succeeded()) {
        bfl::FileSystem::CFile file;
        mStorages->AddTail(storage);
    } else {
        delete storage;
    }
}

void CTPBaseResource::AddWADStorage(const TbDynamicStringTemplate<char>& path) {
    bfl::FileSystem::CWadFileStorage* storage = new bfl::FileSystem::CWadFileStorage;
    if (storage == nullptr) return;
    if (storage->Open(path.mData, 0).Succeeded()) mStorages->AddTail(storage);
    else delete storage;
}

void CTPBaseResource::AddAllStorages(const TbDynamicStringTemplate<char>& path) {
    AddLocalStorage(path);
    AddRemoteStorage(path);
}

void CTPBaseResource::AddLocalStorage(const TbDynamicStringTemplate<char>& path) {
    TbDynamicStringTemplate<char> folder(path);
    bfl::FileSystem::CDiscFileStorage* disc = new bfl::FileSystem::CDiscFileStorage;
    if (disc != nullptr) {
        if (disc->Open(folder.mData).Succeeded()) {
            bfl::FileSystem::CFile file;
            mStorages->AddTail(disc);
        } else {
            delete disc;
        }
    }
    folder += gTPGlobals.mWadExtension;
    bfl::FileSystem::CWadFileStorage* wad = new bfl::FileSystem::CWadFileStorage;
    if (wad != nullptr) {
        if (wad->Open(folder.mData, 0).Succeeded()) mStorages->AddTail(wad);
        else delete wad;
    }
}

void CTPBaseResource::AddRemoteStorage(const TbDynamicStringTemplate<char>& path) {
    TbDynamicStringTemplate<char> folder(gTPFileInfo.GetRemotePath());
    FS_AddDirectorySeparator(folder);
    folder += path;
    bfl::FileSystem::CDiscFileStorage* disc = new bfl::FileSystem::CDiscFileStorage;
    if (disc != nullptr) {
        if (disc->Open(folder.mData).Succeeded()) {
            bfl::FileSystem::CFile file;
            mStorages->AddTail(disc);
        } else {
            delete disc;
        }
    }
    folder += gTPGlobals.mWadExtension;
    bfl::FileSystem::CWadFileStorage* wad = new bfl::FileSystem::CWadFileStorage;
    if (wad != nullptr) {
        if (wad->Open(folder.mData, 0).Succeeded()) mStorages->AddTail(wad);
        else delete wad;
    }
}

bfl::FileSystem::CFileStorage* CTPBaseResource::pGetCurrentSearchStorage() const { return mCurrentSearchStorage; }
bfl::FileSystem::CFileStorage* CTPBaseResource::pGetLastFoundFileStorage() const { return mLastFoundFileStorage; }

TbDynamicStringTemplate<char>& strRemoveRootDirectory(TbDynamicStringTemplate<char>& path) {
    const TbDynamicStringTemplate<char>& root = gTPGlobals.mCurrentFolder;
    if (TPStrEq(TbDynamicStringTemplate<char>(path.Left(static_cast<uint32_t>(root.Length()))), root)) {
        uint32_t start = 1;
        const TbDynamicStringTemplate<char>& separator = gTPGlobals.mSeparator;
        while (TPStrEq(TbDynamicStringTemplate<char>(path.Mid(start, static_cast<uint32_t>(separator.Length()))),
                       separator) == true ||
               TPStrEq(TbDynamicStringTemplate<char>(path.Mid(start, static_cast<uint32_t>(root.Length()))), root) ==
                   true)
            ++start;
        path = path.Mid(start);
    }
    return path;
}

// `out` = `name` past the folder `base` (both without a leading "." and separators, and
// base without a ".wad*" ending); "" and 0 when name is not in base.
int32_t QualifyFilename(TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& base,
                        TbDynamicStringTemplate<char>& out) {
    TbDynamicStringTemplate<char> file;
    TbDynamicStringTemplate<char> folder;
    file = strRemoveRootDirectory(name);
    folder = strRemoveRootDirectory(base);
    RemoveLeadingSeparators(file);
    // (the ending compared is as long as ".wad*:", so only a folder that is just ".wad*" is cut)
    const TbDynamicStringTemplate<char>& wad = gTPGlobals.mWadPattern;
    if (TPStrEq(TbDynamicStringTemplate<char>(folder.Right(static_cast<uint32_t>(gTPGlobals.mWadFolderPattern.Length()))),
                wad) == true) {
        folder = folder.Left(static_cast<uint32_t>(folder.Length() - wad.Length()));
        FS_AddDirectorySeparator(folder);
    }
    // (an unsigned comparison)
    if (static_cast<uint32_t>(file.Length() + wad.Length()) < static_cast<uint32_t>(folder.Length())) {
        out = gTPGlobals.mSearchPath;
        return 0;
    }
    if (TPStrEq(TbDynamicStringTemplate<char>(file.Left(static_cast<uint32_t>(folder.Length()))), folder) == true) {
        out = file.Right(static_cast<uint32_t>(file.Length() - folder.Length()));
        RemoveLeadingSeparators(out);
        return 1;
    }
    out = gTPGlobals.mSearchPath;
    return 0;
}

int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                                  bfl::FileSystem::CFileStorage& storage, uint32_t mode) {
    TbDynamicStringTemplate<char> qualified;
    TbDynamicStringTemplate<char> folder;
    TbDynamicStringTemplate<char> fileName(name);
    folder = mPath;
    if (QualifyFilename(fileName, folder, qualified) == 1) return OpenQualifiedFile(file, qualified);
    return 0;
}

int32_t CTPBaseResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode) {
    if (mStorages != nullptr) {
        bfl::FileSystem::CFileInfo info;   // (unused)
        POSITION position = mStorages->GetHeadPosition();
        while (position != nullptr) {
            TbDynamicStringTemplate<char> qualified;
            TbDynamicStringTemplate<char> folder(gTPGlobals.mSearchPath);
            TbDynamicStringTemplate<char> fileName(name);
            bfl::FileSystem::CFileStorage* storage = mStorages->GetNext(position);
            folder = mPath;
            if (QualifyFilename(fileName, folder, qualified) == 1 && OpenQualifiedFile(file, qualified, *storage) == 1) {
                mLastFoundFileStorage = storage;
                mManager->pSetCurrentResource(this);
                return 1;
            }
        }
    }
    mLastFoundFileStorage = nullptr;
    return 0;
}

int32_t CTPBaseResource::FindFirst(const TbDynamicStringTemplate<char>& pattern, bfl::FileSystem::CFileInfo& info) {
    if (mStorages != nullptr) {
        bfl::FileSystem::CFileInfo blank;
        POSITION position = mStorages->GetHeadPosition();
        while (position != nullptr) {
            bfl::FileSystem::CFileStorage* storage = mStorages->GetNext(position);
            info = blank;
            if (storage->FindFirst(pattern.mData, info).Succeeded()) {
                mCurrentSearchStorage = storage;
                mLastFoundFileStorage = storage;
                delete mSearchPattern;
                mSearchPattern = nullptr;
                mSearchPattern = new TbDynamicStringTemplate<char>(pattern);
                return 1;
            }
        }
    }
    delete mSearchPattern;
    mSearchPattern = nullptr;
    mCurrentSearchStorage = nullptr;
    return 0;
}

int32_t CTPBaseResource::FindNext(bfl::FileSystem::CFileInfo& info) {
    if (mCurrentSearchStorage == nullptr) return 0;
    if (mCurrentSearchStorage->FindNext(info).Succeeded()) return 1;
    // on through the storages after the current one, from the first entry
    POSITION position = mStorages->Find(mCurrentSearchStorage);
    mStorages->GetNext(position);
    while (position != nullptr) {
        bfl::FileSystem::CFileStorage* storage = mStorages->GetNext(position);
        if (storage->FindFirst(mSearchPattern->mData, info).Succeeded()) {
            mCurrentSearchStorage = storage;
            mLastFoundFileStorage = storage;
            return 1;
        }
    }
    delete mSearchPattern;
    mSearchPattern = nullptr;
    mCurrentSearchStorage = nullptr;
    return 0;
}

CTPSharedTextureResource::CTPSharedTextureResource(const TbDynamicStringTemplate<char>& name,
                                                   const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPTextureResource::CTPTextureResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPSoundResource::CTPSoundResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPAdvisorResource::CTPAdvisorResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPIntermediate(name, path) {}

// TPSettings.cpp range of the main app (SimTheme Park 2.04b3): the constructor of the ride database's item list resource
// (0x10161148, 280 bytes). The rest of the class (destructor 0x101621f4, RegisterItem, FindItem, OpenFile) is not translated.

// The original builds the base resource inline (the two strings and the empty list of storages, as CTPBaseResource's
// constructor does), then the texture resource's file storage at 0x40 (CTPTextureResource), and clears the two words it adds
// at 0x48 and 0x4c. The vtable word is written four times on the Mac while the parts are built (the base resource's, the texture
// resource's, the intermediate's, the final one); the host's chain of constructors does the same.
CTPItemListResource::CTPItemListResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPIntermediate(name, path), mF48(nullptr), mItems(nullptr) {}

// ~CTPItemListResource (0x101621f4) is in src/TPSettings_itemres_CTPItemListResource_OpenFile.cpp (unit v5375).

// TPSettings.cpp (main app): item resources used by the ride database.
// SimTheme Park 2.04b3, 0x101612cc..0x101645dc. Mac layouts are in TPResource.h.

using namespace bfl::FileSystem;

CTPItemResource::CTPItemResource(const CFileInfo& info, const TbDynamicStringTemplate<char>& folder)
    : mName(), mPath(), mStorage() {
    InitItem(info, folder);
}

CTPFontResource::CTPFontResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPSpriteResource::CTPSpriteResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPUIResource::CTPUIResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPTextureResource(name, path) {}
CTPTerrainResource::CTPTerrainResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPTextureResource(name, path) {}
CTPLanguageResource::CTPLanguageResource(const TbDynamicStringTemplate<char>& name,
                                         const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}
CTPFMVResource::CTPFMVResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPBaseResource(name, path) {}

CTPIntermediate::~CTPIntermediate() {}
CTPSharedTextureResource::~CTPSharedTextureResource() {}
CTPTextureResource::~CTPTextureResource() {}
CTPSoundResource::~CTPSoundResource() {}
CTPAdvisorResource::~CTPAdvisorResource() {}

// TPSettings.cpp (main app), the item list resource's file opening and destructor:
//   CTPItemListResource::OpenFile 0x10163b90 and 0x101639f0, ~CTPItemListResource 0x101621f4.
// The destructor's body here is the part that is the class's own (the registered items and their list);
// the rest of the original's code is the inlined base destructors.

// 0x101621f4: the items are deleted and the list too; the base destructors do the rest.
CTPItemListResource::~CTPItemListResource() {
    if (mItems != nullptr) {
        while (mItems->mCount != 0) {
            CTPItemResource* item = mItems->RemoveHead();
            if (item != nullptr) itemlist_callee::ItemDelete(item, 1);
        }
        if (mItems != nullptr) itemlist_callee::ItemListDelete(mItems, 1);
        mItems = nullptr;
        mF48 = nullptr;
    }
}

CTPItemResource::~CTPItemResource() {}

CTPFontResource::~CTPFontResource() {}
CTPSpriteResource::~CTPSpriteResource() {}
CTPUIResource::~CTPUIResource() {}
CTPTerrainResource::~CTPTerrainResource() {}
CTPLanguageResource::~CTPLanguageResource() {}
CTPFMVResource::~CTPFMVResource() {}

int32_t CTPIntermediate::GetSamFile(bfl::FileSystem::CFileStorage& storage, bfl::FileSystem::CFile& file) const {
    TbDynamicStringTemplate<char> name(mName);
    MakeFullFilename(gTPGlobals.mSamExtension, name);
    return OpenQualifiedFile(file, name, storage);
}

int32_t CTPIntermediate::GetOnlineSamFile(bfl::FileSystem::CFileStorage& storage, bfl::FileSystem::CFile& file) const {
    TbDynamicStringTemplate<char> name(gTPGlobals.mOnlinePrefix);
    name += mName;
    MakeFullFilename(gTPGlobals.mOnlineSamExtension, name);
    return OpenQualifiedFile(file, name, storage);
}

void MakeFullFilename(const TbDynamicStringTemplate<char>& extension, TbDynamicStringTemplate<char>& name) {
    name += extension;
}

int32_t CTPBaseResource::OpenQualifiedFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                                           bfl::FileSystem::CFileStorage& storage) const {
    return storage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded() ? 1 : 0;
}

int32_t CTPBaseResource::OpenQualifiedFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name) {
    if (mStorages != nullptr) {
        bfl::FileSystem::CFileInfo info;
        bfl::FileSystem::CFileInfo blank;
        POSITION position = mStorages->GetHeadPosition();
        while (position != nullptr) {
            bfl::FileSystem::CFileStorage* storage = mStorages->GetNext(position);
            info = blank;
            if (storage->FindFirst(name.mData, info).Succeeded()) {
                mLastFoundFileStorage = storage;
                return storage->OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded() ? 1 : 0;
            }
        }
    }
    mLastFoundFileStorage = nullptr;
    return 0;
}

int32_t CTPSoundResource::FindSoundFile(const TbDynamicStringTemplate<char>& name,
                                        TbDynamicStringTemplate<char>& path) const {
    TbDynamicStringTemplate<char> pattern(name);
    pattern += gTPGlobals.mAllFiles;
    if (mStorages != nullptr) {
        bfl::FileSystem::CFileInfo info;
        bfl::FileSystem::CFileInfo blank;
        POSITION position = mStorages->GetHeadPosition();
        while (position != nullptr) {
            bfl::FileSystem::CFileStorage* storage = mStorages->GetNext(position);
            info = blank;
            if (storage->FindFirst(pattern.mData, info).Succeeded()) {
                storage->FullPath(path, gTPGlobals.mSearchPath);
                return 1;
            }
        }
    }
    return 0;
}

int32_t CTPItemListResource::RegisterItem(CTPItemResource& item) {
    if (mItems == nullptr)
        mItems = new (std::nothrow) TbPtrList<CTPItemResource>;
    if (mItems == nullptr) return 0;
    mItems->AddHead(&item);
    item.mList = this;
    return 1;
}

void CTPItemResource::InitItem(const CFileInfo& info, const TbDynamicStringTemplate<char>& folder) {
    TbDynamicStringTemplate<char> storagePath;
    mName = info.mName;
    mName = RemoveExtension(gTPGlobals.mWadExtension, mName);
    mPath = folder;
    mPath = RemoveExtension(gTPGlobals.mWadExtension, mPath);
    storagePath = folder;

    const int32_t first = mName.Find("_", 0);
    // The substring overload rejects a needle as long as the entire name. For this
    // fixed one-character needle, guard the existing character overload the same way.
    const int32_t last = mName.mLength > 1 ? mName.FindReverse('_', 0xffffffff) : -1;
    if (first != last && mName.Find("_", static_cast<uint32_t>(first + 1)) == last) {
        mName = mName.Left(static_cast<uint32_t>(last));
        if (mName.Left(1) == "p")
            mName = mName.Right(static_cast<uint32_t>(mName.Length() - 1));
    }

    if (FS_IsWadFile(info)) {
        CWadFileStorage storage;
        if (storage.Open(storagePath.mData, 0).Succeeded())
            mStorage = static_cast<CFileStorage>(storage);
    } else {
        CDiscFileStorage storage;
        if (storage.Open(storagePath.mData).Succeeded())
            mStorage = static_cast<CFileStorage>(storage);
    }
}

int32_t CTPItemListResource::FindItem(const TbDynamicStringTemplate<char>& name) const {
    if (mItems == nullptr) return 0;
    auto* node = mItems->mHead;
    while (node != nullptr) {
        CTPItemResource* item = node->mData;
        node = node->mNext;
        if (TPStrEq(item->mName, name)) return 1;
    }
    return 0;
}

// 0x10163b90: tries the folder of each registered item in turn.
int32_t CTPItemListResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode) {
    if (mItems != nullptr) {
        TbTList<CTPItemResource*, CTPItemResource*>::CNode* node = mItems->mHead;
        while (node != nullptr) {
            TbDynamicStringTemplate<char> out;
            TbDynamicStringTemplate<char> base(gTPGlobals.mSearchPath);
            TbDynamicStringTemplate<char> qualified(name);
            CTPItemResource* item = node->mData;
            node = node->mNext;
            bfl::FileSystem::CFileStorage* storage = &item->mStorage;
            base = item->mPath;
            if (QualifyFilename(qualified, base, out) == 1) {
                if (itemlist_callee::StorageOpen(storage, file, out.mData, 0x80000001u, nullptr) >= 0) {
                    mLastFoundFileStorage = storage;
                    return 1;
                }
            }
        }
    }
    mF48 = nullptr;
    mLastFoundFileStorage = nullptr;
    return 0;
}

// 0x101639f0: `storage` and `mode` are not used.
int32_t CTPItemListResource::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name,
                                      bfl::FileSystem::CFileStorage& storage, uint32_t mode) {
    TbDynamicStringTemplate<char> out;
    TbDynamicStringTemplate<char> base;
    TbDynamicStringTemplate<char> qualified(name);
    base = mPath;
    if (QualifyFilename(qualified, base, out) == 1) return OpenQualifiedFile(file, out);
    return 0;
}

int32_t CTPItemResource::GetSamFile(CFile& file) {
    TbDynamicStringTemplate<char> name;
    name = mName;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetEasySamFile(CFile& file) {
    TbDynamicStringTemplate<char> prefix(gTPGlobals.mF427C);
    prefix += mName;
    TbDynamicStringTemplate<char> name;
    name = prefix;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetOnlineSamFile(CFile& file) {
    TbDynamicStringTemplate<char> prefix(gTPGlobals.mOnlinePrefix);
    prefix += mName;
    TbDynamicStringTemplate<char> name;
    name = prefix;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetCoasterSamFile(CFile& file) {
    TbDynamicStringTemplate<char> name;
    name = gTPGlobals.mF42A4;
    name += gTPGlobals.mSamExtension;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::GetTextFile(CFile& file) {
    TbDynamicStringTemplate<char> language(gText.GetLanguageName());
    TbDynamicStringTemplate<char> name;
    name = language;
    name += gTPGlobals.mF42B8;
    return mStorage.OpenFile(file, name.mData, kOpenForReading, nullptr).Succeeded();
}

int32_t CTPItemResource::HasSounds() {
    CFileInfo info;
    CFileInfo blank;
    if (!mStorage.FindFirst(gTPGlobals.mF4164.mData, info).Succeeded()) return 0;
    info = blank;
    if (!mStorage.FindFirst(gTPGlobals.mF4178.mData, info).Succeeded()) return 0;
    info = blank;
    if (!mStorage.FindFirst(gTPGlobals.mF418C.mData, info).Succeeded()) return 0;
    return 1;
}

int32_t CTPItemResource::HasParticles() {
    CFileInfo info;
    return mStorage.FindFirst(gTPGlobals.mF45B0.mData, info).Succeeded();
}

CTPResourceManager::CTPResourceManager() {
    mResources = nullptr;
    mSharedTextures = nullptr;
    mCurrentResource = nullptr;
}

CTPResourceManager::~CTPResourceManager() {
    if (mResources != nullptr) {
        while (mResources->GetCount() != 0) delete mResources->RemoveHead();
        delete mResources;
        mResources = nullptr;
    }
}

int32_t CTPResourceManager::RegisterResource(CTPBaseResource* resource) {
    if (mResources == nullptr) mResources = new TbPtrList<CTPBaseResource>;
    if (mResources == nullptr) return 0;
    mResources->AddHead(resource);
    resource->mManager = this;
    return 1;
}

int32_t CTPResourceManager::RegisterResource(CTPSharedTextureResource* resource) {
    mSharedTextures = resource;
    if (mResources == nullptr) mResources = new TbPtrList<CTPBaseResource>;
    if (mResources == nullptr) return 0;
    mResources->AddHead(resource);
    resource->mManager = this;
    return 1;
}

int32_t CTPResourceManager::UnregisterResource(CTPBaseResource* resource) {
    if (mResources == nullptr) return 0;
    POSITION position = mResources->Find(resource);
    if (position == nullptr) return 0;
    if (mCurrentResource == resource) mCurrentResource = nullptr;
    delete mResources->RemoveAt(position);
    return 1;
}

int32_t CTPResourceManager::OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name) {
    if (mResources != nullptr) {
        POSITION position = mResources->GetHeadPosition();
        while (position != nullptr) {
            CTPBaseResource* resource = mResources->GetNext(position);
            if (resource->OpenFile(file, name, kOpenForReading) == 1) return 1;
        }
    }
    return 0;
}

int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** resource) {
    if (mResources != nullptr) {
        POSITION position = mResources->GetHeadPosition();
        while (position != nullptr) {
            CTPBaseResource* candidate = mResources->GetNext(position);
            if (TPStrEq(candidate->mName, name) == true) {
                *resource = candidate;
                mCurrentResource = candidate;
                return 1;
            }
        }
    }
    mCurrentResource = nullptr;
    return 0;
}

CTPBaseResource* CTPResourceManager::pGetCurrentResource() const { return mCurrentResource; }
void CTPResourceManager::pSetCurrentResource(CTPBaseResource* resource) { mCurrentResource = resource; }

// The list instances this file emits (the ride database's item list aside).
template class TbTList<bfl::FileSystem::CFileStorage*, bfl::FileSystem::CFileStorage*>;
template class TbPtrList<bfl::FileSystem::CFileStorage>;
template class TbTList<CTPBaseResource*, CTPBaseResource*>;
template class TbPtrList<CTPBaseResource>;

// The fields of a Volume the files store swapped (the "on" flag is a byte).
void SwapForFile(CPlayerSettings::Volume& volume) { volume.mVolume = serialise::SwapWord(volume.mVolume); }
void UnswapForFile(CPlayerSettings::Volume& volume) { volume.mVolume = serialise::UnswapWord(volume.mVolume); }

// The template instances for this file's types (0x1016765c..0x10168374).
template<> int32_t SAVEVARSTRUCT<CPlayerSettings::Volume>(void* file, CPlayerSettings::Volume* value, const char* name,
                                                          int32_t noLog) {
    return serialise::SaveStruct(file, value, name, noLog);
}
template<> int32_t LOADVARSTRUCT<CPlayerSettings::Volume>(void* file, CPlayerSettings::Volume* value, const char* name,
                                                          int32_t noLog) {
    return serialise::LoadStruct(file, value, name, noLog);
}
template<> int32_t LOADVAR<CPlayerSettings::eRENDERER>(void* file, CPlayerSettings::eRENDERER* value, const char* name,
                                                       int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
template<> int32_t LOADVAR<CPlayerSettings::eRESOLUTION>(void* file, CPlayerSettings::eRESOLUTION* value,
                                                         const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
template<> int32_t LOADVAR<CPlayerSettings::eDETAIL>(void* file, CPlayerSettings::eDETAIL* value, const char* name,
                                                     int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
template<> int32_t LOADVAR<CPlayerSettings::eVIDEOCARD>(void* file, CPlayerSettings::eVIDEOCARD* value,
                                                        const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}
template<> int32_t SAVEVAR<CPlayerSettings::eRENDERER>(void* file, CPlayerSettings::eRENDERER* value, const char* name,
                                                       int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}
template<> int32_t SAVEVAR<CPlayerSettings::eRESOLUTION>(void* file, CPlayerSettings::eRESOLUTION* value,
                                                         const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}
template<> int32_t SAVEVAR<CPlayerSettings::eDETAIL>(void* file, CPlayerSettings::eDETAIL* value, const char* name,
                                                     int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}
template<> int32_t SAVEVAR<CPlayerSettings::eVIDEOCARD>(void* file, CPlayerSettings::eVIDEOCARD* value,
                                                        const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);
}
