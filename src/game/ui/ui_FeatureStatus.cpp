// ui_FeatureStatus.cpp: name proven by __sinit_ui_FeatureStatus_cpp in the main application.

#include "analyser.h"
#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "LbTime.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "std_basic_filebuf.h"
#include "std_deque_errorthingy.h"
#include "std_exception_what.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstdint>
#include <cstring>
#include <new>

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

// 0x1019e2d4: nothing.
void FeatureStatusPanel::InitViewWindow() {}

// Unit v51125: small leaves of the main app (see include/std_exception_what.h).

// ---- the callees outside the unit: weak defaults (the real calls) that the unit's test library replaces with recording fakes.
#define TPW_WEAK __attribute__((weak))
// ---- FeatureStatusPanel::DeInitViewWindow 0x1019e314
void FeatureStatusPanel::DeInitViewWindow() { UIMain_DestroyRidePreview(); }

// FeatureStatusPanel (main app, 0x1019e374..0x1019edc4, unit v5311): the status panel of a feature (a ride's scenery piece).
// The panel has the UIStatusPanelFields layout (include/ui_main.h): +0x0c the panel's window, +0x10 the status window, +0x14 the view
// window, +0x1c the text window, +0x20 the table of virtual members (each entry the arena offset of a block whose first 8 bytes are a
// uint64_t (*)(void* panel), the host's way of a virtual call on a Mac-layout object).

// 0x1039486c (initial 0): the thing Show was given.
CThing* gFeatureStatusThing = nullptr;
// 0x102a9948: the window script.
int16_t gFeatureStatusScript[] = {TPW_WINDOW_SCRIPT_102A9948};   // 0x102a9948: the image's words (generated, devtools/gen_game_tables.py)

namespace {

using PanelSlot = uint64_t (*)(void*);

UIStatusPanelFields* Fields(FeatureStatusPanel* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// A virtual call through the table at +0x20: the entry at the Mac byte offset `slot`, the panel as the only argument.
uint64_t CallSlot(FeatureStatusPanel* panel, int32_t slot) {
    uint32_t entry = Fields(panel)->mVtable.get()[slot / 4];
    Ptr32<PanelSlot> block{entry};
    return (*block.get())(panel);
}

CObject* TheObject() { return reinterpret_cast<CObject*>(gStaffRoomPanelThing); }

}  // namespace

// 0x1019e374 (164 bytes)
void FeatureStatusPanel::UpdateViewWindow() {
    UIStatusPanelFields* fields = Fields(this);
    if (fields->mF14 && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = TheObject()->GetDBInfo();
        CoolRect rect;
        fields->mF14->GetClientRect(&rect);
        UIMain_ShowRidePreview(rect, info->mRideTypeNumber, fields->mF14->mZ + 10);
    }
}

// 0x1019e454 (596 bytes): the window at +0x10 has four children: 0x1587 and 0x1585 get a text sprite and a string (0x35, 0x36),
// 0x1586 gets a number object and 0x1584 a cash object, whose virtual SetValue (slot 0x24 of the table at +0x118) is called with 0.
// Every child gets font 6. The allocation, constructors and the virtual call are the allitems_post_callee functions.
void FeatureStatusPanel::InitStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (!fields->mF10) return;

    InterfaceWindow* window = fields->mF10->GetWindow(0x1587);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::SpriteCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x35)));
    }
    window = fields->mF10->GetWindow(0x1586);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::NumberCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        allitems_post_callee::WindowSetValue(window, 0);
    }
    window = fields->mF10->GetWindow(0x1585);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::SpriteCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(0x36)));
    }
    window = fields->mF10->GetWindow(0x1584);
    if (window) {
        void* object = allitems_post_callee::OperatorNew(0x19c);
        if (object) allitems_post_callee::CashCtor(object);
        window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
        window->SetFont(GetFontTableEntry(6));
        allitems_post_callee::WindowSetValue(window, 0);
    }
}

// Unit v51126: the tree constructors and insert_one of the nocase set, the wstring set and the buddy map, the allocator deallocates,
// the Argument destroyers, the nocase wide string destructor, the tear_down of the deque of Argument vectors, two make_iterator /
// getloc pieces, and four forwarders of the UI. The structures keep the Mac's 32-bit words (the arena is 32-bit addressable).

void FeatureStatusPanel::DeInitStatusWindow() {}

// 0x1019e728 (244 bytes)
void FeatureStatusPanel::UpdateStatusWindow() {
    CObject* object = TheObject();
    UIStatusPanelFields* fields = Fields(this);
    object->GetDBInfo();
    if (fields->mF10) {
        InterfaceWindow* countWindow = fields->mF10->GetWindow(0x1586);
        if (countWindow) {
            tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
            id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0xe);
            int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
            countWindow->SetValue(count);
        }
        InterfaceWindow* valueWindow = fields->mF10->GetWindow(0x1584);
        valueWindow->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));
    }
}

// 0x1019e85c (136 bytes)
void FeatureStatusPanel::InitWindow() {
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(this);
    UIStatusPanelFields* fields = Fields(this);
    if (base->FillObjectList(gFeatureStatusThing) < 2) {
        fields->mWindow->GetWindow(0x157c)->Show(0);
        fields->mWindow->GetWindow(0x157d)->Show(0);
    }
    base->InitEditBox(0x157f);
}

void FeatureStatusPanel::DeInitWindow() {}

// 0x1019e958 (208 bytes)
void FeatureStatusPanel::UpdateWindow() {
    CObject* object = TheObject();
    UIStatusPanelFields* fields = Fields(this);
    uint64_t text = CallSlot(this, 0x3c);
    Ptr32<InterfaceWindow> textWindow{fields->mF1C};
    textWindow->SetText(reinterpret_cast<char16_t*>(text));
    if (*reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0x2e) & 0x10)
        fields->mWindow->GetWindow(0x1581)->Show(1);
    else
        fields->mWindow->GetWindow(0x1581)->Show(0);
    CallSlot(this, 0x20);
    CallSlot(this, 0x1c);
}

// 0x1019ea60 (80 bytes)
void FeatureStatusPanel::RideIt() {
    TheObject()->GoFirstPerson(0);
    CallSlot(this, 0x34);
}

// Unit v5904: FeatureStatusMsg 0x1019eae4, __sinit_ui_FeatureStatus_cpp 0x1019eea0, GetMaxYValue 0x1019ef1c, KissMyLines 0x1019f25c.

namespace {

// The panel's table at +0x20: each slot holds the arena offset of a block whose first 8 bytes are the function (the host's virtual call).
void CallFeaturePanelSlot(UIStatusPanelFields* fields, uint32_t slot, void* self) {
    Ptr32<uint32_t> entry;
    entry.mOffset = fields->mVtable.get()[slot / 4];
    typedef uint64_t (*Fn)(void*);
    (*reinterpret_cast<Fn*>(entry.get()))(self);
}

}  // namespace

// 0x1019eae4 (676 bytes): the feature status panel's window handler. Everything that does not return falls to DefaultStatusPanelMsg.
int32_t FeatureStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    FeatureStatusPanel* panel = &gFeatureStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    UIStatusPanelFields* f = reinterpret_cast<UIStatusPanelFields*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) {
            CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
            object->GetDBInfo();
            if (f->mF10.mOffset != 0) {
                InterfaceWindow* countWindow = f->mF10->GetWindow(0x1586);
                if (countWindow != nullptr) {
                    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
                    id.mIndex = *reinterpret_cast<uint16_t*>(reinterpret_cast<uint8_t*>(object) + 0xe);
                    int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
                    countWindow->SetValue(count);
                }
                InterfaceWindow* valueWindow = f->mF10->GetWindow(0x1584);
                // No test for NULL here: the Mac reads the window's table at once.
                valueWindow->SetValue(static_cast<int32_t>(object->GetCurrentResidualValue()));
            }
        }
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12a));
    } else if (message == 0x1e) {
        UIMain_TickRidePreview();
    } else if (message == 0x100) {
        if (a == -1) {
            panel->Remove();
            return 0;
        } else if (a == 0x157b) {
            base->EraseObject();
            return 0;
        } else if (a == 0x157c) {
            base->GetPreviousObject();
        } else if (a == 0x157d) {
            base->GetNextObject();
        } else if (a == 0x1580) {
            CreateAllItemsPanel(kUIObjFeature, nullptr);
            return 0;
        } else if (a == 0x1581) {
            reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GoFirstPerson(0);
            CallFeaturePanelSlot(f, 0x34, panel);
            return 0;
        } else if (a == 0x1582) {
            base->Move();
            return 0;
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x1019edc4 (76 bytes)
void FeatureStatusPanel::Show(CThing* thing) {
    gFeatureStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gFeatureStatusScript, FeatureStatusMsg, 0x157e, 0x1583, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12a));
}

void FeatureStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }

// 0x1019eea0 (76 bytes): gFeatureStatusPanel's constructor, table 0x102a9ba0 at +0x20, mode 3.
void __sinit_ui_FeatureStatus_cpp() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(&gFeatureStatusPanel);
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gFeatureStatusPanel));
    fields->mVtable.set(FeatureStatusPanelTable());
    fields->mMode = 3;
}
