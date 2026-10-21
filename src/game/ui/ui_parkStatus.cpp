// ui_parkStatus.cpp: name proven by __sinit_ui_parkStatus_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keyboard.h"
#include "keytable_actions.h"
#include "LbPalette.h"
#include "LoansPanelMsg.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/msl.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"
#include "ui_Postcard.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

// ui_parkStatus.cpp (main app), unit v5572: CreateParkStatusPanel 0x101aca78 (3932 bytes), the park status panel's creation.

// Unit v51042: UVersion's destructor, the object-state text, the rank text object's Change, the lender list callbacks, the thought and
// name edit handlers (main app, the neighbours of ui_main_panel_UI_Congratulations.cpp).

uint32_t gThoughtState[2] = {};                       // 0x10394a20

// ---- weak callees (the virtuals and the runtime's delete; the test library fakes them)
namespace v51042_callee {
__attribute__((weak)) int32_t WindowGetValue(InterfaceWindow* window) { return window->GetValue(); }
}  // namespace v51042_callee

// 0x101ab968 (116 bytes). Message 0xc answers the address of the state (the Mac's pointer, cut to 32 bits here).
int32_t ThoughtMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0xc) {
        int32_t value = v51042_callee::WindowGetValue(window);
        const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[8]);   // 0x56c
        const unsigned char* text = table->GetConstString(static_cast<uint32_t>(value + 1));
        gThoughtState[1] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(UIMain_ConvertMulti(text)));
        return static_cast<int32_t>(reinterpret_cast<uintptr_t>(gThoughtState));
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// The park status panel: ParkInfoLines 0x101abb90, ParkStatusPanelMsg 0x101abce4, DoDominantThoughts 0x101aba14.

int32_t gParkStatusLastTotal;    // 0x10394a18
int32_t gParkStatusLastInPark;   // 0x10394a1c

namespace {

// A thought number 1 to 0x16 is a frame of the picture; anything else hides it.
void ShowThought(InterfaceWindow* window, uint8_t thought) {
    if (thought != 0 && thought < 0x17) {
        window->SetValue(thought - 1);
    } else {
        window->Show(0);
    }
}

// (percent << 10) / 100 by the multiply the compiler made: mulhw with 0x51eb851f, srawi 5, plus the sign.
int32_t Scale(uint32_t percent) {
    int32_t value = static_cast<int32_t>(percent << 10);
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * value) >> 32);
    int32_t shifted = high >> 5;
    return shifted + static_cast<int32_t>(static_cast<uint32_t>(shifted) >> 31);
}

}  // namespace

// 0x101aba14
void DoDominantThoughts() {
    InterfaceWindow* panel = gParkStatusPanel->GetWindow(0x4a68d1);
    if (panel == nullptr) return;
    uint8_t first, second, third;
    gGameWorld->GetParkAnalyser()->GetThreeMostDominantThoughts(first, second, third);
    ShowThought(panel->GetWindow(0x4a68d4), first);
    ShowThought(panel->GetWindow(0x4a68d3), second);
    ShowThought(panel->GetWindow(0x4a68d2), third);
}

// 0x101abb90: the selected histories of the analyser, one line each.
void ParkInfoLines(CUIGrapher& grapher) {
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    if (gParkStatusFlags & 1)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x20cc0),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[3]), 1, gParkStatusRange);
    if (gParkStatusFlags & 2)
        GraphLine(grapher, reinterpret_cast<CHistory<float, 144>*>(analyser + 0x20f10),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[2]), 1, gParkStatusRange);
    if (gParkStatusFlags & 4)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x21160),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[1]), 1, gParkStatusRange);
    if (gParkStatusFlags & 8)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x205d0),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[4]), 1, gParkStatusRange);
    if (gParkStatusFlags & 0x10)
        GraphLine(grapher, reinterpret_cast<CHistory<int32_t, 144>*>(analyser + 0x213b0),
                  *reinterpret_cast<TbColour*>(gParkStatusColours[0]), 1, gParkStatusRange);
}

// 0x101abce4
int32_t ParkStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x15:
        UIMain_SetTitleText(window, 0x4a68c5, const_cast<char16_t*>(UIMain_GetString(0x96)));
        break;
    case 0x10:
        if (a == 0x80083 && gParkStatusPanel != nullptr) {
            InterfaceWindow* panel = gParkStatusPanel->GetWindow(0x4a68c9);
            int32_t inPark = gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark();
            int32_t total = gGameWorld->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
            if (gParkStatusLastInPark != inPark) {
                gParkStatusLastInPark = inPark;
                panel->GetWindow(0x4a68cc)->SetValue(gParkStatusLastInPark);
            }
            if (gParkStatusLastTotal != total) {
                gParkStatusLastTotal = total;
                panel->GetWindow(0x4a68ca)->SetValue(gParkStatusLastTotal);
            }
            int32_t under = gGameWorld->GetParkAnalyser()->PercentageOfVisitorsWithHappinessUnder(0x21);
            int32_t over = gGameWorld->GetParkAnalyser()->PercentageOfVisitorsWithHappinessOver(0x42);
            gParkStatusPanel->GetWindow(0x4a68d0)->SetValue(Scale(static_cast<uint32_t>(under)));
            gParkStatusPanel->GetWindow(0x4a68cf)->SetValue(Scale(100u - static_cast<uint32_t>(over)));
            DoDominantThoughts();
        }
        break;
    case 5:
        RemoveParkStatusPanel();
        return 1;
    case 0x14:
        gParkStatusPanel = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(199));
        break;
    case 0x100:
        if (a == -1) {
            RemoveParkStatusPanel();
            return 0;
        }
        if (a == 0x4a68c6) {
            CreateAllKidsPanel(nullptr);
            return 0;
        }
        if (a == 0x4a68c8) {
            CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr);
            return 0;
        }
        if (a == 0x4a68c7) {
            CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr);
            return 0;
        }
        break;
    case 0x800:
        gGameWorld->GetBankAccount()->SetAdmissionFee(static_cast<uint32_t>(b));
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

namespace {

// The five histories the flag bits select (analyser offsets 0x20cc0, 0x20f10, 0x21160, 0x205d0, 0x213b0); the
// largest value over the selected ones, 0 when none is selected.
int32_t ParkStatusMaxValue() {
    int32_t value = 0;
    char* analyser = reinterpret_cast<char*>(gGameWorld->GetParkAnalyser());
    if (gParkStatusFlags & 1)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x20cc0), value, gParkStatusRange);
    if (gParkStatusFlags & 2)
        MaxAnalyserValue(reinterpret_cast<CHistory<float, 144>*>(analyser + 0x20f10), value, gParkStatusRange);
    if (gParkStatusFlags & 4)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x21160), value, gParkStatusRange);
    if (gParkStatusFlags & 8)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x205d0), value, gParkStatusRange);
    if (gParkStatusFlags & 0x10)
        MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x213b0), value, gParkStatusRange);
    return value;
}

void ParkStatusXRange() {
    switch (gParkStatusRange) {
    case 0: gParkStatusGrapher.SetXRange(0, 0xb, 0xc); break;
    case 1: gParkStatusGrapher.SetXRange(0, 0x23, 0x24); break;
    case 2: gParkStatusGrapher.SetXRange(0, 0x8f, 0x90); break;
    default: break;
    }
}

void ParkStatusRedraw(InterfaceWindow* window) {
    int32_t value = ParkStatusMaxValue();
    gParkStatusGrapher.SetYRange(0, value, 10);
    DrawMeGraph(window, gParkStatusGrapher, ParkInfoLines);
}

}  // namespace

// 0x101ac044
int32_t GraphPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CoolRect rect;
    if (message == 0x100) {
        uint32_t bit = 0;
        switch (a) {
        case 0x4a68bc: bit = 1; break;
        case 0x4a68bd: bit = 2; break;
        case 0x4a68be: bit = 4; break;
        case 0x4a68bf: bit = 8; break;
        case 0x4a68c0: bit = 0x10; break;
        default: break;
        }
        if (bit != 0) {
            if (b == 1)
                gParkStatusFlags |= bit;
            else
                gParkStatusFlags &= ~bit;
            ParkStatusRedraw(window);
        }
    } else if (message == 0x15) {
        window->GetClientRect(&rect);
        int32_t value = ParkStatusMaxValue();
        SetupGraph(gParkStatusGrapher, rect, 0, value, 10, 0, 0xb, 0xc);
        ParkStatusXRange();
        DrawMeGraph(window, gParkStatusGrapher, ParkInfoLines);
    } else if (message == 0x101) {
        bool hit = false;
        switch (a) {
        case 0x4a68c2: gParkStatusRange = 2; hit = true; break;
        case 0x4a68c3: gParkStatusRange = 0; hit = true; break;
        case 0x4a68c4: gParkStatusRange = 1; hit = true; break;
        default: break;
        }
        if (hit && b == 1) {
            ParkStatusXRange();
            ParkStatusRedraw(window);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

__attribute__((weak)) int16_t gParkStatusScript[] = {TPW_WINDOW_SCRIPT_102AB734};   // 0x102ab734: the image's words (generated, devtools/gen_game_tables.py)
__attribute__((weak)) const char gParkStatusMeshNames[] = "sad_undec.wct\0hap.wct";                 // 0x1024e180

// The callees the host cannot call as the Mac does: weak here, the test library defines its own fakes.
static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceDrawObjectSprite) <= 0x400 && sizeof(InterfaceDrawEngineHGradient) <= 0x400, "the allocation below is 0x400 at least");

namespace parkstatus_callee {
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
__attribute__((weak)) void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
__attribute__((weak)) void DrawSpriteCtor(void* mem) { new (mem) InterfaceDrawObjectSprite(nullptr); }
__attribute__((weak)) void NumberCtor(void* mem) { new (mem) InterfaceNumberTextObject(nullptr); }
__attribute__((weak)) void GradientCtor(void* mem, const char* meshName) {
    new (mem) InterfaceDrawEngineHGradient(const_cast<char*>(meshName));
}
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) {
    window->SetText(const_cast<char16_t*>(text));
}
}  // namespace parkstatus_callee

namespace {

using namespace parkstatus_callee;

// operator new(size) and the constructor when it answered something (it never answers NULL on the Mac, the test is kept).
InterfaceDrawObject* NewSprite() {
    void* mem = OperatorNew(0x19c);
    if (mem != nullptr) SpriteCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(mem));
}

InterfaceDrawObject* NewNumber() {
    void* mem = OperatorNew(0x19c);
    if (mem != nullptr) NumberCtor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceNumberTextObject*>(mem));
}

InterfaceDrawObject* NewGradient(const char* name) {
    void* mem = OperatorNew(0x124);
    if (mem != nullptr) GradientCtor(mem, name);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceDrawEngineHGradient*>(mem));
}

// The centred black text style every label gets: client draw object, justification, font 6, colour (the colour only where `colour`).
void Label(InterfaceWindow* window, bool justify) {
    window->SetClientDrawObject(NewSprite());
    if (justify) window->SetFontJust(1, 1);
    window->SetFont(GetFontTableEntry(6));
    if (justify) window->SetFontColour(0, 0, 0, 0xff);
}

// A label whose text is set through the virtual SetText (the text window first cleared).
void TextLabel(InterfaceWindow* window, int32_t id) {
    Label(window, true);
    window->SetClientDrawText(nullptr);
    WindowSetText(window, UIMain_GetString(id));
}

// The panel's value times 1024 over 100, truncated toward zero (the original's multiply-high form).
int32_t Scale_2(uint32_t value) {
    return static_cast<int32_t>(value << 10) / 100;
}

}  // namespace

// 0x101aca78
void CreateParkStatusPanel() {
    if (gParkStatusPanel != nullptr) return;
    UIMain_CheckPanel();
    SetLastPanel(2, 3);
    InterfaceWindow* built = gGameWindow->Build(gParkStatusScript, ParkStatusPanelMsg);
    if (built == nullptr) return;
    gParkStatusPanel = built;
    UIMain_SetPanel(gParkStatusPanel);
    if (gParkStatusPanel->setAnimTimer(0x80083, 1000) != 0) gParkStatusPanel->startAnimTimer(0x80083);

    InterfaceWindow* title = gParkStatusPanel->GetWindow(0x4a68ce);
    title->SetClientDrawObject(NewSprite());
    title->SetFontJust(1, 1);
    title->SetFont(GetFontTableEntry(6));
    title->SetFontColour(0, 0, 0, 0xff);
    title->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x9c)));

    InterfaceWindow* tabs = gParkStatusPanel->GetWindow(0x4a68d1);
    tabs->GetWindow(0x4a68d2)->SetCallbackFunction(ThoughtMsg);
    tabs->GetWindow(0x4a68d3)->SetCallbackFunction(ThoughtMsg);
    tabs->GetWindow(0x4a68d4)->SetCallbackFunction(ThoughtMsg);
    DoDominantThoughts();

    InterfaceWindow* graph = gParkStatusPanel->GetWindow(0x4a68bb);
    if (graph != nullptr) {
        void* mem = OperatorNew(0xdc);
        if (mem != nullptr) DrawSpriteCtor(mem);
        graph->SetClientDrawObject(static_cast<InterfaceDrawObject*>(static_cast<InterfaceDrawObjectSprite*>(mem)));
        graph->SetCallbackFunction(GraphPanelMsg);
        InterfaceWindow* ranges = graph->GetWindow(0x4a68c1);
        if (ranges != nullptr) {
            TextLabel(ranges->GetWindow(0x4a68c3), 0x16a);
            TextLabel(ranges->GetWindow(0x4a68c4), 0x16b);
            TextLabel(ranges->GetWindow(0x4a68c2), 0x16c);
            switch (gParkStatusRange) {
            case 1:
                static_cast<InterfaceCheckButtons*>(ranges)->SetCurrent(0x4a68c4);
                break;
            case 0:
                static_cast<InterfaceCheckButtons*>(ranges)->SetCurrent(0x4a68c3);
                break;
            case 2:
                static_cast<InterfaceCheckButtons*>(ranges)->SetCurrent(0x4a68c2);
                break;
            default:
                break;
            }
        }
        TextLabel(graph->GetWindow(0x4a68bc), 0x98);
        TextLabel(graph->GetWindow(0x4a68bd), 0x99);
        TextLabel(graph->GetWindow(0x4a68be), 0x9a);
        TextLabel(graph->GetWindow(0x4a68bf), 0x97);
        TextLabel(graph->GetWindow(0x4a68c0), 0x9b);
        if ((gParkStatusFlags & 1) != 0) static_cast<InterfaceButton*>(graph->GetWindow(0x4a68bc))->SetState(1, 1);
        if ((gParkStatusFlags & 2) != 0) static_cast<InterfaceButton*>(graph->GetWindow(0x4a68bd))->SetState(1, 1);
        if ((gParkStatusFlags & 4) != 0) static_cast<InterfaceButton*>(graph->GetWindow(0x4a68be))->SetState(1, 1);
        if ((gParkStatusFlags & 8) != 0) static_cast<InterfaceButton*>(graph->GetWindow(0x4a68bf))->SetState(1, 1);
        if ((gParkStatusFlags & 0x10) != 0) static_cast<InterfaceButton*>(graph->GetWindow(0x4a68c0))->SetState(1, 1);
    }

    InterfaceWindow* visitors = gParkStatusPanel->GetWindow(0x4a68c9);
    InterfaceWindow* window = visitors->GetWindow(0x4a68cb);
    Label(window, false);
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x97)));
    window = visitors->GetWindow(0x4a68cc);
    window->SetClientDrawObject(NewNumber());
    window->SetFont(GetFontTableEntry(6));
    WindowSetValue(window, 0);
    window = visitors->GetWindow(0x4a68cd);
    Label(window, false);
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x9e)));
    window = visitors->GetWindow(0x4a68ca);
    window->SetClientDrawObject(NewNumber());
    window->SetFont(GetFontTableEntry(6));
    WindowSetValue(window, 0);

    gParkStatusLastInPark = -1;
    gParkStatusLastTotal = -1;
    visitors = gParkStatusPanel->GetWindow(0x4a68c9);
    int32_t inPark = gGameWorld->GetParkAnalyser()->NumberOfVisitorsInPark();
    int32_t total = gGameWorld->GetParkAnalyser()->NumberOfVisitorsSinceBeginning();
    if (gParkStatusLastInPark != inPark) {
        gParkStatusLastInPark = inPark;
        WindowSetValue(visitors->GetWindow(0x4a68cc), gParkStatusLastInPark);
    }
    if (gParkStatusLastTotal != total) {
        gParkStatusLastTotal = total;
        WindowSetValue(visitors->GetWindow(0x4a68ca), gParkStatusLastTotal);
    }

    InterfaceWindow* sad = gParkStatusPanel->GetWindow(0x4a68d0);
    sad->SetClientDrawObject(NewGradient(gParkStatusMeshNames));
    WindowSetValue(sad, 0);
    InterfaceWindow* happy = gParkStatusPanel->GetWindow(0x4a68cf);
    happy->SetDrawObject(NewGradient(gParkStatusMeshNames + 0xe));
    happy->SetClientDrawObject(NewSprite());
    happy->SetFontJust(1, 1);
    happy->SetFont(GetFontTableEntry(6));
    happy->SetFontColour(0, 0, 0, 0xff);
    happy->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x9d)));
    happy->setZ(static_cast<int16_t>(gParkStatusPanel->GetWindow(0x4a68d0)->mZ + 2));
    int32_t under = gGameWorld->GetParkAnalyser()->PercentageOfVisitorsWithHappinessUnder(0x21);
    int32_t over = gGameWorld->GetParkAnalyser()->PercentageOfVisitorsWithHappinessOver(0x42);
    WindowSetValue(gParkStatusPanel->GetWindow(0x4a68d0), Scale_2(static_cast<uint32_t>(under)));
    WindowSetValue(gParkStatusPanel->GetWindow(0x4a68cf), Scale_2(100u - static_cast<uint32_t>(over)));
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(199));
}

// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.

// 0x101ada00
void RemoveParkStatusPanel() { UI_SendMessage(gParkStatusPanel, 4, 0, 0); }

// ui_parkStatus.cpp: UI_ParkStatusPanelDateChange 0x101ada64 and the static initialiser __sinit_ui_parkStatus_cpp 0x101adbe4.

CUIGrapher gParkStatusGrapher;                  // 0x10394a28
uint8_t gParkStatusColours[5][5];               // 0x10394a6c
int32_t gParkStatusRange;                       // 0x10394a88
InterfaceWindow* gParkStatusPanel;              // 0x10394a8c
uint32_t gParkStatusFlags = 8;                  // 0x102abbac

// 0x101ada64: the Y range of the graph is the largest of the selected histories; the argument is unused.
void UI_ParkStatusPanelDateChange(int32_t month) {
    (void)month;
    if (gParkStatusPanel == nullptr) return;
    int32_t maxValue = 0;
    uint8_t* analyser = reinterpret_cast<uint8_t*>(gGameWorld->GetParkAnalyser());
    uint32_t flags = gParkStatusFlags;
    if (flags & 1) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x20cc0), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 2) MaxAnalyserValue(reinterpret_cast<CHistory<float, 144>*>(analyser + 0x20f10), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 4) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x21160), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 8) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x205d0), maxValue, gParkStatusRange);
    flags = gParkStatusFlags;
    if (flags & 0x10) MaxAnalyserValue(reinterpret_cast<CHistory<int, 144>*>(analyser + 0x213b0), maxValue, gParkStatusRange);
    gParkStatusGrapher.SetYRange(0, maxValue, 10);
    InterfaceWindow* window = gParkStatusPanel->GetWindow(0x4a68bb);
    DrawMeGraph(window, gParkStatusGrapher, ParkInfoLines);
}

static void SetColour(uint8_t* c, uint8_t r, uint8_t g, uint8_t b) {
    c[1] = r;
    c[2] = g;
    c[3] = b;
    c[0] = 0xff;
    c[4] = LbPalette_FindColour(&_lbGlobalPalette, r, g, b);
}

// 0x101adbe4: five colours (yellow, magenta, red, blue, orange) looked up in the global palette, then the grapher.
void __sinit_ui_parkStatus_cpp() {
    SetColour(gParkStatusColours[4], 0xff, 0xff, 0);
    SetColour(gParkStatusColours[3], 0xff, 0, 0xff);
    SetColour(gParkStatusColours[2], 0xff, 0, 0);
    SetColour(gParkStatusColours[1], 0, 0xbd, 0xec);
    SetColour(gParkStatusColours[0], 0xff, 0x90, 0);
    CUIGrapher_Construct(&gParkStatusGrapher);
}

// The options panel's DoScroll (0x101a8fb8) and StartScroll (0x101a96f8).

// The virtual calls through the table at +0x118 (slot 0x2c SetText, slot 0x24 SetValue).
namespace options_callee {
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace options_callee

namespace options_panel {

namespace {

// Jump table slots (id - 0x1d4d5 in DoScroll, id - 0x1d4d8 in StartScroll).
constexpr uint32_t SOUNDQ_INDEX = 11, SFX_INDEX = 3, MUSIC_INDEX = 5, SPEECH_INDEX = 7, MOVIE_INDEX = 9;
constexpr uint32_t START_SFX = 0, START_MUSIC = 2, START_SPEECH = 4;

// The shared body of the four volume sliders; answers the value after the "off" test.
int32_t Slider(int32_t id, int32_t value, int32_t textId, bool on) {
    if (!on) value = -1;
    InterfaceWindow* window = gSetOnOffRoot->GetWindow(id);
    char16_t local[10];
    if (value < 0)
        swprintf(local, 10, gOptionsPanelNumberFormats, UIMain_GetString(0x14e));
    else
        swprintf(local, 10, gOptionsPanelNumberFormats + 5, value);
    UIMain_GetString(textId);
    wcscat(gUIMainConvertBuffer, local);
    InterfaceWindow* label = window->GetWindow(0x1d4c9);
    options_callee::WindowSetText(label, gUIMainConvertBuffer);
    if (value >= 0) options_callee::WindowSetValue(window, value);
    return value;
}

}  // namespace

// 0x101a8fb8 (1824 bytes).
void DoScroll(int32_t id, int32_t value) {
    CPlayerSettings::Settings& s = gPlayerSettings.mCurrent;
    uint32_t index = static_cast<uint32_t>(id) - 0x1d4d5u;
    if (index > 11) return;
    switch (index) {
    case 0: {
        gPlayerSettings.SetResolution(static_cast<CPlayerSettings::eRESOLUTION>(
            static_cast<int32_t>(static_cast<uint32_t>(value) << 1) / 100 + 1));
        int32_t strId;
        switch (static_cast<int32_t>(s.mResolution)) {
        case 0: strId = 0x155; break;
        case 1: strId = 0x156; break;
        case 2: strId = 0x157; break;
        case 3: strId = 0x158; break;
        default: strId = 0; break;
        }
        InterfaceWindow* window = gSetOnOffRoot->GetWindow(0x1d4d5);
        window = window->GetWindow(0x1d4c9);
        if (strId > -1) {
            char16_t local[64];
            wcsncpy(local, UIMain_GetString(strId), 0x3f);
            UIMain_GetString(0x13f);
            wcscat(gUIMainConvertBuffer, local);
        } else {
            UIMain_GetString(0x13f);
        }
        options_callee::WindowSetText(window, gUIMainConvertBuffer);
        break;
    }
    case 1: {
        int32_t factor = (static_cast<int32_t>(s.mRenderer) == 0) ? 1 : 2;
        gPlayerSettings.SetGraphicsDetail(
            static_cast<CPlayerSettings::eDETAIL>(static_cast<int32_t>(static_cast<uint32_t>(value) * factor) / 100));
        int32_t strId;
        switch (static_cast<int32_t>(s.mDetail)) {
        case 0: strId = 0x150; break;
        case 1: strId = 0x151; break;
        case 2: strId = 0x152; break;
        default: strId = 0; break;
        }
        InterfaceWindow* window = gSetOnOffRoot->GetWindow(0x1d4d6);
        window = window->GetWindow(0x1d4c9);
        if (strId > -1) {
            char16_t local[64];
            wcsncpy(local, UIMain_GetString(strId), 0x3f);
            UIMain_GetString(0x13e);
            wcscat(gUIMainConvertBuffer, local);
        } else {
            UIMain_GetString(0x13e);
        }
        options_callee::WindowSetText(window, gUIMainConvertBuffer);
        break;
    }
    case SOUNDQ_INDEX: {
        gPlayerSettings.SetSoundQuality(value);
        InterfaceWindow* window = gSetOnOffRoot->GetWindow(id);
        char16_t local[10];
        if (value < 0)
            swprintf(local, 10, gOptionsPanelNumberFormats, UIMain_GetString(0x14e));
        else
            swprintf(local, 10, gOptionsPanelNumberFormats + 5, value);
        UIMain_GetString(0x140);
        wcscat(gUIMainConvertBuffer, local);
        InterfaceWindow* label = window->GetWindow(0x1d4c9);
        options_callee::WindowSetText(label, gUIMainConvertBuffer);
        if (value >= 0) options_callee::WindowSetValue(window, value);
        break;
    }
    case SFX_INDEX:
        s.mSfx.mVolume = value;
        value = Slider(id, value, 0x141, s.mSfx.mOn);
        SOUND_SetSfxVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case MUSIC_INDEX:
        s.mMusic.mVolume = value;
        value = Slider(id, value, 0x142, s.mMusic.mOn);
        SOUND_SetMusicVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case SPEECH_INDEX:
        s.mSpeech.mVolume = value;
        value = Slider(id, value, 0x143, s.mSpeech.mOn);
        SOUND_SetSpeechVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case MOVIE_INDEX:
        s.mMovie.mVolume = value;
        Slider(id, value, 0x144, s.mMovie.mOn);
        break;
    default:
        break;
    }
}

// 0x101a96f8 (236 bytes): the sound sliders' scroll start; ids 0x1d4d8 to 0x1d4e0.
void StartScroll(int32_t id, int32_t value) {
    CPlayerSettings::Settings& s = gPlayerSettings.mCurrent;
    uint32_t index = static_cast<uint32_t>(id) - 0x1d4d8u;
    if (index > 8) {
        gOptionsPanelF0C = 0;
        return;
    }
    switch (index) {
    case START_SFX:
        if (s.mSfx.mOn) SOUND_SetSfxVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case START_MUSIC:
        if (s.mMusic.mOn) SOUND_SetMusicVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case START_SPEECH:
        if (s.mSpeech.mOn) SOUND_SetSpeechVolume(value >= 0 ? static_cast<uint32_t>(value) : 0u);
        break;
    case 6:
    case 8:
        break;
    default:
        gOptionsPanelF0C = 0;
        break;
    }
}

}  // namespace options_panel

// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gOptionsPanelScript[] = {TPW_WINDOW_SCRIPT_102AADE4};      // 0x102aade4, CreateOptionsPanel 0x101a9fb8
