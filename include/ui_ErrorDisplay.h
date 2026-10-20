// UI_DialogBox (main app 0x101f1800; the inventory files it under ui_ErrorDisplay.cpp).
// Dialog construction is translated in src/ui_dialogbox.cpp (unit w43); Dialog and its callbacks remain untranslated.
#pragma once
#include <cstdint>

struct dlgboxparams {
    const char16_t* mText;   // 0x00
    uint8_t mRed;            // 0x04
    uint8_t mGreen;          // 0x05
    uint8_t mBlue;           // 0x06
    int32_t mValue8;         // 0x08
    int32_t mValueC;         // 0x0c
};
enum dlgbutton : int32_t {};
using dlgcallback = int32_t (*)(void*);

class InterfaceWindow;
// unit w43: the original returns the dialog window (NULL when Dialog fails).
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2,
                  dlgcallback callback2, dlgbutton button3, dlgcallback callback3, bool flag);

// unit w43
InterfaceWindow* Dialog(int16_t* script, int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t), int32_t modal);   // 0x101f12c8
int32_t DialogBoxCallback(InterfaceWindow*, int32_t, int32_t, int32_t);   // 0x101f1520 (src/ui_dialogbox_callback.cpp, unit w47)
extern int16_t gDialogBoxWindowData[];   // 0x102b1380
extern uint8_t gUIMainKeyTablesDisabled;   // 0x1038cc54
uint8_t UIMain_IsKeyTableDisabled();   // 0x101823c8
void UIMain_DisableAllKeyTables();     // 0x10182364
void UI_PauseTimerSystem();           // 0x101d8b64

// 0x101eed5c: redraws the signs of ride instance `instance` (of mesh `name` in `folder`)
// with the two texts.
int32_t RegenerateRideSign(uint32_t instance, char* folder, char* name, const char16_t* sign1, const char16_t* sign2);

// Also in this range of the inventory (0x101ef430, 0x101ef4e0).
void SetThemeTextureDirectories(int32_t small);   // 1: the small theme textures
// The folders it names: "textures" and "sharetex", or "stexture" and "ssharete" for the
// small textures. Original names unknown.
extern char gThemeTextureFolder[];    // 0x103963cc
extern char gSharedTextureFolder[];   // 0x103963bc, 16 bytes
const char* SEC_AuthorizedUser();                 // a text GAMESYSTEM_Init traces

// unit w43b (src/ui_dialog.cpp)
class InterfaceWindow;
// 0x101f12c8 (Dialog(short*, int (*)(InterfaceWindow*, long, long, long), int), file ui_ErrorDisplay.cpp): makes a full-screen holder window
// under the interface's root window, builds the window script `description` under it (InterfaceWindow::Build, with `callback` as the
// first window's message function) and answers the first window the script made, or NULL when the holder or the script made nothing.
// The holder is then put above everything: with `modal` 0 above the windows that cover the dialog's own rectangle (UI_GetHighestZ), else
// above all windows and given the draw object "lolight" as its client draw object (the dimmed background). The dialog gets `callback`
// (when not NULL) and the keyboard. (UI_DialogBox, 0x101f1800, is another function.)
InterfaceWindow* Dialog(int16_t* description, int32_t (*callback)(InterfaceWindow*, int32_t, int32_t, int32_t), int32_t modal);

// unit w47 (src/ui_dialogbox_callback.cpp)
// 0x101f1448: the message function of the dialogs that have no job of their own: message 0x14 posts message 4 to the parent and lets the
// keyboard be decided again (UIMain_DecideKbCapture); then the window's default handler, and its answer.
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
void UIMain_EnableAllKeyTables();     // 0x10182304: the six tables on again, the bookkeeping byte 0
void UI_UnPauseTimerSystem();         // 0x101d8ba0: after UI_PauseTimerSystem: every running timer starts again from now

// 0x101e0294 (64 bytes, src/ui_progress_cursor.cpp, unit w75): removes the interface's progress bar (tells the window message 4); the global
// is not cleared here. StopProcessing calls it last.
void FuckOffUIProgressBar();
extern InterfaceWindow* gUIProgressBar;   // 0x10395c6c, original name unknown: the window CreateUIProgressBar (0x101e01f0) makes
