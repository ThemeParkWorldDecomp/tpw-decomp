// ui_parkStatus.cpp (main app), unit v5572: CreateParkStatusPanel 0x101aca78 (3932 bytes), the park status panel's creation.
#include <algorithm>
#include <cstdint>
#include <new>

#include "FinanceGraphPanelMsg.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "WindowScripts.inc"

// Untranslated message functions and data this file needs (weak stand-ins; the real ones override them at the link).
__attribute__((weak)) int32_t ThoughtMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }      // 0x101ab968
__attribute__((weak)) int32_t GraphPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }   // 0x101ac044
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
int32_t Scale(uint32_t value) {
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
    WindowSetValue(gParkStatusPanel->GetWindow(0x4a68d0), Scale(static_cast<uint32_t>(under)));
    WindowSetValue(gParkStatusPanel->GetWindow(0x4a68cf), Scale(100u - static_cast<uint32_t>(over)));
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(199));
}
