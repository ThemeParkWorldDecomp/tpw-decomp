// TPMain.cpp: name proven by __sinit_TPMain_cpp in the main application.

#include "3DFont.h"
#include "CBalanceTable.h"
#include "ExitErr.h"
#include "FS_MasterSave.h"
#include "FS_Registry.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "keyboard.h"
#include "LbFile.h"
#include "lobby.h"
#include "OnlineBalance.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ResolutionBalance.h"
#include "SoundBalance.h"
#include "tb_resource_display.h"
#include "tb_texturecache.h"
#include "TbITable.h"
#include "Text.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPMovie.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include <string>

// TPMain.cpp (decompiled from the main app of SimTheme Park 2.04b3: 0x10000208..0x10001c80
// and __sinit_TPMain_cpp at 0x10003220).
//
// Not translated here: SamsUtilities::UDebug (0x10000e40, a variadic no-op that stays a
// platform declaration so tests can record it), the out-of-line UResourceId destructor
// (0x10001ad0, implicit here) and the MSL std::basic_string / exception template copies in
// this range (the host's std::string replaces them).

using namespace SamsUtilities;

ThemeParkWorld* gThemeParkWorld;   // 0x102b2bb0
int32_t gTPMain_102b2bac;          // 0x102b2bac
const char* gAppName;              // 0x102b2ba8
int32_t gTPMain_102b2ba4;          // 0x102b2ba4
uint32_t gTPWFlags;                // 0x102b2b4c
uint8_t gNoSavedSettings;          // 0x102b2b44
uint32_t gGameOptions;             // 0x102b2b48 (between TPMain's globals; TPMain does not use it)
GrafPort* gMainWindow;             // 0x102b2bb4
int32_t gScreenWidth;              // 0x102b2b54
int32_t gScreenHeight;             // 0x102b2b50
float gViewCentreX;                // 0x102b2b6c
float gViewCentreY;                // 0x102b2b68
float gScreenCentreX;              // 0x102b2b64
float gScreenCentreY;              // 0x102b2b60

// __sinit_TPMain_cpp, in construction order. The language names, title and registry key
// are the same strings TPDialog.cpp constructs for itself (gTPDialogLanguages): each file
// has its own copies, as from static definitions in a shared header. Nothing in this file
// reads them.
TbDynamicStringTemplate<char> gTPMainLanguages[16] = {   // 0x102b2c18, 20-byte slots
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
TbDynamicStringTemplate<char> gTPMainTitle("Theme Park World");    // 0x102b2bf8
TbDynamicStringTemplate<char> gTPMainLanguage("Language");         // 0x102b2bd8
TbDynamicStringTemplate<char16_t> gTPMainUpload(u"upload");        // 0x102b2bb8
OnlineConfig gOnlineConfig;                                        // 0x102b2b70

int ThemeParkWorldMain() {   // main, 0x10000208
    ThemeParkWorld world;
    world.Run();
    return 0;
}

ThemeParkWorld::~ThemeParkWorld() {}   // 0x1000028c

bool ThemeParkWorld::Initialise() {   // 0x10000320
    UString projectDir;
    mValue70 = 500000;
    UMicroseconds rate = {static_cast<int64_t>(100) * 1000};   // 100 ms
    mTimer.SetRate(rate, true);
    gThemeParkWorld = this;
    gTPMain_102b2bac = 0;
    gAppName = mName.mText;
    gTPMain_102b2ba4 = 0;

    // A "development data" folder next to the application replaces the application
    // folder as the project directory.
    UFileItem* developmentData;
    {
        UString name("development data");
        developmentData = gThisProcess.GetFolder()->GetItem(name, 0x666f6c64);   // 'fold'
    }
    if (developmentData == nullptr) {
        gThisProcess.GetFolder()->GetPath(projectDir);
    } else {
        developmentData->GetPath(projectDir);
        if (developmentData != nullptr) delete developmentData;
    }
    LbFile_SetProjectDir(projectDir.mText);
    FS_ReadRegistry();
    FS_ReadLanguageRegistry();
    FS_ScanThemes(gTPFileInfo.GetLocalPath());
    LoadSystemBalanceFiles();

    if (gUserInfo != nullptr) delete gUserInfo;
    gUserInfo = nullptr;
    gUserInfo = new CFSUserInfo;
    gUserInfo->GameLoginInit();   // the original's NULL check after new is dead
    if (gOnlineSystem != nullptr) delete gOnlineSystem;
    gOnlineSystem = nullptr;

    InitialiseGameStage1();
    gTPWFlags |= 1;
    InitialiseMemorySystem();
    int32_t loaded;
    {
        TbDynamicStringTemplate<char> config("save:config.tcf");
        loaded = gPlayerSettings.LoadGlobal(config);
    }
    if (loaded == 0) gNoSavedSettings = 1;
    return true;
}

void ThemeParkWorld::Shutdown() {   // 0x10000604
    ClosedownGame();
    GAMESYSTEM_Close();
    EngineCloseDown();
    gExitError.ReportAnyError();
    gText.UnInit();
}

bool ThemeParkWorld::Process() {   // 0x10000680
    if (gTPWFlags & 1) {
        if ((gTPWFlags & 4) == 0) {
            if (gText.Init() == 0) {
                gTPWFlags |= 2;
                return false;
            }
            {
                TbDynamicStringTemplate<char> remote("Theme Park:Theme Park Data:");
                gTPFileInfo.SetRemotePath(remote);
            }
            if (!IsCdPresent()) {
                gTPWFlags |= 2;
                return false;
            }
            gGame.InitResources();
            InitialiseInstancing(0);
            InitialiseGameStage2();
            InitialiseGameStage3();
            if (gUIRealScreenWidth == 0)
                UI_SetRealScreenRez(static_cast<int16_t>(gScreenWidth), static_cast<int16_t>(gScreenHeight));
            gText.SetRez(gScreenWidth);
            TPSaveRegistry();
            if (EngineStartup() == 0) {
                gTPWFlags |= 2;
                return false;
            }
            GAMESYSTEM_Init();
            if (gMainWindow != nullptr) {   // the cursor starts in the middle of the window
                URectangleMac bounds(gMainWindow->portRect);
                bounds.ToGlobal(gMainWindow);
                UPointMac centre;
                centre.x = (bounds.mRect.left + bounds.mRect.right) / 2;
                centre.y = (bounds.mRect.top + bounds.mRect.bottom) / 2;
                UCursor::SetPosition(centre);
            }
            gTPWFlags |= 4;
        }
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
        gOnlineSystem->Tick();
        gTPWFlags |= GAMESYSTEM_Tick();
    }
    return (gTPWFlags & 2) == 0;
}

void ThemeParkWorld::HandleUpdateWindow(GrafPort*) {}   // 0x100008f8

// Key events go to the UI and to the key table as Windows virtual keys. Character keys
// (and space) are sent as their upper-case character; the extended keys (numeric keypad
// operators 0x6a..0x6f except the separator, and any key the shim flags with 0x100) are
// moved into the high byte. Modifier bits: 3 shift, 0x0c command or control, 0x30 option.
void ThemeParkWorld::HandleTyping(char character, char keyCode, uint32_t modifiers, bool down) {   // 0x10000940
    int32_t mods = 0;
    if (modifiers & 0x200) mods |= 3;
    if (modifiers & 0x1100) mods |= 0xc;
    if (modifiers & 0x800) mods |= 0x30;
    uint16_t keyFlags;
    const uint32_t vk = NS_MacDoze::MacToWindowsVirtualKey(static_cast<uint8_t>(keyCode), keyFlags) & 0xffu;
    int16_t key = static_cast<int16_t>(vk);
    const bool keypadOperator = (vk >= 0x6a && vk <= 0x6b) || (vk >= 0x6d && vk <= 0x6f);
    if (vk == 0x20) {
        key = 0;
    } else if (keypadOperator || (keyFlags & 0x100)) {
        key = static_cast<int16_t>(static_cast<uint16_t>(vk << 8));
    }
    const signed char ch = static_cast<signed char>(character);
    auto upper = [ch]() -> int16_t { return ch == -1 ? -1 : msl_upper_map[static_cast<uint8_t>(ch)]; };
    if (down) {
        if (key == 0) key = upper();
        UI_Keydown(key, mods << 16);
        gKeyTable->KeyDown(key, static_cast<int16_t>(mods));
    } else if (key != 0) {
        UI_Keyup(key, (mods << 16) | 0x1000000);
        gKeyTable->KeyUp(key, static_cast<int16_t>(mods));
    } else {
        UI_KeyChar(ch, 0x1000000);
        UI_Keyup(upper(), (mods << 16) | 0x1000000);
        gKeyTable->KeyUp(upper(), static_cast<int16_t>(mods));
    }
}

// The window position is clamped to the display less 4 pixels and scaled to 2048 x 1536.
void ThemeParkWorld::GetUIMousePos(const UEvent& event, int16_t& x, int16_t& y) {   // 0x10000b54
    const UPoint where = event.GetPosition();
    UPointMac local;
    local.x = where.x;
    local.y = where.y;
    local.ToLocal(nullptr);
    const int32_t maxX = gei_Display.mWidth - 4;
    const int32_t maxY = gei_Display.mHeight - 4;
    if (local.x < 0) local.x = 0;
    else if (local.x > maxX) local.x = maxX;
    if (local.y < 0) local.y = 0;
    else if (local.y > maxY) local.y = maxY;
    x = static_cast<int16_t>(static_cast<int32_t>(static_cast<uint32_t>(local.x) << 11) / gUIRealScreenWidth);
    y = static_cast<int16_t>(static_cast<int32_t>(static_cast<uint32_t>(local.y) * 0x600u) / gUIRealScreenHeight);
}

void ThemeParkWorld::HandleMouseMoved(const UEvent& event) {   // 0x10000cb0
    int16_t x, y;
    GetUIMousePos(event, x, y);
    UI_DevicePointerMove(x, y);
}

// Buttons other than 0..2 cannot come from UGamework; the original then passes whatever
// its caller left in r31 as the button, which cannot be reproduced: 0 here.
static int16_t UIButton(MouseButton button) {
    switch (button) {
    case kMouseButton0: return 0;
    case kMouseButton1: return 1;
    case kMouseButton2: return 2;
    default:
        UDebug("what the hell are we doing here?");
        return 0;
    }
}

void ThemeParkWorld::HandleMouseDown(const UEvent& event, MouseButton button) {   // 0x10000d38
    int16_t x, y;
    GetUIMousePos(event, x, y);
    UI_DeviceButtonDown(UIButton(button), x, y);
}

void ThemeParkWorld::HandleMouseUp(const UEvent& event, MouseButton button) {   // 0x10000e90
    int16_t x, y;
    GetUIMousePos(event, x, y);
    UI_DeviceButtonUp(UIButton(button), x, y);
}

void ThemeParkWorld::HandleMouseWheel(const UEvent& event, int32_t delta) {   // 0x10000f94
    int16_t x, y;
    GetUIMousePos(event, x, y);
    UI_DeviceMouseWheel(delta, x, y, static_cast<int16_t>(event.GetKeyModifiers()));
}

// Only the game window counts. Deactivation stops movies and pauses the sound (in the
// lobby) or the game; activation flushes the texture cache and unpauses the sound in the
// lobby (a paused game stays paused).
void ThemeParkWorld::HandleActivateWindow(GrafPort* window, bool active) {   // 0x1000104c
    if (window != gMainWindow) return;
    if (active) {
        if (gei_TCache != nullptr) gei_TCache->Flush(1);
        gTPWFlags |= 8;
        UI_Activate();
        if (gLobby != nullptr) SOUND_UnPause();
    } else {
        gTPWFlags &= ~8u;
        MOVIE_Stop();
        UI_DeActivate();
        if (gLobby != nullptr) SOUND_Pause();
        else gGame.Pause(static_cast<CGame::ePauseFlag>(1), true);
    }
}

// Command 3 (quit) asks for confirmation in the lobby (state 2) and in a park (state
// 10); in any other state it ends UGamework::Run at once.
void ThemeParkWorld::HandleCommand(int32_t command, uint32_t modifiers) {   // 0x10001170
    if (command != 3) {
        UGamework::HandleCommand(command, modifiers);
        return;
    }
    dlgboxparams params;
    params.mText = UIMain_GetString(9);
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    params.mValueC = 0;
    if (gGameSystemState == 10)
        UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(1), MainGameMenuConfirmQuit_OK, dlgbutton(0),
                     nullptr, false);
    else if (gGameSystemState == 2)
        UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(1), MainLobbyMenuConfirmQuit_OK, dlgbutton(0),
                     nullptr, false);
    else
        mRunning = false;
}

// sound.sam, then online.sam, then (if present) Resolution.sam from the data folder;
// each step only if the previous load succeeded.
void ThemeParkWorld::LoadSystemBalanceFiles() {   // 0x1000129c
    TbDynamicStringTemplate<char> dataPath;
    TbDynamicStringTemplate<char> file;
    dataPath = gTPFileInfo.GetDataPath();
    FS_AddDirectorySeparator(dataPath);
    file = dataPath;
    file += gTPGlobals.mSoundBalanceFile;
    if (gSoundBalance.Load(file.mData, CBalanceTable::LoadMode(0)) != 1) return;
    gPlayerSettings.Init();
    file = dataPath;
    file += gTPGlobals.mOnlineBalanceFile;
    if (gOnlineBalance.Load(file.mData, CBalanceTable::LoadMode(0)) != 1) return;
    SetInitialOnlineValues();
    file = dataPath;
    file += gTPGlobals.mResolutionBalanceFile;
    if (LbFile_Exists(file.mData) != 1) return;
    gResolutionBalance.Load(file.mData, CBalanceTable::LoadMode(0));
}

void SetInitialOnlineValues() {   // 0x10001494
    const BalanceValue* online = gOnlineBalance.mValues;
    gOnlineConfig.mMailServer.assign(std::string(online[7].mString), 0, std::string::npos);
    gOnlineConfig.mChatServer.assign(std::string(online[8].mString), 0, std::string::npos);
    gOnlineConfig.mLoginServer.assign(std::string(online[9].mString), 0, std::string::npos);
    gOnlineConfig.mCityServer.assign(std::string(online[10].mString), 0, std::string::npos);
    gOnlineConfig.mNewsServer.assign(std::string(online[11].mString), 0, std::string::npos);
    gOnlineConfig.mVoteServer.assign(std::string(online[12].mString), 0, std::string::npos);
    gOnlineConfig.mComplainEmail.assign(std::string(online[13].mString), 0, std::string::npos);
    gOnlineConfig.mMailPort = static_cast<int16_t>(online[1].mInteger);
    gOnlineConfig.mChatPort = static_cast<int16_t>(online[2].mInteger);
    gOnlineConfig.mLoginPort = static_cast<int16_t>(online[3].mInteger);
    gOnlineConfig.mCityPort = static_cast<int16_t>(online[4].mInteger);
    gOnlineConfig.mNewsPort = static_cast<int16_t>(online[5].mInteger);
    gOnlineConfig.mVotePort = static_cast<int16_t>(online[6].mInteger);
}

bool ThemeParkWorld::CdDialog::Filter(const UEvent& event, int16_t& itemHit) {   // 0x100016e4
    if (LookForCd()) {
        itemHit = static_cast<int16_t>(mDefaultItem);
        mFound = true;
        return true;
    }
    return UDialog::Filter(event, itemHit);
}

// Shows the "cd missing" dialog (message: UI string 9) until the CD appears or the
// player quits; true if the CD is there.
bool ThemeParkWorld::IsCdPresent() {   // 0x100017b8
    CdDialog dialog(UResourceId(UString("cd missing", 10), 0x10000));
    TbDynamicStringTemplate<char16_t> message;
    TbDynamicStringTemplate<char> text;
    auto* strings = static_cast<const TbITable*>(gText.mResourcePointers[3]);
    message = UIMain_ConvertMulti(strings->GetConstString(9));
    UniStringToString(text, message);
    dialog.SetParamText(UString(text.mData), UString::kEmptyString, UString::kEmptyString, UString::kEmptyString);
    dialog.SetItemTitle(1, UString("Quit", 4));
    if (LookForCd()) return true;
    dialog.Show();
    return dialog.mFound;
}

// The CD check: the volume "Theme Park" must hold Theme Park Data:data:ui:Here2000.txt (an
// empty file on the disc) and be locked (bit 7 or 15 of the volume attributes). The path
// is stored as its odd and even characters, interleaved here.
bool ThemeParkWorld::LookForCd() {   // 0x10001b6c
    static const char kOdd[] = "TeePr:hm akDt:aau:ee00tt";
    static const char kEven[] = "hm akTeePr aadt:iHr20.x";
    char path[258];
    const char* odd = kOdd;
    const char* even = kEven;
    char* out = path;
    while (*odd != '\0') {
        out[0] = *odd++;
        out[1] = *even++;
        out += 2;
    }
    *out = '\0';
    c2pstr(path);
    FSSpec spec;
    if (FSMakeFSSpec(0, 0, reinterpret_cast<const unsigned char*>(path), &spec) != 0) return false;
    HVolumeParam volume;
    volume.ioCompletion = nullptr;
    volume.ioNamePtr = nullptr;
    volume.ioVRefNum = spec.vRefNum;
    volume.ioVolIndex = 0;
    if (PBHGetVInfoSync(&volume) != 0) return false;
    return (volume.ioVAtrb & 0x8080) != 0;
}
