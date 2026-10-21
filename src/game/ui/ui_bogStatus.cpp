// ui_bogStatus.cpp: name proven by __sinit_ui_bogStatus_cpp in the main application.

#define TPW_UIMENU_CTOR_EXTERNAL
#include "CellBase.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "InterfaceWindow.h"
#include "LbTime.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "WindowScripts.inc"
#include <cstdint>
#include <cstring>
#include <new>

// BogStatusPanel (main app): the bog status panel's window functions. The panel has the UIStatusPanelFields layout (ui_main.h).

CThing* gBogStatusThing = nullptr;                 // 0x103947fc
int16_t gBogStatusScript[] = {TPW_WINDOW_SCRIPT_102A908C};   // 0x102a908c: the image's words (generated, devtools/gen_game_tables.py)

namespace {

// The host's way of the Mac's virtual call through a table of TVector pointers: the table holds Ptr32 words, each the arena offset of a
// block whose first 8 bytes are the function (see UIStatusPanelFields).
using Slot = uint64_t (*)(void*, uint64_t);

Slot SlotOf(Ptr32<uint32_t> table, uint32_t byteOffset) {
    Ptr32<Slot> entry;
    entry.mOffset = table.get()[byteOffset / 4];
    return *entry.get();
}

}  // namespace

// 0x10199ed0 (236 bytes): with a view window (+0x18): one deeper than it was by 11, hidden, a new text sprite as its client draw
// object, justification 1, 1, font table entry 8, black opaque, no text. The new never answers NULL on the Mac but the test is kept.
void BogStatusPanel::InitViewWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    InterfaceWindow* window = fields->mF18.get();
    if (window != nullptr) {
        window->setZ(static_cast<int16_t>(window->mZ + 0xb));
        fields->mF18.get()->Show(0);
        void* sprite = lender_callee::OperatorNew(0x19c);
        if (sprite != nullptr) lender_callee::SpriteCtor(sprite);
        fields->mF18.get()->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
        fields->mF18.get()->SetFontJust(1, 1);
        InterfaceFont* font = GetFontTableEntry(8);
        fields->mF18.get()->SetFont(font);
        fields->mF18.get()->SetFontColour(0, 0, 0, 0xff);
        fields->mF18.get()->SetClientDrawText(nullptr);
    }
}

// Unit v51115 (and v5L16, the cash constructor): the constructors and destructors of the number-window text objects (the Mac
// inlines the text sprite's constructor and the sprite chain's destructors in each; the host's chain does the same calls), the UIMenu
// constructor, and a few small functions.

// 0x10199ff4, 0x1019a4c4, 0x1019a894, 0x1019a978.
void BogStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }

// 0x1019a050 (164 bytes): with the window at +0x14 and a thing in gStaffRoomPanelThing: the ride preview of the ride record's word at
// +0x78c, in the window's client rectangle, one in front of the window.
void BogStatusPanel::UpdateViewWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (fields->mF14.mOffset != 0 && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
        CoolRect rect;
        fields->mF14.get()->GetClientRect(&rect);
        InterfaceWindow* window = fields->mF14.get();
        // The record's ride type number (+0x78c) by name: the host's record is laid out wider (unit v5L31).
        UIMain_ShowRidePreview(rect, info->mRideTypeNumber, static_cast<int32_t>(window->mZ) + 1);
    }
}

// Unit v5519: BogStatusPanel::InitStatusWindow (0x1019a12c) and UpdateStatusWindow (0x1019a504).

// 0x10237cd8 (untranslated runtime function): a double turned into an integer.
int32_t FloatToCell(double value);

namespace bog_callee {
// 0x101898c8 InterfaceDrawEngineHGradient(mesh), the complete object constructor, named by its symbol so that the test library (which
// replaces HGradientCtor below with a recording fake) links without it.
extern "C++" void HGradientRaw(void* memory, void* mesh) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");

// The constructor built in the memory given (weak: the test library defines its own, recording one).
__attribute__((weak)) void HGradientCtor(void* memory, const char* mesh) { HGradientRaw(memory, const_cast<char*>(mesh)); }
}  // namespace bog_callee

namespace {

using PanelSlot = uint64_t (*)(void*);

// The panel's virtual call: the table at +0x20 holds the arena offset of a block holding the function.
void CallPanel(UIStatusPanelFields* fields, void* panel, uint32_t offset) {
    Ptr32<PanelSlot> block;
    block.mOffset = fields->mVtable.get()[offset / 4];
    (*block.get())(panel);
}

// A text sprite as the client draw object of a window of the status window, font table entry 6, and its text.
void SetUpLabel(UIStatusPanelFields* fields, int32_t id, int32_t stringId) {
    InterfaceWindow* window = fields->mF10->GetWindow(id);
    void* sprite = lender_callee::OperatorNew(0x19c);
    if (sprite != nullptr) lender_callee::SpriteCtor(sprite);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

}  // namespace

// 0x1019a12c (864 bytes): with a status window (+0x10): three labelled texts (strings 0x3c, 0x3d, 0x3f), a number text, the gradient
// bar and the cash text; the number and cash windows start at 0, the bar at 50.
void BogStatusPanel::InitStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (fields->mF10.mOffset == 0) return;
    SetUpLabel(fields, 0x15bbe, 0x3c);
    SetUpLabel(fields, 0x15bbc, 0x3d);
    SetUpLabel(fields, 0x15bbf, 0x3f);

    InterfaceWindow* window = fields->mF10->GetWindow(0x15bbd);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::NumberCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetValue(0);

    window = fields->mF10->GetWindow(0x15bbb);
    UIMain_IndentClient(window, 0, 10);
    object = lender_callee::OperatorNew(0x124);
    if (object != nullptr) bog_callee::HGradientCtor(object, "ridestatbar.wct");
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetValue(0x32);

    window = fields->mF10->GetWindow(0x15bc0);
    object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::CashCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFont(GetFontTableEntry(6));
    window->SetValue(0);
}

void BogStatusPanel::DeInitStatusWindow() {}

// 0x1019a504 (648 bytes): the ride of gStaffRoomPanelThing in the status window (customers, condition, residual value) and the
// open/close and state buttons of the panel window (+0x0c); the panel is told when the object's state changed.
void BogStatusPanel::UpdateStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    object->GetDBInfo();
    if (fields->mF10.mOffset != 0) {
        InterfaceWindow* window = fields->mF10->GetWindow(0x15bbd);
        if (window != nullptr) {
            double sum;
            object->mHistory1A4.GetSumOfLast(sum, 0x1e);
            window->SetValue(FloatToCell(sum));
        }
        window = fields->mF10->GetWindow(0x15bbb);
        if (window != nullptr) {
            int32_t condition = ppc_fctiwz(static_cast<double>(object->mF40));
            // rlwinm 10, 14, 21 keeps the low byte shifted by 10; the divide by 100 is the mulhw / srawi pair.
            int32_t scaled = static_cast<int32_t>((static_cast<uint32_t>(condition) & 0xffu) << 10);
            window->SetValue(scaled / 100);
        }
        // No test for NULL here: the Mac reads the window's table at once.
        window = fields->mF10->GetWindow(0x15bc0);
        uint32_t residual = object->GetCurrentResidualValue();
        window->SetValue(static_cast<int32_t>(residual));
    }
    InterfaceWindow* openButton = fields->mWindow->GetWindow(0x15bc4);
    if (object->IsOpenable() == 0) {
        openButton->Enable(0);
    } else {
        openButton->Enable(1);
        if (object->IsClosed())
            static_cast<InterfaceButton*>(openButton)->SetState(1, 1);
        else
            static_cast<InterfaceButton*>(openButton)->SetState(0, 1);
    }
    InterfaceWindow* stateButton = fields->mWindow->GetWindow(0x15bb4);
    int32_t state = UIMain_GetObjectState(object, 2);
    if (static_cast<uint32_t>(state - 0xb) <= 4u)
        static_cast<InterfaceButton*>(stateButton)->SetState(1, 1);
    else
        static_cast<InterfaceButton*>(stateButton)->SetState(0, 1);
    if (state != fields->mF04) {
        fields->mF04 = state;
        fields->mWindow->GetWindow(0x15bb4);
        CallPanel(fields, this, 0x2c);
    }
}

// 0x1019a7c8 (152 bytes): fills the object list for the thing Show was given; with fewer than 2 objects the two buttons 0x15bb6 and
// 0x15bb7 of the panel's window are hidden (GetWindow's answer is not checked); then the edit box 0x15bc1.
void BogStatusPanel::InitWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(this);
    if (base->FillObjectList(gBogStatusThing) < 2) {
        fields->mWindow.get()->GetWindow(0x15bb6)->Show(0);
        fields->mWindow.get()->GetWindow(0x15bb7)->Show(0);
    }
    base->InitEditBox(0x15bc1);
}

void BogStatusPanel::DeInitWindow() {}

// 0x1019a8cc (120 bytes): the panel's table slot 0x3c answers the text, which goes to the window at +0x1c through that window's
// virtual SetText (slot 0x2c of its table at +0x118); then the panel's slots 0x20 and 0x1c.
void BogStatusPanel::UpdateWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    uint64_t text = SlotOf(fields->mVtable, 0x3c)(this, 0);
    Ptr32<InterfaceWindow> textWindow;
    textWindow.mOffset = fields->mF1C;
    textWindow.get()->SetText(reinterpret_cast<char16_t*>(text));
    SlotOf(fields->mVtable, 0x20)(this, 0);
    SlotOf(fields->mVtable, 0x1c)(this, 0);
}

void BogStatusPanel::ApplyChanges() {}

// Unit v5699: BogStatusMsg 0x1019a9b0, __sinit_ui_bogStatus_cpp 0x1019ad70, CoasterPlaceMouseMode 0x1019b018,
// WhichCheckButtonsAreDown 0x1019b120.

// 0x1019a9b0 (628 bytes): the bog status panel's window handler. Everything not answered falls to DefaultStatusPanelMsg.
int32_t BogStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    BogStatusPanel* panel = &gBogStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message < 0x15) {
        if (message == 0x10) {
            if (a == 0x80080) panel->UpdateStatusWindow();
        } else if (message < 0x10) {
            if (message == 5) {
                panel->Remove();
                return 1;
            }
        } else if (message >= 0x14) {
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12b));
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd5));
        }
    } else if (message == 0x100) {
        if (a == 0x15bb7) {
            base->GetNextObject();
        } else if (a < 0x15bb7) {
            if (a == 0x15bb4) {
                base->RequestMaintenance(b != 1);
                return 0;
            }
            if (a < 0x15bb4) {
                if (a == -1) {
                    panel->Remove();
                    return 0;
                }
            } else {
                if (a < 0x15bb6) {
                    base->EraseObject();
                    return 0;
                }
                base->GetPreviousObject();
            }
        } else {
            if (a == 0x15bc3) {
                base->Move();
                return 0;
            }
            if (a < 0x15bc3) {
                if (a >= 0x15bc2) {
                    CreateAllItemsPanel(kUIObjFeature, gStaffRoomPanelThing);
                    return 0;
                }
            } else if (a < 0x15bc5) {
                base->CloseObject(b == 1 ? 1 : 0);
                panel->UpdateStatusWindow();
            }
        }
    } else if (message < 0x100 && message == 0x1e) {
        UIMain_TickRidePreview();
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x1019ac5c (140 bytes): keeps the thing, opens the window and gives the advisor's word: 0xd5 when the thing's float at +0x40 (cut to
// an integer, low byte) is below 0x19, else 0x12b.
void BogStatusPanel::Show(CThing* thing) {
    gBogStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gBogStatusScript, BogStatusMsg, 0x15bb8, 0x15bba, 0x15bb9);
    float value = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(thing) + 0x40);
    int32_t cut;
    if (value != value || value < -2147483648.0f) cut = INT32_MIN;
    else if (value >= 2147483648.0f) cut = 0x7fffffff;
    else cut = static_cast<int32_t>(value);
    if ((static_cast<uint32_t>(cut) & 0xff) < 0x19)
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd5));
    else
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12b));
}

// Unit v51123: small CFSOnlineSystem members (the FTP post-login callbacks, the login getters, Login, Logout), the park, server and theme
// instances of CFSOnlineSystemList::CompleteGetList, two std::__tree helpers and two status panel members.

#define TPW_COMPLETE_GET_LIST(LISTTYPE, RECTYPE, RECEIVEDTYPE, REMOVEHEAD, DELETER)                              \
    template<> void LISTTYPE::CompleteGetList(int32_t error) {                                                    \
        bool hadError = mF18 != 0;                                                                                \
        if (hadError) {                                                                                           \
            AddUIError(mF18, nullptr);                                                                            \
            mF18 = 0;                                                                                             \
        }                                                                                                         \
        int32_t callback = mF1C;                                                                                  \
        int32_t context = mF20;                                                                                   \
        if (error == 0 && !hadError && mF14 != 0) {                                                               \
            while (mList->GetCount() != 0) {                                                                      \
                RECTYPE* record = REMOVEHEAD(mList);                                                              \
                if (record != nullptr) DELETER(record);                                                           \
            }                                                                                                     \
            mF24 = 0;                                                                                             \
            mF28 = 0;                                                                                             \
            mF28 = 1;                                                                                             \
            if (mReceived != nullptr) {                                                                           \
                StoreReceivedList(mReceived);                                                                     \
                mF24 = mReceived->mF08;                                                                           \
                RECEIVEDTYPE* received = mReceived;                                                               \
                if (received != nullptr) {                                                                        \
                    --received->mRefCount;                                                                        \
                    if (received->mRefCount == 0) {                                                               \
                        if (received->mInfos != nullptr) delete[] received->mInfos;                               \
                        received->mInfos = nullptr;                                                               \
                        ::operator delete(static_cast<void*>(received));                                          \
                    }                                                                                             \
                    mReceived = nullptr;                                                                          \
                }                                                                                                 \
            }                                                                                                     \
        }                                                                                                         \
        int32_t result = 0;                                                                                       \
        if (error != 0 || hadError || mF28 == 0) {                                                                \
            if (mF08 != 0) {                                                                                      \
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();                              \
                gOnlineSystem->Logout();                                                                          \
            }                                                                                                     \
            result = 1;                                                                                           \
        }                                                                                                         \
        if (callback != 0) gOnlineListCallbackCaller(callback, result, context, mList);                           \
    }

#define TPW_CALL_DELETER_PARK(record) gParkRecordDeleter(record)
#define TPW_CALL_DELETER_SERVER(record) gServerRecordDeleter(record)
#define TPW_CALL_DELETER_THEME(record) gThemeRecordDeleter(record)

// ---- the status panels
// 0x1019ad1c
void BogStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }

// 0x1019ad70 (76 bytes): the static constructor of gBogStatusPanel (0x10394800).
void __sinit_ui_bogStatus_cpp() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gBogStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gBogStatusPanel));
    fields->mVtable.set(BogStatusPanelTable());
    fields->mMode = 0x3ea;
}
