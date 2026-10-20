// ui_tooltip.cpp (main app, 0x1018be38..0x1018c8ec, unit w56): the tooltip of the lobby, the text that appears under the pointer over a
// bar. CUIHelpWindow is the one subclass of InterfaceHelpText (include/UI_Base.h, src/ui_helptext.cpp); its one static object is
// gUIHelpWindow. InitHelpText (include/ui_main.h) builds the text window at start-up and makes the object the help text handler; the
// interface's pump then tells it, through the handler's virtual members, when the pointer moves into a window, and it asks that window
// for its help text (message 0xc) and shows it.
#pragma once
#include <cstdint>

#include "UI_Base.h"

class InterfaceWindow;

// The Mac's object is 0x1c bytes (the base class's 0x18, see InterfaceHelpText, plus the text window at +0x18); the vtable is at
// 0x102a7de8 (the base class's slots, in the same order; this class overrides ShowHelpText, HideHelpText, the destructor, Enable,
// Disable, Update, OnMouseMovedIn, OnFrame, Init and DeInit). The class's name is the Mac's; the original names of its data are
// unknown. The base class's +8 short (mShowing) is read here as "initialised": Init sets it to 1 and DeInit to 0.
class CUIHelpWindow : public InterfaceHelpText {
public:
    // The static constructor 0x1018c8ec (__sinit_ui_tooltip_cpp, 96 bytes): the base class's constructor, the vtable, the window NULL, and
    // the registration of the destructor for the end of the program (the host's C++ does that by itself).
    CUIHelpWindow() : mHelpWindow(nullptr) {}
    void ShowHelpText() override;                      // 0x1018bf94
    void HideHelpText() override;                      // 0x1018c124
    ~CUIHelpWindow() override;                         // 0x1018bf00
    void Enable() override;                            // 0x1018c4ac
    void Disable() override;                           // 0x1018c528
    void Update() override;                            // 0x1018c5a8
    void OnMouseMovedIn(InterfaceWindow* window) override;   // 0x1018c1e4
    void OnFrame() override;                           // 0x1018c1b0
    void Init() override;                              // 0x1018c280
    void DeInit() override;                            // 0x1018c470

    InterfaceWindow* mHelpWindow;   // 0x18  the text window Init makes (under the root window); NULL before Init and after DeInit
};

// The tooltip object, constructed at start-up (0x1038cd34).
extern CUIHelpWindow gUIHelpWindow;

// 0x1018be38 (144 bytes): the tooltip window's message function. On message 0x14 (the window is being destroyed) the handler kept in
// the window's extra parameter 0 (the CUIHelpWindow) is told DeInit; then the window's default handler is given the message, and its
// answer is the answer (the Mac's r3 is left as the default handler left it).
int32_t HelpTextMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x1018c694 (228 bytes): shows `text` in the tooltip window (centred on one line when it has no new line, else centred horizontally and
// at the top), or hides the tooltip when the text is empty; nothing when the tooltip is not initialised or not enabled.
void SetHelpTextString(char16_t* text);
// 0x1018c7a4 (292 bytes): moves the tooltip window. (-1, -1) puts it back where it was made; otherwise it goes to (x, y), a coordinate of
// -1 keeping the window's own (its position less the animation's offsets). Nothing when there is no window.
void MoveHelpText(int16_t x, int16_t y);
