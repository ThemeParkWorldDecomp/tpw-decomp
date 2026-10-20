// ShowEditParkNamePanel 0x1019deac (904 bytes) and TwoEditsMsg 0x1019da6c (1,032 bytes), unit v5057 (ui_entryPricePanel.cpp on the Mac):
// the panel with two edit boxes for the two sign names of the "gates" thing (the park's name), and its message function.
#include <cstdint>
#include <new>

#include "Cheat.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

// ---- the unit's own globals (the data is the Mac's; the strings the two cheat pointers lead to are not recovered)
InterfaceWindow* gEditParkNamePanel;      // 0x1039483c
InterfaceWindow* gParkScreenWindow;       // 0x10394848
int16_t gEditParkNameScript[2] = {0, 1};  // 0x102a9834
char16_t gEditSignBufA[15];               // 0x10394782
char16_t gEditSignBufB[15];               // 0x10394764
static const char16_t kCheatTextA[] = u"";
static const char16_t kCheatTextB[] = u"";
const char16_t* gCheatSignA = kCheatTextA;   // 0x102a9940
const char16_t* gCheatSignB = kCheatTextB;   // 0x102a9944

// The Mac's string pool at 0x1024e01c: the assert text, and 0x43 bytes on the name of the draw object of the cursor.
static char kPoolStrings[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n\0hilight";

int32_t TwoEditsMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x802) {
        if (a == 0x3943c) {
            window->GetWindow(0x3943d)->CaptureKb(0);
        } else {
            UI_PostMessage(window, 0x100, -1, 0);
        }
    } else if (message < 0x802) {
        if (message == 0x100) {
            if (a == -1) {
                uint16_t index = gGameWorld->mGatesThing;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kPoolStrings);
                CObject* object = static_cast<CObject*>(gThingArray[index].mThing);
                object->SetSignNameA(gEditSignBufA);
                object->SetSignNameB(gEditSignBufB);
                RegenerateRideSign(object, gEditSignBufA, gEditSignBufB);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
                gMasterSave->SetSignNameA(gEditSignBufA);
                if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
                gMasterSave->SetSignNameB(gEditSignBufB);
                // The texts are scrambled with their positions before the cheat compare; the length is asked again on every pass.
                for (uint32_t i = 0; i < wcslen(gEditSignBufA); i++) gEditSignBufA[i] = static_cast<char16_t>(i ^ gEditSignBufA[i]);
                for (uint32_t i = 0; i < wcslen(gEditSignBufB); i++) gEditSignBufB[i] = static_cast<char16_t>(i ^ gEditSignBufB[i]);
                if (wcscmp(gCheatSignA, gEditSignBufA) == 0 && wcscmp(gCheatSignB, gEditSignBufB) == 0) {
                    CCheat* cheat = reinterpret_cast<CCheat*>(gGameWinCheat);
                    if (gGameWinCheatGuard == 0) {
                        new (cheat) CCheat();
                        gGameWinCheatGuard = 1;
                    }
                    cheat->SetCheatStatus(true);
                }
                UI_SendMessage(window, 4, 0, 0);
                uint16_t index2 = gGameWorld->mGatesThing;
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kPoolStrings);
                char16_t* name = UIMain_GetObjectName(static_cast<CObject*>(gThingArray[index2].mThing));
                gParkScreenWindow->GetWindow(0x4f3af)->SetText(name);
                return 0;
            }
            if (a == -2) {
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
        } else if (message < 0x100 && message == 0x14) {
            UIMain_EnableAllKeyTables();
            gEditParkNamePanel = nullptr;
            UIMain_DecideKbCapture();
        }
    } else {
        if (message == 0x804) {
            UI_SendMessage(window, 4, 0, 0);
            return 0;
        }
        if (message < 0x804) {
            if (a == 0x3943c) window->GetWindow(0x3943d)->CaptureKb(0);
            else window->GetWindow(0x3943c)->CaptureKb(0);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void ShowEditParkNamePanel() {
    if (gEditParkNamePanel != nullptr) return;
    int16_t z = gParkScreenWindow->mZ;
    InterfaceWindow* panel = gParkScreenWindow->Build(gEditParkNameScript, TwoEditsMsg);
    gEditParkNamePanel = panel;
    panel->setZ(static_cast<int16_t>(z + 7));

    InterfaceDrawObject* titleObject = new InterfaceTitleTextObject(nullptr);
    panel->GetWindow(0x3943e)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    panel->GetWindow(0x3943e)->SetFont(titleFont);
    panel->GetWindow(0x3943e)->SetFontJust(1, 1);
    const char16_t* title = UIMain_GetString(0xc2);
    panel->GetWindow(0x3943e)->SetText(const_cast<char16_t*>(title));
    UI_TITLE_COLOUR(panel->GetWindow(0x3943e));

    uint16_t index = gGameWorld->mGatesThing;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kPoolStrings);
    CObject* object = static_cast<CObject*>(gThingArray[index].mThing);

    InterfaceEditBox* editA = static_cast<InterfaceEditBox*>(panel->GetWindow(0x3943c));
    editA->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editA->SetFont(GetFontTableEntry(7));
    editA->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editA->SetStaticTextArea(gEditSignBufA, 0xe);
    editA->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kPoolStrings + 0x43));
    object->GetSignNameA(gUIMainConvertBuffer, 0x3fff);
    editA->SetText(gUIMainConvertBuffer);
    editA->SelectAll();

    InterfaceEditBox* editB = static_cast<InterfaceEditBox*>(panel->GetWindow(0x3943d));
    editB->SetClientDrawObject(new InterfaceCursorPosTextObjectSprite(nullptr));
    editB->SetFont(GetFontTableEntry(7));
    editB->SetFontColour(0xff, 0xff, 0xff, 0xff);
    editB->SetStaticTextArea(gEditSignBufB, 0xe);
    editB->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(kPoolStrings + 0x43));
    object->GetSignNameB(gUIMainConvertBuffer, 0x3fff);
    editB->SetText(gUIMainConvertBuffer);

    UIMain_DisableAllKeyTables();
    panel->GetWindow(0x3943c)->CaptureKb(0);
}
