// Unit v5210: the rest of the online park list panel (ui_onlineParkListPanel.cpp), main app:
//   OnlineParkListLCFormat 0x101ebd58 (the file's LCFormat), OnlineParkListLCPostFormat 0x101ebef4 (its LCPostFormat),
//   OnlineParkListPanelMsg 0x101ebba8 and FillParkList 0x101ec1d4.
// The names LCFormat and LCPostFormat exist at other addresses (the staff room, the outbox ...), so the two row functions carry the
// panel's name here. CreateOnlineParkListPanel (src/PublishPanelMsg.cpp) hands them to SetColumnFormatsAll.
#include <cstdint>
#include <cstring>
#include <string>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"

int32_t gOnlineParkListRowHeight;   // 0x1039622c  the row height OnlineParkListLCFormat works out once (0 until then)
int8_t gOnlineParkListRowInit;      // 0x10396228  0 until the first call has reset the height

namespace {
// std::basic_string<wchar_t> of the Metrowerks library: one pointer to the shared representation.
struct MslWideStringSlot {
    LCStringRep* mRep;
};
}  // namespace

// Weak: the program's own link may supply a better one; the test library fakes it. SwearFilter::Filter takes a std::u16string
// here, the Mac passes the library string: the characters go through a copy and back into the representation's buffer.
namespace parklist_callee {
__attribute__((weak)) bool FilterMsl(SwearFilter* filter, void* slot, bool replace) {
    LCStringRep* rep = static_cast<MslWideStringSlot*>(slot)->mRep;
    std::u16string text(rep->mData, static_cast<size_t>(rep->mLength));
    bool answer = filter->Filter(text, replace);
    std::memcpy(rep->mData, text.data(), text.size() * sizeof(char16_t));
    return answer;
}
}  // namespace parklist_callee

// 0x101ebd58, 380 bytes. The row window of the list: a window as high as the font's height scaled to the screen (+ 6, worked out on
// the first call), with the four column objects (kind 1, attributes 1, ids 2 to 5). The Mac asks the font's virtual at table
// slot +0xc for the height: GetHeight here.
InterfaceWindow* OnlineParkListLCFormat() {
    if (gOnlineParkListRowInit == 0) {
        gOnlineParkListRowHeight = 0;
        gOnlineParkListRowInit = 1;
    }
    if (gOnlineParkListRowHeight == 0) {
        InterfaceFont* font = GetFontTableEntry(7);
        int16_t height = static_cast<int16_t>(font->GetHeight());
        gOnlineParkListRowHeight = (height * 0x600) / gUIRealScreenHeight + 6;
    }
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr)
        lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gOnlineParkListRowHeight));
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    window->CreateObject(1, 1, 2, 0, 0, 0, 0);
    window->CreateObject(1, 1, 3, 0, 0, 0, 0);
    window->CreateObject(1, 1, 4, 0, 0, 0, 0);
    window->CreateObject(1, 1, 5, 0, 0, 0, 0);
    return window;
}

// 0x101ebef4, 684 bytes. Gives the row window and its four children their draw objects: a text sprite for the row and the first
// child (their text cleared), a rank text for the second (draw object, centred), a number text for the third and fourth (client draw
// object, SetValue 0 through the window's virtual at +0x118 / slot 0x24, centred). Font table entry 7 everywhere.
void OnlineParkListLCPostFormat(InterfaceWindow* row) {
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::SpriteCtor(object);
    row->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    row->SetFont(GetFontTableEntry(7));
    row->SetClientDrawText(nullptr);

    InterfaceWindow* child = row->GetChild(0);
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::SpriteCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    child->SetFont(GetFontTableEntry(7));
    child->SetClientDrawText(nullptr);

    child = child->GetNext();
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) parklist_callee::RankCtor(object);
    child->SetDrawObject(static_cast<InterfaceDrawObject*>(object));
    child->SetFont(GetFontTableEntry(7));
    child->SetFontJust(1, 1);

    child = child->GetNext();
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::NumberCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    child->SetFont(GetFontTableEntry(7));
    lender_callee::WindowSetValue(child, 0);
    child->SetFontJust(1, 1);

    child = child->GetNext();
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::NumberCtor(object);
    child->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    child->SetFont(GetFontTableEntry(7));
    lender_callee::WindowSetValue(child, 0);
    child->SetFontJust(1, 1);
}

// 0x101ebba8, 364 bytes. The list panel's messages: 0x14 forgets the panel and releases the lobby window's keyboard capture, 0x100
// with -2 and 0x1000b with 0x1b (escape) remove the panel and call the caller's close function (answer 0), 0x400 (a click in the
// list, window id a, row b) opens the info panel of the park the row holds. Everything else, and the cases that fall through,
// goes to the default window handler.
int32_t OnlineParkListPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == -2) {
            RemoveOnlineParkListPanel();
            if (gOnlineParkListClose != nullptr) gOnlineParkListClose();
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x14) {
            gOnlineParkListPanel = nullptr;
            gLobby->mWindow->CaptureKb(0);
        }
    } else if (message == 0x1000b) {
        if (a == 0x1b) {
            RemoveOnlineParkListPanel();
            if (gOnlineParkListClose != nullptr) gOnlineParkListClose();
            return 0;
        }
    } else if (message < 0x1000b) {
        if (message == 0x400) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
            if (list != nullptr && b != -1) {
                int32_t data;
                list->GetData(b, &data);
                const CFSOnlinePark* park = reinterpret_cast<const CFSOnlinePark*>(static_cast<uintptr_t>(static_cast<uint32_t>(data)));
                CreateOnlineParkInfoPanel(park, window, nullptr, GeneralParkDownloadCompleteCallback, nullptr);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

namespace {
uint32_t WideLength(const char16_t* text) {
    uint32_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}
}  // namespace

// 0x101ec1d4, 560 bytes. Empties the panel's list (window id 0x14beb) and adds a row per park: the park's name (through the swear
// filter when the master save has it on; the master save and the online system are made on first use), the theme's localised
// name, and three numbers of the park (words 0xb8, 0x50, 0x54); the park itself is the row's data. The last row asks for the
// display update (the count runs down from the list's own count: a count that does not match the nodes shifts it).
void FillParkList(TbPtrList<CFSOnlinePark>* parks) {
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOnlineParkListPanel->GetWindow(0x14beb));
    list->RemoveAllItems();
    auto* node = parks->mHead;
    int32_t count = parks->mCount;
    MslWideStringSlot text;
    online_callee::WStringCtor(&text);
    while (node != nullptr) {
        CFSOnlinePark* park = node->mData;
        node = node->mNext;
        const char16_t* name = park->mText0C.mData;
        uint32_t length = WideLength(name);
        online_callee::WStringAssign(&text, name, length);
        if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
        if (gMasterSave->IsSwearFilterOn()) {
            if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
            parklist_callee::FilterMsl(reinterpret_cast<SwearFilter*>(gMasterSave), &text, true);
        }
        UI_LCAdd item[5];
        for (UI_LCAdd& word : item) word.mRaw = 0;
        item[0].mText = text.mRep->mData;
        TbDynamicStringTemplate<char16_t> localised;
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
        {
            TbDynamicStringTemplate<char> theme(gOnlineSystem->mThemeList->ReturnOnlineThemeName(park->mF4C));
            gGame.GetLocalisedThemeName(theme, localised);
        }
        --count;
        item[1].mText = localised.mData;
        item[2].mInt = park->mFB8;
        item[3].mInt = park->mF50;
        item[4].mInt = park->mF54;
        list->AddItem(item, static_cast<int32_t>(reinterpret_cast<uintptr_t>(park)), -1, count == 0 ? 1 : 0);
    }
    list->UpdateDisplay();
    online_callee::WStringDtor(&text, -1);
}
