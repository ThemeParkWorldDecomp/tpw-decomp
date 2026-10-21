// The fakes for TPSettings.cpp's player settings (src/game/TPResource.cpp, src/game/TPSettings.cpp with
// src/game/balance/OptionsBalance.cpp and src/game/balance/SoundBalance.cpp; libtpw_unit_TPSettings.dylib; see
// calllog.h) and the test entry points. Strings are functional and unrecorded; the file
// functions work on one fake file (its bytes to read come from the test, what is written is
// recorded as hex); the serialisation instances of other files (bool and 32-bit integers)
// read and write that file too.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_LoadSave.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "InterfaceFontWrapper.h"
#include "LbFile.h"
#include "OptionsBalance.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "SoundBalance.h"
#include "TPCamera.h"
#include "TPGlobals.h"
#include "TPMovie.h"
#include "TPResource.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TPString.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/interfacelib.h"
#include "ui_ErrorDisplay.h"
#include "ui_parkStatus.h"

#include "fake_strings.h"
#include "fake_file_slots.h"

using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

// ------------------------------------------------------------------ globals of other files

// The game object here is a blank one: the unit sets the fields its code reads.
CGame::CGame() {}
CGame::~CGame() {}
CGame gGame;
// The unit starts from a blank object, not the game's default names.
TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
TPGlobals gTPGlobals;
CText gText;
CSYS_TPFileInfo gTPFileInfo;
CFSLoadSave gLoadSave;
CSaveLoad gSaveLoad;
CLBLobby* gLobby;
CFSUserInfo* gUserInfo;
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
CFSUserInfo::CFSUserInfo() { ev("CFSUserInfo", {newlab(this)}); }
CFSUserInfo::~CFSUserInfo() {}

// ------------------------------------------------------------------ the fake file

namespace {

std::string gFileData;       // what reads return
std::size_t gFilePosition;
char gFileHandle[16];        // the one file LbFile_Open hands out
std::string gDataPath;       // CSYS_TPFileInfo::GetDataPath's text
std::vector<int32_t> gOptionsFile;   // the values CBalanceTable::Load gives an options table

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    std::string out;
    const auto* p = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0; i < size && out.size() < 150; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

// Reads up to `size` bytes; the count read.
uint32_t ReadFile(void* dest, uint32_t size) {
    const std::size_t left = gFileData.size() - gFilePosition;
    const uint32_t n = static_cast<uint32_t>(size < left ? size : left);
    if (n != 0) std::memcpy(dest, gFileData.data() + gFilePosition, n);
    gFilePosition += n;
    return n;
}

}  // namespace

TbError LbFile_Open(void*& file, const char* name, uint32_t mode) {
    ev("LbFile_Open", {mode}, {name});
    const int32_t result = static_cast<int32_t>(ret("LbFile_Open"));
    file = result >= 0 ? gFileHandle : nullptr;
    return TbError(result);
}
TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {lab(file)});
    return TbError(0);
}
// A queue "LbFile_Read" (or Write) value replaces the result; "...:short" makes it transfer
// one byte less. A read that fails or comes up short leaves the buffer alone (what it would
// hold is the file's unswapped bytes, a different number on each byte order).
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    std::vector<uint8_t> bytes(length != 0 ? length : 1);
    uint32_t n = ReadFile(bytes.data(), length);
    if (ret("LbFile_Read:short") != 0 && n != 0) --n;
    *transferred = n;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (n == length && result >= 0 && length != 0) std::memcpy(buffer, bytes.data(), length);
    return TbError(result);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), length}, {Hex(buffer, length).c_str()});
    *transferred = ret("LbFile_Write:short") != 0 ? length - 1 : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

// The serialisation instances of the basic types (other files): they read and write the
// fake file, little-endian, and record their names.
namespace {
template<class Value> int32_t FakeLoad(const char* kind, Value* value, const char* name, int32_t noLog, uint32_t size) {
    ev(kind, {noLog}, {name});
    uint8_t bytes[4] = {0, 0, 0, 0};
    if (ReadFile(bytes, size) != size) return 1;
    uint32_t word = static_cast<uint32_t>(bytes[0]) | static_cast<uint32_t>(bytes[1]) << 8 |
                    static_cast<uint32_t>(bytes[2]) << 16 | static_cast<uint32_t>(bytes[3]) << 24;
    if (size == 1) *value = static_cast<Value>(word & 0xff);
    else *value = static_cast<Value>(static_cast<int32_t>(word));
    return static_cast<int32_t>(ret(kind));
}
template<class Value> int32_t FakeSave(const char* kind, const Value* value, const char* name, int32_t noLog,
                                       uint32_t size) {
    const uint32_t word = static_cast<uint32_t>(*value);
    const uint8_t bytes[4] = {static_cast<uint8_t>(word), static_cast<uint8_t>(word >> 8),
                              static_cast<uint8_t>(word >> 16), static_cast<uint8_t>(word >> 24)};
    ev(kind, {noLog}, {name, Hex(bytes, size).c_str()});
    return static_cast<int32_t>(ret(kind));
}
}  // namespace
template<> int32_t LOADVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<bool>", value, name, noLog, 1);
}
template<> int32_t SAVEVAR<bool>(void* file, bool* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<bool>", value, name, noLog, 1);
}
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return FakeLoad("LOADVAR<int32>", value, name, noLog, 4);
}
template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* value, const char* name, int32_t noLog) {
    return FakeSave("SAVEVAR<int32>", value, name, noLog, 4);
}
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CTRACE(uint32_t level, const char* format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ev("CTRACE", {level}, {text});
}

// ------------------------------------------------------------------ other callees

int16_t Gestalt(uint32_t selector, int32_t* response) {
    ev("Gestalt", {selector});
    *response = static_cast<int32_t>(ret("GestaltValue", selector, 0));
    return static_cast<int16_t>(ret("Gestalt", selector, 0));
}
void SOUND_SetMusicStatus(uint32_t on) { ev("SOUND_SetMusicStatus", {on}); }
void SOUND_SetSfxStatus(uint32_t on) { ev("SOUND_SetSfxStatus", {on}); }
void SOUND_SetSpeechStatus(uint32_t on) { ev("SOUND_SetSpeechStatus", {on}); }
void SOUND_SetMusicVolume(uint32_t volume) { ev("SOUND_SetMusicVolume", {volume}); }
void SOUND_SetSfxVolume(uint32_t volume) { ev("SOUND_SetSfxVolume", {volume}); }
void SOUND_SetSpeechVolume(uint32_t volume) { ev("SOUND_SetSpeechVolume", {volume}); }
void SOUND_StopAllSfx() { ev("SOUND_StopAllSfx"); }
void SOUND_SetSoundQuality(int32_t quality) { ev("SOUND_SetSoundQuality", {quality}); }
void SOUND_Tick() { ev("SOUND_Tick"); }
void MOVIE_SetVolume(uint32_t volume) { ev("MOVIE_SetVolume", {volume}); }
void RoundRotationToNearest90Degrees() { ev("RoundRotationToNearest90Degrees"); }
void RemoveOptionsPanel() { ev("RemoveOptionsPanel"); }
void GS_SetCurrentGameStatus(GS_GAME_STATES status) { ev("GS_SetCurrentGameStatus", {status}); }
void CFSLoadSave::SaveHidden(const TbDynamicStringTemplate<char16_t>& name) {
    ev("CFSLoadSave::SaveHidden", {lab(this)}, {calllog::narrow(name.mData).c_str()});
}
void CFSLoadSave::LoadHidden(const TbDynamicStringTemplate<char16_t>& name) {
    ev("CFSLoadSave::LoadHidden", {lab(this)}, {calllog::narrow(name.mData).c_str()});
}
void CFSLoadSave::Save(const TbDynamicStringTemplate<char16_t>& name) {
    ev("CFSLoadSave::Save", {lab(this)}, {calllog::narrow(name.mData).c_str()});
}
TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnPathToSaveDirectory() {
    ev("CFSUserInfo::ReturnPathToSaveDirectory", {lab(this)});
    return TbDynamicStringTemplate<char16_t>(u"HD:TPW:save");
}
int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    const std::string text = calllog::narrow(in.mData);
    ev("UniStringToString", {lab(&out)}, {text.c_str()});
    out = text.c_str();
    return static_cast<int32_t>(text.size() + 1);
}
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const {
    static TbDynamicStringTemplate<char> sPath;
    ev("CSYS_TPFileInfo::GetDataPath", {lab(this)});
    sPath = gDataPath.c_str();
    return sPath;
}
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("FS_AddDirectorySeparator", {}, {path.mData});
    if (path.mLength == 0 || path.mData[path.mLength - 1] != ':') path += ":";
}
void CBalanceTable::Init() { ev("CBalanceTable::Init"); }
void CBalanceTable::UnInit() { ev("CBalanceTable::UnInit"); }
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("CBalanceTable::Load", {mode}, {path});
    const int32_t result = static_cast<int32_t>(ret("CBalanceTable::Load", 1));
    if (result != 0) {
        BalanceValue* values = GetBalanceValuesStart();
        for (std::size_t k = 0; k < gOptionsFile.size(); ++k) values[k].mInteger = gOptionsFile[k];
    }
    return result;
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("TbITable::FormatString", {lab(this), size, id});
    const std::u16string text = u"Restart the game?";
    std::memcpy(out, text.c_str(), (text.size() + 1) * sizeof(char16_t));
    return 0;
}
int32_t tpw_ps_callback_id(dlgcallback callback) {
    if (callback == nullptr) return 0;
    if (callback == RestartCallback_OK) return 1;
    if (callback == RestartCallback_Cancel) return 2;
    return -1;
}
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2,
                  dlgcallback callback2, dlgbutton button3, dlgcallback callback3, bool flag) {
    ev("UI_DialogBox", {params.mRed, params.mGreen, params.mBlue, params.mValue8, button1, tpw_ps_callback_id(callback1),
                        button2, tpw_ps_callback_id(callback2), button3, tpw_ps_callback_id(callback3), flag ? 1 : 0},
       {calllog::narrow(params.mText).c_str()});
    return nullptr;
}

// ------------------------------------------------------------------ part 2: the resources' callees

// The bullfrog storages. A search fills the entry's header words every time (so the blank
// entry's undefined ones never show) and, when it finds something, the name: "<kind><label>",
// with the name pointer `label % 3` characters into the buffer. Results come from the queues
// "<call>:<storage label>" (default -1).
namespace {
void FillInfo(bfl::FileSystem::CFileInfo& info, const char* kind, int32_t label, bool found) {
    info.mF00 = 0x1000 + static_cast<uint32_t>(label);
    info.mAttributes = static_cast<uint32_t>(label) * 3 + (kind[0] == 'n' ? 1 : 0);
    for (std::size_t k = 0; k < sizeof info.mF08; ++k) info.mF08[k] = static_cast<uint8_t>(label + k);
    if (!found) return;
    std::snprintf(info.mNameBuffer, sizeof info.mNameBuffer, "%s%d", kind, label);
    info.mName = info.mNameBuffer + (label > 0 ? label % 3 : 0);
}
}  // namespace

bfl::FileSystem::CFileStorage::CFileStorage() { ev("CFileStorage", {newlab(this)}); }
bfl::FileSystem::CFileStorage::~CFileStorage() { ev("~CFileStorage", {lab(this)}); }
void bfl::FileSystem::CFileStorage::Close() {}
TbError bfl::FileSystem::CFileStorage::FindFirst(const char* pattern, CFileInfo& info) {
    ev("CFileStorage::FindFirst", {lab(this)}, {pattern});
    const auto result = static_cast<int32_t>(ret("CFileStorage::FindFirst", lab(this), -1));
    FillInfo(info, "first", lab(this), result >= 0);
    return TbError(result);
}
TbError bfl::FileSystem::CFileStorage::FindNext(CFileInfo& info) {
    ev("CFileStorage::FindNext", {lab(this)});
    const auto result = static_cast<int32_t>(ret("CFileStorage::FindNext", lab(this), -1));
    FillInfo(info, "next", lab(this), result >= 0);
    return TbError(result);
}
TbError bfl::FileSystem::CFileStorage::OpenFile(CFile& file, const char* name, uint32_t mode, void* data) {
    ev("CFileStorage::OpenFile", {lab(this), lab(&file), mode, lab(data)}, {name});
    return TbError(static_cast<int32_t>(ret("CFileStorage::OpenFile", lab(this), -1)));
}
TbError bfl::FileSystem::CFileStorage::FullPath(char*, const char*, uint32_t) { return TbError(0); }
// FullPath: "full<label>:" + name.
TbError bfl::FileSystem::CFileStorage::FullPath(TbDynamicStringTemplate<char>& path,
                                                const TbDynamicStringTemplate<char>& name) {
    ev("CFileStorage::FullPath", {lab(this), lab(&path)}, {name.mData});
    const std::string full = "full" + std::to_string(lab(this)) + ":" + name.mData;
    path = full.c_str();
    return TbError(static_cast<int32_t>(ret("CFileStorage::FullPath", 0)));
}
const char* bfl::FileSystem::CFileStorage::GetRootPath() { return ""; }
TbError bfl::FileSystem::CFileStorage::SetAttribs(const char*, uint32_t) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::CreateDir(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Delete(const char*) { return TbError(0); }
TbError bfl::FileSystem::CFileStorage::Move(const char*, const char*) { return TbError(0); }
TbError bfl::FileSystem::CDiscFileStorage::Open(const char* path) {
    ev("CDiscFileStorage::Open", {lab(this)}, {path});
    return TbError(static_cast<int32_t>(ret("CDiscFileStorage::Open", -1)));
}
TbError bfl::FileSystem::CWadFileStorage::Open(const char* path, uint32_t flags) {
    ev("CWadFileStorage::Open", {lab(this), flags}, {path});
    return TbError(static_cast<int32_t>(ret("CWadFileStorage::Open", -1)));
}
bfl::FileSystem::CFile::CFile() : mImplementation(nullptr) { ev("CFile::CFile", {newlab(this)}); }
bfl::FileSystem::CFile::~CFile() { ev("CFile::~CFile", {lab(this)}); }
void bfl::FileSystem::CFile::Close() {}
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t bfl::FileSystem::CFile::GetSize() { return 0; }

// TPString.cpp's comparison (case ignored), recorded.
bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b) {
    ev("TPStrEq", {}, {a.mData, b.mData});
    const char* x = a.mData;
    const char* y = b.mData;
    for (;; ++x, ++y) {
        const int cx = (*x >= 'A' && *x <= 'Z') ? *x + 32 : static_cast<unsigned char>(*x);
        const int cy = (*y >= 'A' && *y <= 'Z') ? *y + 32 : static_cast<unsigned char>(*y);
        if (cx != cy) return false;
        if (cx == 0) return true;
    }
}
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetRemotePath() const {
    ev("CSYS_TPFileInfo::GetRemotePath", {lab(this)});
    return mRemotePath;
}

namespace {
bool gFakeResourceOpen = false;   // the test resources' OpenFile records instead of running

// A resource the tests make: the manager's own are fakes (destructor and OpenFile record).
class TestResource : public CTPBaseResource {
public:
    TestResource(const char* name, const char* path)
        : CTPBaseResource(TbDynamicStringTemplate<char>(name), TbDynamicStringTemplate<char>(path)) {}
    ~TestResource() override {
        if (gFakeResourceOpen) ev("~CTPBaseResource", {lab(this)});
    }
    int32_t OpenFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name, uint32_t mode) override {
        if (!gFakeResourceOpen) return CTPBaseResource::OpenFile(file, name, mode);
        ev("CTPBaseResource::OpenFile", {lab(this), lab(&file), mode}, {name.mData});
        return static_cast<int32_t>(ret("CTPBaseResource::OpenFile", lab(this), 0));
    }
};

// A CTPIntermediate to destroy (clang traps an abstract class's complete destructor, so the
// abstract classes' destructors run as the base parts of these test classes).
class TestIntermediate : public CTPIntermediate {
public:
    TestIntermediate(const char* name, const char* path)
        : CTPIntermediate(TbDynamicStringTemplate<char>(name), TbDynamicStringTemplate<char>(path)) {}
};

// A list's values head to tail and tail to head (labels), and its count.
template<class List> int32_t WalkList(const List* list, int32_t* forward, int32_t* backward, int32_t max) {
    int32_t n = 0;
    for (auto* node = list->mHead; node != nullptr && n < max; node = node->mNext) forward[n++] = lab(node->mData);
    forward[n] = -999;
    n = 0;
    for (auto* node = list->mTail; node != nullptr && n < max; node = node->mPrev) backward[n++] = lab(node->mData);
    backward[n] = -999;
    return list->mCount;
}
template<class List, class Value> void AppendNode(List* list, Value value) {
    auto* node = new typename List::CNode(value);
    node->mPrev = list->mTail;
    if (list->mTail == nullptr) list->mHead = node;
    else list->mTail->mNext = node;
    list->mTail = node;
    ++list->mCount;
}

using StorageList = TbTList<bfl::FileSystem::CFileStorage*, bfl::FileSystem::CFileStorage*>;
using ResourceList = TbTList<CTPBaseResource*, CTPBaseResource*>;
}  // namespace

// ------------------------------------------------------------------ test entry points

extern "C" {

void* tpw_ps_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    static const Entry kEntries[] = {
        {"gPlayerSettings", &gPlayerSettings}, {"gGame", &gGame}, {"gTPGlobals", &gTPGlobals}, {"gText", &gText},
        {"gTPFileInfo", &gTPFileInfo}, {"gLoadSave", &gLoadSave}, {"gSaveLoad", &gSaveLoad}, {"gLobby", &gLobby},
        {"gUserInfo", &gUserInfo}, {"gSoundBalance", &gSoundBalance}, {"gOptionsBalance", &gOptionsBalance},
        {"file", gFileHandle},
    };
    for (const Entry& entry : kEntries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// A settings object's fields in the order of test_TPSettings.py FIELDS (Mac offsets 0..0x98).
#define TPW_PS_SETTINGS(X, s)                                                                            \
    X(s.mRenderer) X(s.mResolution) X(s.mDetail) X(s.mVideoCard) X(s.mSfx.mOn) X(s.mSfx.mVolume)          \
    X(s.mMusic.mOn) X(s.mMusic.mVolume) X(s.mSpeech.mOn) X(s.mSpeech.mVolume) X(s.mMovie.mOn)             \
    X(s.mMovie.mVolume) X(s.mSoundQuality) X(s.mAdvisorOn) X(s.mTutorialOn) X(s.mTooltipsOn)              \
    X(s.mConfirmDeleteOn) X(s.mRMBScrollOn) X(s.mRMBCancelOn) X(s.mIsometricOn)
#define TPW_PS_FIELDS(X, p)                                                                              \
    TPW_PS_SETTINGS(X, p->mCurrent) TPW_PS_SETTINGS(X, p->mStored) X(p->mGameOptions)                     \
    X(p->mExtraEngineOptions) X(p->mDebugGameOptions) X(p->mCurrentTheme) X(p->mDebugGuardUInts)          \
    X(p->mTextureCacheSize) X(p->mCustomOptions) X(p->mGameContinueState) X(p->mGameRefreshState)
void tpw_ps_set(void* object, const int32_t* v) {
    auto* p = static_cast<CPlayerSettings*>(object);
    std::memset(static_cast<void*>(p), 0, sizeof(*p));   // padding included, as the PowerPC side's
    int32_t i = 0;
#define TPW_PS_SET(field) field = static_cast<decltype(field)>(v[i++]);
    TPW_PS_FIELDS(TPW_PS_SET, p)
#undef TPW_PS_SET
}
void tpw_ps_get(const void* object, int32_t* v) {
    const auto* p = static_cast<const CPlayerSettings*>(object);
    int32_t i = 0;
#define TPW_PS_GET(field) v[i++] = static_cast<int32_t>(field);
    TPW_PS_FIELDS(TPW_PS_GET, p)
#undef TPW_PS_GET
}
void* tpw_ps_new_settings() {
    void* block = std::calloc(1, sizeof(CPlayerSettings));
    return block;
}
// A deletable settings object (its constructor runs Init, so it is made while nothing records).
void* tpw_ps_new_object() { return new CPlayerSettings(); }

void tpw_ps_set_balance(void* table, const int32_t* v, int32_t n) {
    BalanceValue* values = static_cast<CBalanceTable*>(table)->GetBalanceValuesStart();
    for (int32_t k = 0; k < n; ++k) values[k].mInteger = v[k];
}
void tpw_ps_set_options_file(const int32_t* v, int32_t n) { gOptionsFile.assign(v, v + n); }
void tpw_ps_set_file(const char* data, int32_t size) {
    gFileData.assign(data, static_cast<std::size_t>(size));
    gFilePosition = 0;
}
void tpw_ps_set_data_path(const char* text) { gDataPath = text; }
void tpw_ps_set_wide(void* s, const char16_t* units) { *static_cast<TbDynamicStringTemplate<char16_t>*>(s) = units; }
void tpw_ps_set_string(void* s, const char* text) { *static_cast<TbDynamicStringTemplate<char>*>(s) = text; }
void* tpw_ps_new_string(const char* text) { return new TbDynamicStringTemplate<char>(text); }
void* tpw_ps_tpglobals_string(const char* name) {
    if (std::strcmp(name, "mWideSeparator") == 0) return &gTPGlobals.mWideSeparator;
    if (std::strcmp(name, "mConfigFile") == 0) return &gTPGlobals.mConfigFile;
    if (std::strcmp(name, "mRefreshName") == 0) return &gTPGlobals.mRefreshName;
    if (std::strcmp(name, "mAutosaveName") == 0) return &gTPGlobals.mAutosaveName;
    if (std::strcmp(name, "mLowOptionsFile") == 0) return &gTPGlobals.mLowOptionsFile;
    if (std::strcmp(name, "mMediumOptionsFile") == 0) return &gTPGlobals.mMediumOptionsFile;
    if (std::strcmp(name, "mHighOptionsFile") == 0) return &gTPGlobals.mHighOptionsFile;
    return nullptr;
}
// The lobby: none, or one with its state.
void* tpw_ps_lobby() {
    static CLBLobby* sLobby = static_cast<CLBLobby*>(std::calloc(1, sizeof(CLBLobby)));
    return sLobby;
}
void tpw_ps_set_text_table(void* table) { gText.mResourcePointers[6] = table; }
void* tpw_ps_text_table() {
    static char sTable[16];
    return sTable;
}
int32_t tpw_ps_restart() { return gGame.mRestart; }
void tpw_ps_set_restart(int32_t value) { gGame.mRestart = value; }
void tpw_ps_set_lobby_state(void* lobby, int32_t state) { static_cast<CLBLobby*>(lobby)->mState = state; }
int32_t tpw_ps_lobby_state(const void* lobby) { return static_cast<const CLBLobby*>(lobby)->mState; }
void* tpw_ps_new_userinfo() { return std::calloc(1, sizeof(CFSUserInfo)); }
int32_t tpw_ps_file_position() { return static_cast<int32_t>(gFilePosition); }

void tpw_ps_sinit_texts(char* out) {
    for (int32_t i = 0; i < 16; ++i) std::strncpy(out + 96 * i, gTPSettingsLanguages[i].mData, 95);
    std::strncpy(out + 96 * 16, gTPSettingsTitle.mData, 95);
    std::strncpy(out + 96 * 17, gTPSettingsLanguage.mData, 95);
}

int32_t tpw_ps_call(const char* name, const int64_t* a) {
    const std::string n(name);
    auto* p = reinterpret_cast<CPlayerSettings*>(a[0]);
    auto text = [&](int k) -> TbDynamicStringTemplate<char>& { return *reinterpret_cast<TbDynamicStringTemplate<char>*>(a[k]); };
    void* file = reinterpret_cast<void*>(a[1]);
    const char* label = reinterpret_cast<const char*>(a[3]);
    const auto noLog = static_cast<int32_t>(a[4]);
    if (n == "CPlayerSettings::~CPlayerSettings") {
        if (p == nullptr) return 0;
        if (static_cast<int16_t>(a[1]) > 0) delete p;
        else p->~CPlayerSettings();
    }
    else if (n == "CPlayerSettings::Init") p->Init();
    else if (n == "CPlayerSettings::GetMusicVolume") return p->GetMusicVolume();
    else if (n == "CPlayerSettings::GetSFXVolume") return p->GetSFXVolume();
    else if (n == "CPlayerSettings::GetSpeechVolume") return p->GetSpeechVolume();
    else if (n == "CPlayerSettings::GetMovieVolume") return p->GetMovieVolume();
    else if (n == "CPlayerSettings::UpdateSettings") p->UpdateSettings();
    else if (n == "CPlayerSettings::ResetSettings") p->ResetSettings();
    else if (n == "CPlayerSettings::StoreSettings") p->StoreSettings();
    else if (n == "CPlayerSettings::SetGameContinueState")
        p->SetGameContinueState(CPlayerSettings::eGAME_CONTINUE_STATE(a[1]));
    else if (n == "CPlayerSettings::ResetGameContinueState") p->ResetGameContinueState();
    else if (n == "CPlayerSettings::SetGameRefreshState") p->SetGameRefreshState(CPlayerSettings::eGAME_REFRESH_STATE(a[1]));
    else if (n == "CPlayerSettings::ResetGameRefreshState") p->ResetGameRefreshState();
    else if (n == "CPlayerSettings::SetSoundQuality") p->SetSoundQuality(static_cast<int32_t>(a[1]));
    else if (n == "CPlayerSettings::SetRendererType") p->SetRendererType(CPlayerSettings::eRENDERER(a[1]));
    else if (n == "CPlayerSettings::SetResolution") p->SetResolution(CPlayerSettings::eRESOLUTION(a[1]));
    else if (n == "CPlayerSettings::SetVideoCard") p->SetVideoCard(CPlayerSettings::eVIDEOCARD(a[1]));
    else if (n == "CPlayerSettings::SetGraphicsDetail") p->SetGraphicsDetail(CPlayerSettings::eDETAIL(a[1]));
    else if (n == "CPlayerSettings::LoadGraphicsDetails") return p->LoadGraphicsDetails(CPlayerSettings::eDETAIL(a[1]));
    else if (n == "CPlayerSettings::HasTextureSizeChanged")
        return p->HasTextureSizeChanged(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])) ? 1 : 0;
    else if (n == "CPlayerSettings::HasNumKidsChanged")
        return p->HasNumKidsChanged(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])) ? 1 : 0;
    else if (n == "CPlayerSettings::HasMPEGRateChanged")
        return p->HasMPEGRateChanged(static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])) ? 1 : 0;
    else if (n == "CPlayerSettings::UpdateVolume") p->UpdateVolume();
    else if (n == "CPlayerSettings::SerialiseLocal")
        return p->SerialiseLocal(file, static_cast<int32_t>(a[2]), static_cast<uint32_t>(a[3]));
    else if (n == "CPlayerSettings::SerialiseGlobal")
        return p->SerialiseGlobal(file, static_cast<int32_t>(a[2]), static_cast<uint32_t>(a[3]));
    else if (n == "CPlayerSettings::SaveGlobal") return p->SaveGlobal(text(1));
    else if (n == "CPlayerSettings::LoadGlobal") return p->LoadGlobal(text(1));
    else if (n == "RestartCallback_OK") return RestartCallback_OK(reinterpret_cast<void*>(a[1]));
    else if (n == "RestartCallback_Cancel") return RestartCallback_Cancel(reinterpret_cast<void*>(a[1]));
    else if (n == "RestartDialog") RestartDialog();
    // the template instances: (file, value field, name, noLog); the value is a field of p
    else if (n == "SAVEVARSTRUCT<Volume>") return SAVEVARSTRUCT(file, &p->mCurrent.mMovie, label, noLog);
    else if (n == "LOADVARSTRUCT<Volume>") return LOADVARSTRUCT(file, &p->mCurrent.mMovie, label, noLog);
    else if (n == "LOADVAR<eRENDERER>") return LOADVAR(file, &p->mCurrent.mRenderer, label, noLog);
    else if (n == "LOADVAR<eRESOLUTION>") return LOADVAR(file, &p->mCurrent.mResolution, label, noLog);
    else if (n == "LOADVAR<eDETAIL>") return LOADVAR(file, &p->mCurrent.mDetail, label, noLog);
    else if (n == "LOADVAR<eVIDEOCARD>") return LOADVAR(file, &p->mCurrent.mVideoCard, label, noLog);
    else if (n == "SAVEVAR<eRENDERER>") return SAVEVAR(file, &p->mCurrent.mRenderer, label, noLog);
    else if (n == "SAVEVAR<eRESOLUTION>") return SAVEVAR(file, &p->mCurrent.mResolution, label, noLog);
    else if (n == "SAVEVAR<eDETAIL>") return SAVEVAR(file, &p->mCurrent.mDetail, label, noLog);
    else if (n == "SAVEVAR<eVIDEOCARD>") return SAVEVAR(file, &p->mCurrent.mVideoCard, label, noLog);
    else return -12345;
    return 0;
}

}  // extern "C"

// ------------------------------------------------------------------ part 2 entry points

extern "C" {

void* tpw_rs_tpglobals_string(const char* name) {
    struct Entry {
        const char* name;
        TbDynamicStringTemplate<char>* string;
    };
    static const Entry kEntries[] = {
        {"mSeparator", &gTPGlobals.mSeparator}, {"mSearchPath", &gTPGlobals.mSearchPath},
        {"mCurrentFolder", &gTPGlobals.mCurrentFolder}, {"mAllFiles", &gTPGlobals.mAllFiles},
        {"mSamExtension", &gTPGlobals.mSamExtension}, {"mOnlineSamExtension", &gTPGlobals.mOnlineSamExtension},
        {"mOnlinePrefix", &gTPGlobals.mOnlinePrefix}, {"mWadExtension", &gTPGlobals.mWadExtension},
        {"mWadPattern", &gTPGlobals.mWadPattern}, {"mWadFolderPattern", &gTPGlobals.mWadFolderPattern},
        {"mRemotePath", &gTPFileInfo.mRemotePath},
    };
    for (const Entry& entry : kEntries)
        if (std::strcmp(entry.name, name) == 0) return entry.string;
    return nullptr;
}
int32_t tpw_rs_text(const void* s, char* out, int32_t size) {
    const auto* string = static_cast<const TbDynamicStringTemplate<char>*>(s);
    std::snprintf(out, static_cast<std::size_t>(size), "%s", string->mData);
    return string->mLength;
}
void tpw_rs_set_fake_open(int32_t on) { gFakeResourceOpen = on != 0; }

// Resources (made while nothing records: the constructor adds a separator and a list).
void* tpw_rs_new_resource(const char* name, const char* path) {
    auto* resource = new TestResource(name, path);
    resource->mPath = path;   // as given
    return resource;
}
// fields: storage list, manager, current search storage, last found storage, search pattern
void tpw_rs_set_fields(void* object, void* const* v) {
    auto* r = static_cast<CTPBaseResource*>(object);
    r->mStorages = static_cast<TbPtrList<bfl::FileSystem::CFileStorage>*>(v[0]);
    r->mManager = static_cast<CTPResourceManager*>(v[1]);
    r->mCurrentSearchStorage = static_cast<bfl::FileSystem::CFileStorage*>(v[2]);
    r->mLastFoundFileStorage = static_cast<bfl::FileSystem::CFileStorage*>(v[3]);
    r->mSearchPattern = static_cast<TbDynamicStringTemplate<char>*>(v[4]);
}
void tpw_rs_get_fields(const void* object, void** v) {
    const auto* r = static_cast<const CTPBaseResource*>(object);
    v[0] = r->mStorages;
    v[1] = r->mManager;
    v[2] = r->mCurrentSearchStorage;
    v[3] = r->mLastFoundFileStorage;
    v[4] = r->mSearchPattern;
}
void* tpw_rs_name(void* object) { return &static_cast<CTPBaseResource*>(object)->mName; }
void* tpw_rs_path(void* object) { return &static_cast<CTPBaseResource*>(object)->mPath; }
// The members the texture resources add (0x40 and 0x48 on the Mac), for labels.
void* tpw_rs_member(void* object, int32_t offset) {
    if (offset == 0x40) return &static_cast<CTPTextureResource*>(static_cast<CTPBaseResource*>(object))->mF40;
    auto* base = static_cast<CTPBaseResource*>(object);
    if (auto* advisor = dynamic_cast<CTPAdvisorResource*>(base)) return &advisor->mF48;
    if (auto* ui = dynamic_cast<CTPUIResource*>(base)) return &ui->mF48;
    return nullptr;
}
// The class of a resource the constructors made, by name.
int32_t tpw_rs_class(const void* object, char* out, int32_t size) {
    const auto* r = static_cast<const CTPBaseResource*>(object);
    const char* name = "?";
    if (dynamic_cast<const CTPAdvisorResource*>(r)) name = "CTPAdvisorResource";
    else if (dynamic_cast<const CTPUIResource*>(r)) name = "CTPUIResource";
    else if (dynamic_cast<const CTPTerrainResource*>(r)) name = "CTPTerrainResource";
    else if (dynamic_cast<const CTPTextureResource*>(r)) name = "CTPTextureResource";
    else if (dynamic_cast<const CTPSharedTextureResource*>(r)) name = "CTPSharedTextureResource";
    else if (dynamic_cast<const CTPSoundResource*>(r)) name = "CTPSoundResource";
    else if (dynamic_cast<const CTPFontResource*>(r)) name = "CTPFontResource";
    else if (dynamic_cast<const CTPSpriteResource*>(r)) name = "CTPSpriteResource";
    else if (dynamic_cast<const CTPLanguageResource*>(r)) name = "CTPLanguageResource";
    else if (dynamic_cast<const CTPFMVResource*>(r)) name = "CTPFMVResource";
    std::snprintf(out, static_cast<std::size_t>(size), "%s", name);
    return 0;
}

void* tpw_rs_alloc(int32_t size) { return std::calloc(1, static_cast<std::size_t>(size)); }

// The resource classes by name: constructing in place, constructing new, destroying.
#define TPW_RS_CLASSES(X)                                                                            \
    X(CTPSharedTextureResource) X(CTPTextureResource) X(CTPSoundResource) X(CTPAdvisorResource)       \
    X(CTPFontResource) X(CTPSpriteResource) X(CTPUIResource) X(CTPTerrainResource)                    \
    X(CTPLanguageResource) X(CTPFMVResource)
void* tpw_rs_construct(const char* cls, const char* name, const char* path) {
    const std::string c(cls);
    const TbDynamicStringTemplate<char> n(name);
    const TbDynamicStringTemplate<char> p(path);
#define TPW_RS_NEW(T) \
    if (c == #T) return static_cast<CTPBaseResource*>(new T(n, p));
    TPW_RS_CLASSES(TPW_RS_NEW)
#undef TPW_RS_NEW
    if (c == "TestIntermediate") return static_cast<CTPBaseResource*>(new TestIntermediate(name, path));
    return nullptr;
}
void* tpw_rs_new_storage(int32_t wad) {
    if (wad != 0) return new bfl::FileSystem::CWadFileStorage;
    return new bfl::FileSystem::CDiscFileStorage;
}
void* tpw_rs_new_storage_list() { return new TbPtrList<bfl::FileSystem::CFileStorage>; }
void tpw_rs_storage_list_append(void* list, void* storage) {
    AppendNode(static_cast<StorageList*>(list), static_cast<bfl::FileSystem::CFileStorage*>(storage));
}
int32_t tpw_rs_storage_list_walk(const void* list, int32_t* forward, int32_t* backward, int32_t max) {
    return WalkList(static_cast<const StorageList*>(list), forward, backward, max);
}
void* tpw_rs_new_resource_list() { return new TbPtrList<CTPBaseResource>; }
void tpw_rs_resource_list_append(void* list, void* resource) {
    AppendNode(static_cast<ResourceList*>(list), static_cast<CTPBaseResource*>(resource));
}
int32_t tpw_rs_resource_list_walk(const void* list, int32_t* forward, int32_t* backward, int32_t max) {
    return WalkList(static_cast<const ResourceList*>(list), forward, backward, max);
}
// A node of a list by index (the PowerPC side's position).
void* tpw_rs_list_node(const void* list, int32_t index) {
    auto* node = static_cast<const StorageList*>(list)->mHead;
    for (int32_t k = 0; k < index && node != nullptr; ++k) node = node->mNext;
    return node;
}
int32_t tpw_rs_list_node_index(const void* list, const void* position) {
    int32_t k = 0;
    for (auto* node = static_cast<const StorageList*>(list)->mHead; node != nullptr; node = node->mNext, ++k)
        if (node == position) return k;
    return position == nullptr ? -1 : -2;
}

void* tpw_rs_new_manager() { return new CTPResourceManager; }
void tpw_rs_set_manager(void* object, void* resources, void* current, void* shared) {
    auto* m = static_cast<CTPResourceManager*>(object);
    m->mResources = static_cast<TbPtrList<CTPBaseResource>*>(resources);
    m->mCurrentResource = static_cast<CTPBaseResource*>(current);
    m->mSharedTextures = static_cast<CTPSharedTextureResource*>(shared);
}
void tpw_rs_get_manager(const void* object, void** v) {
    const auto* m = static_cast<const CTPResourceManager*>(object);
    v[0] = m->mResources;
    v[1] = m->mCurrentResource;
    v[2] = m->mSharedTextures;
}

void* tpw_rs_new_file() { return new bfl::FileSystem::CFile; }
void* tpw_rs_new_info() { return new bfl::FileSystem::CFileInfo; }
void tpw_rs_set_info(void* object, uint32_t f00, uint32_t attributes, const uint8_t* f08, const char* name,
                     int32_t offset) {
    auto* info = static_cast<bfl::FileSystem::CFileInfo*>(object);
    info->mF00 = f00;
    info->mAttributes = attributes;
    std::memcpy(info->mF08, f08, sizeof info->mF08);
    std::memset(info->mNameBuffer, 0, sizeof info->mNameBuffer);
    std::snprintf(info->mNameBuffer + offset, sizeof info->mNameBuffer - static_cast<std::size_t>(offset), "%s", name);
    info->mName = info->mNameBuffer + offset;
}
// words: mF00, mAttributes, the name's offset; f08: the 0x18 bytes; name: from mName.
void tpw_rs_get_info(const void* object, int32_t* words, uint8_t* f08, char* name, int32_t size) {
    const auto* info = static_cast<const bfl::FileSystem::CFileInfo*>(object);
    words[0] = static_cast<int32_t>(info->mF00);
    words[1] = static_cast<int32_t>(info->mAttributes);
    words[2] = static_cast<int32_t>(info->mName - info->mNameBuffer);
    std::memcpy(f08, info->mF08, sizeof info->mF08);
    std::snprintf(name, static_cast<std::size_t>(size), "%s", info->mName);
}

// Runs one function. a: the arguments as the PowerPC side passes them (`this` first).
int64_t tpw_rs_call(const char* name, const int64_t* a) {
    const std::string n(name);
    auto ptr = [&](int k) { return reinterpret_cast<void*>(a[k]); };
    auto text = [&](int k) -> TbDynamicStringTemplate<char>& { return *static_cast<TbDynamicStringTemplate<char>*>(ptr(k)); };
    auto* r = static_cast<CTPBaseResource*>(ptr(0));
    auto* m = static_cast<CTPResourceManager*>(ptr(0));
    auto& file = *static_cast<bfl::FileSystem::CFile*>(ptr(1));
    auto& info = *static_cast<bfl::FileSystem::CFileInfo*>(ptr(2));
    const auto flag = static_cast<int16_t>(a[1]);
    auto destroy = [&](auto* object, auto destructor) {
        if (object == nullptr) return;
        destructor(object);
        if (flag > 0) ::operator delete(static_cast<void*>(object));
    };
#define TPW_RS_CTOR(T) \
    if (n == "ctor:" #T) { new (ptr(0)) T(text(1), text(2)); return 0; }
#define TPW_RS_DTOR(T) \
    if (n == "dtor:" #T) { destroy(static_cast<T*>(r), [](T* o) { o->T::~T(); }); return 0; }
    TPW_RS_CLASSES(TPW_RS_CTOR)
    TPW_RS_CLASSES(TPW_RS_DTOR)
#undef TPW_RS_CTOR
#undef TPW_RS_DTOR
    if (n == "CTPIntermediate::~CTPIntermediate")
        destroy(static_cast<TestIntermediate*>(r), [](TestIntermediate* o) { o->TestIntermediate::~TestIntermediate(); });
    else if (n == "CWadFileStorage::~CWadFileStorage")
        destroy(static_cast<bfl::FileSystem::CWadFileStorage*>(ptr(0)),
                [](bfl::FileSystem::CWadFileStorage* o) { o->bfl::FileSystem::CWadFileStorage::~CWadFileStorage(); });
    else if (n == "CTPBaseResource::~CTPBaseResource")
        destroy(static_cast<TestResource*>(r), [](TestResource* o) { o->TestResource::~TestResource(); });
    else if (n == "CTPBaseResource::AddDiscStorage") r->AddDiscStorage(text(1));
    else if (n == "CTPBaseResource::AddWADStorage") r->AddWADStorage(text(1));
    else if (n == "CTPBaseResource::AddAllStorages") r->AddAllStorages(text(1));
    else if (n == "CTPBaseResource::AddLocalStorage") r->AddLocalStorage(text(1));
    else if (n == "CTPBaseResource::AddRemoteStorage") r->AddRemoteStorage(text(1));
    else if (n == "CTPBaseResource::pGetCurrentSearchStorage") return lab(r->pGetCurrentSearchStorage());
    else if (n == "CTPBaseResource::pGetLastFoundFileStorage") return lab(r->pGetLastFoundFileStorage());
    else if (n == "strRemoveRootDirectory") return lab(&strRemoveRootDirectory(text(0)));
    else if (n == "QualifyFilename") return QualifyFilename(text(0), text(1), text(2));
    else if (n == "CTPBaseResource::OpenFile(storage)")
        return r->CTPBaseResource::OpenFile(file, text(2), *static_cast<bfl::FileSystem::CFileStorage*>(ptr(3)),
                                            static_cast<uint32_t>(a[4]));
    else if (n == "CTPBaseResource::OpenFile") return r->CTPBaseResource::OpenFile(file, text(2), static_cast<uint32_t>(a[3]));
    else if (n == "CTPBaseResource::FindFirst")
        return r->FindFirst(text(1), *static_cast<bfl::FileSystem::CFileInfo*>(ptr(2)));
    else if (n == "CTPBaseResource::FindNext") return r->FindNext(*static_cast<bfl::FileSystem::CFileInfo*>(ptr(1)));
    else if (n == "CTPIntermediate::GetSamFile")
        return static_cast<CTPIntermediate*>(r)->GetSamFile(*static_cast<bfl::FileSystem::CFileStorage*>(ptr(1)),
                                                            *static_cast<bfl::FileSystem::CFile*>(ptr(2)));
    else if (n == "CTPIntermediate::GetOnlineSamFile")
        return static_cast<CTPIntermediate*>(r)->GetOnlineSamFile(*static_cast<bfl::FileSystem::CFileStorage*>(ptr(1)),
                                                                  *static_cast<bfl::FileSystem::CFile*>(ptr(2)));
    else if (n == "MakeFullFilename") MakeFullFilename(text(0), text(1));
    else if (n == "CTPBaseResource::OpenQualifiedFile(storage)")
        return r->OpenQualifiedFile(file, text(2), *static_cast<bfl::FileSystem::CFileStorage*>(ptr(3)));
    else if (n == "CTPBaseResource::OpenQualifiedFile") return r->OpenQualifiedFile(file, text(2));
    else if (n == "CTPSoundResource::FindSoundFile")
        return static_cast<CTPSoundResource*>(r)->FindSoundFile(text(1), text(2));
    else if (n == "CTPResourceManager::CTPResourceManager") new (m) CTPResourceManager;
    else if (n == "CTPResourceManager::~CTPResourceManager")
        destroy(m, [](CTPResourceManager* o) { o->~CTPResourceManager(); });
    else if (n == "CTPResourceManager::RegisterResource") return m->RegisterResource(static_cast<CTPBaseResource*>(ptr(1)));
    else if (n == "CTPResourceManager::RegisterResource(shared)")
        return m->RegisterResource(static_cast<CTPSharedTextureResource*>(static_cast<CTPBaseResource*>(ptr(1))));
    else if (n == "CTPResourceManager::UnregisterResource")
        return m->UnregisterResource(static_cast<CTPBaseResource*>(ptr(1)));
    else if (n == "CTPResourceManager::OpenFile") return m->OpenFile(file, text(2));
    else if (n == "CTPResourceManager::FindResource")
        return m->FindResource(text(1), static_cast<CTPBaseResource**>(ptr(2)));
    else if (n == "CTPResourceManager::pGetCurrentResource") return lab(m->pGetCurrentResource());
    else if (n == "CTPResourceManager::pSetCurrentResource") m->pSetCurrentResource(static_cast<CTPBaseResource*>(ptr(1)));
    else return -12345;
    (void)info;
    return 0;
}

// The list instances. a[0]: the list; returns values as labels, positions as node indices.
int64_t tpw_rs_list_call(const char* name, const int64_t* a) {
    const std::string n(name);
    const bool storages = n.find("CFileStorage") != std::string::npos;
    auto* sl = reinterpret_cast<StorageList*>(a[0]);
    auto* rl = reinterpret_cast<ResourceList*>(a[0]);
    const auto flag = static_cast<int16_t>(a[1]);
    const std::string op = n.substr(n.rfind("::") + 2);
    if (op == "TbTList") {
        if (storages) new (sl) StorageList;
        else new (rl) ResourceList;
    } else if (op == "~TbTList") {
        if (sl == nullptr) return 0;
        if (storages) sl->StorageList::~TbTList();
        else rl->ResourceList::~TbTList();
        if (flag > 0) ::operator delete(static_cast<void*>(sl));
    } else if (op == "~TbPtrList") {
        if (sl == nullptr) return 0;
        using StoragePtrList = TbPtrList<bfl::FileSystem::CFileStorage>;
        using ResourcePtrList = TbPtrList<CTPBaseResource>;
        if (storages) static_cast<StoragePtrList*>(sl)->StoragePtrList::~TbPtrList();
        else static_cast<ResourcePtrList*>(rl)->ResourcePtrList::~TbPtrList();
        if (flag > 0) ::operator delete(static_cast<void*>(sl));
    } else if (op == "Find") {
        POSITION position = storages ? sl->Find(reinterpret_cast<bfl::FileSystem::CFileStorage*>(a[1]))
                                     : rl->Find(reinterpret_cast<CTPBaseResource*>(a[1]));
        return tpw_rs_list_node_index(sl, position);
    } else if (op == "RemoveAll") {
        if (storages) sl->RemoveAll();
        else rl->RemoveAll();
    } else if (op == "RemoveHead") {
        return storages ? lab(sl->RemoveHead()) : lab(rl->RemoveHead());
    } else if (op == "RemoveAt") {
        POSITION position = tpw_rs_list_node(sl, static_cast<int32_t>(a[1]));
        return lab(rl->RemoveAt(position));
    } else if (op == "AddTail") {
        sl->AddTail(reinterpret_cast<bfl::FileSystem::CFileStorage*>(a[1]));
    } else if (op == "AddHead") {
        rl->AddHead(reinterpret_cast<CTPBaseResource*>(a[1]));
    } else {
        return -12345;
    }
    return 0;
}

}  // extern "C"
