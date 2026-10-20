// ui_mainpanel2.cpp (main): the message functions of three windows CreateMainPanel builds (unit w118), and the five of unit w131 (the toolbar's
// buttons, the happy bar, the cash window, the bobble window and the golden stuff window).
#include <cstdint>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "LLCellSystem.h"
#include "UI_Base.h"
#include "World.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "ui_main.h"

// 0x101a5b6c (160 bytes): the toolbar (window 0x1d). Message 0x14 (the window is being destroyed) clears gMainPanel, so that the next
// CreateMainPanel builds again; everything else, and then also that one, goes to the default handler, whose answer this is. (The Mac builds a
// CoolRect on the stack that nothing reads.)
int32_t MainPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gMainPanel = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a5c44 (36 bytes): the gauge window 0x1e. The default handler's answer, nothing else.
int32_t HappyGaugeCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a61d0 (36 bytes): the date window 0x20. The same.
int32_t DateWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a5a2c (256 bytes): the toolbar's buttons (window 0x25). A button message (0x100, a = the child's id) opens the panel of that button;
// every id but these six does nothing. Then the default handler, whose answer this is.
int32_t MainPanelButtonsMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        switch (a) {
            case 0x26: GotoLastUsedPanel(1); break;
            case 0x27:
                if (b == 1) ShowControlPanel();
                else RemoveControlPanel();
                break;
            case 0x28: GotoLastUsedPanel(2); break;
            case 0x29: gUI2DMap.CreateMapPanel(); break;
            case 0x2a: GotoLastUsedPanel(3); break;
            case 0x2b: CreateResearchPanel(); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a76f0 (264 bytes): the happy bar (window 0x1f). Created (0x15): its animation timer 0x80083 of two seconds, started when it was made.
// The timer firing (0x10 with a = 0x80083): when the world has a park analyser (and the window is not NULL), the bar shows the analyser's
// happiness 4 as a part of 1024: 1024 * level / 100, in single precision (fmuls, then fdivs) and truncated (a NaN or too large a number
// saturates like fctiwz). Then the default handler's answer.
int32_t HappyBarMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        if (window->setAnimTimer(0x80083, 2000) != 0) window->startAnimTimer(0x80083);
    } else if (message == 0x10 && a == 0x80083) {
        CParkAnalyser* analyser = (*gGameWorldRef).GetParkAnalyser();
        if (analyser != nullptr && window != nullptr) {
            const float level = CParkAnalyser_GetHappinessLevel(analyser, 4);
            const float scaled = (1024.0f * level) / 100.0f;
            window->SetValue(ppc_fctiwz(scaled));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int8_t gCashWindowGuard;       // 0x10394998
uint32_t gCashWindowFrames;    // 0x1039499c
int32_t gCashWindowWord;       // 0x103949ac

// 0x101a60c0 (208 bytes): the cash window (window 0x2f). Its frame counter (a function static, set to 0 the first time through) counts the
// on-frame messages (0x1e); the counter's value before the increment having bit 1 set (and bit 0 of the pair, "== 2") runs DoCashWin and
// starts the count again; every frame the session cost is set to 0. Created (0x15): the word at 0x103949ac and the counter go to 0.
int32_t CashWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (gCashWindowGuard == 0) {
        gCashWindowFrames = 0;
        gCashWindowGuard = 1;
    }
    if (message == 0x1e) {
        const uint32_t before = gCashWindowFrames;
        gCashWindowFrames = before + 1;
        if ((before & 2) == 2) {
            DoCashWin(window);
            gCashWindowFrames = 0;
        }
        LLCC_SystemSetSessionCost(0);
    } else if (message == 0x15) {
        gCashWindowWord = 0;
        gCashWindowFrames = 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101a67f4 (160 bytes): the bobble window (window 0x2e). Message 0x10006 with a = 1 posts the aerial window the message 0x100003 (a, b = 0).
int32_t BobbleCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006 && a == 1) UI_PostMessage(gAerialWindow, 0x100003, 0, 0);
    return window->CallDefaultWindowHandler(message, a, b);
}

int8_t gGoldenSpendGuard;      // 0x10394990
int32_t gGoldenLastSpending;   // 0x10394994
int8_t gGoldenKeysGuard;       // 0x10394988
int32_t gGoldenLastKeys;       // 0x1039498c

// The master save is made the first time something wants it, and every use of the original tests for it again.
static CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

// 0x101a6404 (948 bytes): the golden stuff window (window 0x33): the number of golden keys (children 0x36, which holds the text, and 0x34) and the spending power (0x37, with the text, and 0x35)
// of the master save, redrawn when they change. The two "last shown" values (-1 first, and
// when the window is created, 0x15) sit behind guard bytes that every call tests first. On-frame (0x1e): when the game is not the easy kind
// (type 2) the keys are looked at: a changed count (HowManyKeys is asked again for the store, for the test against 0 and for the text) hides
// both children at 0 and else shows both and puts "%d  x" in 0x36. The spending power is looked at whatever the game type: changed, it
// hides both children at 0, else shows both and puts it in 0x37 as text (the same "%d  x").
int32_t GoldenStuffMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (gGoldenSpendGuard == 0) {
        gGoldenLastSpending = -1;
        gGoldenSpendGuard = 1;
    }
    if (gGoldenKeysGuard == 0) {
        gGoldenLastKeys = -1;
        gGoldenKeysGuard = 1;
    }
    if (message == 0x1e) {
        char16_t text[8];
        if (TheGameType().mType != kGameTypeEasy) {
            if (gGoldenLastKeys != MasterSave()->HowManyKeys()) {
                gGoldenLastKeys = MasterSave()->HowManyKeys();
                if (MasterSave()->HowManyKeys() == 0) {
                    window->GetWindow(0x36)->Show(0);
                    window->GetWindow(0x34)->Show(0);
                } else {
                    window->GetWindow(0x36)->Show(1);
                    window->GetWindow(0x34)->Show(1);
                    swprintf(text, 8, u"%d  x", MasterSave()->HowManyKeys());
                    window->GetWindow(0x36)->SetText(text);
                }
            }
        }
        const int32_t spending = MasterSave()->SpendingPower();
        if (gGoldenLastSpending != spending) {
            gGoldenLastSpending = spending;
            if (spending == 0) {
                window->GetWindow(0x37)->Show(0);
                window->GetWindow(0x35)->Show(0);
            } else {
                window->GetWindow(0x37)->Show(1);
                window->GetWindow(0x35)->Show(1);
                swprintf(text, 8, u"%d  x", gGoldenLastSpending);
                window->GetWindow(0x37)->SetText(text);
            }
        }
    } else if (message == 0x15) {
        gGoldenLastSpending = -1;
        gGoldenLastKeys = -1;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
