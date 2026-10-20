// ui_main.cpp (main app): UI_Congratulations 0x1018724c, UI_Bankrupt 0x10186f34, CongratsWindowMsg 0x10186b9c and the file's static
// initialiser __sinit_ui_main_cpp 0x101877b8 (unit v5022).
#include <cstdint>
#include <new>

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "MapWho.h"
#include "TPSettings.h"
#include "TbITable.h"
#include "TbResource.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "keyboard.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_main.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

// ---- the image's data (only the first word of each is known) and the file's globals
int16_t gCongratsScript[] = {0, 1};                // 0x102a772c
int16_t gBankruptScript[] = {0, 1};                // 0x102a7756
UI_RectDelta gCongratsAnim[] = {{0x10, 0, 0, 0}};  // 0x102a79d4
UI_RectDelta gBankruptAnim[] = {{-1, -1, 0, 0}};   // 0x102a7914
int32_t gCongratsTicks;                            // 0x10380b78
int32_t gUIMainWordCCF8;                           // 0x1038ccf8
int32_t gUIMainWordCCFC;                           // 0x1038ccfc
uint8_t gUIMainNodeLight[12];                      // 0x1038ccec
uint8_t gUIMainNodeCursors[12];                    // 0x1038cc6c
uint8_t gUIMainNodeFrame[12];                      // 0x10380c08
uint8_t gUIMainNodeSlider[12];                     // 0x10380be8
uint8_t gUIMainNodePlain[12];                      // 0x10380bc8
uint8_t gUIMainNodeButton[12];                     // 0x10380ba8

void UIMain_DestroyLight(void* object) { static_cast<CLight*>(object)->~CLight(); }
void UIMain_DestroyCursorTable(void* object) { static_cast<CursorTable*>(object)->~CursorTable(); }
void UIMain_DestroyString(void* object) {
    static_cast<TbDynamicStringTemplate<char>*>(object)->~TbDynamicStringTemplate();
}

// 0x1018724c, 436 bytes.
void UI_Congratulations(CObject* object) {
    UIMain_GetFullyOutOfFirstPerson();
    object->GoFirstPerson(0);
    DisableAllKeytables();
    gUIMainKeyTablesDisabled = 1;
    if (GameWindowToggle_Hidden() != 0) ToggleGameWindow();
    if (gUIPanelWindow != nullptr) {
        UI_SendMessage(gUIPanelWindow, 5, 0, 0);
        gUIPanelWindow = nullptr;
        UIMain_DecideKbCapture();
    }
    UI_SendMessage(gGameWindow, 6, 0, 0);
    UI_SendMessage(gFirstPersonWindow, 6, 0, 0);
    InterfaceWindow* created = gUIRootWindow->CreateObject(1, 1, 0x7472, 0, 0, 0x7ff, 0x5ff);
    InterfaceWindow* window = created->Build(gCongratsScript, nullptr);
    window->Enable(0);   // the virtual at table offset 0x1c
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gCongratsAnim, 0x23);
    created->SetCallbackFunction(CongratsWindowMsg);
    created->CaptureKb(0);
}

// 0x10186f34, 428 bytes. The callback is set before the script is built.
void UI_Bankrupt(CObject* object) {
    UIMain_GetFullyOutOfFirstPerson();
    object->GoFirstPerson(0);
    DisableAllKeytables();
    gUIMainKeyTablesDisabled = 1;
    if (GameWindowToggle_Hidden() != 0) ToggleGameWindow();
    if (gUIPanelWindow != nullptr) {
        UI_SendMessage(gUIPanelWindow, 5, 0, 0);
        gUIPanelWindow = nullptr;
        UIMain_DecideKbCapture();
    }
    UI_SendMessage(gGameWindow, 6, 0, 0);
    UI_SendMessage(gFirstPersonWindow, 6, 0, 0);
    InterfaceWindow* created = gUIRootWindow->CreateObject(1, 1, 0x7472, 0, 0, 0x7ff, 0x5ff);
    created->SetCallbackFunction(BankruptWindowMsg);
    InterfaceWindow* window = created->Build(gBankruptScript, nullptr);
    CoolRect rect;
    window->GetPosition(&rect, 1);
    window->setPivot(static_cast<int16_t>(rect.mRight >> 1), static_cast<int16_t>(rect.mBottom >> 1));
    window->startAnim(gBankruptAnim, 0x23);
    created->CaptureKb(0);
    gBankruptWindow = created;
}

// The end of the window (inlined twice in the original): the winning sequence over, the windows and key tables back, and for the easy
// game type a dialog with the interface string 0x1d7. The string is converted into gUIMainConvertBuffer when the converter (resource 1)
// is loaded; the dialog's text is that buffer, or NULL when the table has no such string.
static void CongratsClose(InterfaceWindow* window) {
    UI_SendMessage(window, 4, 0, 0);
    gGameWorld->EndWinningSequence();
    UI_SendMessage(gFirstPersonWindow, 6, 0, 0);
    UI_SendMessage(gGameWindow, 6, 1, 0);
    EnableAllKeytables();
    gUIMainKeyTablesDisabled = 0;
    UIMain_DecideKbCapture();
    if (TheGameType().mType == kGameTypeEasy) {
        const unsigned char* string = static_cast<TbITable*>(gText.mResourcePointers[6])->GetConstString(0x1d7);
        char16_t* text = gUIMainConvertBuffer;
        if (string == nullptr) {
            text = nullptr;
        } else if (gText.mResourcePointers[1] != nullptr) {
            static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0])->ConvertString(gUIMainConvertBuffer, string, 0x200);
        }
        dlgboxparams params;
        params.mText = text;
        params.mRed = 0xff;
        params.mGreen = 0xff;
        params.mBlue = 0xff;
        params.mValue8 = 4;
        params.mValueC = 0;
        UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(0), nullptr, static_cast<dlgbutton>(0), nullptr, false);
    }
}

// 0x10186b9c, 860 bytes.
int32_t CongratsWindowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        if (a == 1 && *reinterpret_cast<const uint8_t*>(&gPlayerSettings.mCurrent.mRMBCancelOn) != 0 &&   // a byte test (cmplwi), not a bool's low bit

            static_cast<int32_t>(IveGotTicks() - static_cast<uint32_t>(gCongratsTicks)) > 2000) {
            CongratsClose(window);
            return 0;
        }
    } else if (message < 0x10006) {
        if (message == 0x15) gCongratsTicks = static_cast<int32_t>(IveGotTicks());
    } else if (message == 0x1000b) {
        if (a == 0x1b && ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0) CongratsClose(window);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101877b8, 436 bytes.
void __sinit_ui_main_cpp() {
    gUIMainWordCCFC = 0;
    gUIMainWordCCF8 = 0x10000;
    // The interface's light: the Mac builds a CLightParallel in place (the link base first, then the light with its virtual base left
    // out), points its vtable words at the 0x10278c58 record and sets flag 2; the host builds the light.
    CLight* light = new (&gUILight) CLight();
    light->mFlags = 2;
    light->SetNormal(0.0f, 0.0f, 1.0f);
    light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    __register_global_object(light, reinterpret_cast<void*>(&UIMain_DestroyLight), gUIMainNodeLight);
    CursorTable* table = new (&gCursorTable) CursorTable();
    __register_global_object(table, reinterpret_cast<void*>(&UIMain_DestroyCursorTable), gUIMainNodeCursors);
    auto* frame = new (&gUIFrameName) TbDynamicStringTemplate<char>("!frame");
    __register_global_object(frame, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodeFrame);
    auto* slider = new (&gUISliderName) TbDynamicStringTemplate<char>("!slider");
    __register_global_object(slider, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodeSlider);
    auto* plain = new (&gUIPlainFrameName) TbDynamicStringTemplate<char>("!f_plain");
    __register_global_object(plain, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodePlain);
    auto* button = new (&gUIButtonName) TbDynamicStringTemplate<char>("!cbut");
    __register_global_object(button, reinterpret_cast<void*>(&UIMain_DestroyString), gUIMainNodeButton);
    // Last the original stores the two vtable words of gUIClockTick (the object's own construction): nothing to do here.
}
