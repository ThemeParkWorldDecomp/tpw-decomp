// ui_staffStatus.cpp: name proven by __sinit_ui_staffStatus_cpp in the main application.

#include "AdvisorScoring.h"
#include "analyser.h"
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>
#include <new>

// The staff, show and upgrade status panels' view-window functions and the small free functions around them (unit v51051), main app.

// ---- StaffStatusPanel
void StaffStatusPanel::InitViewWindow() {}       // 0x101c41a0
void StaffStatusPanel::DeInitViewWindow() { UIMain_DestroySpritePreview(); }   // 0x101c41dc

// StaffStatusPanel (unit v5107): FireMe 0x101c5400, Show 0x101c54f0, InitWindow 0x101c4c34, UpdateViewWindow 0x101c423c.
// The panel object has the Mac layout UIStatusPanelFields (ui_main.h).

int16_t gStaffStatusScript[] = {TPW_WINDOW_SCRIPT_102AEDD0};   // 0x102aedd0: the image's words (generated, devtools/gen_game_tables.py)

void StaffStatusPanel::UpdateViewWindow() {
    if (gStaffRoomPanelThing != nullptr) {
        UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
        CoolRect rect;
        fields->mF14->GetClientRect(&rect);
        UIMain_ShowSpritePreview(rect, reinterpret_cast<CPeep*>(gStaffRoomPanelThing), fields->mF14->mZ + 1, 8.0f);
    }
}

// StaffStatusPanel::InitStatusWindow (0x101c4304), UpdateStatusWindow (0x101c4990) and UpdateWindow (0x101c4d2c).
// The object has the UIStatusPanelFields layout (status window at 0x10, window at 0x0c, name window at 0x1c, table at 0x20).

namespace {

// The two mesh names the Mac keeps one after the other (0x1024e774 and +0xe = 0x1024e782).
char kStatusBarNames[] = "happygrad.wct\0ridestatbar.wct";

constexpr int32_t kStaffTextTable = 6;   // gText + 0x564: mResourcePointers[6]

// Gives the window `window` a text sprite as client draw object, the font number 6 and, when given, the text.
void SpriteWindow(InterfaceWindow* window, int32_t stringId) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(6));
    if (stringId != 0) window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

}  // namespace

void StaffStatusPanel::InitStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (fields->mF10.get() == nullptr) return;

    SpriteWindow(fields->mF10->GetWindow(0x51a), 0x40);
    SpriteWindow(fields->mF10->GetWindow(0x51d), 0x41);
    SpriteWindow(fields->mF10->GetWindow(0x519), 0x43);
    SpriteWindow(fields->mF10->GetWindow(0x518), 0x45);
    SpriteWindow(fields->mF10->GetWindow(0x51c), 0x44);
    SpriteWindow(fields->mF10->GetWindow(0x516), 0x42);

    InterfaceWindow* window = fields->mF10->GetWindow(0x51b);
    {
        InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
        window->SetClientDrawObject(sprite);
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(nullptr);
    }

    window = fields->mF10->GetWindow(0x513);
    UIMain_IndentClient(window, 0, 10);
    window->SetClientDrawObject(new InterfaceDrawEngineHGradient(kStatusBarNames));
    window->SetValue(100);

    window = fields->mF10->GetWindow(0x514);
    UIMain_IndentClient(window, 0, 10);
    window->SetClientDrawObject(new InterfaceDrawEngineHGradient(kStatusBarNames + 0xe));
    window->SetValue(100);

    window = fields->mF10->GetWindow(0x51e);
    {
        InterfaceCashTextObject* cash = new InterfaceCashTextObject(nullptr);
        window->SetClientDrawObject(cash);
        window->SetFont(GetFontTableEntry(6));
        window->SetValue(0);
    }

    window = fields->mF10->GetWindow(0x515);
    UIMain_IndentClient(window, 0, 10);
    window->SetClientDrawObject(new InterfaceDrawEngineHGradient(kStatusBarNames + 0xe));
    window->SetValue(500);

    window = fields->mF10->GetWindow(0x517);
    {
        InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
        window->SetClientDrawObject(sprite);
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(nullptr);
    }
}

void StaffStatusPanel::DeInitStatusWindow() {}   // 0x101c4950

void StaffStatusPanel::UpdateStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
    if (fields->mF10.get() == nullptr) return;

    int32_t state = staff->GetGeneralStaffState();
    int32_t duration = staff->GetEmploymentDuration();
    TbIntegerVTag tag(8, duration);
    static_cast<TbITable*>(gText.mResourcePointers[kStaffTextTable])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1bb, &tag);

    InterfaceWindow* window = fields->mF10->GetWindow(0x51b);
    window->SetText(gUIMainConvertBuffer);

    window = fields->mF10->GetWindow(0x51e);
    window->SetValue(static_cast<int32_t>(staff->GetSalary()));

    window = fields->mF10->GetWindow(0x514);
    // (skill << 10) / 100 on the unsigned word (mulhwu by 0x51eb851f, then srwi 5).
    uint32_t skill = static_cast<uint32_t>(staff->GetSkill()) << 10;
    window->SetValue(static_cast<int32_t>(skill / 100));

    window = fields->mF10->GetWindow(0x51f);
    window->SetFrame(UIMain_GetStaffStateFrame(state));

    window = fields->mF10->GetWindow(0x517);
    if (state == 1 && staff->HasPatrolArea() == 0) state = 0;
    UIMain_SetStaffStateText(window, state);

    // The two bars: the float truncated (fctiwz), its low byte shifted by 10, divided by 100 (signed, but never negative).
    int32_t happiness = static_cast<int32_t>(staff->mHappiness);
    window = fields->mF10->GetWindow(0x513);
    window->SetValue(static_cast<int32_t>(((static_cast<uint32_t>(happiness) & 0xff) << 10)) / 100);

    int32_t energy = static_cast<int32_t>(staff->mEnergy);
    window = fields->mF10->GetWindow(0x515);
    window->SetValue(static_cast<int32_t>(((static_cast<uint32_t>(energy) & 0xff) << 10)) / 100);
}

void StaffStatusPanel::InitWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gStaffStatusThing) < 2) {
        fields->mWindow->GetWindow(0x50d)->Show(0);
        fields->mWindow->GetWindow(0x50f)->Show(0);
    }
    reinterpret_cast<UIStatusPanel*>(this)->InitEditBox(0x520);
}

void StaffStatusPanel::DeInitWindow() {}         // 0x101c4cf0

void StaffStatusPanel::UpdateWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
    if (staff == nullptr) return;

    Ptr32<InterfaceWindow> nameWindow;
    nameWindow.mOffset = fields->mF1C;
    nameWindow->SetText(const_cast<char16_t*>(staff->GetName()));

    // The two virtual calls through the table at 0x20: entries 0x20 and 0x1c (each an arena offset of a block holding the function).
    Ptr32<void (*)(StaffStatusPanel*)> entry;
    entry.mOffset = fields->mVtable.get()[0x20 / 4];
    (*entry.get())(this);
    entry.mOffset = fields->mVtable.get()[0x1c / 4];
    (*entry.get())(this);

    InterfaceButton* button = static_cast<InterfaceButton*>(fields->mWindow->GetWindow(0x510));
    if (staff->HasPatrolArea() != 0) {
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd4));
        button->SetState(1, 1);
    } else {
        button->SetState(0, 1);
    }

    InterfaceWindow* window = fields->mWindow->GetWindow(0x50e);
    if (staff->CanBePickedUp() != 0) {
        window->Enable(1);
    } else {
        window->Enable(0);
    }
}

// ui_staffRoomStatus.cpp / ui_showStatus.cpp / ui_staffStatus.cpp pieces (main app, UI):
//   0x101c2b10 __sinit_ui_showStatus_cpp, 0x101c30fc StaffRoomListFiller, 0x101c3360 StaffRoomLCFormat (the file's LCFormat),
//   0x101c40f0 __sinit_ui_staffRoomStatus_cpp, 0x101c4ec0 StaffStatusViewWindowCallback, 0x101c5878 StaffStatusPanel::ShowEditNamePanel.

int16_t gStaffEditNameScript[] = {TPW_WINDOW_SCRIPT_102AF16C};   // 0x102af16c: the image's words (generated, devtools/gen_game_tables.py)

// The callees the host cannot call as the Mac does (new plus constructor, the virtuals through __ptr_glue): weak, faked by the test library.
namespace staffroom_callee {
__attribute__((weak)) InterfaceDrawObject* NewTitleText() { return new InterfaceTitleTextObject(nullptr); }
__attribute__((weak)) InterfaceDrawObject* NewCursorText() { return new InterfaceCursorPosTextObjectSprite(nullptr); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) { window->SetText(const_cast<char16_t*>(text)); }
}  // namespace staffroom_callee

// 0x101c4ec0
int32_t StaffStatusViewWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) reinterpret_cast<UIStatusPanel*>(&gStaffStatusPanel)->ZoomTo();
    return window->CallDefaultWindowHandler(message, a, b);
}

// The staff status panel's message functions (main app, UI): StaffStatusMsg 0x101c4f8c (912 bytes) and OneEditMsg 0x101c56bc (388 bytes).

namespace {

// The Mac calls a virtual member of a panel through its table at +0x20: the slot holds an arena offset of a block holding the function.
void CallSlot(UIStatusPanelFields* fields, uint32_t slot, void* self) {
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = fields->mVtable.get()[slot / 4];
    (*entry.get())(self);
}

}  // namespace

int32_t StaffStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    StaffStatusPanel* panel = &gStaffStatusPanel;
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(panel);

    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message < 0x15) {
        if (message == 0x10) {
            if (static_cast<uint32_t>(a - 0x80000) == 0x80) panel->UpdateStatusWindow();
        } else if (message < 0x10) {
            if (message == 5) {
                panel->Remove();
                return 1;
            }
        } else if (message >= 0x14) {
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc0));
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd4));
        }
    } else if (message == 0x100) {
        CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
        if (a == 0x50e) {
            panel->SetSelected();
            staff->PickMeUp();
            CallSlot(fields, 0x34, panel);
            return 0;
        } else if (a < 0x50e) {
            if (a == 0x50b) {
                CreateAllStaffPanel(staff->GetStaffType(), gStaffRoomPanelThing);
                return 0;
            } else if (a < 0x50b) {
                if (a == -1) {
                    panel->Remove();
                    return 0;
                }
            } else if (a < 0x50d) {
                panel->FireMe();
                return 0;
            } else {
                reinterpret_cast<UIStatusPanel*>(panel)->GetPreviousObject();
            }
        } else if (a == 0x521) {
            ShowStaffPanel(gStaffRoomPanelThing);
            return 0;
        } else if (a < 0x521) {
            if (a == 0x510) {
                bool handled;
                if (b == 1) {
                    panel->SetSelected();
                    CMMPathDraw* pathDraw = static_cast<CMMPathDraw*>(::operator new(0x14));
                    if (pathDraw != nullptr) new (pathDraw) CMMPathDraw(static_cast<CELL_MODES>(0x36));
                    if (gMouse == nullptr) {
                        CMouse* mouse = static_cast<CMouse*>(::operator new(1));
                        if (mouse != nullptr) {
                            CMMDefault* defaultMode = static_cast<CMMDefault*>(::operator new(8));
                            if (defaultMode != nullptr) new (defaultMode) CMMDefault();
                            mouse->AssignMouseMode(defaultMode);
                            gMouseWord1E0 = -1;
                            gMouseWord1E4 = -1;
                        }
                        gMouse = mouse;
                    }
                    gMouse->AssignMouseMode(pathDraw);
                    CallSlot(fields, 0x34, panel);
                    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc1));
                    handled = true;
                } else {
                    staff->CancelPatrolArea();
                    handled = false;
                }
                if (handled) return 0;
            } else if (a < 0x510) {
                reinterpret_cast<UIStatusPanel*>(panel)->GetNextObject();
            }
        }
    } else if (message < 0x100 && message == 0x1e) {
        CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
        if (staff != nullptr) {
            InterfaceWindow* button = fields->mWindow->GetWindow(0x50e);
            if (staff->CanBePickedUp() != 0) {
                button->Enable(1);
            } else {
                button->Enable(0);
            }
            UIMain_UpdateSpritePreview();
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

void StaffStatusPanel::SetSelected() { UI_SetSelectedPeep(reinterpret_cast<CPeep*>(gStaffRoomPanelThing)); }   // 0x101c5358
// The Mac's argument is the staff member's address; here FireMe keeps the thing's arena offset in the dialog's word (DialogBoxCallback
// hands it over unchanged, as a 32-bit word widened to a pointer), so the offset is turned back into the thing's host address first
// (0 stays null). A word that is already an address in the arena is taken as it is.
int32_t fireme(void* word) {   // 0x101c53b8
    void* context = word;
    if (word != nullptr && !tpw::InArena(word)) context = tpw::gArenaBase + static_cast<uint32_t>(reinterpret_cast<uintptr_t>(word));
    reinterpret_cast<CStaff*>(context)->SackMe();
    return 1;
}

void StaffStatusPanel::FireMe() {
    if (gPlayerSettings.mCurrent.mConfirmDeleteOn) {
        dlgboxparams params;
        params.mText = UIMain_GetString(0x18d);
        params.mRed = 0xff;
        params.mGreen = 0xff;
        params.mBlue = 0xff;
        params.mValue8 = 4;
        // the Mac stores the thing's address in the word; the host stores its arena offset (0 for null). An address outside the arena
        // (the game's things are all inside it) keeps its low 32 bits, as the Mac's word was.
        params.mValueC = gStaffRoomPanelThing == nullptr ? 0
                         : tpw::InArena(gStaffRoomPanelThing) ? static_cast<int32_t>(tpw::ArenaOffset(gStaffRoomPanelThing))
                                                              : static_cast<int32_t>(reinterpret_cast<uintptr_t>(gStaffRoomPanelThing));
        UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), fireme, static_cast<dlgbutton>(0),
                     nullptr, false);
    } else {
        reinterpret_cast<CStaff*>(gStaffRoomPanelThing)->SackMe();
    }
    // The virtual call: table pointer at +0x20, entry 0x34 (an arena offset of a block holding the function).
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    Ptr32<void (*)(StaffStatusPanel*)> entry;
    entry.mOffset = fields->mVtable.get()[0x34 / 4];
    (*entry.get())(this);
}

void StaffStatusPanel::Show(CThing* thing) {
    gStaffStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gStaffStatusScript, StaffStatusMsg, 0x511, 0x512, 0);
    reinterpret_cast<UIStatusPanelFields*>(this)->mF14->SetCallbackFunction(StaffStatusViewWindowCallback);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc0));
}

void StaffStatusPanel::ChangeName(char16_t* name) {   // 0x101c558c
    if (gStaffRoomPanelThing != nullptr) reinterpret_cast<CStaff*>(gStaffRoomPanelThing)->SetName(name);
}
const char16_t* StaffStatusPanel::GetName() {   // 0x101c55f8
    if (gStaffRoomPanelThing == nullptr) return nullptr;
    return reinterpret_cast<CStaff*>(gStaffRoomPanelThing)->GetName();
}
void StaffStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }   // 0x101c5668

int32_t OneEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x802) {
        UI_PostMessage(window, 0x100, -1, 0);
    } else if (message < 0x802) {
        if (message == 0x100) {
            if (a == -1) {
                CStaff* staff = static_cast<CStaff*>(window->getExtraParam(0));
                staff->SetName(gStaffEditName);
                UIStatusPanelFields* fields = static_cast<UIStatusPanelFields*>(window->getExtraParam(1));
                CallSlot(fields, 0xc, fields);
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            } else if (a == -2) {
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
        } else if (message < 0x100 && message == 0x14) {
            UIMain_EnableAllKeyTables();
            static_cast<UIStatusPanel*>(window->getExtraParam(1))->EditNamePanelGone();
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x804) {
        UI_SendMessage(window, 4, 0, 0);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101c5878
void StaffStatusPanel::ShowEditNamePanel() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    Ptr32<InterfaceWindow>& windowField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 8);
    Ptr32<InterfaceWindow>& parentField = *reinterpret_cast<Ptr32<InterfaceWindow>*>(self + 0xc);
    if (windowField.mOffset != 0) return;
    CThing* thing = gStaffRoomPanelThing;
    if (thing == nullptr) return;
    uint8_t type = reinterpret_cast<uint8_t*>(thing)[2];
    if (!(type == 4 || type == 5 || type == 6 || type == 8 || type == 7)) return;
    CStaff* staff = reinterpret_cast<CStaff*>(thing);

    InterfaceWindow* parent = parentField.get();
    int16_t z = parent->mZ;   // the Mac's +0xdc, by name: the host window is laid out differently
    InterfaceWindow* window = parent->Build(gStaffEditNameScript, OneEditMsg);
    windowField.set(window);
    window->setZ(static_cast<int16_t>(z + 10));
    window->setExtraParam(0, thing);
    window->setExtraParam(1, this);

    InterfaceDrawObject* titleObject = staffroom_callee::NewTitleText();
    window->GetWindow(0x5daa)->SetClientDrawObject(titleObject);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    window->GetWindow(0x5daa)->SetFont(titleFont);
    window->GetWindow(0x5daa)->SetFontJust(1, 1);
    const char16_t* title = UIMain_GetString(0xc3);
    staffroom_callee::WindowSetText(window->GetWindow(0x5daa), title);
    UI_TITLE_COLOUR(window->GetWindow(0x5daa));

    InterfaceEditBox* edit = static_cast<InterfaceEditBox*>(window->GetWindow(0x5dab));
    InterfaceDrawObject* cursor = staffroom_callee::NewCursorText();
    edit->SetClientDrawObject(cursor);
    edit->SetFont(GetFontTableEntry(7));
    edit->SetFontColour(0xff, 0xff, 0xff, 0xff);
    edit->SetStaticTextArea(gStaffEditName, 0x20);
    edit->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
    staffroom_callee::WindowSetText(edit, staff->GetName());
    edit->SelectAll();
    UIMain_DisableAllKeyTables();
    edit->CaptureKb(0);
}

// ui_upgradeStatus.cpp: the upgrade status panel's window handler (UpgradeStatusMsg, 0x101c63b4) and ui_staffStatus.cpp's static
// initialiser (__sinit_ui_staffStatus_cpp, 0x101c5bec). Unit v5548.

int32_t gStaffStatusF40;   // 0x10395b40
int32_t gStaffStatusF44;   // 0x10395b44

// The staff panel is built with the UIStatusPanel constructor, then its table pointer (+0x20) gets the Mac's table at 0x102af264 (on the host
// an arena block, src/ui_StatusPanel_tables.cpp) and its mode word (+0) -1.
void __sinit_ui_staffStatus_cpp() {
    gStaffStatusF44 = 0;
    gStaffStatusF40 = 0x10000;
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gStaffStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gStaffStatusPanel));
    fields->mVtable.set(StaffStatusPanelTable());
    fields->mMode = -1;
}
