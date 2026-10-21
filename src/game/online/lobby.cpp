// lobby.cpp: name proven by __sinit_lobby_cpp in the main application.

#include "analyser.h"
#include "CellBase.h"
#include "Clock.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "MapCell.h"
#include "OptionsBalance.h"
#include "ppc_math.h"
#include "rsse.h"
#include "TbDynamicStringTemplate.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "TrackCell.h"
#include "UI_Base.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <new>

// lobby.cpp (main app of SimTheme Park 2.04b3): the lobby object and the base class of its
// systems (0x100a56c8..0x100a63ac). PARTIAL: CLBLobby::WindowMessagesFn and the mail icon's
// message function are untranslated, and so are the four systems (meshes, view, flying
// meshes, screens).
//
// The lobby is a list of systems. Each system adds itself to the list of the lobby that
// exists when it is made (gLobby) and takes itself out when it is destroyed; the lobby
// passes Init, Tick, Render and Close on to every system in the list, in the order they
// were made.

CLBLobby* gLobby;
CLBScreenSystem* gLobbyScreens;
CLBFlyingMeshSystem* gLobbyFlyingMeshes;
CLBLobbyView* gLobbyView;
CLBLobbyMeshes* gLobbyMeshes;
int32_t gLobbyReturnFlag;

// The description of the lobby's window, for InterfaceWindow::Build (the script format is described in src/InterfaceWindow_script.cpp).
// Its first window is the lobby's panel (id 0xbf431), the size of the whole 2048 x 1536 screen and visible, with the mail icon
// (0xbf432) in the lower right corner; Build gives the panel the message function and returns it, so it is CLBLobby::mWindow. Each
// line is one record of the script and the words are the ones in the program image at 0x1029e382 (the test compares all 38).
int16_t gLobbyWindowData[] = {
    0, 1, 1, 0, -3023, 11, 0, 0, 2048, 1536,   // 0: child, kind 1 (window), attributes 0x1, id 0xbf431 (the lobby panel, the whole screen), rect (0, 0, 2048, 1536)
      17, -1, -1,   // 17: help text number -1
      18, -1, -1,   // 18: word at +0x90 is -1
      0, 1, 1, 0, -3022, 11, 1904, 1402, 2007, 1504,   // 0: child, kind 1 (window), attributes 0x1, id 0xbf432 (the mail icon), rect (1904, 1402, 2007, 1504)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -21133, -6734,   // 1: draw object 0xe5b2ad73
        5,   // 5: end of this level
      5,   // 5: end of this level
    5,   // 5: end of this level
};
static_assert(sizeof(gLobbyWindowData) == 38 * sizeof(int16_t), "gLobbyWindowData has 38 words");

template class TbTList<CLBLobbySystem*, CLBLobbySystem*>;

CLBLobbySystem::CLBLobbySystem() { gLobby->Register(this); }
CLBLobbySystem::~CLBLobbySystem() { gLobby->UnRegister(this); }

// (The step at 0x08 is left as the memory had it.)
CLBLobby::CLBLobby() {
    mWidth = 0;
    mHeight = 0;
    mState = 0;
    mSystems = new TbPtrList<CLBLobbySystem>;
    mWindow = nullptr;
    mF0C = 0;
    mGlobals = nullptr;
    mTime = 0;
    mLastTime = 0;
    mTickTime = 0;
    mF1C = 0;
    mWidth = 0x400;
    mHeight = 0x300;
    mStarted = static_cast<uint32_t>(std::time(nullptr));
}

CLBLobby::~CLBLobby() { DeleteSystemList(); }

int32_t LB_Lobby_WindowMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (gLobby == nullptr) return 0;
    return gLobby->WindowMessagesFn(window, message, a, b);
}

// The lobby's mail icon (main app 0x100a59ec; the inventory files it in lobby.cpp; unit w40): the window handler of the small envelope
// the lobby window shows when the player has mail waiting (CLBLobby::Init gives it to the window with id 0xbf432, and
// UpdateOutboxMailIcon, src/UI_IslandLobby.cpp, shows and hides it).

// 0x100a59ec. Only message 0x10006 (the icon was clicked) is handled here. The online system is made first when there is none (the
// lobby makes it too, but a click can come before). A player who is logged in gets the outbox panel; anyone else a dialog box with
// the interface string 0x1d9, white, a button of kind 2 (the first of three button slots, the other two unused) and no callbacks. Every
// message, the click included, then goes to the window's default handler and its answer is the function's.
int32_t LB_Lobby_MailIconMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
        if (gOnlineSystem->AreWeLoggedIn() != 0) {
            CreateOnlineOutboxPanel();
        } else {
            dlgboxparams params;
            params.mText = UIMain_GetString(0x1d9);
            params.mRed = 0xff;
            params.mGreen = 0xff;
            params.mBlue = 0xff;
            params.mValue8 = 4;
            params.mValueC = 0;
            UI_DialogBox(params, dlgbutton(2), nullptr, dlgbutton(0), nullptr, dlgbutton(0), nullptr, false);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t CLBLobby::Init() {
    mState = 1;
    if (gLobbyWindowDefinition != nullptr) {
        mWindow = gLobbyWindowDefinition->Build(gLobbyWindowData, LB_Lobby_WindowMessagesFn);
        mWindow->CaptureKb(0);
        InterfaceWindow* mail = mWindow->GetWindow(0xbf432);
        if (mail != nullptr) {
            mail->SetCallbackFunction(LB_Lobby_MailIconMessagesFn);
            if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
            // (the size word of the outbox's first list of numbers, at 0x8c of the user info)
            if (gUserInfo->mOutbox.mF28.empty()) UI_SendMessage(mail, 6, 0, 0);
        }
        UI_CreateVersionNumber(false);
    }
    const int32_t now = gClock.Read();
    mTime = now;
    mLastTime = now;
    mF1C = 0;
    SND_RegisterGlobalLobbySounds();
    mGlobals = new CLBLobbyGlobals;
    mGlobals->ParseLobbyDescriptor();
    gLobbyMeshes = new CLBLobbyMeshes;
    gLobbyView = new CLBLobbyView;
    gLobbyFlyingMeshes = new CLBFlyingMeshSystem;
    gLobbyScreens = new CLBScreenSystem;
    for (POSITION position = mSystems->GetHeadPosition(); position != nullptr;) mSystems->GetNext(position)->Init();
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    if (gOnlineSystem->mF108 == 0) {
        if (gLobbyReturnFlag == 0) {
            if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
            if (gUserInfo->mGameUser == -1) CreateLoginPanel();
            gLobbyScreens->mThemeIslands->Show();
        } else {
            gLobbyScreens->mDownloadGlobe->Show();
            gLobbyReturnFlag = 0;
        }
    } else {
        gLobbyScreens->mPublishGlobe->Show();
    }
    gLobbyScreens->mAdvisor->Show();
    return mState;
}

// The step is the fixed clock's, at most half a second; systems get it in hundredths of
// its milliseconds.
int32_t CLBLobby::Tick() {
    mLastTime = mTime;
    mTime = gClock.ReadFixed();
    mTickTime = static_cast<uint32_t>(mTime - mLastTime);
    if (mTickTime > 500) mTickTime = 500;
    mTickUnits = static_cast<float>(mTickTime) / 100.0f;
    for (POSITION position = mSystems->GetHeadPosition(); position != nullptr;) mSystems->GetNext(position)->Tick();
    return mState;
}

int32_t CLBLobby::Render() {
    for (POSITION position = mSystems->GetHeadPosition(); position != nullptr;) mSystems->GetNext(position)->Render();
    return mState;
}

// The four systems go in this order: flying meshes, view, meshes, screens.
int32_t CLBLobby::Close() {
    for (POSITION position = mSystems->GetHeadPosition(); position != nullptr;) mSystems->GetNext(position)->Close();
    delete gLobbyFlyingMeshes;
    gLobbyFlyingMeshes = nullptr;
    delete gLobbyView;
    gLobbyView = nullptr;
    delete gLobbyMeshes;
    gLobbyMeshes = nullptr;
    delete gLobbyScreens;
    gLobbyScreens = nullptr;
    delete mGlobals;
    mGlobals = nullptr;
    SND_FreeGlobalLobbySounds();
    UI_SendMessage(mWindow, 4, 0, 0);
    return mState;
}

CLBLobbyGlobals::~CLBLobbyGlobals() {}

// Destroys every system still in the list (each takes itself out of it as it goes, which
// by then finds nothing), then the list.
void CLBLobby::DeleteSystemList() {
    while (mSystems->mCount != 0) {
        CLBLobbySystem* system = mSystems->RemoveHead();
        if (system != nullptr) delete system;
    }
    delete mSystems;
    mSystems = nullptr;
}

void CLBLobby::Register(CLBLobbySystem* system) { mSystems->AddTail(system); }

void CLBLobby::UnRegister(CLBLobbySystem* system) {
    POSITION position = mSystems->Find(system);
    if (position != nullptr) mSystems->RemoveAt(position);
}

// lobby.cpp (main app), unit w48: CLBLobby::SendQuit 0x100a643c (32 bytes). It ignores `this` and reads the globals: the game is
// told to restart and the lobby that runs is told its state is 3, which ends the lobby loop.

void CLBLobby::SendQuit() {
    gGame.mRestart = 1;
    gLobby->mState = 3;
}

// The lobby's flying meshes (main app of SimTheme Park 2.04b3, 0x100aa5d4..0x100aba0c; the
// inventory files them under lobby_globe.cpp): CLBFlyingMeshInstance 0x100aa5d4 (constructor),
// 0x100aa9dc (destructor), 0x100aaa80 (Tick); CLBFlyingMeshGroup 0x100aae1c, 0x100ab008, 0x100ab1a8,
// 0x100ab2fc, 0x100ab3b4, 0x100ab464, 0x100ab4d8; CLBFlyingMeshSystem 0x100ab578, 0x100ab620,
// 0x100ab874, 0x100ab8ac, 0x100ab8f0, 0x100ab99c, 0x100aba0c (Init, Tick and Render are in
// src/lobby_islands_tick.cpp); and two odds and ends: CLBLobby::WindowMessagesFn 0x100a6484 and
// PAR_GetEmitterParticleCount 0x100ba170.
//
// The theme description's FLYINGMESH( lines make groups (ParseThemeDescriptor). A group
// registers with the system and then makes its instances a few at a time on its ticks, up to the
// share of its count that the LOBBYOBJECTS option allows. An instance is a mesh of the lobby's
// mesh system that flies from one random point of the group's box to another: each tick it turns
// a tenth of the way towards the goal, moves, and picks a new goal when it is near enough.

// ---- CLBLobby::WindowMessagesFn

// The lobby window's messages (the window calls LB_Lobby_WindowMessagesFn). The bits of the word at 0x1c are
// switched on by message 0x10005 and off by 0x10004 for the argument 0, 1 or 2 (bit 1, 2 or 4; any other argument
// does nothing): the window's keyboard modifiers or mouse buttons. After the message's own work every system is
// given the message, and the window's default handler answers.
int32_t CLBLobby::WindowMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x10003:
        mWidth = static_cast<int32_t>(static_cast<uint32_t>(a) >> 16);
        mHeight = static_cast<int32_t>(static_cast<uint32_t>(a) & 0xffff);
        break;
    case 0x14:
        if (gLobby != nullptr) mWindow = nullptr;
        break;
    case 0x11:
        if (a == 1) UpdateOutboxMailIcon();
        break;
    case 0x22:
        mF1C = 0;
        break;
    case 0x10005:
        if (a >= 0 && a <= 2) mF1C = static_cast<int32_t>(static_cast<uint32_t>(mF1C) | (1u << a));
        break;
    case 0x10004:
        if (a >= 0 && a <= 2) mF1C = static_cast<int32_t>(static_cast<uint32_t>(mF1C) & ~(1u << a));
        break;
    default:
        break;
    }
    if (gLobby != nullptr) {
        for (POSITION position = mSystems->GetHeadPosition(); position != nullptr;)
            mSystems->GetNext(position)->WindowMessagesFn(message, a, b);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x100a66b0, 12 bytes.
int32_t CLBLobby::IsKeyDown(uint32_t flag) {
    return static_cast<int32_t>(static_cast<uint32_t>(mF1C) & flag);
}

// ---- 0x100a6be0 (148 bytes): the static initialiser of lobby.cpp. The objects and registration nodes have no known names (invented;
// the Mac addresses are in the comments).
void UIMain_DestroyWString(void* object);   // 0x103bcbb8
void UIMain_DestroyString(void* object);    // 0x103bcd38
extern "C" void __register_global_object(void* object, void* destructor, void* node);

alignas(8) uint8_t gLobbyWString48AC[64];   // 0x102e48ac
alignas(8) uint8_t gLobbyWString488C[64];   // 0x102e488c
uint8_t gLobbyNode48C0[16];                 // 0x102e48c0
uint8_t gLobbyNode48A0[16];                 // 0x102e48a0
uint8_t gLobbyNode487C[16];                 // 0x102e487c

// The strings: 0x1023e586 (wide), 0x1023e586 + 0xe (wide), 0x1023e585 (narrow, empty).
static const char16_t kLobbyW1[] = u"upload";
static const char16_t kLobbyW2[] = u"gms.dat";

void __sinit_lobby_cpp() {
    using W = TbDynamicStringTemplate<char16_t>;
    void* wide1 = new (static_cast<void*>(gLobbyWString48AC)) W(kLobbyW1);
    __register_global_object(wide1, reinterpret_cast<void*>(&UIMain_DestroyWString), gLobbyNode48C0);
    void* wide2 = new (static_cast<void*>(gLobbyWString488C)) W(kLobbyW2);
    __register_global_object(wide2, reinterpret_cast<void*>(&UIMain_DestroyWString), gLobbyNode48A0);
    void* narrow = new (static_cast<void*>(&gOnlineUploadFile)) TbDynamicStringTemplate<char>("");
    __register_global_object(narrow, reinterpret_cast<void*>(&UIMain_DestroyString), gLobbyNode487C);
}
