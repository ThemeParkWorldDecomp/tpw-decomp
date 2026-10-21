// ui_staffPanel.cpp: name proven by __sinit_ui_staffPanel_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include <cstdint>
#include <new>

// 0x101bf32c (620 bytes): the next thing of the kind the location switch chose (1 handymen, 2 mechanics, 3 guards, 4 entertainers,
// 5 researchers, 6 visitors; any other value clears the thing). After the current one; when there is none, the first.
void GetNextStaffMember() {
    UIIteratorTypes type;
    switch (gStaffPanelLocation) {
        case 1: type = kUIIterHandymen; break;
        case 2: type = kUIIterMechanics; break;
        case 3: type = kUIIterGuards; break;
        case 4: type = kUIIterEntertainers; break;
        case 5: type = kUIIterResearchers; break;
        case 6: type = kUIIterVisitors; break;
        default: gStaffPanelThing = nullptr; return;
    }
    gStaffPanelThing = UI_IterateThingNext(type, gStaffPanelThing);
    if (gStaffPanelThing != nullptr) return;
    gStaffPanelThing = UI_IterateThingNext(type, nullptr);
}

// The staff panel's status and location leaves (unit v5076): UpdateStatus 0x101bf5c4, UpdateLocator 0x101bf798, StaffLocationSwitch 0x101bf9f4,
// hackfollowmode 0x101bfb48. The thing is a staff member when its kind byte is 4 to 8 (the original tests 4, 5, 6, 8, 7 in turn).

int8_t gStaffPanelFrameGuard;   // 0x10395a2c
int32_t gStaffPanelFrame;       // 0x10395a30
int32_t gStaffPanelLocation;    // 0x10395a34

namespace {

bool IsStaffKind(const CThing* thing) {
    uint8_t kind = thing->mKind;
    return kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7;
}

// The words of other kinds of thing that the panel reads (the classes are not declared): +0x30 and +0x1d8, 32-bit words.
int32_t ThingWord(const CThing* thing, int32_t offset) {
    return *reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(thing) + offset);
}

}  // namespace

// 0x101bf5c4 (432 bytes)
void UpdateStatus() {
    if (gStaffPanelFrameGuard == 0) {
        gStaffPanelFrame = 9;
        gStaffPanelFrameGuard = 1;
    }
    CThing* thing = gStaffPanelThing;
    if (thing == nullptr) return;
    if (IsStaffKind(thing)) {
        int32_t state = reinterpret_cast<CStaff*>(thing)->GetGeneralStaffState();
        int32_t frame = UIMain_GetStaffStateFrame(state);
        if (frame != gStaffPanelFrame) {
            gStaffPanelFrame = frame;
            gStaffPanelC->GetWindow(0x148)->SetFrame(gStaffPanelFrame);
        }
    } else {
        int32_t value = ThingWord(thing, 0x30);
        if (value != 0)
            gStaffPanelC->GetWindow(0x14a)->SetValue(value - 1);
        else
            gStaffPanelC->GetWindow(0x14a)->SetValue(0x29a);
    }
}

// 0x101bf798 (464 bytes)
void UpdateLocator() {
    if (gStaffPanelThing == nullptr) return;
    InterfaceWindow* window = gStaffPanelC->GetWindow(0x147);
    CThing* thing = gStaffPanelThing;
    if (IsStaffKind(thing)) {
        window->SetText(const_cast<char16_t*>(reinterpret_cast<CStaff*>(thing)->GetName()));
    } else {
        int32_t value = ThingWord(thing, 0x1d8);
        const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[6]);
        if (value == 0) {
            table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c9);
        } else {
            TbIntegerVTag tag(0x18, value);
            table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
        }
        window->SetText(gUIMainConvertBuffer);
    }
    UpdateStatus();
    UIMain_ZoomToPeep(reinterpret_cast<CPeep*>(gStaffPanelThing));
}

// gameplay_leaves.cpp (main, unit v5L07): small leaves a park run reaches, from different Mac files: CellIsRide 0x1006d718
// (the inventory's Debug.cpp range), LocatorByeByeKid 0x101bf98c (ui_staffPanel.cpp), CObject::IsCoaster 0x10110cd8 (Object.cpp) and CAdvisor::FinalDestroy 0x1000a0ec (Advisor.cpp).

// 0x101bf98c (56 bytes): a kid leaves; when the staff panels show it, the slide-out panel 1 is closed.
void LocatorByeByeKid(CThing* thing) {
    if (gStaffPanelThing == thing) RemoveSlideoutPanel(1);
}

// 0x101bf9f4 (296 bytes)
void StaffLocationSwitch(int32_t which) {
    if (gStaffPanelLocation == 0) {
        gStaffPanelLocation = which;
        GetNextStaffMember();
        if (gStaffPanelThing != nullptr) {
            gMainPanel->link(gStaffPanelC, 1, 1);
            gStaffPanelC->Show(1);
            UpdateLocator();
            gFirstPersonFlags |= 0x1000;
        }
    } else {
        CThing* before = gStaffPanelThing;
        if (gStaffPanelLocation != which) gStaffPanelThing = nullptr;
        gStaffPanelLocation = which;
        GetNextStaffMember();
        if (gStaffPanelThing == nullptr) {
            gStaffPanelC->Show(0);
            gStaffPanelC->unlink();
            gFirstPersonFlags &= 0xffffefffu;
        } else {
            if (before == nullptr) {
                gMainPanel->link(gStaffPanelC, 1, 1);
                gStaffPanelC->Show(1);
            }
            UpdateLocator();
            gFirstPersonFlags |= 0x1000;
        }
    }
}

// 0x101bfb48 (616 bytes): zooms to the staff member and, for a staff kind with a patrol area, draws the area's rectangle (mode 0x136) in four lines.
void hackfollowmode() {
    if (gStaffPanelThing == nullptr) return;
    if ((gFirstPersonFlags & 0x1000) == 0) return;
    UIMain_ZoomToPeep(reinterpret_cast<CPeep*>(gStaffPanelThing));
    CThing* thing = gStaffPanelThing;
    if (!IsStaffKind(thing)) return;
    CStaff* staff = reinterpret_cast<CStaff*>(thing);
    if (staff->HasPatrolArea() == 0) return;
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> start;
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> end;
    staff->GetPatrolArea(&start, &end);
    uint32_t a = static_cast<uint32_t>(start.mIndex) - 1;
    uint32_t b = static_cast<uint32_t>(end.mIndex) - 1;
    int32_t x1 = static_cast<int32_t>(a & 0x7f);
    int32_t x2 = static_cast<int32_t>(b & 0x7f);
    int32_t y1 = static_cast<int32_t>(a >> 7);
    int32_t y2 = static_cast<int32_t>(b >> 7);
    const CELL_MODES mode = static_cast<CELL_MODES>(0x136);
    int32_t ex, ey, sx;

    ex = x2; ey = y1; sx = x1;
    if (x1 != x2) { sx = x1 + 1; ex = ex - 1; }
    LLCC_DrawLine(mode, sx, y1, &ex, &ey);

    ex = x2; ey = y2;
    LLCC_DrawLine(mode, x2, y1, &ex, &ey);

    ex = x2; ey = y2; sx = x1;
    if (x1 != x2) { sx = x1 + 1; ex = ex - 1; }
    LLCC_DrawLine(mode, sx, y2, &ex, &ey);

    ex = x1; ey = y1;
    LLCC_DrawLine(mode, x1, y2, &ex, &ey);
}

// ui_staffPanel_msg.cpp (main, unit w131): the staff panels' message functions: StaffPanelMsg 0x101bfdd8 (the panels 0x95c and 0x963),
// StaffLocMsg 0x101c00f0 (the location panel 0x146) and ModifiedCheckButtonsCallback 0x101bffb0 (the check button groups 0x95d). Their
// builders are in src/ui_staffPanel_init.cpp.

CThing* gStaffPanelThing;   // 0x10395a3c

// 0x101bfdd8 (416 bytes): the staff panels. A check button chosen (0x101) while the staff panel is up (gStaffPanelState not 0): the ids 0x95e
// to 0x964 (an unsigned test of a - 0x95e below 7, through a table of the Mac) switch the location panel to 1, 2, 5, 4, 3, nothing (0x963) and 6,
// each only when b is 1. On-frame (0x1e): hackfollowmode, then UpdateStatus. Destroyed (0x14): gStaffPanelA is forgotten. 0x1001d8 (the panel
// is closed): the thing is forgotten, the location panel hidden and unlinked, the state cleared, the first person flag 0x1000 cleared and the
// advisor messages 0x11f and 0xc6 cancelled. Then the default handler's answer.
int32_t StaffPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101) {
        if (gStaffPanelState != 0 && static_cast<uint32_t>(a - 0x95e) <= 6) {
            switch (a - 0x95e) {
                case 0: if (b == 1) StaffLocationSwitch(1); break;
                case 1: if (b == 1) StaffLocationSwitch(2); break;
                case 2: if (b == 1) StaffLocationSwitch(5); break;
                case 3: if (b == 1) StaffLocationSwitch(4); break;
                case 4: if (b == 1) StaffLocationSwitch(3); break;
                case 6: if (b == 1) StaffLocationSwitch(6); break;
                default: break;
            }
        }
    } else if (message == 0x1e) {
        hackfollowmode();
        UpdateStatus();
    } else if (message == 0x14) {
        gStaffPanelA = nullptr;
    } else if (message == 0x1001d8) {
        gStaffPanelThing = nullptr;
        gStaffPanelC->Show(0);
        gStaffPanelC->unlink();
        gStaffPanelState = 0;
        gFirstPersonFlags &= 0xffffefffu;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11f));
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc6));
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101bffb0 (248 bytes): the check button groups. A button (0x100) makes the group's current button that one (SetCurrent) and tells the
// PARENT a check button was chosen (0x101, a, 1); a button that is not pressed (b not 1) is first drawn pressed (state 1, redraw 1). The
// child is looked up first, and may be NULL. Answers 1; any other message goes to the default handler, whose answer this is.
int32_t ModifiedCheckButtonsCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        InterfaceWindow* child = window->GetWindow(a);
        if (b == 1) {
            static_cast<InterfaceCheckButtons*>(window)->SetCurrent(a);
        } else {
            static_cast<InterfaceCheckButtons*>(window)->SetCurrent(a);
            static_cast<InterfaceButton*>(child)->SetState(1, 1);
        }
        UI_PostMessage(window->GetParent(), 0x101, a, 1);
        return 1;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101c00f0 (468 bytes): the staff location panel (window 0x146). 0x10002 hides its child 0x149 and 0x10001 shows it; destroyed (0x14):
// gStaffPanelC is forgotten. 0x10006 with a = 0: the panel shows the staff panel's thing: the status panel of a staff member for the kinds 4 to
// 8, the visitor status panel for every other kind. Then the default handler's answer.
int32_t StaffLocMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        window->GetWindow(0x149)->Show(0);
    } else if (message == 0x14) {
        gStaffPanelC = nullptr;
    } else if (message == 0x10001) {
        window->GetWindow(0x149)->Show(1);
    } else if (message == 0x10006 && a == 0) {
        CThing* thing = gStaffPanelThing;
        const uint32_t kind = thing->mKind;
        if (kind - 4 <= 4) gStaffStatusPanel.Show(thing);
        else gVisitorStatusPanel.Show(thing);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// ui_staffPanel.cpp (main): InitStaffPanel 0x101c02fc (688 bytes) (unit w118; DeInitStaffPanel is in src/ui_staffPanel.cpp, unit w116).
//
// Builds the two staff scripts under the root window and leaves them ready for the staff screens: the staff panels 0x95c and 0x963 and
// the location panel 0x146 are taken out of the root's list of windows (unlink) and hidden. Every window global is reloaded before each use
// and nothing the builds answer is tested for NULL; gStaffPanelState is zeroed last.

int32_t gStaffPanelState;   // 0x10395a38

void InitStaffPanel() {
    gUIRootWindow->Build(gStaffPanelScriptA, nullptr);   // (the answer is dropped)
    gStaffPanelA = gUIRootWindow->GetWindow(0x95c);
    gStaffPanelB = gUIRootWindow->GetWindow(0x963);
    gStaffPanelA->SetCallbackFunction(StaffPanelMsg);
    gStaffPanelB->SetCallbackFunction(StaffPanelMsg);
    gStaffPanelA->unlink();
    gStaffPanelB->unlink();
    gStaffPanelA->setExtraParam(0, reinterpret_cast<void*>(3));
    gStaffPanelB->setExtraParam(0, reinterpret_cast<void*>(3));
    gStaffPanelA->GetWindow(0x95d)->SetCallbackFunction(ModifiedCheckButtonsCallback);
    gStaffPanelB->GetWindow(0x95d)->SetCallbackFunction(ModifiedCheckButtonsCallback);
    gStaffPanelA->Show(0);
    gStaffPanelB->Show(0);
    gStaffPanelC = gUIRootWindow->Build(gStaffPanelScriptB, StaffLocMsg);
    gStaffPanelC->setCursor(2);
    gStaffPanelC->unlink();
    // the location panel's text 0x147: its own text sprite, white, font 11, no text
    InterfaceWindow* text = gStaffPanelC->GetWindow(0x147);
    InterfaceTextObjectSprite* sprite = new (std::nothrow) InterfaceTextObjectSprite(nullptr);
    text->SetClientDrawObject(sprite);
    text->SetFontColour(255, 255, 255, 255);
    text->SetFont(GetFontTableEntry(0xb));
    text->SetClientDrawText(nullptr);
    text->Enable(0);
    gStaffPanelC->GetWindow(0x148)->Enable(0);
    // the highlight 0x149 draws the library's "hilight" object
    InterfaceWindow* highlight = gStaffPanelC->GetWindow(0x149);
    char hilight[] = "hilight";   // 0x1024e642
    InterfaceDrawObject* object = DrawObjectLibrary::FindDrawObject(hilight);
    highlight->SetDrawObject(object);
    highlight->Enable(0);
    highlight->Show(0);
    gStaffPanelC->Show(0);
    gStaffPanelState = 0;
}

// ui_staffPanel.cpp (main): DeInitStaffPanel 0x101c05d4 (148 B).

InterfaceWindow* gStaffPanelA;   // 0x10395a48
InterfaceWindow* gStaffPanelB;   // 0x10395a44
InterfaceWindow* gStaffPanelC;   // 0x10395a40
void DeInitStaffPanel() {
    if (gStaffPanelA != nullptr) delete gStaffPanelA;
    if (gStaffPanelB != nullptr) delete gStaffPanelB;
    if (gStaffPanelC != nullptr) delete gStaffPanelC;
}

// Unit v5343: ShowStaffPanel 0x101c0690 and ShowKidLocatorPanel 0x101c0a38 (the staff and kid locator slide-out panels).

namespace {

// The mouse block both functions end with: make the global mouse when there is none (mode CMMDefault, words 1E0/1E4 set to -1).
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse;
        CMMDefault* mode = new CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    int32_t current = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
    if (current == 9) {
        CMMDefault* mode = new CMMDefault;
        if (gMouse == nullptr) {
            CMouse* mouse = new CMouse;
            CMMDefault* inner = new CMMDefault;
            mouse->AssignMouseMode(inner);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
            gMouse = mouse;
        }
        gMouse->AssignMouseMode(mode);
    }
}

}  // namespace

void ShowStaffPanel(CThing* thing) {
    InterfaceWindow* panel;
    if (thing != nullptr && thing->mKind == 1) {
        panel = gStaffPanelB;
        gStaffPanelState = 2;
    } else {
        panel = gStaffPanelA;
        gStaffPanelState = 1;
    }
    LinkSlideoutPanel(panel, 0);
    InterfaceCheckButtons* buttons = static_cast<InterfaceCheckButtons*>(panel->GetWindow(0x95d));
    if (thing != nullptr) {
        gStaffPanelThing = thing;
        int32_t location = -1;
        int32_t id = 0;
        switch (thing->mKind) {
            // the jump table at 0x102ae3c4 (kinds 0, 2, 3 and above 8 fall to the common code)
            case 1: location = 6; id = 0x964; break;
            case 4: location = 2; id = 0x95f; break;
            case 5: location = 1; id = 0x95e; break;
            case 6: location = 4; id = 0x961; break;
            case 7: location = 3; id = 0x962; break;
            case 8: location = 5; id = 0x960; break;
            default: break;
        }
        if (location >= 0) {
            gStaffPanelLocation = location;
            buttons->SetCurrent(id);
        }
        gMainPanel->link(gStaffPanelC, 1, 1);
        gStaffPanelC->Show(1);
        UpdateLocator();
        gFirstPersonFlags |= 0x1000;
    } else {
        gStaffPanelThing = nullptr;
        gStaffPanelLocation = 0;
        buttons->SetCurrent(-1);
    }
    if (gStaffPanelLocation == 6) {
        gStaffPanelC->GetWindow(0x148)->Show(0);
        gStaffPanelC->GetWindow(0x14a)->Show(1);
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11f));
    } else {
        gStaffPanelC->GetWindow(0x148)->Show(1);
        gStaffPanelC->GetWindow(0x14a)->Show(0);
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc6));
    }
    UIMain_CheckPanel();
    EnsureMouse();
}

void ShowKidLocatorPanel() {
    LinkSlideoutPanel(gStaffPanelB, 0);
    gStaffPanelState = 2;
    gStaffPanelLocation = 6;
    static_cast<InterfaceCheckButtons*>(gStaffPanelB->GetWindow(0x95d))->SetCurrent(-1);
    gStaffPanelThing = nullptr;
    gStaffPanelC->GetWindow(0x148)->Show(0);
    gStaffPanelC->GetWindow(0x14a)->Show(1);
    UIMain_CheckPanel();
    EnsureMouse();
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11f));
}

// The staff, show and upgrade status panels' view-window functions and the small free functions around them (unit v51051), main app.

// The words 0x10395a4c and 0x10395a50 (names invented).
int32_t gStaffPanelWord4c;   // 0x10395a4c
int32_t gStaffPanelWord50;   // 0x10395a50

void RemoveStaffPanel() { RemoveSlideoutPanel(0); }   // 0x101c0c5c
int32_t StaffPanelUp() { return gStaffPanelState; }   // 0x101c0cac
// ---- the staff panel's free functions
void __sinit_ui_staffPanel_cpp() {   // 0x101c0cdc
    gStaffPanelWord50 = 0;
    gStaffPanelWord4c = 0x10000;
}

// The main panel's and the staff panels' window scripts (main); signed 16-bit records, in image order.

// 0x102ae164, 195 words
int16_t gStaffPanelScriptA[195] = {
    0, 1, 1, 0, 2396, 0, 474, 1159, 941, 1470,
    17, -1, -1,
    18, -1, -1,
    0, 8, 1, 0, 2397, 0, 526, 1191, 893, 1439,
    17, -1, -1,
    18, -1, -1,
    0, 2, 17, 0, 2398, 0, 546, 1207, 649, 1310,
    17, 482, 0,
    18, 482, 0,
    1, -11240, 4945,
    5,
    0, 2, 17, 0, 2399, 0, 659, 1207, 761, 1310,
    17, 483, 0,
    18, 483, 0,
    1, 22997, 13816,
    5,
    0, 2, 17, 0, 2400, 0, 715, 1320, 817, 1422,
    17, 486, 0,
    18, 486, 0,
    1, -17092, 21166,
    5,
    0, 2, 17, 0, 2401, 0, 602, 1320, 705, 1422,
    17, 484, 0,
    18, 484, 0,
    1, -21826, 30999,
    5,
    0, 2, 17, 0, 2402, 0, 771, 1207, 873, 1310,
    17, 485, 0,
    18, 485, 0,
    1, 20817, 25448,
    5,
    1, -12040, -17669,
    5,
    5,
    0, 1, 1, 0, 2403, 0, -37, 1159, 429, 1470,
    17, -1, -1,
    18, -1, -1,
    0, 8, 1, 0, 2397, 0, 12, 1191, 379, 1439,
    17, -1, -1,
    18, -1, -1,
    0, 2, 17, 0, 2404, 0, 144, 1263, 247, 1366,
    17, 489, 0,
    18, 489, 0,
    1, 10809, 14185,
    5,
    1, -12040, -17669,
    5,
    5,
    5,
};

// 0x102ae2ea, 95 words
int16_t gStaffPanelScriptB[95] = {
    0, 1, 1, 0, 326, 0, 448, 964, 1074, 1066,
    17, 552, 0,
    18, -1, -1,
    0, 1, 1, 0, 327, 0, 483, 992, 986, 1037,
    17, -1, -1,
    18, -1, -1,
    5,
    1, 15424, -25732,
    0, 6, 1, 0, 328, 0, 998, 992, 1043, 1037,
    17, -1, -1,
    18, -1, -1,
    1, -3580, -10656,
    5,
    0, 1, 1, 0, 329, 0, 483, 992, 986, 1037,
    17, -1, -1,
    18, -1, -1,
    5,
    0, 6, 1, 0, 330, 0, 988, 983, 1052, 1047,
    17, -1, -1,
    18, -1, -1,
    1, 19001, 20612,
    5,
    5,
    5,
};
