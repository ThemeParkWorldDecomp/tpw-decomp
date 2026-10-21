// ui_showStatus.cpp: name proven by __sinit_ui_showStatus_cpp in the main application.

#include "analyser.h"
#include "helpers/ui_showStatusHelpers.h"
#include "helpers/ui_staffRoomStatusHelpers.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstdint>
#include <new>

// The staff, show and upgrade status panels' view-window functions and the small free functions around them (unit v51051), main app.

void ShowStatusPanel::InitViewWindow() {   // 0x101c0d24
    UIStatusPanelFields* fields = Fields(this);
    InterfaceWindow* window = fields->mF18.get();
    if (window != nullptr) {
        window->setZ(static_cast<int16_t>(window->mZ + 0xb));
        fields->mF18.get()->Show(0);
        void* sprite = statusinit_callee::OperatorNew(0x19c);
        if (sprite != nullptr) statusinit_callee::SpriteCtor(sprite);
        fields->mF18.get()->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
        fields->mF18.get()->SetFontJust(1, 1);
        InterfaceFont* font = GetFontTableEntry(8);
        fields->mF18.get()->SetFont(font);
        fields->mF18.get()->SetFontColour(0, 0, 0, 0xff);
        fields->mF18.get()->SetClientDrawText(nullptr);
    }
}
void ShowStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }   // 0x101c0e48
void ShowStatusPanel::UpdateViewWindow() { UpdateRidePreview(this); }   // 0x101c0ea4

// 0x1024e668: "ridestatbar.wct", then at +0x10 "happygrad.wct".
const char gStatusMeshNames[] = "ridestatbar.wct\0happygrad.wct";

// The real versions (the host cannot call the Mac's new plus constructor); the test library has strong fakes.
static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceDrawEngineHGradient) <= 0x400, "the allocation below is 0x400 at least");

namespace statusinit_callee {
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
__attribute__((weak)) void SpriteCtor(void* mem) { new (mem) InterfaceTextObjectSprite(nullptr); }
__attribute__((weak)) void CashCtor(void* mem) { new (mem) InterfaceCashTextObject(nullptr); }
__attribute__((weak)) void GradientCtor(void* mem, const char* meshName) {
    new (mem) InterfaceDrawEngineHGradient(const_cast<char*>(meshName));
}
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
}  // namespace statusinit_callee

namespace {

using namespace statusinit_callee;

// A window id's text draw object, the font 6 and the string (0: no text is given, the text is NULL).
InterfaceWindow* TextChild(InterfaceWindow* parent, int32_t id, int32_t stringId, bool hasText) {
    InterfaceWindow* window = parent->GetWindow(id);
    void* object = OperatorNew(0x19c);
    if (object != nullptr) SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(hasText ? const_cast<char16_t*>(UIMain_GetString(stringId)) : nullptr);
    return window;
}

void CashChild(InterfaceWindow* parent, int32_t id) {
    InterfaceWindow* window = parent->GetWindow(id);
    void* object = OperatorNew(0x19c);
    if (object != nullptr) CashCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    WindowSetValue(window, 0);
}

void GradientChild(InterfaceWindow* parent, int32_t id, const char* mesh, int32_t value) {
    InterfaceWindow* window = parent->GetWindow(id);
    UIMain_IndentClient(window, 0, 10);
    void* object = OperatorNew(0x124);
    if (object != nullptr) GradientCtor(object, mesh);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    WindowSetValue(window, value);
}

}  // namespace

// 0x101c0f80 (2128 bytes)
void ShowStatusPanel::InitStatusWindow() {
    const char* mesh = gStatusMeshNames;
    InterfaceWindow* parent = reinterpret_cast<UIStatusPanelFields*>(this)->mF10.get();
    if (parent == nullptr) return;
    TextChild(parent, 0xa09c, 0x29, true);
    TextChild(parent, 0xa095, 0x2a, true);
    TextChild(parent, 0xa09a, 0x2b, true);
    TextChild(parent, 0xa098, 0x2c, true);
    TextChild(parent, 0xa09b, 0x2d, true);
    TextChild(parent, 0xa097, 0x2e, true);
    TextChild(parent, 0xa0a1, 0x2f, true);
    TextChild(parent, 0xa09f, 0x30, true);
    TextChild(parent, 0xa09d, 0, false);
    GradientChild(parent, 0xa096, mesh, 0x32);
    CashChild(parent, 0xa093);
    CashChild(parent, 0xa092);
    GradientChild(parent, 0xa099, mesh, 100);
    GradientChild(parent, 0xa094, mesh, 0x32);
    CashChild(parent, 0xa0a0);
    GradientChild(parent, 0xa09e, mesh + 0x10, 100);
}

void ShowStatusPanel::DeInitStatusWindow() {}   // 0x101c1808

// Unit v5547: the sideshow ("show") status panel.
//   0x101c1848 ShowStatusPanel::UpdateStatusWindow (1036 bytes): the figures of the show (gStaffRoomPanelThing) in the panel's windows
//   0x101c2508 ShowStatusPanel::ApplyChanges (132 bytes): the panel's price, chance and prize to the thing
//   0x101c261c ShowStatusPanel::applytoallcallback (168 bytes): the same for every thing of the same type
//   0x101c2a38 ShowStatusPanel::Show (80 bytes)
// The panel has the UIStatusPanelFields layout and ints at +0x24 (chance), +0x28 (prize), +0x2c (price).

CThing* gShowStatusThing;                  // 0x10395a54
int16_t gShowStatusScript[] = {TPW_WINDOW_SCRIPT_102AE3E8};   // 0x102ae3e8: the image's words (generated, devtools/gen_game_tables.py)

namespace {

struct ShowPanelFields : UIStatusPanelFields {
    int32_t mF24;   // 0x24 the chance of winning
    int32_t mF28;   // 0x28 the prize value
    int32_t mF2C;   // 0x2c the price per use
};

ShowPanelFields* Fields_2(ShowStatusPanel* panel) { return reinterpret_cast<ShowPanelFields*>(panel); }

// The runtime function at 0x10237cd8: a double cut to an integer (the low 32 bits of the 64-bit cut).
// Observed: a negative value (and NaN) answers 0; a larger one saturates like an unsigned conversion.
int32_t ToInt(double value) {
    if (!(value > 0.0)) return 0;
    if (value >= 4294967295.0) return -1;
    return static_cast<int32_t>(static_cast<uint32_t>(value));
}

// (value << 10) / 100 with a signed divide (mulhw by 0x51eb851f, srawi 5, plus the sign).
int32_t Percent(uint32_t value) { return static_cast<int32_t>(value << 10) / 100; }

void CallNotify(ShowPanelFields* f, void* self) {
    typedef uint64_t (*Fn)(void*);
    Ptr32<uint32_t> entry;
    entry.mOffset = f->mVtable.get()[0x2c / 4];
    (*reinterpret_cast<Fn*>(entry.get()))(self);
}

void Apply(CObject* object, ShowPanelFields* f) {
    object->SetPricePerUse(f->mF2C);
    if (object->GetDBInfo()->Value(kRideSlot_NonWinnableSideshow).mInteger == 0) {
        object->SetChanceOfWinning(static_cast<uint8_t>(f->mF24));
        object->SetPrizeValue(f->mF28);
    }
}

}  // namespace

void ShowStatusPanel::UpdateStatusWindow() {
    ShowPanelFields* f = Fields_2(this);
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->GetDBInfo();
    if (f->mF10.mOffset != 0) {
        InterfaceWindow* parent = f->mF10.get();
        // No test for NULL on this window: the Mac reads its table at once.
        InterfaceWindow* window = parent->GetWindow(0xa09d);
        double sum1;
        object->mHistory1A4.GetSumOfLast(sum1, 0x1e);
        ShowStatusTag tagA(0x19, ToInt(sum1));
        double sum2;
        object->mHistory22C.GetSumOfLast(sum2, 0x1e);
        int32_t walked = ToInt(sum2);
        double sum3;
        object->mHistory1A4.GetSumOfLast(sum3, 0x1e);
        ShowStatusTag tagB(0x1a, ToInt(sum3) + walked);
        TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cc, &tagA, &tagB);
        window->SetClientDrawText(gUIMainConvertBuffer);

        window = f->mF10.get()->GetWindow(0xa096);
        if (window != nullptr) window->SetValue(Percent(object->GetPercentageOfWinners()));

        window = f->mF10.get()->GetWindow(0xa093);
        if (window != nullptr) {
            double sum;
            object->mHistory6C.GetSumOfLast(sum, 0x1e);
            window->SetValue(ToInt(sum));
        }

        window = f->mF10.get()->GetWindow(0xa092);
        if (window != nullptr) window->SetValue(object->GetProfitability());

        window = f->mF10.get()->GetWindow(0xa099);
        if (window != nullptr) {
            int32_t level = object->GetProspectiveExcitementLevel(f->mF28, f->mF2C, f->mF24);
            window->SetValue(Percent(static_cast<uint32_t>(level) & 0xff));
        }

        window = f->mF10.get()->GetWindow(0xa094);
        if (window != nullptr) window->SetValue(Percent(static_cast<uint32_t>(object->GetCustomerSatisfaction())));

        // The next two windows are not tested for NULL either.
        window = f->mF10.get()->GetWindow(0xa0a0);
        window->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));

        window = f->mF10.get()->GetWindow(0xa09e);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
        cell.mIndex = static_cast<uint16_t>(object->mBase.mCellX + (object->mBase.mCellY << 7) + 1);
        int32_t happiness = gGameWorld->GetParkAnalyser()->GetAverageHappinessAroundPoint(cell, 1);
        window->SetValue(Percent(static_cast<uint32_t>(happiness)));
    }
    int32_t state = UIMain_GetObjectState(object, 2);
    if (state != f->mF04) {
        f->mF04 = state;
        CallNotify(f, this);
    }
}

namespace showstatus_callee {
__attribute__((weak)) InterfaceDrawObject* NewCashText() { return new InterfaceCashTextObject(nullptr); }
__attribute__((weak)) InterfaceDrawObject* NewTextSprite() { return new InterfaceTextObjectSprite(nullptr); }
}  // namespace showstatus_callee

namespace {

void SetUpText(InterfaceWindow* window) {
    window->SetFontJust(1, 1);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontColour(0, 0, 0, 0xff);
}

void SetUpCash(InterfaceWindow* window, int32_t maximum) {
    window->SetClientDrawObject(showstatus_callee::NewCashText());
    int32_t value = 0;
    int32_t high = maximum;
    static_cast<InterfaceVaryBox*>(window)->SetMinMaxValues(&value, &high);
    value = 1;
    static_cast<InterfaceVaryBox*>(window)->SetInc(&value);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontJust(1, 1);
    window->SetFontColour(0, 0, 0, 0xff);
}

void SetUpLabel(InterfaceWindow* window, int32_t string) {
    window->SetClientDrawObject(showstatus_callee::NewTextSprite());
    SetUpText(window);
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(string)));
}

}  // namespace

// 0x101c1c90 (1224 bytes)
void ShowStatusPanel::InitWindow() {
    UIStatusPanel* panel = reinterpret_cast<UIStatusPanel*>(this);
    InterfaceWindow* parent = reinterpret_cast<UIStatusPanelFields*>(this)->mWindow.get();
    if (panel->FillObjectList(gShowStatusThing) < 2) {
        parent->GetWindow(0xa08c)->Show(0);
        parent->GetWindow(0xa08d)->Show(0);
    }
    CoolRect rect;
    InterfaceWindow* window = parent->GetWindow(0xa08e);
    if (window != nullptr) static_cast<InterfaceScrollBar*>(window)->SetMinMax(0, 100);
    window = parent->GetWindow(0xa090);
    if (window != nullptr) SetUpCash(window, 10000);
    window = parent->GetWindow(0xa08f);
    if (window != nullptr) SetUpCash(window, 500);
    window = parent->GetWindow(0xa0a7);
    if (window != nullptr) SetUpLabel(window, 0x31);
    window = parent->GetWindow(0xa0a6);
    if (window != nullptr) SetUpLabel(window, 0x32);
    window = parent->GetWindow(0xa0a5);
    if (window != nullptr) SetUpLabel(window, 0x33);
    panel->InitEditBox(0xa0a4);
}

void ShowStatusPanel::DeInitWindow() {}         // 0x101c218c

// Unit v51007: ShowStatusPanel::UpdateWindow (0x101c21c4), __sinit_ui_upgradeStatus_cpp (0x101c6738), YearEndPanelMsg (0x101c67b4).

namespace {

// The panel's table at +0x20 holds, on the host, the arena offset of a block whose first 8 bytes are a function pointer (see CallNotify
// in ShowStatusPanel_UpdateStatusWindow.cpp). The Mac calls the entry through __ptr_glue with the panel in r3.
void* TableEntry(UIStatusPanelFields* f, uint32_t slotOffset) {
    Ptr32<uint32_t> entry;
    entry.mOffset = f->mVtable.get()[slotOffset / 4];
    return entry.get();
}

void CallVoid(UIStatusPanelFields* f, uint32_t slotOffset, void* self) {
    typedef void (*Fn)(void*);
    (*reinterpret_cast<Fn*>(TableEntry(f, slotOffset)))(self);
}

}  // namespace

void ShowStatusPanel::UpdateWindow() {
    UIStatusPanelFields* f = reinterpret_cast<UIStatusPanelFields*>(this);
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);

    typedef const char16_t* (*TextFn)(void*);
    const char16_t* answer = (*reinterpret_cast<TextFn*>(TableEntry(f, 0x3c)))(this);
    Ptr32<InterfaceWindow> window1c;
    window1c.mOffset = f->mF1C;
    parkstatus_callee::WindowSetText(window1c.get(), answer);
    CallVoid(f, 0x20, this);
    CallVoid(f, 0x1c, this);

    InterfaceWindow* parent = f->mWindow.get();
    InterfaceWindow* window = parent->GetWindow(0xa08a);
    if (window != nullptr) {
        InterfaceButton* button = static_cast<InterfaceButton*>(window);
        if (object->IsClosed()) button->SetState(1, 1);
        else button->SetState(0, 1);
    }

    window = f->mWindow.get()->GetWindow(0xa08e);
    parkstatus_callee::WindowSetValue(window, static_cast<int32_t>(object->GetChanceOfWinning()));

    window = f->mWindow.get()->GetWindow(0xa08f);
    parkstatus_callee::WindowSetValue(window, static_cast<int32_t>(object->GetPricePerUse()));

    window = f->mWindow.get()->GetWindow(0xa090);
    parkstatus_callee::WindowSetValue(window, static_cast<int32_t>(object->GetPrizeValue()));

    int32_t show = object->GetDBInfo()->Value(kRideSlot_NonWinnableSideshow).mInteger != 0 ? 0 : 1;
    f->mWindow.get()->GetWindow(0xa090)->Show(show);
    f->mWindow.get()->GetWindow(0xa08e)->Show(show);
    f->mWindow.get()->GetWindow(0xa0a6)->Show(show);
    f->mWindow.get()->GetWindow(0xa0a7)->Show(show);
    const char16_t* text = UIMain_GetString(show ? 0x33 : 0x34);
    parkstatus_callee::WindowSetText(f->mWindow.get()->GetWindow(0xa0a5), text);
}

void ShowStatusPanel::ApplyChanges() { Apply(reinterpret_cast<CObject*>(gStaffRoomPanelThing), Fields_2(this)); }

int32_t _applytoall(CThing* thing) {   // 0x101c25c0
    gShowStatusPanel.applytoallcallback(thing);
    return 1;
}

void ShowStatusPanel::applytoallcallback(CThing* thing) {
    if (reinterpret_cast<CObject*>(thing)->mObjectType == reinterpret_cast<CObject*>(gStaffRoomPanelThing)->mObjectType)
        Apply(reinterpret_cast<CObject*>(thing), Fields_2(this));
}

namespace {

// The host's virtual call on a Mac-layout panel: the table at +0x20 holds Ptr32 words, each the arena offset of a
// block whose first 8 bytes are a uint64_t (*)(void*).
void CallSlot(UIStatusPanelFields* f, uint32_t slot, void* self) {
    Ptr32<uint32_t> entry;
    entry.mOffset = f->mVtable.get()[slot / 4];
    typedef uint64_t (*Fn)(void*);
    (*reinterpret_cast<Fn*>(entry.get()))(self);
}

}  // namespace

// 0x101c2708: the show-status panel's message handler (the 0x100 command ids are 0xa089 to 0xa0a9).
int32_t ShowStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ShowStatusPanel* panel = &gShowStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    UIStatusPanelFields* f = reinterpret_cast<UIStatusPanelFields*>(panel);
    int32_t* words = reinterpret_cast<int32_t*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) panel->UpdateStatusWindow();
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd0));
    } else if (message == 0x1e) {
        UIMain_TickRidePreview();
    } else if (message == 0x100) {
        if (a == 0xa08c) {
            base->GetPreviousObject();
        } else if (a < 0xa08c) {
            if (a == 0xa089) {
                CreateAllItemsPanel(kUIObjSideshow, gStaffRoomPanelThing);
                return 0;
            }
            if (a < 0xa089) {
                if (a < 0 && a >= -2) {
                    panel->Remove();
                    return 0;
                }
            } else if (a >= 0xa08b) {
                base->EraseObject();
                return 0;
            } else {
                base->CloseObject(b == 1 ? 1 : 0);
                panel->UpdateStatusWindow();
            }
        } else {
            if (a == 0xa0a8) {
                base->Move();
                return 0;
            }
            if (a < 0xa0a8) {
                if (a < 0xa08e) base->GetNextObject();
            } else if (a < 0xa0aa) {
                UI_IterateThingList(kUIIterSideshows, _applytoall);
                CallSlot(f, 0x1c, panel);
                return 0;
            }
        }
    } else if (message == 0x800) {
        if (a == 0xa08f) {
            words[0x2c / 4] = b;
            CallSlot(f, 0x1c, panel);
        } else if (a < 0xa08f) {
            if (a >= 0xa08e) {
                words[0x24 / 4] = b;
                CallSlot(f, 0x1c, panel);
            }
        } else if (a < 0xa091) {
            words[0x28 / 4] = b;
            CallSlot(f, 0x1c, panel);
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

void ShowStatusPanel::Show(CThing* thing) {
    gShowStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gShowStatusScript, ShowStatusMsg, 0xa0a2, 0xa091, 0xa0a3);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd0));
}

// ---- ShowStatusPanel
void ShowStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101c2abc

// ui_staffRoomStatus.cpp / ui_showStatus.cpp / ui_staffStatus.cpp pieces (main app, UI):
//   0x101c2b10 __sinit_ui_showStatus_cpp, 0x101c30fc StaffRoomListFiller, 0x101c3360 StaffRoomLCFormat (the file's LCFormat),
//   0x101c40f0 __sinit_ui_staffRoomStatus_cpp, 0x101c4ec0 StaffStatusViewWindowCallback, 0x101c5878 StaffStatusPanel::ShowEditNamePanel.

uint32_t* gShowStatusTable;                            // 0x102ae980 (the same)

// 0x101c2b10
void __sinit_ui_showStatus_cpp() {
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gShowStatusPanel));
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gShowStatusPanel);
    fields->mVtable.set(MakeTable(gShowStatusTable, ShowStatusPanelTable));
    fields->mMode = 2;
}
