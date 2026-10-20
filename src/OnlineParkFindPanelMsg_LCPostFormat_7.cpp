// The park find panel's list functions (ui_onlineParkFindPanel.cpp): the row maker and the attach function of its list control, and the
// function that fills the list with the parks found.
#include <cstdint>
#include <new>
#include <string>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "UI_Base.h"
#include "lobby.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"
#include "ui_tooltip.h"

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
