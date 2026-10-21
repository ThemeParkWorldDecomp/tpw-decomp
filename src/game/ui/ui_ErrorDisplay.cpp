// ui_ErrorDisplay.cpp: name proven by __sinit_ui_ErrorDisplay_cpp in the main application.

#include "Debug.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "MslWStringTree.h"
#include "online.h"
#include "platform/msl.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTList.h"
#include "Text.h"
#include "TPGlobals.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <cstring>
#include <new>

// Dialog (main app, SimTheme Park 2.04b3, 0x101f12c8, 328 bytes; the inventory files it under ui_ErrorDisplay.cpp): the window maker the
// game's dialogs start from (UI_DialogBox is another function). Unit w43b.

namespace {

char kDimDrawObject[] = "lolight";   // the draw object of the dimmed background of a modal dialog (0x1024f3e5)

}  // namespace

// QUIRKS: the dialog's depth is worked out after the script has been built (and the holder's z is the highest plus one, as a short); a
// modal dialog asks for the highest depth of all windows, not for the windows over the dialog's rectangle.
InterfaceWindow* Dialog(int16_t* description, InterfaceWindowCallback callback, int32_t modal) {
    InterfaceWindow* dialog = nullptr;
    InterfaceWindow* holder = gUIRootWindow->CreateObject(1, 1, 1, 0, 0, 0x7ff, 0x5ff);
    if (holder != nullptr) {
        dialog = holder->Build(description, callback);
        if (dialog != nullptr) {
            int32_t highest;
            if (modal == 0) {
                CoolRect rect;
                dialog->GetPosition(&rect, 1);
                highest = UI_GetHighestZ(nullptr, &rect);
            } else {
                highest = UI_GetHighestZ(nullptr, nullptr);
                holder->SetClientDrawObject(DrawObjectLibrary::FindDrawObject(kDimDrawObject));
            }
            holder->setZ(static_cast<int16_t>(highest + 1));
            if (callback != nullptr) dialog->SetCallbackFunction(callback);
            dialog->CaptureKb(0);
        }
    }
    return dialog;
}

// The dialog box's message functions (main app, SimTheme Park 2.04b3; the inventory files them under ui_ErrorDisplay.cpp), and the two
// small functions they call that undo UI_DialogBox's pause and key table switch-off. Unit w47: DialogBoxCallback 0x101f1520 (676 bytes),
// DefaultDialogCallback 0x101f1448 (152), UIMain_EnableAllKeyTables 0x10182304 (48) and UI_UnPauseTimerSystem 0x101d8ba0 (96; the
// inventory files it under ui_cityLobbyController.cpp).
//
// UI_DialogBox (src/ui_dialogbox.cpp) keeps in the dialog's extra parameters 0 to 2 the buttons' callbacks (NULL for none), 3 their
// argument, 4 the key table bookkeeping byte it found (-1: it left the tables alone) and 5 whether it paused the game.

namespace {

constexpr int32_t kMessageClose = 4;
constexpr int32_t kMessageDestroy = 0x14;
constexpr int32_t kMessageButton = 0x100;
constexpr int32_t kFirstButton = 0x9873c8;   // the three button windows UI_DialogBox makes have these ids, in order

// The extra parameters are the Mac's 32-bit words.
int32_t ExtraWord(InterfaceWindow* window, int16_t index) {
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(index)));
}

}  // namespace

// 0x101f1448. The listing keeps the default handler's answer in r3 (the decompiler's text says void).
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageDestroy) {
        UI_PostMessage(window->GetParent(), kMessageClose, 0, 0);
        UIMain_DecideKbCapture();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// A button message (0x100) with one of the three button ids calls that button's callback with the argument (a NULL callback counts as
// having answered 1); a nonzero answer closes the dialog (message 4) and the answer is 0. Any other id, a zero answer, and every other
// message but 0x14 go to the default handler. When the dialog is destroyed (0x14) the key tables are put back as the dialog found them
// (parameter 4: -1 leaves them, 0 enables all, anything else disables all) and, if the dialog paused the game (parameter 5), the game and
// the timers are unpaused unless the game's word at 0x3c is set already or the lobby runs; then the parent is told to close and the
// keyboard is decided again.
int32_t DialogBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == kMessageButton) {
        int16_t button = -1;
        if (a == kFirstButton + 1) {
            button = 1;
        } else if (a < kFirstButton + 1) {
            if (a > kFirstButton - 1) button = 0;
        } else if (a < kFirstButton + 3) {
            button = 2;
        }
        if (button >= 0) {
            void* argument = window->getExtraParam(3);
            dlgcallback callback = reinterpret_cast<dlgcallback>(window->getExtraParam(button));
            const int32_t answer = callback == nullptr ? 1 : callback(argument);
            if (answer != 0) {
                UI_SendMessage(window, kMessageClose, 0, 0);
                return 0;
            }
        }
    } else if (message == kMessageDestroy) {
        if (ExtraWord(window, 4) != -1) {
            if (static_cast<uint32_t>(ExtraWord(window, 4)) == 0) UIMain_EnableAllKeyTables();
            else UIMain_DisableAllKeyTables();
        }
        if (static_cast<uint32_t>(ExtraWord(window, 5)) != 0 && gGame.mF3C == 0 && gLobby == nullptr) {
            gGame.mF3C = 1;
            gGame.Unpause();
            UI_UnPauseTimerSystem();
        }
    }
    if (message == kMessageDestroy) {
        UI_PostMessage(window->GetParent(), kMessageClose, 0, 0);
        UIMain_DecideKbCapture();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void UIMain_EnableAllKeyTables();

void UI_UnPauseTimerSystem();

// ui_ErrorDisplay.cpp (main app), unit w43: UI_DialogBox 0x101f1800, 1008 bytes.

extern uint8_t gUIMainKeyTablesDisabled;

uint8_t UIMain_IsKeyTableDisabled();

void UIMain_DisableAllKeyTables();

void UI_PauseTimerSystem();

InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2,
                            dlgcallback callback2, dlgbutton button3, dlgcallback callback3, bool flag) {
    InterfaceWindow* window = Dialog(gDialogBoxWindowData, DialogBoxCallback, 1);
    if (window == nullptr) return nullptr;
    CoolRect unused;
    window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    window->SetFont(GetFontTableEntry(static_cast<uint32_t>(params.mValue8)));
    window->SetFontColour(static_cast<int8_t>(params.mRed), static_cast<int8_t>(params.mGreen),
                          static_cast<int8_t>(params.mBlue), 0xff);
    window->SetClientDrawText(const_cast<char16_t*>(params.mText));
    const dlgbutton buttons[3] = {button1, button2, button3};
    const dlgcallback callbacks[3] = {callback1, callback2, callback3};
    for (int16_t index = 0; index < 3; ++index) {
        InterfaceWindow* button = window->GetWindow(0x9873c8 + index);
        if (buttons[index] == dlgbutton(0)) {
            button->Show(0);
        } else {
            InterfaceDrawObject* draw = nullptr;
            if (buttons[index] == dlgbutton(1)) draw = DrawObjectLibrary::FindDrawObject(const_cast<char*>("b_okay"));
            else if (buttons[index] == dlgbutton(2)) draw = DrawObjectLibrary::FindDrawObject(const_cast<char*>("b_exit"));
            button->SetDrawObject(draw);
            button->Show(1);
            window->setExtraParam(index, reinterpret_cast<void*>(callbacks[index]));
        }
    }
    window->setExtraParam(3, reinterpret_cast<void*>(static_cast<intptr_t>(params.mValueC)));
    if (flag) {
        window->setExtraParam(4, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
    } else {
        window->setExtraParam(4, reinterpret_cast<void*>(static_cast<uintptr_t>(UIMain_IsKeyTableDisabled())));
        UIMain_DisableAllKeyTables();
    }
    window->setExtraParam(5, nullptr);
    if (gGame.mF3C == 1 && gLobby == nullptr) {
        gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
        gGame.mF3C = 0;
        UI_PauseTimerSystem();
        window->setExtraParam(5, reinterpret_cast<void*>(static_cast<intptr_t>(1)));
    }
    return window;
}

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

// ---- the error dialog's handler: closing it (0x14) forgets it, then the standard dialog handler
// 0x101f1c54
int32_t ErrorMessageCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gUIErrorDialog = nullptr;
    return DialogBoxCallback(window, message, a, b);
}

// UIInterface's mouse, research button, kid and staff room events (0x101f0d14..0x101f10f8) and the error dialog's queue
// (ErrorDlgCancel 0x101f1cd0, AddUIError 0x101f1df0, the block table's destructor 0x101f2520). The inventory files them under
// ui_ErrorDisplay.cpp.

// 0x101f1cd0: the dialog's button. Under the mutex, when an error waits it puts the oldest one's text into the dialog (the virtual at
// slot 0x2c of the table at +0x118) and takes it off. Answers whether nothing waited.
int32_t ErrorDlgCancel(void*) {
    int32_t empty = 1;
    gUIErrorMutex.lock();
    if (!gUIErrorQueue.empty()) {
        const errorthingy error = gUIErrorQueue.front();
        const unsigned char* text = error.mTable->GetConstString(error.mId);
        char16_t* converted = UIMain_ConvertMulti(text);
        findpanel_callee::WindowSetText(gUIErrorDialog, converted);
        gUIErrorQueue.pop_front();
        empty = 0;
    }
    gUIErrorMutex.unlock();
    return empty;
}

// 0x101f1df0: a NULL table means the text resources' table (gText + 0x558).
void AddUIError(int32_t error, TbITable* table) {
    if (table == nullptr) table = static_cast<TbITable*>(gText.mResourcePointers[3]);
    errorthingy record;
    record.mTable = table;
    record.mId = static_cast<uint32_t>(error);
    gUIErrorMutex.lock();
    gUIErrorQueue.push_back(record);
    gUIErrorMutex.unlock();
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

std::deque<errorthingy> gUIErrorQueue;
bthread_mutex gUIErrorMutex;
InterfaceWindow* gUIErrorDialog;

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect);

// ---- the error display

// The mutex and the queue are the C++ runtime's on the Mac (a CodeWarrior std::deque of 8-byte records): the host's
// std::deque stands in for the queue. The record is copied before it is popped, and its string is looked up after.
void TickUIErrors() {
    gUIErrorMutex.lock();
    if (!gUIErrorQueue.empty() && gUIErrorDialog == nullptr) {
        if (UI_GetHighestZ(nullptr, nullptr) < 0x23) {
            const errorthingy error = gUIErrorQueue.front();
            gUIErrorQueue.pop_front();
            const unsigned char* text = error.mTable->GetConstString(error.mId);
            dlgboxparams params;
            params.mText = UIMain_ConvertMulti(text);
            params.mRed = 0xff;
            params.mGreen = 0xff;
            params.mBlue = 0xff;
            params.mValue8 = 4;
            params.mValueC = 0;
            gUIErrorDialog = UI_DialogBox(params, dlgbutton(2), ErrorDlgCancel, dlgbutton(0), nullptr, dlgbutton(0), nullptr, false);
            gUIErrorDialog->SetCallbackFunction(ErrorMessageCallback);
        }
    }
    gUIErrorMutex.unlock();
}

// ui_ErrorDisplay.cpp's static initialiser (0x101f499c, unit v5134).

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

// The registered destructors' descriptors (the Mac's TVectors 0x102685e0 ~queue, 0x102686a8 ~bthread_mutex) and the chain nodes
// (0x10396470, 0x10396448; 12 bytes each). The host stands in with its own data.
uint32_t gSinitErrorQueueDtor[2];    // 0x102685e0
uint32_t gSinitErrorMutexDtor[2];    // 0x102686a8
uint32_t gSinitErrorQueueNode[3];    // 0x10396470
uint32_t gSinitErrorMutexNode[3];    // 0x10396448

// Weak stand-ins so the launcher links; the static initialiser never runs on the host (the test library defines the real fakes).
namespace errsinit_callee {
__attribute__((weak)) void DequeCtor(void*, void*) {}
__attribute__((weak)) void DequeCopy(void*, const void*) {}
__attribute__((weak)) void* BthreadMutexCtor(void* self) { return self; }
}  // namespace errsinit_callee

void __sinit_ui_ErrorDisplay_cpp() {
    uint32_t temp[7];       // r1+0x3c: the temporary deque (0x1c bytes); word 4 (+0x10) is the block table pointer
    uint32_t allocator;     // r1+0x38
    errsinit_callee::DequeCtor(temp, &allocator);
    errsinit_callee::DequeCopy(&gUIErrorQueue, temp);
    errdeque_callee::TearDown(reinterpret_cast<MslErrorDeque*>(temp));
    if (temp[4] != 0) errdeque_callee::OperatorDelete(reinterpret_cast<void*>(static_cast<uintptr_t>(temp[4])));
    __register_global_object(&gUIErrorQueue, gSinitErrorQueueDtor, gSinitErrorQueueNode);
    void* mutex = errsinit_callee::BthreadMutexCtor(&gUIErrorMutex);
    __register_global_object(mutex, gSinitErrorMutexDtor, gSinitErrorMutexNode);
}

// ui_ErrorDisplay.cpp, unit w43: dialog window script at main 0x102b1380.

int16_t gDialogBoxWindowData[] = {
    0, 1, 1, 0, 29639, 152, 500, 233, 1516, 830,   // level 0, record 0
    17, -1, -1,   // level 1, record 17
    18, -1, -1,   // level 1, record 18
    3, 610, 276, 1345, 753,   // level 1, record 3
    0, 2, 1, 0, 29640, 152, 1387, 610, 1470, 694,   // level 1, record 0
    17, -1, -1,   // level 2, record 17
    18, -1, -1,   // level 2, record 18
    5,   // level 2, record 5
    1, -22219, -10036,   // level 1, record 1
    0, 2, 1, 0, 29641, 152, 1379, 522, 1462, 605,   // level 1, record 0
    17, -1, -1,   // level 2, record 17
    18, -1, -1,   // level 2, record 18
    5,   // level 2, record 5
    0, 2, 1, 0, 29642, 152, 1379, 433, 1462, 517,   // level 1, record 0
    17, -1, -1,   // level 2, record 17
    18, -1, -1,   // level 2, record 18
    5,   // level 2, record 5
    5,   // level 1, record 5
    5,   // level 0, record 5
};
static_assert(sizeof(gDialogBoxWindowData) / sizeof(int16_t) == 77);
