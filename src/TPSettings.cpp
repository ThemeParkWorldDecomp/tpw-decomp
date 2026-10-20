// TPSettings.cpp (decompiled from the main app of SimTheme Park 2.04b3). PARTIAL: the
// resources (CTPBaseResource and its subclasses, CTPResourceManager, QualifyFilename and the
// list instances, 0x1015f464..0x10165a70), CPlayerSettings (0x10165ad0..0x10168374, with
// the LOADVAR/SAVEVAR instances for its types) and __sinit_TPSettings_cpp (0x101683d0). The
// ride database's resources (CTPItemListResource and CTPItemResource: 0x10161148, 0x101612cc,
// 0x101621f4..0x10162470 and 0x10163494..0x10164574; their list: 0x10164610..0x101647dc and
// 0x10164d9c) are untranslated.
#include "TPSettings.h"
#include "TPResource.h"

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "LbFile.h"
#include "OptionsBalance.h"
#include "Serialise.h"
#include "SoundBalance.h"
#include "TPCamera.h"
#include "TPGlobals.h"
#include "TPMovie.h"
#include "TPSound.h"
#include "TPString.h"
#include "TbITable.h"
#include "Text.h"
#include "lobby.h"
#include "platform/interfacelib.h"
#include "ui_ErrorDisplay.h"
#include "ui_parkStatus.h"

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
CPlayerSettings gPlayerSettings;                                       // 0x1038071c

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

// ------------------------------------------------------------------ player settings

namespace {

// Init's machine classes: megabytes of memory and processor MHz.
constexpr int32_t kSlowMemory = 64;
constexpr int32_t kSlowSpeed = 300;
constexpr int32_t kFastMemory = 192;
constexpr int32_t kFastSpeed = 450;
// Global settings files carry this version first.
constexpr int32_t kSettingsVersion = 1;
constexpr uint32_t kLbFileRead = 0x80000001;
constexpr uint32_t kLbFileCreate = 0xc0000010;

// sound.sam's value slots (gSoundBalance).
enum SoundSlot : int32_t {
    kMpegThreshold = 6, kDefaultQuality = 9, kSoundOn = 11, kSfxVolume = 12, kMusicVolume = 13,
    kSpeechVolume = 14, kMovieVolume = 15,
};
int32_t Sound(SoundSlot slot) { return gSoundBalance.mValues[slot].mInteger; }

// The options files' slots LoadGraphicsDetails compares (gOptionsBalance).
enum OptionsSlot : int32_t {
    kTextureSize = 1, kRestartOption11 = 11, kRestartOption13 = 13, kRestartOption14 = 14, kRestartOption15 = 15,
    kNumKids = 24,
};

}  // namespace

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

CPlayerSettings::~CPlayerSettings() {}

void CPlayerSettings::Init() {
    mCurrent.mRenderer = eRENDERER(1);
    int32_t memory;
    if (Gestalt(0x72616d20 /* 'ram ' */, &memory) == 0) memory /= 0x100000;
    else memory = 64;
    int32_t speed;
    if (Gestalt(0x70636c6b /* 'pclk' */, &speed) == 0) speed /= 1000000;
    else speed = 266;
    if (memory <= kSlowMemory || speed <= kSlowSpeed) {
        mCurrent.mResolution = eRESOLUTION(1);
        mCurrent.mDetail = eDETAIL(0);
        mCurrent.mSoundQuality = Sound(kDefaultQuality) / 2;
    } else if (memory < kFastMemory || speed < kFastSpeed) {
        mCurrent.mResolution = eRESOLUTION(1);
        mCurrent.mDetail = eDETAIL(1);
        mCurrent.mSoundQuality = Sound(kDefaultQuality);
    } else {
        mCurrent.mResolution = eRESOLUTION(2);
        mCurrent.mDetail = eDETAIL(2);
        mCurrent.mSoundQuality = (Sound(kDefaultQuality) + 100) / 2;
    }
    mCurrent.mVideoCard = eVIDEOCARD(0);
    mCurrent.mSfx.mVolume = Sound(kSfxVolume);
    mCurrent.mMusic.mVolume = Sound(kMusicVolume);
    mCurrent.mSpeech.mVolume = Sound(kSpeechVolume);
    mCurrent.mMovie.mVolume = Sound(kMovieVolume);
    if (Sound(kSoundOn) != 0) {
        mCurrent.mSfx.mOn = true;
        mCurrent.mMusic.mOn = true;
        mCurrent.mSpeech.mOn = true;
        mCurrent.mMovie.mOn = true;
    }
    mCurrent.mAdvisorOn = true;
    mCurrent.mTutorialOn = true;
    mCurrent.mTooltipsOn = true;
    mCurrent.mConfirmDeleteOn = true;
    mCurrent.mRMBScrollOn = false;
    mCurrent.mRMBCancelOn = true;
    mCurrent.mIsometricOn = true;
    mStored = mCurrent;
    UpdateVolume();
}

int32_t CPlayerSettings::GetMusicVolume() const { return mCurrent.mMusic.mVolume; }
int32_t CPlayerSettings::GetSFXVolume() const { return mCurrent.mSfx.mVolume; }
int32_t CPlayerSettings::GetSpeechVolume() const { return mCurrent.mSpeech.mVolume; }
int32_t CPlayerSettings::GetMovieVolume() const { return mCurrent.mMovie.mVolume; }

void CPlayerSettings::UpdateSettings() {
    bool restarting = false;
    if (mCurrent.mDetail != mStored.mDetail) LoadGraphicsDetails(mCurrent.mDetail);
    if (HasMPEGRateChanged(mCurrent.mSoundQuality, mStored.mSoundQuality)) {
        SetGameContinueState(eGAME_CONTINUE_STATE(1));
        SetGameRefreshState(eGAME_REFRESH_STATE(1));
    }
    if (mCurrent.mRenderer != mStored.mRenderer) SetGameContinueState(eGAME_CONTINUE_STATE(2));
    if (mCurrent.mResolution != mStored.mResolution) SetGameContinueState(eGAME_CONTINUE_STATE(2));
    if (mCurrent.mVideoCard != mStored.mVideoCard) SetGameContinueState(eGAME_CONTINUE_STATE(2));
    if (mGameContinueState == 1) {
        // a park is refreshed in place: kept in a hidden save around the sound change
        const uint32_t refresh = mGameRefreshState;
        if (gLobby == nullptr) gLoadSave.SaveHidden(gTPGlobals.mRefreshName);
        if ((refresh & 1) != 0) SOUND_StopAllSfx();
        SOUND_SetSoundQuality(mCurrent.mSoundQuality);
        if (gLobby == nullptr) gLoadSave.LoadHidden(gTPGlobals.mRefreshName);
    } else if (mGameContinueState == 2) {
        RestartDialog();
        restarting = true;
    }
    UpdateVolume();
    TbDynamicStringTemplate<char16_t> widePath;
    TbDynamicStringTemplate<char> path;
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    widePath = gUserInfo->ReturnPathToSaveDirectory();
    widePath += gTPGlobals.mWideSeparator;
    widePath += gTPGlobals.mConfigFile;
    UniStringToString(path, widePath);
    SaveGlobal(path);
    if (mCurrent.mIsometricOn) RoundRotationToNearest90Degrees();
    SOUND_Tick();
    if (!restarting) RemoveOptionsPanel();
}

void CPlayerSettings::ResetSettings() {
    mCurrent = mStored;
    ResetGameContinueState();
    ResetGameRefreshState();
}

void CPlayerSettings::StoreSettings() {
    mStored = mCurrent;
    SetGameContinueState(eGAME_CONTINUE_STATE(0));
    ResetGameRefreshState();
}

void CPlayerSettings::SetGameContinueState(eGAME_CONTINUE_STATE state) {
    if (state > mGameContinueState) mGameContinueState = state;
}
void CPlayerSettings::ResetGameContinueState() { mGameContinueState = eGAME_CONTINUE_STATE(0); }
void CPlayerSettings::SetGameRefreshState(eGAME_REFRESH_STATE state) { mGameRefreshState |= static_cast<uint32_t>(state); }
void CPlayerSettings::ResetGameRefreshState() { mGameRefreshState &= ~7u; }
void CPlayerSettings::SetSoundQuality(int32_t quality) { mCurrent.mSoundQuality = quality; }
void CPlayerSettings::SetRendererType(eRENDERER type) { mCurrent.mRenderer = type; }
void CPlayerSettings::SetResolution(eRESOLUTION resolution) {
    if (mCurrent.mResolution != resolution) mCurrent.mResolution = resolution;
}
void CPlayerSettings::SetVideoCard(eVIDEOCARD card) {
    if (mCurrent.mVideoCard != card) mCurrent.mVideoCard = card;
}
void CPlayerSettings::SetGraphicsDetail(eDETAIL detail) {
    if (mCurrent.mDetail != detail) mCurrent.mDetail = detail;
}

int32_t CPlayerSettings::LoadGraphicsDetails(eDETAIL detail) {
    COptionsBalance options;
    TbDynamicStringTemplate<char> path;
    path = gTPFileInfo.GetDataPath();
    FS_AddDirectorySeparator(path);
    if (detail == 0) path += gTPGlobals.mLowOptionsFile;
    else if (detail == 1) path += gTPGlobals.mMediumOptionsFile;
    else if (detail == 2) path += gTPGlobals.mHighOptionsFile;
    if (options.Load(path.mData, CBalanceTable::LoadMode(0)) == 0) return 0;
    // a change of these options needs the game restarted
    const auto restart = [this]() {
        if (mGameContinueState < 2) mGameContinueState = eGAME_CONTINUE_STATE(2);
    };
    const auto value = [](const COptionsBalance& table, OptionsSlot slot) { return table.mValues[slot].mInteger; };
    if (HasTextureSizeChanged(value(gOptionsBalance, kTextureSize), value(options, kTextureSize))) restart();
    if (HasNumKidsChanged(value(gOptionsBalance, kNumKids), value(options, kNumKids))) restart();
    for (const OptionsSlot slot : {kRestartOption11, kRestartOption13, kRestartOption14, kRestartOption15})
        if (value(options, slot) != value(gOptionsBalance, slot)) restart();
    return 1;
}

bool CPlayerSettings::HasTextureSizeChanged(int32_t now, int32_t before) const { return now != before; }
bool CPlayerSettings::HasNumKidsChanged(int32_t now, int32_t before) const { return now != before; }

bool CPlayerSettings::HasMPEGRateChanged(int32_t now, int32_t before) const {
    const int32_t threshold = Sound(kMpegThreshold);
    if (before > threshold && now <= threshold) return true;
    if (before <= threshold && now > threshold) return true;
    return false;
}

void CPlayerSettings::UpdateVolume() {
    const Settings& settings = gPlayerSettings.mCurrent;   // the global's, whichever object is asked
    if (settings.mMusic.mOn) {
        SOUND_SetMusicStatus(1);
        SOUND_SetMusicVolume(static_cast<uint32_t>(settings.mMusic.mVolume));
    } else {
        SOUND_SetMusicStatus(0);
    }
    if (settings.mSpeech.mOn) {
        SOUND_SetSpeechStatus(1);
        SOUND_SetSpeechVolume(static_cast<uint32_t>(settings.mSpeech.mVolume));
    } else {
        SOUND_SetSpeechStatus(0);
    }
    if (settings.mSfx.mOn) {
        SOUND_SetSfxStatus(1);
        SOUND_SetSfxVolume(static_cast<uint32_t>(settings.mSfx.mVolume));
    } else {
        SOUND_SetSfxStatus(0);
    }
    // the movie player's volume is 0 .. 0x3ff, the setting a percentage
    if (settings.mMovie.mOn) MOVIE_SetVolume(static_cast<uint32_t>(settings.mMovie.mVolume * 0x3ff / 100));
    else MOVIE_SetVolume(0);
}

int32_t CPlayerSettings::SerialiseLocal(void* file, int32_t save, uint32_t version) {
    if (save != 0) {
        if (SAVEVARSTRUCT(file, &mCurrent.mSfx, "SFXVolume", 1) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mCurrent.mMusic, "MusicVolume", 1) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mCurrent.mSpeech, "SpeechVolume", 1) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mCurrent.mMovie, "MovieVolume", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mAdvisorOn, "AdvisorOn", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mTutorialOn, "TutorialOn", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mTooltipsOn, "TooltipsOn", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mConfirmDeleteOn, "ConfirmDeleteOn", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mRMBScrollOn, "RMBScrollOn", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mRMBCancelOn, "RMBCancelOn", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mIsometricOn, "IsometricOn", 1) != 0) return 1;
    } else {
        if (LOADVARSTRUCT(file, &mCurrent.mSfx, "SFXVolume", 1) != 0) return 1;
        if (LOADVARSTRUCT(file, &mCurrent.mMusic, "MusicVolume", 1) != 0) return 1;
        if (LOADVARSTRUCT(file, &mCurrent.mSpeech, "SpeechVolume", 1) != 0) return 1;
        if (LOADVARSTRUCT(file, &mCurrent.mMovie, "MovieVolume", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mAdvisorOn, "AdvisorOn", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mTutorialOn, "TutorialOn", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mTooltipsOn, "TooltipsOn", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mConfirmDeleteOn, "ConfirmDeleteOn", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mRMBScrollOn, "RMBScrollOn", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mRMBCancelOn, "RMBCancelOn", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mIsometricOn, "IsometricOn", 1) != 0) return 1;
    }
    UpdateVolume();
    return 0;
}

int32_t CPlayerSettings::SerialiseGlobal(void* file, int32_t save, uint32_t version) {
    if (save != 0) {
        if (SAVEVAR(file, &mCurrent.mRenderer, "Renderer", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mResolution, "Resolution", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mDetail, "Detail", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mVideoCard, "VideoCard", 1) != 0) return 1;
        if (SAVEVARSTRUCT(file, &mCurrent.mMovie, "MovieVolume", 1) != 0) return 1;
        if (SAVEVAR(file, &mCurrent.mSoundQuality, "SoundQuality", 1) != 0) return 1;
    } else {
        if (LOADVAR(file, &mCurrent.mRenderer, "Renderer", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mResolution, "Resolution", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mDetail, "Detail", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mVideoCard, "VideoCard", 1) != 0) return 1;
        if (LOADVARSTRUCT(file, &mCurrent.mMovie, "MovieVolume", 1) != 0) return 1;
        if (LOADVAR(file, &mCurrent.mSoundQuality, "SoundQuality", 1) != 0) return 1;
    }
    UpdateVolume();
    return 0;
}

int32_t CPlayerSettings::SaveGlobal(TbDynamicStringTemplate<char>& path) {
    void* file;
    if (!LbFile_Open(file, path.mData, kLbFileCreate).Succeeded()) {
        CTRACE(3, "Failed to open file %s", path.mData);
        return 0;
    }
    int32_t version = kSettingsVersion;
    if (SAVEVAR(file, &version, "version", 1) != 0 || SerialiseGlobal(file, 1, static_cast<uint32_t>(version)) != 0) {
        LbFile_Close(file);
        return 0;
    }
    LbFile_Close(file);
    return 1;
}

int32_t CPlayerSettings::LoadGlobal(TbDynamicStringTemplate<char>& path) {
    Init();
    void* file;
    if (!LbFile_Open(file, path.mData, kLbFileRead).Succeeded()) {
        CTRACE(3, "Couldn't open file %s ", path.mData);
        return 0;
    }
    int32_t version;
    if (LOADVAR(file, &version, "versionNo", 1) != 0 || static_cast<uint32_t>(version) < static_cast<uint32_t>(kSettingsVersion) ||
        SerialiseGlobal(file, 0, static_cast<uint32_t>(version)) != 0) {
        LbFile_Close(file);
        return 0;
    }
    LbFile_Close(file);
    return 1;
}

int32_t RestartCallback_OK(void* data) {
    GS_SetCurrentGameStatus(GS_LEAVE_PARK);
    gGame.mRestart = 1;
    if (gLobby == nullptr) {
        gLoadSave.Save(gTPGlobals.mAutosaveName);
        gGame.mRestart = 1;
    } else {
        gLobby->mState = 3;
    }
    RemoveOptionsPanel();
    return 1;
}

int32_t RestartCallback_Cancel(void* data) {
    gPlayerSettings.mGameContinueState = CPlayerSettings::eGAME_CONTINUE_STATE(0);
    return 1;
}

void RestartDialog() {
    char16_t text[0x100];
    static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(text, 0xff, 0x193);
    dlgboxparams params;
    params.mText = text;
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    UI_DialogBox(params, dlgbutton(2), RestartCallback_OK, dlgbutton(1), RestartCallback_Cancel, dlgbutton(0), nullptr,
                 false);
}
