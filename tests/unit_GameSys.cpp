// The fakes for GameSys.cpp and Main.cpp (libtpw_unit_GameSys.dylib; see calllog.h) and
// the test entry points. Each fake records the event its PowerPC twin in test_GameSys.py
// records. Fakes are functional where a value flows back into the unit: strings, paths,
// the lobby's and the online client's states, the game type, the exit error.
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>
#include <string>

#include "platform/msl.h"
#include "ActionRecorder.h"
#include "Advisor.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Clock.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_LoadSave.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "InGame.h"
#include "LbTime.h"
#include "Main.h"
#include "MapWho.h"
#include "Mouse.h"
#include "OptionsBalance.h"
#include "SaveLoad.h"
#include "SoundBalance.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPDialog.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "Text.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "bfl_FileSystem.h"
#include "calllog.h"
#include "intrface.h"
#include "keyboard.h"
#include "lobby.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "tb_capture.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using calllog::ev;
using calllog::fbits;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

// Texts the case provides (tpw_gs_set_text), read by the fakes that return text.
enum : int32_t { kTextLanguage, kTextAuthorizedUser, kTextDataPath, kTextThemeName, kTextFTPPath, kTexts };
extern "C" {
char tpw_gs_texts[kTexts][160];
int32_t tpw_gs_argc = 0;
char tpw_gs_argv_text[8][96];
}

namespace {

char* dup(const char* text) {
    const std::size_t n = std::strlen(text) + 1;
    char* out = static_cast<char*>(std::malloc(n));
    std::memcpy(out, text, n);
    return out;
}

char16_t* dup16(const char16_t* text) {
    std::size_t n = 0;
    while (text[n] != 0) ++n;
    auto* out = static_cast<char16_t*>(std::malloc((n + 1) * sizeof(char16_t)));
    std::memcpy(out, text, (n + 1) * sizeof(char16_t));
    return out;
}

const char* text_or_null(const char* text) { return text != nullptr ? text : "(null)"; }

}  // namespace

// ------------------------------------------------------------------ bullfrog strings (functional)

template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup("");
    mLength = 0;}
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup(text);
    mLength = static_cast<int32_t>(std::strlen(text));}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(
    const TbDynamicStringTemplate<char>& other) {
    mData = dup(other.mData);
    mLength = other.mLength;
    return *this;
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& text) {
    const auto& other = static_cast<const TbDynamicStringTemplate<char>&>(text);
    std::string joined = std::string(mData) + other.mData;
    mData = dup(joined.c_str());
    mLength = static_cast<int32_t>(joined.size());
    return *this;
}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup16(u"");
    mLength = 0;}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = dup16(text);
    mLength = 0;}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(
    const TbDynamicStringTemplate<char16_t>& other) {
    mData = dup16(other.mData);
    return *this;
}

// ------------------------------------------------------------------ globals of other files

uint32_t gTPWFlags;
GrafPort* gMainWindow;
int32_t gScreenHeight;
uint32_t gGameOptions;
uint32_t gTPDialogOptions;
CClock gClock;
CText gText;
// The game object here is a blank one: the unit sets the fields its code reads.
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
CSYS_TPFileInfo gTPFileInfo;
// The unit starts from a blank object, not the game's default names.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CExitError gExitError;
CPlayerSettings gPlayerSettings;
// TPSettings.cpp's own members this unit does not test (unrecorded).
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
COptionsBalance gOptionsBalance;
CSoundBalance gSoundBalance;
CFSUserInfo* gUserInfo;
CFSOnlineSystem* gOnlineSystem;
COnlineControl* gOnlineControl;
CFSGlobalMasterSave* gMasterSave;
CMessageCentre* gMessageCentre;
CMouse* gMouse;
CLBLobby* gLobby;
CIntrface gIntrface;
UIInterface gUIInterface;
CFSLoadSave gLoadSave;
CSaveLoad gSaveLoad;
CActionRecorder gActionRecorder;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
uint32_t gFirstPersonFlags;
sMatrix gCameraMatrix;
float gCameraTarget[3];
s3DPoint gPointerWorldPosn;
s3DPoint gPickupWorldPosn;
uint8_t gSmallTexturesOnly;
int32_t gSoundBanks[8];
CSplash gei_Splash;
CTextureCache* gei_TCache;
CMapWho* gei_MapWho;
CSystem gei_System;
CDisplay gei_Display;
CDisplay::CDisplay() {}   // the unit keeps a blank display of its own
CMatrix gei_ProjectionMatrix;
CGamut* gei_Gamut;
CRain* gei_Rain;
CSpriteManager* gei_SpriteManager;
CCapture gei_Capture;

// Construction and destruction of those globals (not recorded: they run at load time).
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
void CBalanceTable::Init() {}
COptionsBalance::~COptionsBalance() {}
const BalanceDescription* COptionsBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* COptionsBalance::GetBalanceValuesStart() { return mValues; }
CSoundBalance::~CSoundBalance() {}
const BalanceDescription* CSoundBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CSoundBalance::GetBalanceValuesStart() { return mValues; }
CSplash::~CSplash() {}
CSystem::CSystem() {}
CCommandLine::CCommandLine() { ev("CCommandLine", {newlab(this)}); }
CCommandLine::~CCommandLine() {}

// ------------------------------------------------------------------ C library (this library only)

extern "C" time_t time(time_t* out) {
    ev("time");
    const time_t value = static_cast<time_t>(ret("time"));
    if (out != nullptr) *out = value;
    return value;
}

extern "C" struct tm* localtime(const time_t* when) {
    static struct tm result;
    ev("localtime", {static_cast<int32_t>(*when)});
    std::memset(&result, 0, sizeof(result));
    result.tm_mday = static_cast<int>(ret("localtime.mday"));
    return &result;
}

void msl_srand(uint32_t seed) { ev("srand", {seed}); }

// ------------------------------------------------------------------ platform

void SamsUtilities::UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
void SamsUtilities::UThread::Yield() { ev("Yield"); }
uint64_t SamsUtilities::UTimer::GetAbsolute() {
    ev("GetAbsolute");
    return static_cast<uint64_t>(ret("GetAbsolute"));
}
int16_t NS_MacDoze::GetAsyncKeyState(int32_t key) {
    ev("GetAsyncKeyState", {key});
    return static_cast<int16_t>(ret("GetAsyncKeyState", key, 0));
}
int32_t NS_MacDoze::ScreenToClient(GrafPort* window, tagPOINT* point) {
    ev("ScreenToClient", {lab(window), point->x, point->y});
    point->x += static_cast<int32_t>(ret("ScreenToClient.dx"));
    point->y += static_cast<int32_t>(ret("ScreenToClient.dy"));
    return 1;
}
int32_t NS_MacDoze::_stricmp(const char* a, const char* b) {
    ev("_stricmp", {}, {a, b});
    for (;; ++a, ++b) {
        const int ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : static_cast<unsigned char>(*a);
        const int cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : static_cast<unsigned char>(*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (ca == 0) return 0;
    }
}
uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock");
    return static_cast<uint32_t>(ret("LbTime_GetClock"));
}

// ------------------------------------------------------------------ engine

int32_t CSplash::Init(char* language, char* path, int32_t frames) {
    ev("CSplash::Init", {lab(this), frames}, {text_or_null(language), text_or_null(path)});
    return 1;
}
int32_t CSplash::Close() {
    ev("CSplash::Close", {lab(this)});
    return 1;
}
void CTextureCache::Flush(int32_t mode) { ev("CTextureCache::Flush", {lab(this), mode}); }
int32_t CMapWho::ResetAll() { ev("CMapWho::ResetAll", {lab(this)}); return 1; }
int32_t CMapWho::Reset(int32_t list) { ev("CMapWho::Reset", {lab(this), list}); return 1; }
void CMapWho::SetFinalCallback(void (*callback)()) {
    ev("CMapWho::SetFinalCallback",
       {lab(this), callback == nullptr ? 0 : callback == &PostCullOperations ? 1 : -1});
}
void CMatrix::Projection(float nearPlane, float farPlane, float fov) {
    ev("CMatrix::Projection", {lab(this), fbits(nearPlane), fbits(farPlane), fbits(fov)});
}
void CGamut::Init(float fov) { ev("CGamut::Init", {lab(this), fbits(fov)}); }
int32_t RenderSystem_Flush() {
    ev("RenderSystem_Flush");
    return 1;
}
int32_t CRain::Load(int32_t kind, char* path) {
    ev("CRain::Load", {lab(this), kind}, {text_or_null(path)});
    return 1;
}
int32_t CSpriteManager::LoadBlobShadow(char* path) {
    ev("CSpriteManager::LoadBlobShadow", {lab(this)}, {text_or_null(path)});
    return 1;
}
int32_t CCapture::SetCapture(char* name) {
    ev("CCapture::SetCapture", {lab(this)}, {name});
    return 1;
}
bfl::FileSystem::CFileStorage::CFileStorage() { ev("CFileStorage", {newlab(this)}); }
bfl::FileSystem::CFileStorage::~CFileStorage() { ev("~CFileStorage", {lab(this)}); }
bfl::FileSystem::CDiscFile::CDiscFile() { ev("CDiscFile", {newlab(this)}); }
bfl::FileSystem::CDiscFile::~CDiscFile() { ev("~CDiscFile", {lab(this)}); }
void bfl::FileSystem::CFile::Close() { ev("CFile::Close", {lab(this)}); }
// Members of the library classes this unit does not use (unrecorded).
bfl::FileSystem::CFile::CFile() : mImplementation(nullptr) {}
bfl::FileSystem::CFile::~CFile() {}
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t bfl::FileSystem::CFile::GetSize() { return 0; }
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile&, const char*, uint32_t, void*) { return TbError(0); }
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char*, CFileInfo&) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo&) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::FullPath(TbDynamicStringTemplate<char>&, const TbDynamicStringTemplate<char>&) {
    return TbError(0);
}
TbError bfl::FileSystem::CDiscFile::Open(const char* name, uint32_t mode) {
    ev("CDiscFile::Open", {lab(this), mode}, {name});
    return TbError(static_cast<int32_t>(ret("CDiscFile::Open", -1)));
}

// ------------------------------------------------------------------ main app

int32_t CClock::Read() {
    ev("CClock::Read", {lab(this)});
    return static_cast<int32_t>(ret("CClock::Read"));
}
void CFixableTime::UseFixedTime(uint32_t step) { ev("UseFixedTime", {lab(this), step}); }
void CFixableTime::CancelFixedTime() { ev("CancelFixedTime", {lab(this)}); }

void UI_HideCursor() { ev("UI_HideCursor"); }
void UI_ShowCursor() { ev("UI_ShowCursor"); }
int16_t UI_GetButtonFlags() {
    ev("UI_GetButtonFlags");
    return static_cast<int16_t>(ret("UI_GetButtonFlags"));
}
void UI_DevicePointerMove(int16_t x, int16_t y) { ev("UI_DevicePointerMove", {x, y}); }

void UIInterface::Process() { ev("UIInterface::Process", {lab(this)}); }
void UIInterface::InitGeneral() { ev("UIInterface::InitGeneral", {lab(this)}); }
void UIInterface::DeInitGeneral() { ev("UIInterface::DeInitGeneral", {lab(this)}); }
void UIInterface::InitGame() { ev("UIInterface::InitGame", {lab(this)}); }
void UIInterface::DeInitGame() { ev("UIInterface::DeInitGame", {lab(this)}); }
void UIInterface::InitLobby() { ev("UIInterface::InitLobby", {lab(this)}); }
void UIInterface::DeInitLobby() { ev("UIInterface::DeInitLobby", {lab(this)}); }
void UIInterface::InitOnline() { ev("UIInterface::InitOnline", {lab(this)}); }
void UIInterface::DeInitOnline() { ev("UIInterface::DeInitOnline", {lab(this)}); }
void SetThemeTextureDirectories(int32_t small) { ev("SetThemeTextureDirectories", {small}); }
const char* SEC_AuthorizedUser() {
    ev("SEC_AuthorizedUser");
    return tpw_gs_texts[kTextAuthorizedUser];
}
void UIMain_GetFullyOutOfFirstPerson() { ev("UIMain_GetFullyOutOfFirstPerson"); }

char* CText::GetLanguageName() {
    ev("CText::GetLanguageName", {lab(this)});
    return tpw_gs_texts[kTextLanguage];
}

void CGame::Unpause() { ev("CGame::Unpause", {lab(this)}); }
int32_t CGame::GetCurrentTheme() const {
    ev("CGame::GetCurrentTheme", {lab(this)});
    return static_cast<int32_t>(ret("CGame::GetCurrentTheme"));
}
void CGame::SetCurrentTheme(int32_t theme) { ev("CGame::SetCurrentTheme(long)", {lab(this), theme}); }
void CGame::SetCurrentTheme(const TbDynamicStringTemplate<char>& name) {
    ev("CGame::SetCurrentTheme(name)", {lab(this)}, {name.mData});
}
const TbDynamicStringTemplate<char>& CGame::GetCurrentThemeName() {
    static TbDynamicStringTemplate<char> sName;
    ev("CGame::GetCurrentThemeName", {lab(this)});
    sName = TbDynamicStringTemplate<char>(tpw_gs_texts[kTextThemeName]);
    return sName;
}
void CGame::InitThemeResources(const TbDynamicStringTemplate<char>& path) {
    ev("CGame::InitThemeResources", {lab(this)}, {path.mData});
}
int32_t CGame::InitWorld() {
    ev("CGame::InitWorld", {lab(this)});
    return static_cast<int32_t>(ret("CGame::InitWorld", 1));
}
int32_t CGame::InitTheme(char* path) {
    ev("CGame::InitTheme", {lab(this)}, {text_or_null(path)});
    return static_cast<int32_t>(ret("CGame::InitTheme", 1));
}
void CGame::ExitLevel() { ev("CGame::ExitLevel", {lab(this)}); }
void CGame::Iterate() { ev("CGame::Iterate", {lab(this)}); }
void CGame::DeInit() { ev("CGame::DeInit", {lab(this)}); }
int32_t CCommandLine::GetArgc() {
    ev("CCommandLine::GetArgc", {lab(this)});
    return tpw_gs_argc;
}
char** CCommandLine::GetArgv() {
    static char* sArgv[9];
    ev("CCommandLine::GetArgv", {lab(this)});
    for (int32_t i = 0; i < 8; ++i) sArgv[i] = tpw_gs_argv_text[i];
    sArgv[8] = nullptr;
    return sArgv;
}

void CExitError::ReportAnyError() { ev("CExitError::ReportAnyError", {lab(this)}); }
void CExitError::SetError(ErrorType type, uint32_t value, const char* text) {
    ev("CExitError::SetError", {lab(this), type, value}, {text});
    mError = 1;
}

void InitialiseKeytables() { ev("InitialiseKeytables"); }
void DeInitialiseKeyTables() { ev("DeInitialiseKeyTables"); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* first = va_arg(args, const char*);
    va_end(args);
    ev("CTRACE", {level}, {format, text_or_null(first)});
}
void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void TickRideHighlights() { ev("TickRideHighlights"); }
void TickFlashObjects() { ev("TickFlashObjects"); }

const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const {
    static TbDynamicStringTemplate<char> sPath;
    ev("CSYS_TPFileInfo::GetDataPath", {lab(this)});
    sPath = TbDynamicStringTemplate<char>(tpw_gs_texts[kTextDataPath]);
    return sPath;
}
void CSYS_TPFileInfo::SetDataPath(const TbDynamicStringTemplate<char>& path) {
    ev("CSYS_TPFileInfo::SetDataPath", {lab(this)}, {path.mData});
}
void CSYS_TPFileInfo::SetLevelPath(const TbDynamicStringTemplate<char>& path) {
    ev("CSYS_TPFileInfo::SetLevelPath", {lab(this)}, {path.mData});
}
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("FS_AddDirectorySeparator", {}, {path.mData});
    path += TbDynamicStringTemplate<char>(":");
}
uint8_t FS_FindFilePath(const TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& path) {
    ev("FS_FindFilePath", {}, {name.mData});
    path = TbDynamicStringTemplate<char>((std::string("Found:") + name.mData).c_str());
    return static_cast<uint8_t>(ret("FS_FindFilePath"));
}
char* FindFilePath(const char* name, uint32_t flags, int32_t* result) {
    static char sPath[200];
    ev("FindFilePath", {flags, result == nullptr ? 0 : -1}, {name});
    std::snprintf(sPath, sizeof(sPath), "Found:%s", name);
    return sPath;
}

void ADV_Init() { ev("ADV_Init"); }
void ADV_Close() { ev("ADV_Close"); }
void ADV_Kill(int32_t mode) { ev("ADV_Kill", {mode}); }
void ADV_Tick2() { ev("ADV_Tick2"); }
void ADV_Tick() { ev("ADV_Tick"); }
void LoadAdvisor(char* path, char* a, char* b, uint32_t flags) {
    ev("LoadAdvisor", {flags}, {text_or_null(path), text_or_null(a), text_or_null(b)});
}
void TickAdvisor(uint32_t which) { ev("TickAdvisor", {which}); }
void DestroyAdvisor(uint32_t which) { ev("DestroyAdvisor", {which}); }
void BMP_Tick() { ev("BMP_Tick"); }

void PAR_LoadLibrary(char* path) { ev("PAR_LoadLibrary", {}, {text_or_null(path)}); }
void PAR_InitParticleSystem(void* owner, int32_t density, int32_t maxParticles) {
    ev("PAR_InitParticleSystem", {lab(owner), density, maxParticles});
}
void PAR_CloseParticleSystem() { ev("PAR_CloseParticleSystem"); }
void PAR_SetLowSpec(int32_t on) { ev("PAR_SetLowSpec", {on}); }
void PAR_Tick() { ev("PAR_Tick"); }
void TickRideAnims() { ev("TickRideAnims"); }
void RSSE_Init() { ev("RSSE_Init"); }
int32_t RSSE_Tick() {
    ev("RSSE_Tick");
    return 0;
}
void RSSE_RenderTick() { ev("RSSE_RenderTick"); }
void RSSE_Close() { ev("RSSE_Close"); }
void RSSE_SetReverbOnEax() { ev("RSSE_SetReverbOnEax"); }

int32_t MOVIE_Play(const char* path, GrafPort* window, int32_t x, int32_t y, int32_t flags) {
    ev("MOVIE_Play", {lab(window), x, y, flags}, {path});
    return 1;
}
int32_t MOVIE_Tick() {
    ev("MOVIE_Tick");
    return static_cast<int32_t>(ret("MOVIE_Tick"));
}
void MOVIE_Stop() { ev("MOVIE_Stop"); }
void MOVIE_Init() { ev("MOVIE_Init"); }
void MOVIE_Close() { ev("MOVIE_Close"); }

int32_t SOUND_Init(char* name, uint32_t flags, int32_t quality) {
    ev("SOUND_Init", {flags, quality}, {name});
    return 0;
}
void SOUND_Close() { ev("SOUND_Close"); }
void SOUND_SetSoundQuality(int32_t quality) { ev("SOUND_SetSoundQuality", {quality}); }
void SOUND_SetGlobalVolume(uint32_t volume) { ev("SOUND_SetGlobalVolume", {volume}); }
void SOUND_SetMusicVolume(uint32_t volume) { ev("SOUND_SetMusicVolume", {volume}); }
void SOUND_SetSfxVolume(uint32_t volume) { ev("SOUND_SetSfxVolume", {volume}); }
void SOUND_SetSpeechVolume(uint32_t volume) { ev("SOUND_SetSpeechVolume", {volume}); }
void SOUND_StopAllSfx() { ev("SOUND_StopAllSfx"); }
void SOUND_FadeOutAllSfx() { ev("SOUND_FadeOutAllSfx"); }
void SOUND_Tick() { ev("SOUND_Tick"); }
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {entity, bank, event, a, b, c});
    return 0;
}
void SOUND_SetEarPosition(float x, float y, float z, float fx, float fy, float fz, float ux, float uy, float uz) {
    ev("SOUND_SetEarPosition",
       {fbits(x), fbits(y), fbits(z), fbits(fx), fbits(fy), fbits(fz), fbits(ux), fbits(uy), fbits(uz)});
}
void SOUND_SetOcclusionProcessing(int32_t on) { ev("SOUND_SetOcclusionProcessing", {on}); }
void SND_DuckVolume(int32_t duck) { ev("SND_DuckVolume", {duck}); }
void SND_StartParkMusic() { ev("SND_StartParkMusic"); }
void SND_StopParkMusic() { ev("SND_StopParkMusic"); }
void SND_SetParkMusicLevel(int32_t level) { ev("SND_SetParkMusicLevel", {level}); }
void SND_SetChatterLevel(int32_t level) { ev("SND_SetChatterLevel", {level}); }
void SND_SetRainLevel(int32_t level) { ev("SND_SetRainLevel", {level}); }
void SND_RegisterGlobalSounds() { ev("SND_RegisterGlobalSounds"); }
void SND_RegisterLocalParkSounds(const TbDynamicStringTemplate<char>& theme) {
    ev("SND_RegisterLocalParkSounds", {}, {theme.mData});
}
void SND_FreeGlobalSounds() { ev("SND_FreeGlobalSounds"); }
void SND_FreeLocalParkSounds() { ev("SND_FreeLocalParkSounds"); }

void SCR_CloseScriptSystem() { ev("SCR_CloseScriptSystem"); }
void SCR_ScriptTick() { ev("SCR_ScriptTick"); }
void SCR_BuildScriptSpriteList() { ev("SCR_BuildScriptSpriteList"); }
void DC_TickDynamicCellMeshes() { ev("DC_TickDynamicCellMeshes"); }
void STATIC_WhiteOut() { ev("STATIC_WhiteOut"); }
int32_t CParkAnalyser::HowBusyIsPark() const {
    ev("CParkAnalyser::HowBusyIsPark", {lab(this)});
    return static_cast<int32_t>(ret("CParkAnalyser::HowBusyIsPark"));
}
int32_t CParkAnalyser::CountNeighboursOfType(CThing::ThingModel model, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id,
                                             int32_t a, int32_t b) {
    ev("CParkAnalyser::CountNeighboursOfType", {lab(this), model, id.mIndex, a, b});
    return static_cast<int32_t>(ret("CParkAnalyser::CountNeighboursOfType"));
}

CParkAnalyser::CParkAnalyser() {}   // declared for CWorld::SetupNewLevel (unit w117), which this module does not link
namespace {
CParkAnalyser gFakeAnalyser;
}
extern "C" CParkAnalyser* tpw_gs_analyser() { return &gFakeAnalyser; }
void CWorld::DoGameTurn() { ev("CWorld::DoGameTurn", {lab(this)}); }
void CWorld::DoOnlineGameTurn() { ev("CWorld::DoOnlineGameTurn", {lab(this)}); }
void CWorld::UpdateThingsInEngine(float step) { ev("CWorld::UpdateThingsInEngine", {lab(this), fbits(step)}); }
void CWorld::UpdateBumperCarsInEngine(float step) {
    ev("CWorld::UpdateBumperCarsInEngine", {lab(this), fbits(step)});
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("CWorld::GetParkAnalyser", {lab(this)});
    return &gFakeAnalyser;
}

void AnimateRides(uint32_t mode) { ev("AnimateRides", {mode}); }
void EngineTick1() { ev("EngineTick1"); }
void EngineTick2() { ev("EngineTick2"); }
void EngineTick3() { ev("EngineTick3"); }
int32_t LevelInit1() {
    ev("LevelInit1");
    return static_cast<int32_t>(ret("LevelInit1", 1));
}
int32_t LevelInit2() {
    ev("LevelInit2");
    return static_cast<int32_t>(ret("LevelInit2", 1));
}
void LevelDeInit(bool restart) { ev("LevelDeInit", {restart ? 1 : 0}); }
void TickLevelLights(int32_t force) { ev("TickLevelLights", {force}); }
void CreateScratchGeomBuffer(int32_t size) { ev("CreateScratchGeomBuffer", {size}); }
void DestroyScratchGeomBuffer() { ev("DestroyScratchGeomBuffer"); }
void ResetCursorOverSprites(s3DPoint* position) { ev("ResetCursorOverSprites", {lab(position)}); }
void CalculatePointerWorldPosn(s3DPoint* position, float distance, s3DPoint* pickup, int32_t* result) {
    ev("CalculatePointerWorldPosn", {lab(position), fbits(distance), lab(pickup), lab(result)});
}
void GetMapCellFromPointerWorldPosn(s3DPoint* position) { ev("GetMapCellFromPointerWorldPosn", {lab(position)}); }

void HandleCamera() { ev("HandleCamera"); }
void CoasterTickPreRender() { ev("CoasterTickPreRender"); }
void CoasterTickPostRender() { ev("CoasterTickPostRender"); }
void CoasterUserInputTick() { ev("CoasterUserInputTick"); }

void ESP_InitialiseAndLoadSprites(bool a, bool lobby) { ev("ESP_InitialiseAndLoadSprites", {a ? 1 : 0, lobby ? 1 : 0}); }
void ESP_RemoveSprites() { ev("ESP_RemoveSprites"); }
void FLY_Init() { ev("FLY_Init"); }
void FLY_SetFlyZone() { ev("FLY_SetFlyZone"); }
void FLY_Tick() { ev("FLY_Tick"); }
void FLY_Draw(float step) { ev("FLY_Draw", {fbits(step)}); }
void FLY_Close() { ev("FLY_Close"); }

void CIntrface::DrawFPSCounter(float fps) { ev("CIntrface::DrawFPSCounter", {lab(this), fbits(fps)}); }
void CIntrface::Process() { ev("CIntrface::Process", {lab(this)}); }
void CIntrface::DoPointedThingComputations() { ev("CIntrface::DoPointedThingComputations", {lab(this)}); }

void CMouse::AssignMouseMode(CMouseMode* mode) { ev("CMouse::AssignMouseMode", {lab(this), lab(mode)}); }

CLBLobby::CLBLobby() { ev("CLBLobby", {newlab(this)}); }
CLBLobby::~CLBLobby() { ev("~CLBLobby", {lab(this)}); }
int32_t CLBLobby::Init() {
    ev("CLBLobby::Init", {lab(this)});
    return mState;
}
int32_t CLBLobby::Tick() {
    ev("CLBLobby::Tick", {lab(this)});
    mState = static_cast<int32_t>(ret("CLBLobby::Tick.state", mState));
    return mState;
}
int32_t CLBLobby::Render() {
    ev("CLBLobby::Render", {lab(this)});
    return mState;
}
void CLBLobby::Register(CLBLobbySystem*) {}
void CLBLobby::UnRegister(CLBLobbySystem*) {}
int32_t CLBLobby::Close() {
    ev("CLBLobby::Close", {lab(this)});
    return static_cast<int32_t>(ret("CLBLobby::Close"));
}

COnlineControl::COnlineControl() {
    ev("COnlineControl", {newlab(this)});
    mClientReady = 0;
}
COnlineControl::~COnlineControl() { ev("~COnlineControl", {lab(this)}); }
void COnlineControl::InitClient() { ev("COnlineControl::InitClient", {lab(this)}); }
void COnlineControl::InitGameWorld() { ev("COnlineControl::InitGameWorld", {lab(this)}); }
void COnlineControl::Shutdown() { ev("COnlineControl::Shutdown", {lab(this)}); }
bool COnlineControl::WantToShutDown() {
    ev("COnlineControl::WantToShutDown", {lab(this)});
    return (ret("COnlineControl::WantToShutDown") & 0xff) != 0;
}
void COnlineControl::Update() { ev("COnlineControl::Update", {lab(this)}); }
void COnlineControl::ProcessQueue() {
    ev("COnlineControl::ProcessQueue", {lab(this)});
    mClientReady = static_cast<uint8_t>(ret("COnlineControl::ProcessQueue.ready", mClientReady));
}

namespace {
// The theme list has a constructor now (src/FS_OnlineList.cpp, not linked here): this one is only ever asked for its name function (a
// fake), so its storage is left raw.
alignas(16) unsigned char gFakeThemeListStore[sizeof(CFSOnlineSystemThemeList)];
CFSOnlineSystemThemeList* FakeThemeList() { return reinterpret_cast<CFSOnlineSystemThemeList*>(gFakeThemeListStore); }
CFSFTPParkInfo gFakeParkInfo;
}
extern "C" CFSOnlineSystemThemeList* tpw_gs_themelist() { return FakeThemeList(); }
extern "C" CFSFTPParkInfo* tpw_gs_parkinfo() { return &gFakeParkInfo; }
CFSOnlineSystem::CFSOnlineSystem() {
    ev("CFSOnlineSystem", {newlab(this)});
    mThemeList = FakeThemeList();
    mFTPParkPending = static_cast<int32_t>(ret("CFSOnlineSystem.pending"));
}
CFSOnlineSystem::~CFSOnlineSystem() { ev("~CFSOnlineSystem", {lab(this)}); }
CFSFTPParkInfo* CFSOnlineSystem::GetFTPDownloadParkInfo() {
    ev("CFSOnlineSystem::GetFTPDownloadParkInfo", {lab(this)});
    return &gFakeParkInfo;
}
char* CFSOnlineSystem::ReturnFTPDownloadLocalPath() {
    ev("CFSOnlineSystem::ReturnFTPDownloadLocalPath", {lab(this)});
    return tpw_gs_texts[kTextFTPPath];
}
char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    static char sName[32];
    ev("CFSOnlineSystemThemeList::ReturnOnlineThemeName", {lab(this), theme});
    std::snprintf(sName, sizeof(sName), "Theme%d", theme);
    return sName;
}
CGameType::CGameType() : mType(kGameTypeNormal) { ev("CGameType", {lab(this)}); }
void CGameType::SetGameType(GameType type) {
    ev("CGameType::SetGameType", {lab(this), type});
    mType = type;
}

namespace {
TbDynamicStringTemplate<char16_t>* gFakeSlotName;
}
CFSUserInfo::CFSUserInfo() {
    ev("CFSUserInfo", {newlab(this)});
    mGameUser = static_cast<int32_t>(ret("CFSUserInfo.user", -1));
}
CFSUserInfo::~CFSUserInfo() { ev("~CFSUserInfo", {lab(this)}); }
// CFSUserInfo's e-mail members (unrecorded).
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
const TbStringBase<char16_t>* CFSUserInfo::GetGameLoginSlotUserName(EFSUserInfoGameUser slot) {
    ev("CFSUserInfo::GetGameLoginSlotUserName", {lab(this), slot});
    if (gFakeSlotName == nullptr) gFakeSlotName = new TbDynamicStringTemplate<char16_t>(u"Somebody");
    return ret("CFSUserInfo::GetGameLoginSlotUserName") != 0 ? gFakeSlotName : nullptr;
}
void CFSUserInfo::CreateNewGameLoginUserInSlot(EFSUserInfoGameUser slot, const TbDynamicStringTemplate<char16_t>& name,
                                               int32_t value) {
    ev("CFSUserInfo::CreateNewGameLoginUserInSlot", {lab(this), slot, value}, {calllog::narrow(name.mData).c_str()});
}
void CFSUserInfo::GameLogin(EFSUserInfoGameUser slot) { ev("CFSUserInfo::GameLogin", {lab(this), slot}); }
void CFSUserInfo::GameLogout() { ev("CFSUserInfo::GameLogout", {lab(this)}); }

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {newlab(this)}); }
CFSGlobalMasterSave::~CFSGlobalMasterSave() { ev("~CFSGlobalMasterSave", {lab(this)}); }
bool CFSGlobalMasterSave::IsEasyModeUser() const {
    ev("CFSGlobalMasterSave::IsEasyModeUser", {lab(this)});
    return (ret("CFSGlobalMasterSave::IsEasyModeUser") & 0xff) != 0;
}
bool CFSGlobalMasterSave::IsFirstTimePlayer() {
    ev("CFSGlobalMasterSave::IsFirstTimePlayer", {lab(this)});
    return (ret("CFSGlobalMasterSave::IsFirstTimePlayer") & 0xff) != 0;
}
void CFSGlobalMasterSave::ClearFirstTimePlayer() { ev("CFSGlobalMasterSave::ClearFirstTimePlayer", {lab(this)}); }

void CFSLoadSave::LoadHidden(const TbDynamicStringTemplate<char16_t>& name) {
    ev("CFSLoadSave::LoadHidden", {lab(this)}, {calllog::narrow(name.mData).c_str()});
}
void CFSLoadSave::Save(const TbDynamicStringTemplate<char16_t>& name) {
    ev("CFSLoadSave::Save", {lab(this)}, {calllog::narrow(name.mData).c_str()});
}
void CFSLoadSave::LoadMostRecent() { ev("CFSLoadSave::LoadMostRecent", {lab(this)}); }
int32_t CSaveLoad::Load(const char* path, CSaveLoadHeader* header, LoadType type) {
    ev("CSaveLoad::Load", {lab(this), lab(header), type}, {text_or_null(path)});
    return 1;   // (GAMESYSTEM_Tick ignores the answer)
}
void CActionRecorder::ReplayRecording(ReplayType type) { ev("CActionRecorder::ReplayRecording", {lab(this), type}); }

CMsgEvent::CMsgEvent(EventType type) : mEvent(type) { ev("CMsgEvent", {newlab(this), type}); }
CMsgEvent::~CMsgEvent() {}
int32_t CMsgEvent::GetType() const { return 0; }
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("CMessageCentre::PostMessage", {lab(this), lab(&message)});
    return 1;
}
// The 29 subscription headers of a CMessageCentre: the first constructed is the centre's
// first member, so it labels the centre; the last destroyed records its end.
namespace {
int32_t gHeadersMade = 0;
int32_t gHeadersGone = 0;
}
extern "C" void tpw_gs_reset_headers() { gHeadersMade = gHeadersGone = 0; }
CSubscriptionHeader::CSubscriptionHeader() {
    if (gHeadersMade++ % 0x1d == 0) ev("CMessageCentre", {newlab(this)});
}
CSubscriptionHeader::~CSubscriptionHeader() {
    if (++gHeadersGone % 0x1d == 0) ev("~CMessageCentre");
}

void CInGame::Init() { ev("CInGame::Init"); }
void CreateRestartParkSaveGame() { ev("CreateRestartParkSaveGame"); }

// ------------------------------------------------------------------ test entry points

extern "C" {

void* tpw_gs_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    static const Entry kEntries[] = {
        {"gTPWFlags", &gTPWFlags}, {"gMainWindow", &gMainWindow}, {"gScreenHeight", &gScreenHeight},
        {"gGameOptions", &gGameOptions}, {"gTPDialogOptions", &gTPDialogOptions}, {"gClock", &gClock},
        {"gText", &gText}, {"gGame", &gGame}, {"gTPFileInfo", &gTPFileInfo}, {"gTPGlobals", &gTPGlobals},
        {"gExitError", &gExitError}, {"gPlayerSettings", &gPlayerSettings}, {"gOptionsBalance", &gOptionsBalance},
        {"gSoundBalance", &gSoundBalance}, {"gUserInfo", &gUserInfo}, {"gOnlineSystem", &gOnlineSystem},
        {"gOnlineControl", &gOnlineControl}, {"gMasterSave", &gMasterSave}, {"gMessageCentre", &gMessageCentre},
        {"gMouse", &gMouse}, {"gLobby", &gLobby}, {"gIntrface", &gIntrface}, {"gUIInterface", &gUIInterface},
        {"gLoadSave", &gLoadSave}, {"gSaveLoad", &gSaveLoad}, {"gActionRecorder", &gActionRecorder},
        {"gGameWorld", &gGameWorld}, {"gFirstPersonFlags", &gFirstPersonFlags}, {"gCameraMatrix", &gCameraMatrix},
        {"gCameraTarget", &gCameraTarget}, {"gPointerWorldPosn", &gPointerWorldPosn},
        {"gPickupWorldPosn", &gPickupWorldPosn}, {"gSmallTexturesOnly", &gSmallTexturesOnly},
        {"gSoundBanks", &gSoundBanks}, {"gei_Splash", &gei_Splash}, {"gei_TCache", &gei_TCache},
        {"gei_MapWho", &gei_MapWho}, {"gei_System", &gei_System}, {"gei_Display", &gei_Display},
        {"gei_ProjectionMatrix", &gei_ProjectionMatrix}, {"gei_Gamut", &gei_Gamut}, {"gei_Rain", &gei_Rain},
        {"gei_SpriteManager", &gei_SpriteManager}, {"gei_Capture", &gei_Capture},
        {"gCommandLine", &gCommandLine}, {"gCommandLineOptions", &gCommandLineOptions},
        {"gCurrentGameStatus", &gCurrentGameStatus}, {"gScreenshotNumber", &gScreenshotNumber},
        {"gFirstParkFrame", &gFirstParkFrame}, {"gRestartParkClocks", &gRestartParkClocks},
        {"gWasFirstPerson", &gWasFirstPerson}, {"gGameSys_103bc178", &gGameSys_103bc178},
        {"gTickCount", &gTickCount}, {"gTickNow", &gTickNow}, {"gLastTurnTime", &gLastTurnTime},
        {"gLastFlyTime", &gLastFlyTime}, {"gTickTime", &gTickTime}, {"gGameSystemState", &gGameSystemState},
        {"gHudState", &gHudState}, {"gScreenshotMovie", &gScreenshotMovie},
        {"gRightButtonFrames", &gRightButtonFrames}, {"gLeftButtonFrames", &gLeftButtonFrames},
        {"gGameSystemInitialised", &gGameSystemInitialised}, {"gTurnsThisFrame", &gTurnsThisFrame},
        {"gLastFrameClock", &gLastFrameClock}, {"gFrameTime", &gFrameTime}, {"gWhiteOutClock", &gWhiteOutClock},
        {"gWhiteOutTime", &gWhiteOutTime}, {"gGameType", &TheGameType()},
    };
    for (const Entry& entry : kEntries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// Objects the cases point the unit's pointer globals at (never freed by the unit unless
// a case deletes them, which calllog's operator delete then records).
void* tpw_gs_new(const char* kind) {
    if (std::strcmp(kind, "CLBLobby") == 0) return std::calloc(1, sizeof(CLBLobby));
    if (std::strcmp(kind, "CFSUserInfo") == 0) return std::calloc(1, sizeof(CFSUserInfo));
    if (std::strcmp(kind, "CFSOnlineSystem") == 0) return std::calloc(1, sizeof(CFSOnlineSystem));
    if (std::strcmp(kind, "COnlineControl") == 0) return std::calloc(1, sizeof(COnlineControl));
    if (std::strcmp(kind, "CFSGlobalMasterSave") == 0) return std::calloc(1, sizeof(CFSGlobalMasterSave));
    if (std::strcmp(kind, "CMessageCentre") == 0) return std::calloc(1, sizeof(CMessageCentre));
    if (std::strcmp(kind, "CMouse") == 0) return std::calloc(1, sizeof(CMouse));
    if (std::strcmp(kind, "CWorld") == 0) {
        static CWorld* sWorld = static_cast<CWorld*>(std::calloc(1, sizeof(CWorld)));
        return sWorld;
    }
    return std::calloc(1, 64);   // engine objects reached through pointers (texture cache, map...)
}

void tpw_gs_set_text(int32_t which, const char* text) {
    std::strncpy(tpw_gs_texts[which], text, 159);
    tpw_gs_texts[which][159] = 0;
}

void tpw_gs_set_args(int32_t argc, const char* const* argv) {
    tpw_gs_argc = argc;
    for (int32_t i = 0; i < 8; ++i) {
        std::strncpy(tpw_gs_argv_text[i], i < argc ? argv[i] : "", 95);
        tpw_gs_argv_text[i][95] = 0;
    }
}

void tpw_gs_set_strings() {
    gTPGlobals.mLevelsFolder = TbDynamicStringTemplate<char>("levels");
    gTPGlobals.mSaveExtension = TbDynamicStringTemplate<char>(".TPWS");
    gTPGlobals.mAutosaveName = TbDynamicStringTemplate<char16_t>(u"autosave");
    gTPGlobals.mRestartName = TbDynamicStringTemplate<char16_t>(u"restart");
    gTPGlobals.mDataFolder = TbDynamicStringTemplate<char>("data");
}

void tpw_gs_set_values(const int32_t* v) {
    // the values a case gives the fields the unit reads (order: test_GameSys.py VALUES)
    gPlayerSettings.mCurrent.mDetail = CPlayerSettings::eDETAIL(v[0]);
    gPlayerSettings.mCurrent.mSoundQuality = v[1];
    gPlayerSettings.mCustomOptions = v[2];
    gOptionsBalance.mValues[1].mInteger = v[3];
    gOptionsBalance.mValues[22].mInteger = v[4];
    gSoundBalance.mValues[12].mInteger = v[5];
    gSoundBalance.mValues[13].mInteger = v[6];
    gSoundBalance.mValues[14].mInteger = v[7];
    gSoundBanks[3] = v[8];
    gIntrface.mF24 = static_cast<uint16_t>(v[9]);
    gIntrface.mShowFPS = v[10];
    gGame.mF3C = v[11];
    gExitError.mError = v[12];
    gCommandLineOptions.mLoadFile = v[13];
    gCommandLineOptions.mF404 = v[14];
    gCommandLineOptions.mFlMouse = v[15];
    gCommandLineOptions.mBwCursor = v[16];
    std::memset(gCommandLineOptions.mFile, 0, sizeof(gCommandLineOptions.mFile));
    TheGameType().mType = static_cast<GameType>(v[17]);
    gei_System.mFlags = static_cast<uint32_t>(v[18]);
    gei_Display.mFlags = static_cast<uint32_t>(v[19]);
    if (gGameWorld != nullptr) gGameWorld->mF1DA738 = v[20];
    gFakeParkInfo.mTheme = v[21];
}

void tpw_gs_get_values(int32_t* v) {
    v[0] = gPlayerSettings.mCurrent.mDetail;
    v[1] = gPlayerSettings.mCurrent.mSoundQuality;
    v[2] = gPlayerSettings.mCustomOptions;
    v[3] = gOptionsBalance.mValues[1].mInteger;
    v[4] = gOptionsBalance.mValues[22].mInteger;
    v[5] = gSoundBalance.mValues[12].mInteger;
    v[6] = gSoundBalance.mValues[13].mInteger;
    v[7] = gSoundBalance.mValues[14].mInteger;
    v[8] = gSoundBanks[3];
    v[9] = gIntrface.mF24;
    v[10] = gIntrface.mShowFPS;
    v[11] = gGame.mF3C;
    v[12] = gExitError.mError;
    v[13] = gCommandLineOptions.mLoadFile;
    v[14] = gCommandLineOptions.mF404;
    v[15] = gCommandLineOptions.mFlMouse;
    v[16] = gCommandLineOptions.mBwCursor;
    v[17] = TheGameType().mType;
    v[18] = static_cast<int32_t>(gei_System.mFlags);
    v[19] = static_cast<int32_t>(gei_Display.mFlags);
    v[20] = gGameWorld != nullptr ? gGameWorld->mF1DA738 : 0;
    v[21] = gFakeParkInfo.mTheme;
}

// Fields of the objects the pointer globals point at.
void tpw_gs_set_object_fields(int32_t lobbyState, int32_t userInfoUser, int32_t onlinePending, int32_t clientReady) {
    if (gLobby != nullptr) gLobby->mState = lobbyState;
    if (gUserInfo != nullptr) gUserInfo->mGameUser = userInfoUser;
    if (gOnlineSystem != nullptr) {
        gOnlineSystem->mFTPParkPending = onlinePending;
        gOnlineSystem->mThemeList = FakeThemeList();
    }
    if (gOnlineControl != nullptr) gOnlineControl->mClientReady = static_cast<uint8_t>(clientReady);
}

void tpw_gs_get_object_fields(int32_t* out) {
    out[0] = gLobby != nullptr ? gLobby->mState : 0;
    out[1] = gUserInfo != nullptr ? gUserInfo->mGameUser : 0;
    out[2] = gOnlineSystem != nullptr ? gOnlineSystem->mFTPParkPending : 0;
    out[3] = gOnlineControl != nullptr ? gOnlineControl->mClientReady : 0;
}

void tpw_gs_GAMESYSTEM_Init() { GAMESYSTEM_Init(); }
uint32_t tpw_gs_GAMESYSTEM_Tick() { return GAMESYSTEM_Tick(); }
void tpw_gs_GAMESYSTEM_Close() { GAMESYSTEM_Close(); }
uint8_t tpw_gs_UseSmallTextures() { return UseSmallTextures() ? 1 : 0; }
void tpw_gs_PlayMeTheLogoAnim() { PlayMeTheLogoAnim(); }
void tpw_gs_PlayMeARandomFMV() { PlayMeARandomFMV(); }
void tpw_gs_PostCullOperations() { PostCullOperations(); }
void tpw_gs_GAMESYSTEM_Screenshot() { GAMESYSTEM_Screenshot(); }
void tpw_gs_GAMESYSTEM_ScreenshotMovie(int32_t on) { GAMESYSTEM_ScreenshotMovie(on); }
void tpw_gs_GAMESYSTEM_SetHudState(int32_t state) { GAMESYSTEM_SetHudState(state); }
void tpw_gs_GS_SetCurrentGameStatus(int32_t status) { GS_SetCurrentGameStatus(static_cast<GS_GAME_STATES>(status)); }
int32_t tpw_gs_GS_GetCurrentGameStatus() { return GS_GetCurrentGameStatus(); }
void tpw_gs_init_time() { init_time(); }
void tpw_gs_InitialiseWeather() { InitialiseWeather(); }
void tpw_gs_InitialiseBlobShadow() { InitialiseBlobShadow(); }
void tpw_gs_InitialisePersistentSystems() { InitialisePersistentSystems(); }
void tpw_gs_DeInitialisePersistentSystems() { DeInitialisePersistentSystems(); }
int32_t tpw_gs_GameSys_RestartLevelReInit() { return GameSys_RestartLevelReInit(); }
int32_t tpw_gs_GameSys_RestartLevelDeInit() { return GameSys_RestartLevelDeInit(); }
const char* tpw_gs_get_theme_path() { return get_theme_path(); }
int32_t tpw_gs_ControlInit() {
    static char* sArgv[9];
    for (int32_t i = 0; i < 8; ++i) sArgv[i] = tpw_gs_argv_text[i];
    return ControlInit(tpw_gs_argc, sArgv);
}
void tpw_gs_ControlIterate() { ControlIterate(); }
void tpw_gs_ControlDeInit() { ControlDeInit(); }
const char* tpw_gs_command_file() { return gCommandLineOptions.mFile; }

// The texts the static initialisers built (for the sinit comparison).
void tpw_gs_sinit_texts(char* out) {
    std::string all;
    all += calllog::narrow(gGameSysUpload.mData) + "|";
    for (const auto& language : gGameSysLanguages) all += std::string(language.mData) + "|";
    all += std::string(gGameSysTitle.mData) + "|" + gGameSysLanguage.mData + "|" +
           calllog::narrow(gGameSysGmsFile.mData) + "|";
    std::strncpy(out, all.c_str(), 1023);
    out[1023] = 0;
}

}  // extern "C"

extern "C" {

// __sinit_Main_Cpp's object: constructs one into `buffer` (0x410 bytes, filled by the
// test) the way the static initialiser does.
void tpw_gs_construct_options(void* buffer) { new (buffer) CommandLineOptions; }

}  // extern "C"

#include "fake_strings.h"
#include "fake_file_slots.h"
