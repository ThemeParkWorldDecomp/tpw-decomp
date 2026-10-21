// Host side of test_TPMain.py: weak placeholders for every callee of TPMain.cpp that is
// not translated yet (they record their calls in tpw_tpm_log, in the same shape the
// PowerPC stubs of tpmain_support.py produce), plus extern "C" wrappers.
//
// Objects are compared by label: tpw_tpm_objs[i] holds the host address of label i
// (the test registers the same labels on the PowerPC side); NULL is label 0, an unknown
// pointer -1, and objects constructed during a case get labels from 40 upwards.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "3DFont.h"
#include "CBalanceTable.h"
#include "ExitErr.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_Registry.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "LbFile.h"
#include "OnlineBalance.h"
#include "ResolutionBalance.h"
#include "SoundBalance.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "tb_resource_display.h"
#include "tb_texturecache.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

#define TPW_WEAK __attribute__((weak))

using namespace SamsUtilities;

extern "C" {
struct TpmEvent {
    int32_t id;
    int32_t a[8];
    char text[4][96];
};
int32_t tpw_tpm_on = 0;
int32_t tpw_tpm_n = 0;
TpmEvent tpw_tpm_log[256];
int32_t tpw_tpm_ret[128];          // scripted results, by event id
const void* tpw_tpm_objs[64];      // label -> host address
int32_t tpw_tpm_next_label = 40;
// Scripted behaviour of the platform fakes.
int32_t tpw_tpm_has_dev = 0;       // GetItem("development data") finds an item
char tpw_tpm_dev_path[96];         // its path
char tpw_tpm_app_path[96];         // the application folder's path
int32_t tpw_tpm_dx = 0, tpw_tpm_dy = 0;          // ToGlobal adds, ToLocal subtracts
int32_t tpw_tpm_vk_flags = 0;                    // MacToWindowsVirtualKey's flags out
int32_t tpw_tpm_dialog_item = 0;                 // UDialog::mDefaultItem after construction
int32_t tpw_tpm_show_finds = 0;                  // Show sets the CdDialog's mFound
int16_t tpw_tpm_vref = 0, tpw_tpm_vatrb = 0;     // FSMakeFSSpec / PBHGetVInfoSync answers
int32_t tpw_tpm_scan_result = 0;
char tpw_tpm_local_path[96], tpw_tpm_data_path[96];
}

// Event ids, in the order of tpmain_support.EVENTS.
enum {
    E_FS_ReadRegistry, E_FS_ReadLanguageRegistry, E_GetLocalPath, E_GetDataPath, E_SetRemotePath,
    E_FS_AddDirectorySeparator, E_FS_ScanThemes, E_InitialiseGameStage1, E_InitialiseGameStage2,
    E_InitialiseGameStage3, E_TPSaveRegistry, E_ClosedownGame, E_InitialiseMemorySystem,
    E_InitialiseInstancing, E_EngineStartup, E_EngineCloseDown, E_LoadGlobal, E_PlayerSettingsInit,
    E_OnlineCtor, E_OnlineDtor, E_OnlineTick, E_UserInfoCtor, E_UserInfoDtor, E_GameLoginInit,
    E_ReportAnyError, E_TextUnInit, E_TextInit, E_TextSetRez, E_GAMESYSTEM_Close, E_GAMESYSTEM_Init,
    E_GAMESYSTEM_Tick, E_InitResources, E_GamePause, E_UI_SetRealScreenRez, E_KeyDown, E_KeyUp,
    E_UI_Keydown, E_UI_Keyup, E_UI_KeyChar, E_UI_DevicePointerMove, E_UI_DeviceButtonDown,
    E_UI_DeviceButtonUp, E_UI_DeviceMouseWheel, E_UDebug, E_MOVIE_Stop, E_SOUND_Pause, E_SOUND_UnPause,
    E_UI_Activate, E_UI_DeActivate, E_UIMain_GetString, E_UI_DialogBox, E_BalanceLoad,
    E_UniStringToString, E_UIMain_ConvertMulti, E_GameworkCtor, E_GameworkDtor, E_Run,
    E_BaseHandleCommand, E_GetFolder, E_GetItem, E_GetPath, E_ItemDelete, E_SetRate,
    E_DialogCtor, E_DialogDtor, E_DialogFilter, E_DialogShow, E_SetParamText, E_SetItemTitle,
    E_ToGlobal, E_SetPosition, E_ToLocal, E_SetProjectDir, E_LbFileExists, E_MacToWindowsVirtualKey,
    E_TCacheFlush, E_GetConstString, E_c2pstr, E_FSMakeFSSpec, E_PBHGetVInfoSync, E_OnlineConfigCtor,
};

namespace {

TpmEvent* note(int32_t id) {
    if (!tpw_tpm_on || tpw_tpm_n >= 256) return nullptr;
    TpmEvent* e = &tpw_tpm_log[tpw_tpm_n++];
    std::memset(e, 0, sizeof *e);
    e->id = id;
    return e;
}

int32_t label(const void* p) {
    if (p == nullptr) return 0;
    for (int32_t i = 1; i < 64; ++i)
        if (tpw_tpm_objs[i] == p) return i;
    return -1;
}

int32_t new_label(const void* p) {
    for (int32_t i = 1; i < 64; ++i)   // a reused block is a new object
        if (tpw_tpm_objs[i] == p) tpw_tpm_objs[i] = nullptr;
    if (tpw_tpm_next_label < 64) {
        tpw_tpm_objs[tpw_tpm_next_label] = p;
        return tpw_tpm_next_label++;
    }
    return -1;
}

void put_text(TpmEvent* e, int32_t slot, const char* text) {
    if (e != nullptr && text != nullptr) std::strncpy(e->text[slot], text, 95);
}

void put_wide(TpmEvent* e, int32_t slot, const char16_t* text) {
    if (e == nullptr || text == nullptr) return;
    for (int32_t i = 0; i < 95 && text[i] != 0; ++i) e->text[slot][i] = static_cast<char>(text[i] & 0x7f);
}

char* copy_text(const char* text, size_t length) {
    char* p = static_cast<char*>(std::malloc(length + 1));
    std::memcpy(p, text, length);
    p[length] = '\0';
    return p;
}

template<class Char> size_t length_of(const Char* text) {
    size_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}

template<class Char> Char* copy_string(const Char* text) {
    size_t n = text != nullptr ? length_of(text) : 0;
    Char* p = static_cast<Char*>(std::malloc((n + 1) * sizeof(Char)));
    for (size_t i = 0; i < n; ++i) p[i] = text[i];
    p[n] = 0;
    return p;
}

char kEmptyText[1] = {0};
char16_t kEmptyWide[1] = {0};

}  // namespace

#ifdef TPW_LEGACY_STRING_FAKES
// These two specialised fakes live in shim_placeholders.cpp.
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char*);
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();
#include "legacy_main_strings.inc"
#endif

// ---- Sam's Utilities (functional where the unit needs values, recorded otherwise)
TPW_WEAK UString::UString() : mLength(0), mText(kEmptyText) {}
TPW_WEAK UString::UString(const char* text) : mLength(static_cast<uint32_t>(std::strlen(text))) {
    mText = copy_text(text, mLength);
}
TPW_WEAK UString::UString(const char* text, uint32_t length) : mLength(length) { mText = copy_text(text, length); }
TPW_WEAK UString::~UString() {}
TPW_WEAK const UString UString::kEmptyString;
TPW_WEAK UResourceId::UResourceId(const UString& type, int32_t id) : mType(type.mText, type.mLength), mId(id) {}
TPW_WEAK UFileItem::~UFileItem() {}
TPW_WEAK UFileItem* UFolder::GetItem(const UString&, int32_t) { return nullptr; }

namespace {
void set_ustring(UString& s, const char* text) {
    s.mLength = static_cast<uint32_t>(std::strlen(text));
    s.mText = copy_text(text, s.mLength);
}

class FakeItem : public UFileItem {
public:
    ~FakeItem() override {
        if (auto* e = note(E_ItemDelete)) e->a[0] = label(this);
    }
    void GetPath(UString& path) override {
        if (auto* e = note(E_GetPath)) e->a[0] = label(this);
        set_ustring(path, tpw_tpm_dev_path);
    }
};

class FakeFolder : public UFolder {
public:
    ~FakeFolder() override {}
    UFileItem* GetItem(const UString& name, int32_t type) override {
        auto* e = note(E_GetItem);
        if (e) {
            e->a[0] = label(this);
            e->a[1] = type;
            put_text(e, 0, name.mText);
        }
        if (!tpw_tpm_has_dev) return nullptr;
        auto* item = new FakeItem;
        int32_t l = new_label(item);
        if (e) e->a[2] = l;
        return item;
    }
    void GetPath(UString& path) override {
        if (auto* e = note(E_GetPath)) e->a[0] = label(this);
        set_ustring(path, tpw_tpm_app_path);
    }
};
}  // namespace

static FakeFolder tpw_tpm_app_folder;

TPW_WEAK UProcess SamsUtilities::gThisProcess;
TPW_WEAK UFolder* UProcess::GetFolder() {
    if (auto* e = note(E_GetFolder)) e->a[0] = label(this);
    return &tpw_tpm_app_folder;
}

TPW_WEAK void UTimer::SetRate(const UMicroseconds& rate, bool start) {
    if (auto* e = note(E_SetRate)) {
        e->a[0] = label(this);
        e->a[1] = static_cast<int32_t>(rate.mValue >> 32);
        e->a[2] = static_cast<int32_t>(rate.mValue);
        e->a[3] = start;
    }
}

TPW_WEAK UGamework::UGamework(int32_t menuBarId) : mRunning(true), mValue70(0) {
    if (auto* e = note(E_GameworkCtor)) {
        e->a[0] = new_label(this);
        e->a[1] = menuBarId;
    }
}
TPW_WEAK UGamework::~UGamework() {
    if (auto* e = note(E_GameworkDtor)) e->a[0] = label(this);
}
TPW_WEAK void UGamework::Run() {
    if (auto* e = note(E_Run)) {
        e->a[0] = label(this);
        e->a[1] = dynamic_cast<ThemeParkWorld*>(this) != nullptr;
    }
}
TPW_WEAK bool UGamework::Initialise() { return true; }
TPW_WEAK void UGamework::Shutdown() {}
TPW_WEAK bool UGamework::Process() { return true; }
TPW_WEAK void UGamework::HandleCommand(int32_t command, uint32_t modifiers) {
    if (auto* e = note(E_BaseHandleCommand)) {
        e->a[0] = label(this);
        e->a[1] = command;
        e->a[2] = static_cast<int32_t>(modifiers);
    }
}
TPW_WEAK void UGamework::HandleMouseMoved(const UEvent&) {}
TPW_WEAK void UGamework::HandleMouseUp(const UEvent&, MouseButton) {}
TPW_WEAK void UGamework::HandleMouseDown(const UEvent&, MouseButton) {}
TPW_WEAK void UGamework::HandleMouseWheel(const UEvent&, int32_t) {}
TPW_WEAK void UGamework::HandleTyping(char, char, uint32_t, bool) {}
TPW_WEAK void UGamework::HandleUpdateWindow(GrafPort*) {}
TPW_WEAK void UGamework::HandleActivateWindow(GrafPort*, bool) {}

TPW_WEAK URectangle::~URectangle() {}
TPW_WEAK URectangleMac::URectangleMac(const Rect& rect) {
    mRect.left = rect.left;
    mRect.top = rect.top;
    mRect.right = rect.right;
    mRect.bottom = rect.bottom;
}
TPW_WEAK void URectangleMac::ToGlobal(GrafPort* port) {
    if (auto* e = note(E_ToGlobal)) {
        e->a[0] = label(port);
        e->a[1] = mRect.left;
        e->a[2] = mRect.top;
        e->a[3] = mRect.right;
        e->a[4] = mRect.bottom;
    }
    mRect.left += tpw_tpm_dx;
    mRect.right += tpw_tpm_dx;
    mRect.top += tpw_tpm_dy;
    mRect.bottom += tpw_tpm_dy;
}
TPW_WEAK void UCursor::SetPosition(const UPointMac& position) {
    if (auto* e = note(E_SetPosition)) {
        e->a[0] = position.x;
        e->a[1] = position.y;
    }
}
TPW_WEAK void UPointMac::ToLocal(GrafPort* port) {
    if (auto* e = note(E_ToLocal)) {
        e->a[0] = label(port);
        e->a[1] = x;
        e->a[2] = y;
    }
    x -= tpw_tpm_dx;
    y -= tpw_tpm_dy;
}
// Host events are 16-byte EventRecords in native byte order (where.v 0x0a, where.h 0x0c,
// modifiers 0x0e).
TPW_WEAK UPoint UEvent::GetPosition() const {
    const auto* bytes = reinterpret_cast<const uint8_t*>(this);
    int16_t v, h;
    std::memcpy(&v, bytes + 0x0a, 2);
    std::memcpy(&h, bytes + 0x0c, 2);
    return UPoint{h, v};
}
TPW_WEAK uint16_t UEvent::GetKeyModifiers() const {
    uint16_t m;
    std::memcpy(&m, reinterpret_cast<const uint8_t*>(this) + 0x0e, 2);
    return m;
}

TPW_WEAK UDialog::UDialog(const UResourceId& id, bool option) : mDefaultItem(tpw_tpm_dialog_item) {
    if (auto* e = note(E_DialogCtor)) {
        e->a[0] = new_label(this);
        e->a[1] = id.mId;
        e->a[2] = option;
        put_text(e, 0, id.mType.mText);
    }
}
TPW_WEAK UDialog::~UDialog() {
    if (auto* e = note(E_DialogDtor)) e->a[0] = label(this);
}
TPW_WEAK bool UDialog::Filter(const UEvent& event, int16_t& itemHit) {
    if (auto* e = note(E_DialogFilter)) {
        e->a[0] = label(this);
        e->a[1] = label(&event);
        e->a[2] = itemHit;
    }
    itemHit = static_cast<int16_t>(tpw_tpm_ret[E_DialogFilter] >> 8);
    return (tpw_tpm_ret[E_DialogFilter] & 0xff) != 0;
}
TPW_WEAK void UDialog::Show() {
    if (auto* e = note(E_DialogShow)) e->a[0] = label(this);
    if (tpw_tpm_show_finds) static_cast<ThemeParkWorld::CdDialog*>(this)->mFound = true;
}
TPW_WEAK void UDialog::SetParamText(const UString& p0, const UString& p1, const UString& p2, const UString& p3) {
    if (auto* e = note(E_SetParamText)) {
        e->a[0] = label(this);
        put_text(e, 0, p0.mText);
        put_text(e, 1, p1.mText);
        put_text(e, 2, p2.mText);
        put_text(e, 3, p3.mText);
    }
}
TPW_WEAK void UDialog::SetItemTitle(int32_t item, const UString& title) {
    if (auto* e = note(E_SetItemTitle)) {
        e->a[0] = label(this);
        e->a[1] = item;
        put_text(e, 0, title.mText);
    }
}

// ---- toolbox, MSL, macdoze
TPW_WEAK unsigned char* c2pstr(char* text) {
    auto* e = note(E_c2pstr);
    put_text(e, 0, text);
    size_t n = std::strlen(text);
    std::memmove(text + 1, text, n);
    text[0] = static_cast<char>(n);
    return reinterpret_cast<unsigned char*>(text);
}
TPW_WEAK OSErr FSMakeFSSpec(int16_t vRefNum, int32_t dirID, const unsigned char* name, FSSpec* spec) {
    if (auto* e = note(E_FSMakeFSSpec)) {
        e->a[0] = vRefNum;
        e->a[1] = dirID;
        std::memcpy(e->text[0], name + 1, name[0] < 95 ? name[0] : 95);
    }
    spec->vRefNum = tpw_tpm_vref;
    return static_cast<OSErr>(tpw_tpm_ret[E_FSMakeFSSpec]);
}
TPW_WEAK OSErr PBHGetVInfoSync(HVolumeParam* pb) {
    if (auto* e = note(E_PBHGetVInfoSync)) {
        e->a[0] = label(pb->ioCompletion);
        e->a[1] = label(pb->ioNamePtr);
        e->a[2] = pb->ioVRefNum;
        e->a[3] = pb->ioVolIndex;
    }
    pb->ioVAtrb = tpw_tpm_vatrb;
    return static_cast<OSErr>(tpw_tpm_ret[E_PBHGetVInfoSync]);
}
// The "C" locale's table, as MSL's __upper_map.
#define TPW_U(i) static_cast<unsigned char>(((i) >= 'a' && (i) <= 'z') ? (i) - 32 : (i))
#define TPW_U4(i) TPW_U(i), TPW_U((i) + 1), TPW_U((i) + 2), TPW_U((i) + 3)
#define TPW_U16(i) TPW_U4(i), TPW_U4((i) + 4), TPW_U4((i) + 8), TPW_U4((i) + 12)
#define TPW_U64(i) TPW_U16(i), TPW_U16((i) + 16), TPW_U16((i) + 32), TPW_U16((i) + 48)
TPW_WEAK extern const unsigned char msl_upper_map[256] = {TPW_U64(0), TPW_U64(64), TPW_U64(128), TPW_U64(192)};
TPW_WEAK uint16_t NS_MacDoze::MacToWindowsVirtualKey(uint8_t macKeyCode, uint16_t& flags) {
    if (auto* e = note(E_MacToWindowsVirtualKey)) e->a[0] = macKeyCode;
    flags = static_cast<uint16_t>(tpw_tpm_vk_flags);
    return static_cast<uint16_t>(tpw_tpm_ret[E_MacToWindowsVirtualKey]);
}

// ---- bullfrog files
#ifdef TPW_LEGACY_STRING_FAKES
TPW_WEAK void LbFile_SetProjectDir(const char* directory) { put_text(note(E_SetProjectDir), 0, directory); }
TPW_WEAK uint8_t LbFile_Exists(const char* name) {
    put_text(note(E_LbFileExists), 0, name);
    return static_cast<uint8_t>(tpw_tpm_ret[E_LbFileExists]);
}
#endif

// ---- engine, ltms
TPW_WEAK CTextureCache* gei_TCache;
TPW_WEAK void CTextureCache::Flush(int32_t mode) {
    if (auto* e = note(E_TCacheFlush)) {
        e->a[0] = label(this);
        e->a[1] = mode;
    }
}
extern "C" {
unsigned char tpw_tpm_ui_string9[96];
}
TPW_WEAK const unsigned char* TbITable::GetConstString(uint32_t id) const {
    if (auto* e = note(E_GetConstString)) {
        e->a[0] = label(this);
        e->a[1] = static_cast<int32_t>(id);
    }
    return tpw_tpm_ui_string9;
}

// ---- main app: untranslated callees
TPW_WEAK int32_t gGameSystemState;
TPW_WEAK CSYS_TPFileInfo gTPFileInfo;
TPW_WEAK CFSUserInfo* gUserInfo;
TPW_WEAK CFSOnlineSystem* gOnlineSystem;
TPW_WEAK CPlayerSettings gPlayerSettings;
// gGame itself is src/game/TPGlobals.cpp's (a weak blank next to it would be constructed twice);
// its constructor and destructor here leave it alone, as the tests expect.
TPW_WEAK CGame::CGame() {}
TPW_WEAK CGame::~CGame() {}
TPW_WEAK CExitError gExitError;
TPW_WEAK CKeyTable* gKeyTable;
TPW_WEAK int16_t gUIRealScreenWidth;
TPW_WEAK int16_t gUIRealScreenHeight;
TPW_WEAK CText gText;
TPW_WEAK CLBLobby* gLobby;
// gTPGlobals is src/game/TPGlobals.cpp's now (a second definition here would be built and
// destroyed a second time over the same object).

namespace {
TbDynamicStringTemplate<char>* path_string(const char* text) {
    auto* s = static_cast<TbDynamicStringTemplate<char>*>(std::calloc(1, sizeof(TbDynamicStringTemplate<char>)));
    new (s) TbDynamicStringTemplate<char>;
    s->mData = copy_text(text, std::strlen(text));
    s->mLength = static_cast<int32_t>(std::strlen(text));
    return s;
}
void simple(int32_t id) { note(id); }
void with_this(int32_t id, const void* self) {
    if (auto* e = note(id)) e->a[0] = label(self);
}
}  // namespace

TPW_WEAK TbError FS_ReadRegistry() {
    simple(E_FS_ReadRegistry);
    return TbError(tpw_tpm_ret[E_FS_ReadRegistry]);
}
TPW_WEAK TbError FS_ReadLanguageRegistry() {
    simple(E_FS_ReadLanguageRegistry);
    return TbError(tpw_tpm_ret[E_FS_ReadLanguageRegistry]);
}
TPW_WEAK const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const {
    with_this(E_GetLocalPath, this);
    return *path_string(tpw_tpm_local_path);
}
TPW_WEAK const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const {
    with_this(E_GetDataPath, this);
    return *path_string(tpw_tpm_data_path);
}
TPW_WEAK void CSYS_TPFileInfo::SetRemotePath(const TbDynamicStringTemplate<char>& path) {
    auto* e = note(E_SetRemotePath);
    if (e) e->a[0] = label(this);
    put_text(e, 0, path.mData);
}
TPW_WEAK void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    put_text(note(E_FS_AddDirectorySeparator), 0, path.mData);
    size_t n = std::strlen(path.mData);
    char* p = static_cast<char*>(std::malloc(n + 2));
    std::memcpy(p, path.mData, n);
    p[n] = ':';
    p[n + 1] = '\0';
    path.mData = p;
    path.mLength = static_cast<int32_t>(n + 1);
}
TPW_WEAK TbError FS_ScanThemes(const TbDynamicStringTemplate<char>& localPath) {
    put_text(note(E_FS_ScanThemes), 0, localPath.mData);
    return TbError(tpw_tpm_scan_result);
}
TPW_WEAK void InitialiseGameStage1() { simple(E_InitialiseGameStage1); }
TPW_WEAK void InitialiseGameStage2() { simple(E_InitialiseGameStage2); }
TPW_WEAK void InitialiseGameStage3() { simple(E_InitialiseGameStage3); }
TPW_WEAK void TPSaveRegistry() { simple(E_TPSaveRegistry); }
TPW_WEAK void ClosedownGame() { simple(E_ClosedownGame); }
TPW_WEAK void InitialiseMemorySystem() { simple(E_InitialiseMemorySystem); }
TPW_WEAK void InitialiseInstancing(uint32_t flags) {
    if (auto* e = note(E_InitialiseInstancing)) e->a[0] = static_cast<int32_t>(flags);
}
TPW_WEAK int32_t EngineStartup() {
    simple(E_EngineStartup);
    return tpw_tpm_ret[E_EngineStartup];
}
TPW_WEAK void EngineCloseDown() { simple(E_EngineCloseDown); }
TPW_WEAK int32_t CPlayerSettings::LoadGlobal(TbDynamicStringTemplate<char>& path) {
    auto* e = note(E_LoadGlobal);
    if (e) e->a[0] = label(this);
    put_text(e, 0, path.mData);
    return tpw_tpm_ret[E_LoadGlobal];
}
TPW_WEAK void CPlayerSettings::Init() { with_this(E_PlayerSettingsInit, this); }
TPW_WEAK CPlayerSettings::~CPlayerSettings() {}
TPW_WEAK CFSOnlineSystem::CFSOnlineSystem() {
    if (auto* e = note(E_OnlineCtor)) e->a[0] = new_label(this);
}
TPW_WEAK CFSOnlineSystem::~CFSOnlineSystem() { with_this(E_OnlineDtor, this); }
TPW_WEAK void CFSOnlineSystem::Tick() { with_this(E_OnlineTick, this); }
// The real Tick and StopProcessing (src/game/online/FS_Online.cpp, unit w62) are linked in for OnlineConfig and call these; the login ticker
// stands for the whole tick (one OnlineTick event per Tick, as before), the others do nothing.
TPW_WEAK void CFSOnlineSystem::TickLogin() { with_this(E_OnlineTick, this); }
TPW_WEAK void CFSOnlineSystem::TickNews() {}
TPW_WEAK void CFSOnlineSystem::TickCity() {}
TPW_WEAK void CFSOnlineSystem::TickUpload() {}
TPW_WEAK void CFSOnlineSystem::TickFTPDownload() {}
TPW_WEAK void CFSOnlineSystem::InitNews() {}
TPW_WEAK void CFSOnlineSystem::InitCity() {}
TPW_WEAK void CFSOnlineSystem::InitUpload() {}
TPW_WEAK void CFSOnlineSystem::InitFTPDownload() {}
TPW_WEAK CFSUserInfo::CFSUserInfo() {
    if (auto* e = note(E_UserInfoCtor)) e->a[0] = new_label(this);
}
TPW_WEAK CFSUserInfo::~CFSUserInfo() { with_this(E_UserInfoDtor, this); }
// CFSUserInfo's e-mail members (email.cpp, untranslated).
TPW_WEAK Outbox::Outbox() {}
TPW_WEAK Outbox::~Outbox() {}
TPW_WEAK AddressBook::AddressBook() {}
TPW_WEAK AddressBook::~AddressBook() {}
TPW_WEAK void CFSUserInfo::GameLoginInit() { with_this(E_GameLoginInit, this); }
TPW_WEAK void CExitError::ReportAnyError() { with_this(E_ReportAnyError, this); }
TPW_WEAK void CText::UnInit() { with_this(E_TextUnInit, this); }
TPW_WEAK int32_t CText::Init() {
    with_this(E_TextInit, this);
    return tpw_tpm_ret[E_TextInit];
}
TPW_WEAK void CText::SetRez(int32_t width) {
    if (auto* e = note(E_TextSetRez)) {
        e->a[0] = label(this);
        e->a[1] = width;
    }
}
TPW_WEAK void GAMESYSTEM_Close() { simple(E_GAMESYSTEM_Close); }
TPW_WEAK void GAMESYSTEM_Init() { simple(E_GAMESYSTEM_Init); }
TPW_WEAK uint32_t GAMESYSTEM_Tick() {
    simple(E_GAMESYSTEM_Tick);
    return static_cast<uint32_t>(tpw_tpm_ret[E_GAMESYSTEM_Tick]);
}
TPW_WEAK void CGame::InitResources() { with_this(E_InitResources, this); }
TPW_WEAK void CGame::Pause(ePauseFlag flag, bool on) {
    if (auto* e = note(E_GamePause)) {
        e->a[0] = label(this);
        e->a[1] = flag;
        e->a[2] = on;
    }
}
TPW_WEAK void UI_SetRealScreenRez(int16_t width, int16_t height) {
    if (auto* e = note(E_UI_SetRealScreenRez)) {
        e->a[0] = width;
        e->a[1] = height;
    }
}
TPW_WEAK int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    if (auto* e = note(E_KeyDown)) {
        e->a[0] = label(this);
        e->a[1] = key;
        e->a[2] = modifiers;
    }
    return 0;
}
TPW_WEAK int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    if (auto* e = note(E_KeyUp)) {
        e->a[0] = label(this);
        e->a[1] = key;
        e->a[2] = modifiers;
    }
    return 0;
}
namespace {
void two(int32_t id, int32_t a, int32_t b) {
    if (auto* e = note(id)) {
        e->a[0] = a;
        e->a[1] = b;
    }
}
void three(int32_t id, int32_t a, int32_t b, int32_t c) {
    if (auto* e = note(id)) {
        e->a[0] = a;
        e->a[1] = b;
        e->a[2] = c;
    }
}
}  // namespace
TPW_WEAK bool UI_Keydown(int16_t key, int32_t flags) {
    two(E_UI_Keydown, key, flags);
    return false;
}
TPW_WEAK bool UI_Keyup(int16_t key, int32_t flags) {
    two(E_UI_Keyup, key, flags);
    return false;
}
TPW_WEAK bool UI_KeyChar(int16_t character, int32_t flags) {
    two(E_UI_KeyChar, character, flags);
    return false;
}
TPW_WEAK void UI_DevicePointerMove(int16_t x, int16_t y) { two(E_UI_DevicePointerMove, x, y); }
TPW_WEAK void UI_DeviceButtonDown(int16_t button, int16_t x, int16_t y) { three(E_UI_DeviceButtonDown, button, x, y); }
TPW_WEAK void UI_DeviceButtonUp(int16_t button, int16_t x, int16_t y) { three(E_UI_DeviceButtonUp, button, x, y); }
TPW_WEAK void UI_DeviceMouseWheel(int32_t delta, int16_t x, int16_t y, int16_t modifiers) {
    if (auto* e = note(E_UI_DeviceMouseWheel)) {
        e->a[0] = delta;
        e->a[1] = x;
        e->a[2] = y;
        e->a[3] = modifiers;
    }
}
// UDebug's placeholder lives in shim_placeholders.cpp (other suites record it too); this
// suite observes it through tpw_udebug_hook.
extern "C" void (*tpw_udebug_hook)(const char* format);
namespace {
void udebug_hook(const char* format) { put_text(note(E_UDebug), 0, format); }
struct UDebugHookInstaller {
    UDebugHookInstaller() { tpw_udebug_hook = udebug_hook; }
} sUDebugHookInstaller;
}  // namespace
TPW_WEAK void MOVIE_Stop() { simple(E_MOVIE_Stop); }
TPW_WEAK void SOUND_Pause() { simple(E_SOUND_Pause); }
TPW_WEAK void SOUND_UnPause() { simple(E_SOUND_UnPause); }
TPW_WEAK void UI_Activate() { simple(E_UI_Activate); }
TPW_WEAK void UI_DeActivate() { simple(E_UI_DeActivate); }
extern "C" {
char16_t tpw_tpm_quit_text[8] = {u'Q', u'u', u'i', u't', u'?', 0};
char16_t tpw_tpm_converted[96];
}
TPW_WEAK const char16_t* UIMain_GetString(int32_t id) {
    if (auto* e = note(E_UIMain_GetString)) e->a[0] = id;
    return tpw_tpm_quit_text;
}
TPW_WEAK char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    if (auto* e = note(E_UIMain_ConvertMulti)) put_text(e, 0, reinterpret_cast<const char*>(text));
    size_t n = 0;
    for (; text[n] != 0 && n < 95; ++n) tpw_tpm_converted[n] = text[n];
    tpw_tpm_converted[n] = 0;
    return tpw_tpm_converted;
}
TPW_WEAK int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    auto* e = note(E_UniStringToString);
    put_wide(e, 0, in.mData);
    size_t n = length_of(in.mData);
    char* p = static_cast<char*>(std::malloc(n + 1));
    for (size_t i = 0; i < n; ++i) p[i] = static_cast<char>(in.mData[i] & 0xff);
    p[n] = '\0';
    out.mData = p;
    out.mLength = static_cast<int32_t>(n);
    return static_cast<int32_t>(n) + 1;   // the characters written, with the NUL
}
TPW_WEAK int32_t MainLobbyMenuConfirmQuit_OK(void*) { return 0; }
TPW_WEAK int32_t MainGameMenuConfirmQuit_OK(void*) { return 0; }
TPW_WEAK InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2,
                           dlgcallback callback2, dlgbutton button3, dlgcallback callback3, bool flag) {
    auto callback_label = [](dlgcallback cb) -> int32_t {
        if (cb == nullptr) return 0;
        if (cb == MainLobbyMenuConfirmQuit_OK) return 1;
        if (cb == MainGameMenuConfirmQuit_OK) return 2;
        return -1;
    };
    if (auto* e = note(E_UI_DialogBox)) {
        e->a[0] = params.mText == tpw_tpm_quit_text ? 1 : -1;
        e->a[1] = params.mRed | (params.mGreen << 8) | (params.mBlue << 16);
        e->a[2] = params.mValue8;
        e->a[3] = params.mValueC;
        e->a[4] = button1 | (button2 << 8) | (button3 << 16);
        e->a[5] = callback_label(callback1) | (callback_label(callback2) << 8) | (callback_label(callback3) << 16);
        e->a[6] = flag;
    }
    return nullptr;
}
TPW_WEAK int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    auto* e = note(E_BalanceLoad);
    if (e) {
        e->a[0] = label(this);
        e->a[1] = mode;
    }
    put_text(e, 0, path);
    // Results per table: label 7 sound, 8 online, 9 resolution (bits 0..7, 8..15, 16..23).
    int32_t which = label(this) - 7;
    return which >= 0 && which < 3 ? (tpw_tpm_ret[E_BalanceLoad] >> (8 * which)) & 0xff : -1;
}

// ---- wrappers
extern "C" {
void tpw_tpm_reset() {
    tpw_tpm_n = 0;
    tpw_tpm_next_label = 40;
    for (int32_t i = 40; i < 64; ++i) tpw_tpm_objs[i] = nullptr;
}
void* tpw_tpm_world_new() {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    auto* w = new ThemeParkWorld;
    tpw_tpm_on = saved;
    return w;
}
void tpw_tpm_world_delete(ThemeParkWorld* w) {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    delete w;
    tpw_tpm_on = saved;
}
// world fields: 0 mRunning, 1 mValue70; name text for gAppName
void tpw_tpm_world_set(ThemeParkWorld* w, int32_t running, int32_t value70, const char* name) {
    w->mRunning = running != 0;
    w->mValue70 = value70;
    w->mName.mText = copy_text(name, std::strlen(name));
    w->mName.mLength = static_cast<uint32_t>(std::strlen(name));
}
void tpw_tpm_world_get(ThemeParkWorld* w, int32_t* out) {
    out[0] = w->mRunning;
    out[1] = w->mValue70;
}
const void* tpw_tpm_world_timer(ThemeParkWorld* w) { return &w->mTimer; }
const char* tpw_tpm_world_name(ThemeParkWorld* w) { return w->mName.mText; }

void* tpw_tpm_addr(const char* name) {
    std::string n(name);
    if (n == "gThemeParkWorld") return &gThemeParkWorld;
    if (n == "gTPMain_102b2bac") return &gTPMain_102b2bac;
    if (n == "gAppName") return &gAppName;
    if (n == "gTPMain_102b2ba4") return &gTPMain_102b2ba4;
    if (n == "gTPWFlags") return &gTPWFlags;
    if (n == "gNoSavedSettings") return &gNoSavedSettings;
    if (n == "gMainWindow") return &gMainWindow;
    if (n == "gScreenWidth") return &gScreenWidth;
    if (n == "gScreenHeight") return &gScreenHeight;
    if (n == "gGameSystemState") return &gGameSystemState;
    if (n == "gUserInfo") return &gUserInfo;
    if (n == "gOnlineSystem") return &gOnlineSystem;
    if (n == "gKeyTable") return &gKeyTable;
    if (n == "gUIRealScreenWidth") return &gUIRealScreenWidth;
    if (n == "gUIRealScreenHeight") return &gUIRealScreenHeight;
    if (n == "gLobby") return &gLobby;
    if (n == "gei_TCache") return &gei_TCache;
    if (n == "gei_Display") return &gei_Display;
    if (n == "gText") return &gText;
    if (n == "gGame") return &gGame;
    if (n == "gTPFileInfo") return &gTPFileInfo;
    if (n == "gPlayerSettings") return &gPlayerSettings;
    if (n == "gExitError") return &gExitError;
    if (n == "gSoundBalance") return &gSoundBalance;
    if (n == "gOnlineBalance") return &gOnlineBalance;
    if (n == "gResolutionBalance") return &gResolutionBalance;
    if (n == "gThisProcess") return &gThisProcess;
    if (n == "app_folder") return &tpw_tpm_app_folder;
    if (n == "upper_map") return const_cast<unsigned char*>(msl_upper_map);
    return nullptr;
}
void tpw_tpm_set_display(int32_t width, int32_t height) {
    gei_Display.mWidth = width;
    gei_Display.mHeight = height;
}
void tpw_tpm_set_uistrings(const void* table) { gText.mResourcePointers[3] = const_cast<void*>(table); }
void tpw_tpm_set_globals_files(const char* sound, const char* online, const char* resolution) {
    gTPGlobals.mSoundBalanceFile = TbDynamicStringTemplate<char>(sound);
    gTPGlobals.mOnlineBalanceFile = TbDynamicStringTemplate<char>(online);
    gTPGlobals.mResolutionBalanceFile = TbDynamicStringTemplate<char>(resolution);
}
void tpw_tpm_set_online_balance(const int32_t* ports, const char* const* servers) {
    for (int32_t i = 0; i < 6; ++i) gOnlineBalance.mValues[1 + i].mInteger = ports[i];
    for (int32_t i = 0; i < 7; ++i) gOnlineBalance.mValues[7 + i].mString = const_cast<char*>(servers[i]);
}
// gOnlineConfig: 7 strings (mail, chat, login, city, news, vote, complain) into a
// 7 x 96 buffer, then the six ports.
void tpw_tpm_get_online_config(char* strings, int32_t* ports) {
    const std::string* s[7] = {&gOnlineConfig.mMailServer, &gOnlineConfig.mChatServer, &gOnlineConfig.mLoginServer,
                               &gOnlineConfig.mCityServer, &gOnlineConfig.mNewsServer, &gOnlineConfig.mVoteServer,
                               &gOnlineConfig.mComplainEmail};
    for (int32_t i = 0; i < 7; ++i) std::strncpy(strings + 96 * i, s[i]->c_str(), 95);
    const int16_t p[6] = {gOnlineConfig.mMailPort, gOnlineConfig.mChatPort, gOnlineConfig.mLoginPort,
                          gOnlineConfig.mCityPort, gOnlineConfig.mNewsPort, gOnlineConfig.mVotePort};
    for (int32_t i = 0; i < 6; ++i) ports[i] = p[i];
}
// Rebuilds gOnlineConfig on zeroed memory with the translated constructor (the PowerPC
// side runs the original on a zeroed object).
void tpw_tpm_reset_online_config() {
    gOnlineConfig.~OnlineConfig();
    std::memset(static_cast<void*>(&gOnlineConfig), 0, sizeof gOnlineConfig);
    new (&gOnlineConfig) OnlineConfig;
}

// The host's sizes of the two objects Initialise creates (0xd4 and 0x404 on the Mac).
uint64_t tpw_tpm_sizeof_userinfo() { return sizeof(CFSUserInfo); }
uint64_t tpw_tpm_sizeof_online() { return sizeof(CFSOnlineSystem); }

void* tpw_tpm_new_userinfo() {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    auto* p = new CFSUserInfo;
    tpw_tpm_on = saved;
    return p;
}
void* tpw_tpm_new_online() {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    auto* p = new CFSOnlineSystem;
    tpw_tpm_on = saved;
    return p;
}
// The texts of the strings __sinit_TPMain_cpp constructs, 19 x 96 (wide as ASCII).
void tpw_tpm_sinit_texts(char* out) {
    for (int32_t i = 0; i < 16; ++i) std::strncpy(out + 96 * i, gTPMainLanguages[i].mData, 95);
    std::strncpy(out + 96 * 16, gTPMainTitle.mData, 95);
    std::strncpy(out + 96 * 17, gTPMainLanguage.mData, 95);
    for (int32_t i = 0; i < 95 && gTPMainUpload.mData[i] != 0; ++i) out[96 * 18 + i] = static_cast<char>(gTPMainUpload.mData[i]);
}

int32_t tpw_tpm_main() { return ThemeParkWorldMain(); }
int32_t tpw_tpm_Initialise(ThemeParkWorld* w) { return w->Initialise(); }
void tpw_tpm_Shutdown(ThemeParkWorld* w) { w->Shutdown(); }
int32_t tpw_tpm_Process(ThemeParkWorld* w) { return w->Process(); }
void tpw_tpm_HandleUpdateWindow(ThemeParkWorld* w, GrafPort* window) { w->HandleUpdateWindow(window); }
void tpw_tpm_HandleTyping(ThemeParkWorld* w, int8_t c, int8_t key, uint32_t modifiers, uint8_t down) {
    w->HandleTyping(static_cast<char>(c), static_cast<char>(key), modifiers, down != 0);
}
void tpw_tpm_GetUIMousePos(ThemeParkWorld* w, const UEvent* event, int16_t* x, int16_t* y) {
    w->GetUIMousePos(*event, *x, *y);
}
void tpw_tpm_HandleMouseMoved(ThemeParkWorld* w, const UEvent* event) { w->HandleMouseMoved(*event); }
void tpw_tpm_HandleMouseDown(ThemeParkWorld* w, const UEvent* event, int32_t button) {
    w->HandleMouseDown(*event, static_cast<MouseButton>(button));
}
void tpw_tpm_HandleMouseUp(ThemeParkWorld* w, const UEvent* event, int32_t button) {
    w->HandleMouseUp(*event, static_cast<MouseButton>(button));
}
void tpw_tpm_HandleMouseWheel(ThemeParkWorld* w, const UEvent* event, int32_t delta) {
    w->HandleMouseWheel(*event, delta);
}
void tpw_tpm_HandleActivateWindow(ThemeParkWorld* w, GrafPort* window, uint8_t active) {
    w->HandleActivateWindow(window, active != 0);
}
void tpw_tpm_HandleCommand(ThemeParkWorld* w, int32_t command, uint32_t modifiers) { w->HandleCommand(command, modifiers); }
void tpw_tpm_LoadSystemBalanceFiles(ThemeParkWorld* w) { w->LoadSystemBalanceFiles(); }
void tpw_tpm_SetInitialOnlineValues() { SetInitialOnlineValues(); }
int32_t tpw_tpm_IsCdPresent(ThemeParkWorld* w) { return w->IsCdPresent(); }
int32_t tpw_tpm_LookForCd() { return ThemeParkWorld::LookForCd(); }
// Filter on a CdDialog built here (its constructor events are not part of the case).
int32_t tpw_tpm_Filter(const UEvent* event, int16_t* item, int32_t default_item, int32_t* found) {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    UString type("cd missing", 10);
    UResourceId id(type, 0x10000);
    auto* dialog = new ThemeParkWorld::CdDialog(id);
    dialog->mDefaultItem = default_item;
    tpw_tpm_objs[30] = dialog;
    tpw_tpm_on = saved;
    int32_t r = dialog->Filter(*event, *item);
    *found = dialog->mFound;
    tpw_tpm_on = 0;
    delete dialog;
    tpw_tpm_on = saved;
    return r;
}
// The deleting destructor of a CdDialog (flag > 0) or the plain one.
void tpw_tpm_CdDialog_destroy(int32_t deleting) {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    UString type("cd missing", 10);
    UResourceId id(type, 0x10000);
    auto* dialog = new ThemeParkWorld::CdDialog(id);
    tpw_tpm_objs[30] = dialog;
    tpw_tpm_on = saved;
    if (deleting) {
        delete dialog;
    } else {
        dialog->~CdDialog();
    }
}
void tpw_tpm_World_destroy(int32_t deleting) {
    int32_t saved = tpw_tpm_on;
    tpw_tpm_on = 0;
    auto* w = new ThemeParkWorld;
    tpw_tpm_objs[1] = w;
    tpw_tpm_on = saved;
    if (deleting) {
        delete w;
    } else {
        w->~ThemeParkWorld();
    }
}
}

#ifdef TPW_LEGACY_STRING_FAKES
#include "fake_strings.h"
#endif
