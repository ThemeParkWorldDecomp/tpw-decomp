// TPSettings.cpp: name proven by __sinit_TPSettings_cpp in the main application.

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

// TPSettings.cpp (decompiled from the main app of SimTheme Park 2.04b3). PARTIAL: the
// resources (CTPBaseResource and its subclasses, CTPResourceManager, QualifyFilename and the
// list instances, 0x1015f464..0x10165a70), CPlayerSettings (0x10165ad0..0x10168374, with
// the LOADVAR/SAVEVAR instances for its types) and __sinit_TPSettings_cpp (0x101683d0). The
// ride database's resources (CTPItemListResource and CTPItemResource: 0x10161148, 0x101612cc,
// 0x101621f4..0x10162470 and 0x10163494..0x10164574; their list: 0x10164610..0x101647dc and
// 0x10164d9c) are untranslated.

CPlayerSettings gPlayerSettings;                                       // 0x1038071c

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

template<> int32_t SAVEVARSTRUCT<CPlayerSettings::Volume>(void* file, CPlayerSettings::Volume* value, const char* name,
                                                          int32_t noLog);

template<> int32_t LOADVARSTRUCT<CPlayerSettings::Volume>(void* file, CPlayerSettings::Volume* value, const char* name,
                                                          int32_t noLog);

template<> int32_t LOADVAR<CPlayerSettings::eRENDERER>(void* file, CPlayerSettings::eRENDERER* value, const char* name,
                                                       int32_t noLog);

template<> int32_t LOADVAR<CPlayerSettings::eRESOLUTION>(void* file, CPlayerSettings::eRESOLUTION* value,
                                                         const char* name, int32_t noLog);

template<> int32_t LOADVAR<CPlayerSettings::eDETAIL>(void* file, CPlayerSettings::eDETAIL* value, const char* name,
                                                     int32_t noLog);

template<> int32_t LOADVAR<CPlayerSettings::eVIDEOCARD>(void* file, CPlayerSettings::eVIDEOCARD* value,
                                                        const char* name, int32_t noLog);

template<> int32_t SAVEVAR<CPlayerSettings::eRENDERER>(void* file, CPlayerSettings::eRENDERER* value, const char* name,
                                                       int32_t noLog);

template<> int32_t SAVEVAR<CPlayerSettings::eRESOLUTION>(void* file, CPlayerSettings::eRESOLUTION* value,
                                                         const char* name, int32_t noLog);

template<> int32_t SAVEVAR<CPlayerSettings::eDETAIL>(void* file, CPlayerSettings::eDETAIL* value, const char* name,
                                                     int32_t noLog);

template<> int32_t SAVEVAR<CPlayerSettings::eVIDEOCARD>(void* file, CPlayerSettings::eVIDEOCARD* value,
                                                        const char* name, int32_t noLog);

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
