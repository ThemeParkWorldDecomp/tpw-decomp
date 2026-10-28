// ui_onlineParkFindPanel.cpp: name proven by __sinit_ui_onlineParkFindPanel_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
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
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTList.h"
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
#include "WindowScripts.inc"
#include <cstdint>
#include <new>
#include <string>

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

// 0x101e9324, 48 bytes.
void FindParksPostGetPlayerParksCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* parks) {
    if (error == 0) FillOnlineParkFindList(parks);
}

// OnlineParkFindPanelMsg 0x101e93b0 (700 bytes) and CreateOnlinePanel 0x101e8f08 (916 bytes), Mac file ui_onlineParkFindPanel.cpp.

char16_t gFindPanelPlayerName[64];              // 0x103960a4
void (*gFindPanelCloseCallback)();              // 0x103960c4
int32_t gFindPanelWord;                         // 0x103960c8

namespace {
const char kStopSearchText[] = "WL / MW : Stop the search and display whatever has been got in the search\n";   // 0x1024ef8d
}

// The park find panel's message function. 0x14: clears the word and gives the keyboard back; 0x1f: the edit box (0xe05a) gives it up;
// 0x100 with the search button (0xe059): 1 starts the search, else a debug text; with -2 closes; 0x400: the park of the list row opens its
// info panel; 0x802: presses the search button; 0x1000b with 0x1b: closes. Everything else is the default handler's.
int32_t OnlineParkFindPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    bool close = false;
    if (message == 0x100) {
        if (a == 0xe059) {
            if (b == 1) {
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();   // the original asks for 0x404 bytes
                gOnlineSystem->mParksByPlayer->GetList(gFindPanelPlayerName, FindParksPostGetPlayerParksCallbackFn, nullptr);
                ShowFindListSearching();
            } else {
                SamsUtilities::UDebug(kStopSearchText);
            }
        } else if (a < 0xe059 && a == -2) {
            close = true;
        }
    } else if (message < 0x100) {
        if (message != 0x15) {
            if (message < 0x15) {
                if (message >= 0x14) {
                    gFindPanelWord = 0;
                    gLobby->mWindow->CaptureKb(0);
                }
            } else if (message == 0x1f) {
                window->GetWindow(0xe05a)->CaptureKb(0);
            }
        }
    } else if (message == 0x802) {
        InterfaceButton* button = static_cast<InterfaceButton*>(window->GetWindow(0xe059));
        if (static_cast<int16_t>(button->GetState()) == 0) {
            UI_PostMessage(window, 0x100, 0xe059, 1);
            static_cast<InterfaceButton*>(window->GetWindow(0xe059))->SetState(1, 1);
            static_cast<InterfaceEditBox*>(window->GetWindow(a))->SelectAll();
        }
    } else if (message < 0x802) {
        if (message == 0x400) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
            if (list != nullptr && b != -1) {
                int32_t data;
                list->GetData(b, &data);
                if (data != 0 && data != -1)
                    CreateOnlineParkInfoPanel(reinterpret_cast<const CFSOnlinePark*>(static_cast<uintptr_t>(static_cast<uint32_t>(data))), window,
                                              nullptr, GeneralParkDownloadCompleteCallback, nullptr);
                return 0;
            }
        }
    } else if (message == 0x1000b && a == 0x1b) {
        close = true;
    }
    if (close) {
        RemoveOnlineParkFindPanel();
        if (gFindPanelCloseCallback != nullptr) gFindPanelCloseCallback();
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// The park find panel's list functions (ui_onlineParkFindPanel.cpp): the row maker and the attach function of its list control, and the
// function that fills the list with the parks found.

// Callees the host cannot call as the Mac does: weak definitions here for the program, strong fakes in the test library.
namespace onlineparkfind_callee {
// The Mac calls a virtual through the font entry's table (__ptr_glue); the class is not recovered, so the height getter stands in.
__attribute__((weak)) int16_t FontHeight(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }
__attribute__((weak)) CFSGlobalMasterSave* NewMasterSave() { return new CFSGlobalMasterSave(); }
__attribute__((weak)) void WStrCtor(void* mem) { new (mem) TbDynamicStringTemplate<char16_t>(); }
__attribute__((weak)) void WStrDtor(void* mem) { static_cast<TbDynamicStringTemplate<char16_t>*>(mem)->~TbDynamicStringTemplate(); }
__attribute__((weak)) void CStrCtor(void* mem, const char* text) { new (mem) TbDynamicStringTemplate<char>(text); }
__attribute__((weak)) void CStrDtor(void* mem) { static_cast<TbDynamicStringTemplate<char>*>(mem)->~TbDynamicStringTemplate(); }
}  // namespace onlineparkfind_callee

uint8_t gOnlineParkFindRowInit;     // 0x1039609e
int32_t gOnlineParkFindRowHeight;   // 0x103960a0

// 0x101e96b0 (420): the row window: a height worked out from font 7 once (the first call after the guard byte was cleared), five children.
InterfaceWindow* OnlineParkFindLCFormat() {
    if (static_cast<int8_t>(gOnlineParkFindRowInit) == 0) {
        gOnlineParkFindRowHeight = 0;
        gOnlineParkFindRowInit = 1;
    }
    if (gOnlineParkFindRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(7);
        int32_t height = onlineparkfind_callee::FontHeight(font);
        gOnlineParkFindRowHeight = (height * 0x600) / gUIRealScreenHeight + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (window != nullptr)
        lender_callee::WindowCtor(window, nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gOnlineParkFindRowHeight));
    window->CreateObject(1, 1, 2, 0, 0, 0, 0);
    window->CreateObject(1, 1, 3, 0, 0, 0, 0);
    window->CreateObject(1, 1, 4, 0, 0, 0, 0);
    window->CreateObject(1, 1, 5, 0, 0, 0, 0);
    window->CreateObject(1, 1, 6, 0, 0, 0, 0);
    return window;
}

namespace {
InterfaceDrawObject* NewSprite() {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    return static_cast<InterfaceDrawObject*>(memory);
}
InterfaceDrawObject* NewNumber() {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::NumberCtor(memory);
    return static_cast<InterfaceDrawObject*>(memory);
}
}  // namespace

// 0x101e9874 (840): the row's six windows get their draw objects and fonts (font 7): the window itself and the next two a text, the third
// a rank text, the last two numbers.
void OnlineParkFindLCPostFormat(InterfaceWindow* window) {
    window->SetClientDrawObject(NewSprite());
    window->SetFont(GetFontTableEntry(7));
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    child->SetClientDrawObject(NewSprite());
    child->SetFont(GetFontTableEntry(7));
    child->SetClientDrawText(nullptr);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetClientDrawObject(NewSprite());
    child->SetFont(GetFontTableEntry(7));
    child->SetClientDrawText(nullptr);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    void* rank = lender_callee::OperatorNew(0x19c);
    if (rank != nullptr) parklist_callee::RankCtor(rank);
    child->SetDrawObject(static_cast<InterfaceDrawObject*>(rank));
    child->SetFont(GetFontTableEntry(7));
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetClientDrawObject(NewNumber());
    child->SetFont(GetFontTableEntry(7));
    lender_callee::WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
    child = child->GetNext();
    child->SetClientDrawObject(NewNumber());
    child->SetFont(GetFontTableEntry(7));
    lender_callee::WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
}

// 0x101e9bf0 (748): empties the find panel's list (id 0xe05b) and adds a row per park (name, theme, city, three numbers; the park is the
// row's data), or one "nothing found" row when the count is 0; then the search button (0xe059) is released. Nothing happens without the panel.
void FillOnlineParkFindList(TbPtrList<CFSOnlinePark>* parks) {
    if (gOnlineParkFindPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    list->RemoveAllItems();
    auto* node = parks->mHead;
    int32_t count = parks->mCount;
    std::u16string text;
    LCWord item[6];
    if (count == 0) {
        for (LCWord& cell : item) cell.mRaw = 0;
        item[0].mText = UIMain_GetString(0x133);
        item[1].mText = gOnlineParkFindWide;
        item[2].mText = gOnlineParkFindWide;
        list->AddItem(item, 0, -1, 1);
    } else {
        while (node != nullptr) {
            CFSOnlinePark* park = node->mData;
            node = node->mNext;
            const char16_t* name = park->mText0C.mData;
            text.assign(name, std::char_traits<char16_t>::length(name));
            if (gMasterSave == nullptr) gMasterSave = onlineparkfind_callee::NewMasterSave();
            if (gMasterSave->IsSwearFilterOn()) {
                if (gMasterSave == nullptr) gMasterSave = onlineparkfind_callee::NewMasterSave();
                reinterpret_cast<SwearFilter*>(gMasterSave)->Filter(text, true);
            }
            for (LCWord& cell : item) cell.mRaw = 0;
            item[0].mText = text.c_str();
            union Wide {
                TbDynamicStringTemplate<char16_t> mString;
                Wide() {}
                ~Wide() {}
            } localised;
            union Narrow {
                TbDynamicStringTemplate<char> mString;
                Narrow() {}
                ~Narrow() {}
            } themeName;
            onlineparkfind_callee::WStrCtor(&localised.mString);
            if (gOnlineSystem == nullptr) gOnlineSystem = findpanel_callee::NewOnlineSystem();
            char* theme = gOnlineSystem->mThemeList->ReturnOnlineThemeName(park->mF4C);
            onlineparkfind_callee::CStrCtor(&themeName.mString, theme);
            gGame.GetLocalisedThemeName(themeName.mString, localised.mString);
            onlineparkfind_callee::CStrDtor(&themeName.mString);
            item[1].mText = localised.mString.mData;
            if (gOnlineSystem == nullptr) gOnlineSystem = findpanel_callee::NewOnlineSystem();
            CFSOnlineCity* city = park_info_callee::ReturnCityInfo(gOnlineSystem->mCityList, park->mF48);
            item[2].mText = city->mName;
            count--;
            item[3].mInt = park->mFB8;
            item[4].mInt = park->mF50;
            item[5].mInt = park->mF54;
            list->AddItem(item, static_cast<int32_t>(reinterpret_cast<uintptr_t>(park)), -1, count == 0 ? 1 : 0);
            onlineparkfind_callee::WStrDtor(&localised.mString);
        }
    }
    InterfaceButton* button = static_cast<InterfaceButton*>(gOnlineParkFindPanel->GetWindow(0xe059));
    button->SetState(0, 1);
}

int16_t gOnlineParkFindScript[] = {TPW_WINDOW_SCRIPT_102B0C8C};   // 0x102b0c8c: the words are the image's, generated at configure time (devtools/gen_game_tables.py, WindowScripts.inc)
// 0x1024f01e: empty wide strings, the cells of the find list point at +0 and +2 (bytes).
const char16_t gOnlineParkFindWide[4] = {0, 0, 0, 0};

#ifndef TPW_FINDPANEL_UNIT_TEST
// The callees the host cannot call as the Mac does: weak here, defined by the test library (which defines TPW_FINDPANEL_UNIT_TEST).
namespace findpanel_callee {
__attribute__((weak)) InterfaceDrawObject* NewCursorSprite() { return new InterfaceCursorPosTextObjectSprite(nullptr); }
__attribute__((weak)) CFSOnlineSystem* NewOnlineSystem() { return new CFSOnlineSystem(); }
__attribute__((weak)) const char16_t* LoginCustomerName(CFSOnlineSystem* system) { return online_callee::LoginCustomerName(system); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
}  // namespace findpanel_callee
#endif

namespace {
char kHilight[] = "hilight";   // 0x1024efd8
}  // namespace

// 0x101e9f28, 164 bytes: the find list shows the "searching" row.
void ShowFindListSearching() {
    if (gOnlineParkFindPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    list->RemoveAllItems();
    UI_LCAdd item[6] = {};
    item[0].mText = UIMain_GetString(0x134);
    item[1].mText = reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gOnlineParkFindWide) + 2);
    item[2].mText = gOnlineParkFindWide;
    list->AddItem(item, -1, -1, 1);
}

// 0x101e9ff8, 1084 bytes: builds the park find panel (once) and fills in its edit box and list.
void CreateOnlineParkFindPanel(void (*onCancel)()) {
    if (gOnlineParkFindPanel != nullptr) return;
    gOnlineParkFindPanel = gLobby->mWindow->Build(gOnlineParkFindScript, OnlineParkFindPanelMsg);
    gFindPanelCloseCallback = onCancel;
    InterfaceEditBox* edit = static_cast<InterfaceEditBox*>(gOnlineParkFindPanel->GetWindow(0xe05a));
    InterfaceDrawObject* sprite = findpanel_callee::NewCursorSprite();
    edit->SetClientDrawObject(sprite);
    edit->SetFont(GetFontTableEntry(6));
    edit->SetStaticTextArea(gFindPanelPlayerName, 0xf);
    edit->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kHilight));
    if (gOnlineSystem == nullptr) gOnlineSystem = findpanel_callee::NewOnlineSystem();
    findpanel_callee::WindowSetText(edit, const_cast<char16_t*>(findpanel_callee::LoginCustomerName(gOnlineSystem)));
    edit->CaptureKb(0);

    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    list->SetColumnFormatsAll(OnlineParkFindLCFormat, OnlineParkFindLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetColumnType(1, kLCText);
    list->SetColumnType(2, kLCText);
    list->SetColumnType(3, kLCNumber);
    list->SetColumnType(4, kLCNumber);
    list->SetColumnType(5, kLCNumber);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(kHilight));
    for (int32_t i = 0; i < 6; ++i) {
        const char16_t* text = UIMain_GetString(0x12c + i);
        InterfaceWindow* column = list->GetWindow(0x10 + i);
        MAKECOLUMNBUTTON(column, const_cast<char16_t*>(text), 8, 0xff, 0xef, 0, 0x17d + i);
    }
    UI_LCAdd item[6] = {};
    item[0].mText = UIMain_GetString(0x133);
    item[1].mText = gOnlineParkFindWide;
    item[2].mText = gOnlineParkFindWide;
    list->AddItem(item, 0, -1, 1);
    static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x12b);
    UIMain_SetTitleText(gOnlineParkFindPanel, 0xe05c, gUIMainConvertBuffer);
    AdvisorResponse response;
    response.mId = 0x254;
    gLobbyScreens->mAdvisor->Add(response, 1);
}

// Unit v5550: the online park info panel's messages and the park find panel (ui_onlineParkInfoPanel.cpp, ui_onlineParkFindPanel.cpp):
//   OnlineParkInfoPanelMsg 0x101ea7f0, GeneralParkDownloadCompleteCallback 0x101ea734, VisitParkPostDownloadCallbackFn 0x101ea684,
//   __sinit_ui_onlineParkFindPanel_cpp 0x101ea5d8, UpdateParkFindPanelProgress 0x101ea4e4, RemoveOnlineParkFindPanel 0x101ea468

void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

InterfaceWindow* gOnlineParkFindPanel = nullptr;                    // 0x103960c8
alignas(16) uint8_t gOnlineParkFindStringA[0x40];                   // 0x103960cc
alignas(16) uint8_t gOnlineParkFindNodeA[0x10];                     // 0x103960e0
alignas(16) uint8_t gOnlineParkFindStringB[0x40];                   // 0x103960ec
alignas(16) uint8_t gOnlineParkFindNodeB[0x10];                     // 0x10396100

namespace {

// The wide strings of the pool at 0x1024f01e (+0x0a, +0x4d, +0x5b).
const char16_t kProgressFormat[] = u"%d %%";
const char16_t kTextB[] = u"upload";
const char16_t kTextA[] = u"gms.dat";

}  // namespace

// 0x101ea468, 76 bytes: the advisor forgets its lines and the find panel is told to close.
void RemoveOnlineParkFindPanel() {
    gLobbyScreens->mAdvisor->ClearAll();
    UI_SendMessage(gOnlineParkFindPanel, 4, 0, 0);
}
// 0x101ea4e4, 192 bytes: when the find panel exists and its first entry is not yet a park (data -1), the entry's second column shows
// the progress text. The first column of the item is not set (the original passes whatever its stack held).
void UpdateParkFindPanelProgress(int32_t percent) {
    if (gOnlineParkFindPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkFindPanel->GetWindow(0xe05b));
    int32_t data = 0;
    list->GetData(0, &data);
    if (data == -1) {
        swprintf(gUIMainConvertBuffer, 0x3fff, kProgressFormat, percent);
        UI_LCAdd item[2];
        item[0].mRaw = 0;
        item[1].mRaw = 0;
        item[1].mText = gUIMainConvertBuffer;
        list->ChangeItem(0, item, 2, 1, -1);
    }
}

// 0x101ea5d8, 116 bytes: the two wide string globals of the file.
void __sinit_ui_onlineParkFindPanel_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* b = gOnlineParkFindStringB;
    void* a = gOnlineParkFindStringA;
    ::new (b) TbDynamicStringTemplate<char16_t>(kTextB);
    __register_global_object(b, dtor, gOnlineParkFindNodeB);
    ::new (a) TbDynamicStringTemplate<char16_t>(kTextA);
    __register_global_object(a, dtor, gOnlineParkFindNodeA);
}
