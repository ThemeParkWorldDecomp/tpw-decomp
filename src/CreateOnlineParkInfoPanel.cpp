// CreateOnlineParkInfoPanel (main app, 0x101eabb4, 3660 bytes): the panel that shows one online park (name, owner, comment, the
// numbers, the theme, the rating, whether it can be downloaded). Does nothing when the panel is open already. Otherwise it keeps
// a copy of the park and the caller's three words, builds the panel from its window script (under `parent`, shifted down 200 for
// a parent, under the lobby's window otherwise), and writes the texts into its child windows. Everything the original builds
// goes through a sprite text object per window (font 6) and TbDynamicStringTemplate scratch text; the title window gets the
// title text object (font 5, centred) and the park's name, the window 0xe5930 gets the park's second text.
#include <cstdint>
#include <string>

#include "Advisor.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

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
