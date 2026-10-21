// ui_onlineParkInfoPanel.cpp: name proven by __sinit_ui_onlineParkInfoPanel_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "Debug.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "ui_tooltip.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>
#include <new>
#include <string>

// Unit v5550: the online park info panel's messages and the park find panel (ui_onlineParkInfoPanel.cpp, ui_onlineParkFindPanel.cpp):
//   OnlineParkInfoPanelMsg 0x101ea7f0, GeneralParkDownloadCompleteCallback 0x101ea734, VisitParkPostDownloadCallbackFn 0x101ea684,
//   __sinit_ui_onlineParkFindPanel_cpp 0x101ea5d8, UpdateParkFindPanelProgress 0x101ea4e4, RemoveOnlineParkFindPanel 0x101ea468

InterfaceWindow* gOnlineParkInfoPanel = nullptr;                    // 0x103961e4
void (*gOnlineParkInfoClose)() = nullptr;                           // 0x10396114
const void* gVisitParkContext = nullptr;                            // 0x1039610c
void (*gVisitParkPostDownload)(int32_t, const void*) = nullptr;     // 0x10396110
alignas(16) uint8_t gOnlineParkInfoParkStorage[sizeof(CFSOnlinePark)];   // 0x10396118
// The program's definitions of the two virtual calls (slots 0x30 and 0x4c of lobby screens, untranslated: nothing happens). Weak, so the
// unit's test library can define recording fakes.
namespace parkinfo_callee {
__attribute__((weak)) void OnlineIslandsSlot30(void*) {}
__attribute__((weak)) void DownloadGlobeSlot4c(void*) {}
}  // namespace parkinfo_callee

namespace {

// 0x101ea6xx and the like: the panel is removed by the parent, or the parent is told to close (message 4).
void CloseFromParent(InterfaceWindow* window) {
    if (window->GetParent() == gLobby->mWindow)
        RemoveOnlineParkInfoPanel();
    else
        UI_SendMessage(window->GetParent(), 4, 0, 0);
}

CFSOnlineSystem* TheOnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = mainmenu_callee::NewOnlineSystem();
    return gOnlineSystem;
}

// Escape (message 0x100 with -2, message 0x1000b with 0x1b): when the parent is the root window the lobby window is shown again; then
// the panel is removed and the caller's close function runs.
int32_t Escape(InterfaceWindow* window) {
    if (window->GetParent() == gUIRootWindow) UI_SendMessage(gLobby->mWindow, 6, 1, 0);
    RemoveOnlineParkInfoPanel();
    if (gOnlineParkInfoClose != nullptr) gOnlineParkInfoClose();
    return 0;
}

}  // namespace

// 0x101ea684, 116 bytes: removes the panel, tells the caller's function (always set; not tested for NULL) and, on an error, shows the
// lobby's window again.
void VisitParkPostDownloadCallbackFn(int32_t error, const void* context) {
    (void)context;
    RemoveOnlineParkInfoPanel();
    gVisitParkPostDownload(error, gVisitParkContext);
    if (error != 0) UI_SendMessage(gLobby->mWindow, 6, 1, 0);
}
// 0x101ea734, 124 bytes: a finished download (error 0) sets the lobby's state to 2 and the game type to online.
void GeneralParkDownloadCompleteCallback(int32_t error, const void* context) {
    (void)context;
    if (error == 0) {
        gLobby->mState = 2;
        TheGameType().SetGameType(kGameTypeOnline);
    }
}

// 0x101ea7f0, 896 bytes. 0x14 forgets the panel and gives the keyboard back; 0x100 with 0xe592e (visit) removes the panel and asks for
// the park's download, with 0xe592f (unpublish) unpublishes the park, removes the panel and shows the lobby's screen, with -2 (cancel)
// and 0x1000b with 0x1b (escape) close it. Everything else, and the cases that fall through, goes to the default window handler.
int32_t OnlineParkInfoPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CFSOnlinePark* park = reinterpret_cast<CFSOnlinePark*>(gOnlineParkInfoParkStorage);
    if (message == 0x100) {
        if (a == 0xe592e) {
            CloseFromParent(window);
            TheOnlineSystem()->RequestFTPDownload(*park, VisitParkPostDownloadCallbackFn, nullptr);
            return 0;
        }
        if (a < 0xe592e) {
            if (a == -2) return Escape(window);
        } else if (a < 0xe5930) {
            TheOnlineSystem()->UnpublishPark(park->mParkID);
            CloseFromParent(window);
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            if (mainmenu_callee::ScreenIsVisible(gLobbyScreens->mOnlineIslands))
                parkinfo_callee::OnlineIslandsSlot30(gLobbyScreens->mOnlineIslands);
            else
                parkinfo_callee::DownloadGlobeSlot4c(gLobbyScreens->mDownloadGlobe);
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x14) {
            gOnlineParkInfoPanel = nullptr;
            // The parent's destroying flag (the Mac's byte 0) by name: the host window has its vtable pointer first.
            if (window->GetParent() != nullptr && window->GetParent()->mDestroying == 0)
                window->GetParent()->CaptureKb(0);
            else
                UIMain_DecideKbCapture();
        }
    } else if (message == 0x1000b && a == 0x1b) {
        return Escape(window);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// CreateOnlineParkInfoPanel (main app, 0x101eabb4, 3660 bytes): the panel that shows one online park (name, owner, comment, the
// numbers, the theme, the rating, whether it can be downloaded). Does nothing when the panel is open already. Otherwise it keeps
// a copy of the park and the caller's three words, builds the panel from its window script (under `parent`, shifted down 200 for
// a parent, under the lobby's window otherwise), and writes the texts into its child windows. Everything the original builds
// goes through a sprite text object per window (font 6) and TbDynamicStringTemplate scratch text; the title window gets the
// title text object (font 5, centred) and the park's name, the window 0xe5930 gets the park's second text.

// The globals of the panel (new here; the panel's message function is the other user).
// gOnlineParkInfoPanel (0x103961e4) is defined in OnlineParkInfoPanelMsg.cpp.
CFSOnlinePark gOnlineParkInfoPark;                                       // 0x10396118
void (*gOnlineParkInfoCancel)() = nullptr;                               // 0x10396114
void (*gOnlineParkInfoDownload)(int32_t, const void*) = nullptr;        // 0x10396110
void* gOnlineParkInfoContext = nullptr;                                  // 0x1039610c

namespace {

// The literals of the string pool at 0x1024f08a (the Mac's pool offsets in the comments).
const char16_t kFmtA[] = u"%ls : %ls"; // +0x00
const char16_t kSep[] = u" : ";         // +0x14
const char16_t kFmtB[] = u"%ls : %d";  // +0x1c
const char16_t kFmtC[] = u"%ls : -";   // +0x2e
const char16_t kFmtD[] = u"%ls : ";    // +0x3e

CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    return gOnlineSystem;
}

CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    return gMasterSave;
}

// The window gets a sprite text object, font 6 and the text.
void SetSpriteText(InterfaceWindow* window, char16_t* text) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(text);
}

}  // namespace

void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(),
                               void (*onDownload)(int32_t, const void*), void* context) {
    if (gOnlineParkInfoPanel != nullptr) return;
    gOnlineParkInfoPark = *park;
    gOnlineParkInfoCancel = onCancel;
    gOnlineParkInfoDownload = onDownload;
    gOnlineParkInfoContext = context;
    int16_t shift = 0;
    if (parent == nullptr) {
        parent = gLobby->mWindow;
    } else {
        shift = 200;
    }
    int16_t z = static_cast<int16_t>(UI_GetHighestZ(nullptr, nullptr));
    gOnlineParkInfoPanel = parent->Build(gOnlineParkInfoScript, OnlineParkInfoPanelMsg);
    gOnlineParkInfoPanel->setZ(static_cast<int16_t>(z + 4));
    gOnlineParkInfoPanel->ShiftPosition(0, shift);
    InterfaceWindow* info = gOnlineParkInfoPanel->GetWindow(0xe5932);
    CFSOnlinePark* p = &gOnlineParkInfoPark;
    TbDynamicStringTemplate<char16_t> text;
    CFSOnlineCity* city = park_info_callee::ReturnCityInfo(OnlineSystem()->mCityList, p->mF48);
    if (city->mF60 != 0) {
        InterfaceWindow* window = info->GetWindow(0xe5937);
        TbDynamicStringTemplate<char16_t> themeText;
        {
            TbDynamicStringTemplate<char> themeId(OnlineSystem()->mThemeList->ReturnOnlineThemeName(p->mF4C));
            gGame.GetLocalisedThemeName(themeId, themeText);
        }
        const char16_t* name = themeText.mData;
        text.Format(kFmtA, UIMain_GetString(0x110), name);
        SetSpriteText(window, text.mData);
        gOnlineParkInfoPanel->GetWindow(0xe592f)->Show(0);
    } else {
        InterfaceWindow* window = info->GetWindow(0xe5937);
        text.Format(kFmtA, UIMain_GetString(0x113), p->mText34.mData);
        const char16_t* owner = p->mText34.mData;
        int32_t same = NS_MacDoze::_wcsicmp(online_callee::LoginCustomerName(OnlineSystem()), owner);
        gOnlineParkInfoPanel->GetWindow(0xe592f)->Show(same == 0 ? 1 : 0);
        if (MasterSave()->IsSwearFilterOn()) {
            std::u16string copy(text.mData);
            reinterpret_cast<SwearFilter*>(MasterSave())->Filter(copy, true);
        }
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe5934);
        text = UIMain_GetString(0x115);
        text += kSep;
        {
            TbIntegerVTag first(0x10, static_cast<int32_t>(static_cast<uint16_t>(p->mFA8)));
            TbIntegerVTag second(0x11, static_cast<int32_t>(static_cast<uint16_t>(p->mFAA)));
            TbIntegerVTag third(0x12, static_cast<int32_t>(static_cast<uint16_t>(p->mFAC)));
            static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c0, &first, &second, &third);
        }
        text += gUIMainConvertBuffer;
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe5936);
        const char16_t* comment = p->mText94.mData;
        text = UIMain_GetString(0x114);
        if (wcslen(comment) == 0) comment = UIMain_GetString(0x11c);
        text += kSep;
        text += comment;
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe5933);
        text.Format(kFmtB, UIMain_GetString(0x117), p->mF50);
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe5938);
        text.Format(kFmtB, UIMain_GetString(0x118), p->mF54);
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe5939);
        TbDynamicStringTemplate<char16_t> themeText;
        {
            TbDynamicStringTemplate<char> themeId(OnlineSystem()->mThemeList->ReturnOnlineThemeName(p->mF4C));
            gGame.GetLocalisedThemeName(themeId, themeText);
        }
        const char16_t* name = themeText.mData;
        text.Format(kFmtA, UIMain_GetString(0x110), name);
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe593a);
        if (p->mFB8 == 0x7fffffff) {
            text.Format(kFmtC, UIMain_GetString(0x111));
        } else {
            text.Format(kFmtB, UIMain_GetString(0x111), p->mFB8);
        }
        SetSpriteText(window, text.mData);

        window = info->GetWindow(0xe5935);
        text.Format(kFmtD, UIMain_GetString(0x11b));
        if (p->mFBC == 0) {
            text += UIMain_GetString(0x14c);
        } else {
            text += UIMain_GetString(0x14d);
        }
        SetSpriteText(window, text.mData);
    }

    InterfaceWindow* window = gOnlineParkInfoPanel->GetWindow(0xe5930);
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(nullptr);
    window->SetFontWrap(1, 0);
    window->SetFontJust(0, 0);
    std::u16string name;
    const char16_t* second = p->mText20.mData;
    name.assign(second, wcslen(second));
    if (MasterSave()->IsSwearFilterOn()) reinterpret_cast<SwearFilter*>(MasterSave())->Filter(name, true);
    window->SetText(const_cast<char16_t*>(name.c_str()));
    const char16_t* first = p->mText0C.mData;
    name.assign(first, wcslen(first));
    if (MasterSave()->IsSwearFilterOn()) reinterpret_cast<SwearFilter*>(MasterSave())->Filter(name, true);
    InterfaceTitleTextObject* title = new InterfaceTitleTextObject(nullptr);
    gOnlineParkInfoPanel->GetWindow(0xe5931)->SetClientDrawObject(title);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    gOnlineParkInfoPanel->GetWindow(0xe5931)->SetFont(titleFont);
    gOnlineParkInfoPanel->GetWindow(0xe5931)->SetFontJust(1, 1);
    const char16_t* titleText = name.c_str();
    gOnlineParkInfoPanel->GetWindow(0xe5931)->SetText(const_cast<char16_t*>(titleText));
    UI_TITLE_COLOUR(gOnlineParkInfoPanel->GetWindow(0xe5931));
    gOnlineParkInfoPanel->CaptureKb(0);
    AdvisorResponse response;
    response.mId = 0x251;
    gLobbyScreens->mAdvisor->Add(response, 1);
}

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

// 0x101eba64, 76 bytes.
void RemoveOnlineParkInfoPanel() {
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gOnlineParkInfoPanel, 4, 0, 0);
}

// Unit v51037, main app: the buttons menu underlay's message function and its hide function (Mac file ui_onlineParkFindPanel.cpp), the static
// initialisers of the online park info and list panels, RemoveOnlineParkListPanel, and four small UIInterface functions.

// ---- the new globals (names invented): the objects the two static initialisers build and their __register_global_object records.
alignas(16) uint8_t gOnlineParkInfoStringA[64];   // 0x10396208
alignas(16) uint8_t gOnlineParkInfoNodeA[16];     // 0x1039621c
alignas(16) uint8_t gOnlineParkInfoStringB[64];   // 0x103961e8
alignas(16) uint8_t gOnlineParkInfoNodeB[16];     // 0x103961fc
alignas(16) uint8_t gOnlineParkInfoNodePark[16];  // 0x103961d8
alignas(16) uint8_t gOnlineParkInfoParkDtor[16];  // stands for the destructor word 0x10267550 (a TVector of CFSOnlinePark::~CFSOnlinePark)
namespace {
// The literals of the string pools at 0x1024f08a (+0x83, +0x91) and 0x1024f170 (+0x37, +0x45).
const char16_t kInfoTextA[] = u"upload";
const char16_t kInfoTextB[] = u"gms.dat";
}  // namespace

// 0x101ebae0 (144 bytes)
void __sinit_ui_onlineParkInfoPanel_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* a = ::new (gOnlineParkInfoStringA) TbDynamicStringTemplate<char16_t>(kInfoTextA);
    __register_global_object(a, dtor, gOnlineParkInfoNodeA);
    void* b = ::new (gOnlineParkInfoStringB) TbDynamicStringTemplate<char16_t>(kInfoTextB);
    __register_global_object(b, dtor, gOnlineParkInfoNodeB);
    CFSOnlinePark* park = ::new (gOnlineParkInfoParkStorage) CFSOnlinePark();
    __register_global_object(park, gOnlineParkInfoParkDtor, gOnlineParkInfoNodePark);
}

// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gOnlineParkInfoScript[] = {TPW_WINDOW_SCRIPT_102B0E58};    // 0x102b0e58, CreateOnlineParkInfoPanel 0x101eabb4
