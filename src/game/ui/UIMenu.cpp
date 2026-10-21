// UIMenu.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#define TPW_UIMENU_CTOR_EXTERNAL
#include "FillLenderList.h"
#include "FS_OnlineLoginPanel.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "platform/msl.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// 0x1018aa48 (232 bytes): a menu item's window handler. 0x14 (destroy) releases the string in extra parameter 1;
// 0x10006 plays the sound and tells the parent the item's id (message 0x100001); the rest goes to the default handler.
int32_t UIMenu_DefaultItemCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        UIMain_UISound(0xc1);
        int32_t id = window->GetID();
        UI_SendMessage(window->GetParent(), 0x100001, id, 0);
        return 0;
    }
    if (message < 0x10006 && message == 0x14) {
        InterfaceStringHeap::ReleaseString(static_cast<char16_t*>(window->getExtraParam(1)));
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x1018ab78 (136 bytes): on destroy (0x14) the menu (extra parameter 0, no NULL check) forgets its window (UIMenu::mWindow, the
// Mac's word 0x10; the whole host pointer, so a dialog closed later does not hand the keyboard to it), then the default handler.
int32_t UIMenu_DefaultCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) {
        static_cast<UIMenu*>(window->getExtraParam(0))->mWindow = nullptr;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.

// 0x1018ac44: word 0 the default item handler (the host keeps a handle), 4 zero, 8 -1, 0xc zero, 0x10 zero.
UIMenu::UIMenu() {
    mWindow = nullptr;
    mF00 = UIMenuHandlerHandle(UIMenu_DefaultItemCallback);
    mF04 = 0;
    mF08 = 0xffffffffu;
    mShown = 0;
}

// UIMenu::Init (main app 0x1018ad20, 260 bytes) and UIMenu::AddItem (0x1018ae78, 572 bytes; the inventory files them under ui_utils.cpp;
// unit v5163): Init makes the menu's full-screen window and hooks the handlers; AddItem adds a text button.
//
// The word at 0 (mF00) holds the item handler (a function pointer on the Mac). The host's pointers are wider than the word, so the
// word holds a handle that UIMenuHandlerHandle makes of the pointer (0 for NULL), see the end of this file.
// The callees that the host cannot call as the Mac does are the menu_callee functions below (weak: the test library defines its own).

// The callees the host cannot call as the Mac does: the program's versions here (weak), the test library's fakes in
// tests/unit_UIMenu_AddItem.cpp replace them.
namespace menu_callee {
// 0x101c7448: CoolRect::CoolRect() on the storage.
__attribute__((weak)) void RectCtor(void* mem) { new (mem) CoolRect(); }
// 0x101c74e0: CoolRect::~CoolRect() with the delete flag -1 (nothing freed).
__attribute__((weak)) void RectDtor(void* mem) { static_cast<CoolRect*>(mem)->~CoolRect(); }
// 0x1018fbe8: InterfaceTitleTextObject::InterfaceTitleTextObject(NULL).
__attribute__((weak)) void TitleTextCtor(void* mem) { new (mem) InterfaceTitleTextObject(nullptr); }
// 0x101d7b60: InterfaceStringHeap::GetAndCopy (a stand-in on the Mac side, no translation yet: the program's stand-in answers the text).
__attribute__((weak)) void* GetAndCopy(const char16_t* text) { return const_cast<char16_t*>(text); }
// The virtual call through slot 0x2c of the table at +0x118 (SetText).
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
// rand from the C library.
__attribute__((weak)) int32_t Rand() { return std::rand(); }
}  // namespace menu_callee

namespace {
using Handler = int32_t (*)(InterfaceWindow*, int32_t, int32_t, int32_t);
Handler sHandlers[64];
uint32_t sHandlerCount = 0;
char kLolight[] = "lolight";   // 0x1024db68
}  // namespace

uint32_t UIMenuHandlerHandle(int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t)) {
    if (handler == nullptr) return 0;
    for (uint32_t i = 0; i < sHandlerCount; i++)
        if (sHandlers[i] == handler) return i + 1;
    if (sHandlerCount >= 64) return 0;
    sHandlers[sHandlerCount++] = handler;
    return sHandlerCount;
}

int32_t (*UIMenuHandlerFromHandle(uint32_t handle))(InterfaceWindow*, int32_t, int32_t, int32_t) {
    return (handle != 0 && handle <= sHandlerCount) ? sHandlers[handle - 1] : nullptr;
}

void UIMenu::Init(int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t),
                  int32_t (*itemHandler)(InterfaceWindow*, int32_t, int32_t, int32_t)) {
    if (mWindow != nullptr) return;
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) {
        uint32_t first = static_cast<uint32_t>(menu_callee::Rand());
        uint32_t second = static_cast<uint32_t>(menu_callee::Rand());
        uint32_t product = second * first;
        uint32_t third = static_cast<uint32_t>(menu_callee::Rand());
        lender_callee::WindowCtor(memory, nullptr, 1, static_cast<int32_t>(third + product), 0, 0, 0x7ff, 0x5ff);
    }
    mWindow = static_cast<InterfaceWindow*>(memory);
    if (mWindow == nullptr) return;
    mWindow->SetCallbackFunction(handler);
    mWindow->SetDrawObject(DrawObjectLibrary::FindDrawObject(kLolight));
    mWindow->setExtraParam(0, this);
    if (itemHandler != nullptr) mF00 = UIMenuHandlerHandle(itemHandler);
    mWindow->Show(0);
}

int32_t UIMenu::AddItem(int32_t position, const char16_t* text, int32_t id, int32_t a, int32_t b, int32_t c, int32_t d) {
    TbRect limit = {0, 0, 0x800, 0x600};
    TbRect size = {0, 0, 0, 0};
    UI_MeasureText(text, limit, static_cast<int8_t>(a), 0, 0, 0, &size);
    alignas(CoolRect) unsigned char rectStorage[sizeof(CoolRect)];
    CoolRect* rect = reinterpret_cast<CoolRect*>(rectStorage);
    menu_callee::RectCtor(rect);
    int32_t width = static_cast<int32_t>(static_cast<uint32_t>(size.right - size.left + 1) << 11) / gUIRealScreenWidth;
    int32_t height = ((size.bottom - size.top) + 5) * 0x600 / gUIRealScreenHeight;
    mWindow->GetPosition(rect, 1);
    int32_t left = rect->mLeft;
    int32_t right = rect->mRight;
    int32_t x = left + ((right - left) >> 1) - (width >> 1);
    int32_t bottom = position + height;
    InterfaceWindow* item = mWindow->CreateObject(1, 1, id, static_cast<int16_t>(x), static_cast<int16_t>(position),
                                                  static_cast<int16_t>(x + width), static_cast<int16_t>(bottom));
    if (item != nullptr) {
        item->SetCallbackFunction(UIMenuHandlerFromHandle(mF00));
        item->setExtraParam(1, menu_callee::GetAndCopy(text));
        void* memory = lender_callee::OperatorNew(0x19c);
        if (memory != nullptr) menu_callee::TitleTextCtor(memory);
        item->SetClientDrawObject(static_cast<InterfaceDrawObject*>(memory));
        item->SetFont(GetFontTableEntry(static_cast<uint32_t>(a)));
        item->SetFontJust(1, 1);
        menu_callee::WindowSetText(item, const_cast<char16_t*>(text));
        item->SetFontColour(b, c, d, 0xff);
        mF04 = mF04 + 1;
    }
    menu_callee::RectDtor(rect);
    return bottom;
}

// UIMenu::Show 0x1018b0e0, UIMenu::Hide 0x1018b1fc, UIMenu::~UIMenu 0x1018ac8c (main app, ui_utils.cpp).

// The runtime's operator delete 0x10238218 (a weak default; the test library defines a recording fake).
__attribute__((weak)) void menu_callee::OperatorDelete(void* p) { ::operator delete(p); }

void UIMenu::Show(int32_t a) {
    if (mWindow == nullptr || mShown != 0) return;
    mShown = 1;
    mWindow->Show(1);
    int16_t z = static_cast<int16_t>(UI_GetHighestZ(nullptr, nullptr) + 1);
    if (a != 0) {
        gUIRootWindow->link(mWindow, 1, 1);
        mWindow->setZ(z);
    } else {
        UIMain_CheckPanel();
        gUIRootWindow->link(mWindow, 1, 1);
        mWindow->setZ(z);
    }
    mWindow->CaptureKb(0);
}

void UIMenu::Hide() {
    if (mShown == 0) return;
    mShown = 0;
    // The original calls Show on the window without a NULL test, then tests it for unlink.
    mWindow->Show(0);
    InterfaceWindow* window = mWindow;
    asm volatile("" : "+r"(window));   // keep the compiler from deducing non-NULL from the call above
    if (window != nullptr) window->unlink();
    mWindow->UnCaptureKb(0);
    UIMain_DecideKbCapture();
}

// The destructor with its hidden delete flag (r4, a short): the window is told 4, then the storage goes when the flag is above 0.
UIMenu* UIMenuDestruct(UIMenu* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mWindow != nullptr) UI_SendMessage(self->mWindow, 4, 0, 0);
        if (deleteFlag > 0) menu_callee::OperatorDelete(self);
    }
    return self;
}

// ui_main.cpp (main app), unit w49: UIMain_DecideKbCapture 0x1018116c (612 bytes) decides which window has the keyboard after a dialog,
// a panel or the game window has closed. With MainMenuUp 0x101fc1ec (80 bytes; the Mac file ui_mainmenu.cpp) and UIMenu::IsShown
// 0x1018b298 (8 bytes; ui_utils.cpp) and the windows' globals, which the Mac keeps in the data of those files.

int32_t UIMenu::IsShown() { return mShown; }
