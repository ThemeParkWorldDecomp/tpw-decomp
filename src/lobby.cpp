// lobby.cpp (main app of SimTheme Park 2.04b3): the lobby object and the base class of its
// systems (0x100a56c8..0x100a63ac). PARTIAL: CLBLobby::WindowMessagesFn and the mail icon's
// message function are untranslated, and so are the four systems (meshes, view, flying
// meshes, screens).
//
// The lobby is a list of systems. Each system adds itself to the list of the lobby that
// exists when it is made (gLobby) and takes itself out when it is destroyed; the lobby
// passes Init, Tick, Render and Close on to every system in the list, in the order they
// were made.
#include "lobby.h"

#include <ctime>

#include "Clock.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_main.h"

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

CLBLobbyGlobals::~CLBLobbyGlobals() {}

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
