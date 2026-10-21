// ui_utils.cpp (main app, the inventory files UIMenu::IsShown there). PARTIAL: the main menu object (0x14 bytes on the Mac, made by
// InitialiseMainMenu and InitialiseLobbyMenu, destroyed by HideMainMenu); only the two words the keyboard decision reads are named.
#pragma once
#include <cstdint>

class InterfaceWindow;

class UIMenu {
public:
    // 0x1018b298 (src/game/ui/UIMenu.cpp): the word at 0xc.
    int32_t IsShown();
    // Unit v5230 (src/game/ui/ui_mainmenu.cpp; stand-ins elsewhere, fakes in the test).
#ifdef TPW_UIMENU_CTOR_EXTERNAL
    UIMenu();                                                                       // 0x1018ac44 (36)
#else
    // Other units' libraries (a global UIMenu) get an inline one so they link on their own; unit v5230 sees the external one.
    UIMenu() : mF00(0), mF04(0), mF08(0), mShown(0), mWindow(nullptr) {}
#endif
    void Init(int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t),
              int32_t (*itemHandler)(InterfaceWindow*, int32_t, int32_t, int32_t));   // 0x1018ad20 (260)
    // 0x1018ae78 (572): adds an item at the position, answers the position it ended at (the callers add 5 to it).
    int32_t AddItem(int32_t position, const char16_t* text, int32_t id, int32_t a, int32_t b, int32_t c, int32_t d);

    uint32_t mF00;              // 0x00  (a pointer on the Mac; not read by the translated code)
    int32_t mF04;                // 0x04  not read
    uint32_t mF08;               // 0x08  not read
    int32_t mShown;              // 0x0c  IsShown
    InterfaceWindow* mWindow;    // 0x10  the menu's window (the keyboard goes to it while the menu is up)
    // Unit v5303 (methods only, the layout is unchanged).
    void Show(int32_t a);        // 0x1018b0e0 (248)
    void Hide();                 // 0x1018b1fc (120)
};
// 0x1018ac8c (112): UIMenu::~UIMenu called with the delete flag 1 by HideMainMenu (a free function so UIMenu keeps a trivial destructor).
void DestroyUIMenu(UIMenu* menu);

// The menus' handlers (untranslated; the Mac holds each as a TVector in the TOC, the address in the comment).
int32_t MainLobbyMenuCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101fb354, TVector 0x10268608
int32_t MainGameMenuCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);    // 0x101fae30, TVector 0x10268628
int32_t MainMenuItemCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);    // 0x101fa9dc, TVector 0x10268650

// 0x101fb760 (1348) and 0x101fbcd0 (864): make gMainMenu (src/game/ui/ui_mainmenu.cpp).
void InitialiseMainMenu();
void InitialiseLobbyMenu();

// Unit v5303 (src/game/ui/ui_mainmenu.cpp): the menu's methods the two functions call (stand-ins elsewhere, fakes in the test).
// 0x10396a54 (byte, bss): set by CreateMainMenu (1 when the pause is not taken by it), read by HideMainMenu.
extern uint8_t gMainMenuPauseFlag;
// 0x101fc05c / 0x101fc140.
void CreateMainMenu(int32_t a);
void HideMainMenu();

// Unit v5163 (src/game/ui/UIMenu.cpp, src/game/ui/UI_Base.cpp): the word at 0 of the menu holds the item handler. The Mac stores the function
// pointer; the host stores a handle (0 for NULL) that these two convert.
uint32_t UIMenuHandlerHandle(int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t));
int32_t (*UIMenuHandlerFromHandle(uint32_t handle))(InterfaceWindow*, int32_t, int32_t, int32_t);
// Callees of Init and AddItem that the host cannot call as the Mac does (src/ui_main_kbcapture_UIMenu_AddItem_callees.cpp for the
// program, fakes in the test library).
namespace menu_callee {
void RectCtor(void* mem);                                  // 0x101c7448 CoolRect::CoolRect
void RectDtor(void* mem);                                  // 0x101c74e0 CoolRect::~CoolRect (flag -1)
void TitleTextCtor(void* mem);                             // 0x1018fbe8 InterfaceTitleTextObject(NULL)
void* GetAndCopy(const char16_t* text);                    // 0x101d7b60 InterfaceStringHeap::GetAndCopy
void WindowSetText(InterfaceWindow* window, char16_t* text);   // the virtual at slot 0x2c of the table at +0x118
int32_t Rand();                                            // 0x10239148 rand
}  // namespace menu_callee

// Unit v5771 (src/game/ui/UIMenu.cpp): 0x1018ac8c (112) UIMenu::~UIMenu with its hidden short delete flag
// (r4); answers this. And the runtime's operator delete 0x10238218 (weak default in the src file, a recording fake in the test).
UIMenu* UIMenuDestruct(UIMenu* self, int16_t deleteFlag);
namespace menu_callee {
void OperatorDelete(void* p);
}

// Unit v5958 (src/game/ui/ui_mainmenu.cpp): 0x101fa8d4 (212): the main menu item's fade,
// one step per frame: extra parameter 2 counts up (the colour brightens to white, then stops) or down (back to the item colour).
void itemOnFrame(InterfaceWindow* window);
