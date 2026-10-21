// ui_visitorStatus.cpp: name proven by __sinit_ui_visitorStatus_cpp in the main application.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "bfl_FileSystem.h"
#include "Clock.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FillLenderList.h"
#include "Game.h"
#include "InGame.h"
#include "InterfaceWindow.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "platform/arena.h"
#include "Ptr32.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "TbTList.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).

// 0x10141f1c (100 bytes): the pending-delete list. The nodes are packed on the Mac (id at +0, next at +2, previous at +6); the host's
// are CNodes with native pointers. Head and tail are cleared first, then every node is freed (its next read before the free).
template<> void
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::RemoveAll() {
    CNode* node = mHead;
    mTail = nullptr;
    mHead = nullptr;
    while (node != nullptr) {
        CNode* next = node->mNext;
        ::operator delete(node);
        node = next;
    }
    mCount = 0;
}

// CWorldSavvy::GetWorld (main app, 0x1014205c, 12 bytes; unit w129).

// A leaf that ignores `this` and answers the world the pointer at 0x1037ec0c holds (gWorld, which World_init.cpp assigns; not gGameWorld at 0x1037ebfc).
// A NULL gWorld is as invalid here as on the Mac.
CWorld& CWorldSavvy::GetWorld() const { return *gWorld; }

// Unit v51072: CActionRecorder::ProcessAction (count, text), the 8 byte stand-ins of the recorder and the visitor panel, the destructors
// of the wide string, CMemFile and the thing id list, the file initialisers of InGame.cpp and ExitErr.cpp and a few key table actions.

namespace v51072_callee {
__attribute__((weak)) void OperatorDelete(void* block) { ::operator delete(block); }
__attribute__((weak)) void ThingIdListRemoveAll(void* list) {
    using List = TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>;
    static_cast<List*>(list)->RemoveAll();
}
}  // namespace v51072_callee

// 0x10141e34
void ThingIdListDestroy(void* list, int16_t flag) {
    if (list == nullptr) return;
    v51072_callee::ThingIdListRemoveAll(list);
    if (flag > 0) v51072_callee::OperatorDelete(list);
}

// 0x10142008
void ThingArrayEntryInit(uint32_t* entry) {
    entry[0] = 0;
    entry[1] = 0;
    entry[2] = 0;
    entry[3] = 0;
    entry[4] = 0;
}

// 0x10142098
CMap* CWorldSavvy::GetMap() const { return gWorldMap; }
void VisitorStatusPanel::InitViewWindow() {}       // 0x101420d0

void VisitorStatusPanel::DeInitViewWindow() { UIMain_DestroySpritePreview(); }   // 0x10142110

// VisitorStatusPanel::UpdateStatusWindow (0x10142ae4, 1056 bytes) and UpdateViewWindow (0x10142170, 140 bytes).
// The panel and the visitor are reached by Mac offsets (the panel as UIStatusPanelFields, the visitor's fields by raw offset).

namespace visitor_callee {
void WindowSetValue(InterfaceWindow* window, int32_t value);   // the virtual SetValue (slot 0x24 of the table at +0x118)
void WindowSetText(InterfaceWindow* window, const char16_t* text);   // the virtual at slot 0x2c
void RectCtor(void* rect);   // 0x101c7448 CoolRect::CoolRect()
void RectDtor(void* rect);   // 0x101c74e0 CoolRect::~CoolRect() (flag -1)
__attribute__((weak)) void WindowSetValue(InterfaceWindow* window, int32_t value) { window->SetValue(value); }
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) {
    window->SetText(const_cast<char16_t*>(text));
}
__attribute__((weak)) void RectCtor(void* rect) { new (rect) CoolRect(); }
__attribute__((weak)) void RectDtor(void* rect) { static_cast<CoolRect*>(rect)->~CoolRect(); }
}  // namespace visitor_callee

namespace {

UIStatusPanelFields* Fields(VisitorStatusPanel* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

void CallSlot(VisitorStatusPanel* panel, uint32_t slot) {
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = Fields(panel)->mVtable.get()[slot / 4];
    (*entry.get())(panel);
}

uint32_t Word(const uint8_t* thing, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, thing + offset, 4);
    return value;
}

// fctiwz's low word (saturating; NaN gives 0x80000000).
int32_t Fctiwz(float value) {
    if (!(value > -2147483648.0f)) return INT32_MIN;
    if (value >= 2147483648.0f) return INT32_MAX;
    return static_cast<int32_t>(value);
}

TbITable* Table(int index) { return static_cast<TbITable*>(gText.mResourcePointers[index]); }

}  // namespace

// 0x10142170 (140 bytes)
void VisitorStatusPanel::UpdateViewWindow() {
    if (gStaffRoomPanelThing == nullptr) return;
    alignas(8) uint8_t storage[0x20];
    visitor_callee::RectCtor(storage);
    CoolRect* rect = reinterpret_cast<CoolRect*>(storage);
    Fields(this)->mF14.get()->GetClientRect(rect);
    InterfaceWindow* window = Fields(this)->mF14.get();
    UIMain_ShowSpritePreview(*rect, reinterpret_cast<CPeep*>(gStaffRoomPanelThing), window->mZ + 2, 12.0f);
    visitor_callee::RectDtor(storage);
}

// The real constructors by their symbols, weak so that the test library (which fakes both callees and does not link them) builds.
extern "C" void VspGradientCtorSym(void*, void*) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv") __attribute__((weak));

namespace vsp_callee {
__attribute__((weak)) void GradientCtor(void* mem, void* data) { VspGradientCtorSym(mem, data); }
// Stand-in: InterfaceVisitorTimeTextObject (0x1018edf4) is not declared anywhere; the sprite constructor stands for it.
__attribute__((weak)) void TimeCtor(void* mem) { lender_callee::SpriteCtor(mem); }
}  // namespace vsp_callee

namespace {

// operator new, then (when it answered) the constructor; as the Mac: the object may be NULL, it is given to the window anyway.
template <typename Ctor>
InterfaceDrawObject* Make(uint32_t size, Ctor ctor) {
    void* mem = lender_callee::OperatorNew(size);
    if (mem) ctor(mem);
    return static_cast<InterfaceDrawObject*>(static_cast<InterfaceTextObjectSprite*>(mem));
}

InterfaceWindow* Text(InterfaceWindow* parent, int32_t id, int32_t stringId) {
    InterfaceWindow* w = parent->GetWindow(id);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
    return w;
}

}  // namespace

void VisitorStatusPanel::InitStatusWindow() {
    if (mWindow == nullptr) return;
    InterfaceWindow* root = mWindow;
    Text(root, 0x372c, 0x46);
    Text(root, 0x3725, 0x47);
    Text(root, 0x3726, 0x48);
    Text(root, 0x3727, 0x4b);
    Text(root, 0x3728, 0x49);
    Text(root, 0x3729, 0x4a);
    Text(root, 0x372a, 0x4c);
    Text(root, 0x372b, 0x4d);

    InterfaceWindow* w = root->GetWindow(0x3724);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::CashCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    lender_callee::WindowSetValue(w, 0x29a);

    w = root->GetWindow(0x372d);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { vsp_callee::TimeCtor(m); }));
    w->SetFont(GetFontTableEntry(6));

    static const int32_t kNumbers[] = {0x372e, 0x372f, 0x3730, 0x3731};
    for (int32_t id : kNumbers) {
        w = root->GetWindow(id);
        w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::NumberCtor(m); }));
        w->SetFont(GetFontTableEntry(6));
        lender_callee::WindowSetValue(w, 0);
    }

    w = root->GetWindow(0x3732);
    if (w) {
        UIMain_IndentClient(w, 0, 10);
        // 0x10247de3: "happygrad.wct"
        w->SetClientDrawObject(Make(0x124, [](void* m) { vsp_callee::GradientCtor(m, const_cast<char*>("happygrad.wct")); }));
    }

    w = root->GetWindow(0x3733);
    w->SetClientDrawObject(Make(0x19c, [](void* m) { lender_callee::SpriteCtor(m); }));
    w->SetFont(GetFontTableEntry(6));
    w->SetClientDrawText(nullptr);
}

void VisitorStatusPanel::DeInitStatusWindow() {}   // 0x10142aa0

// 0x10142ae4 (1056 bytes)
void VisitorStatusPanel::UpdateStatusWindow() {
    UIStatusPanelFields* f = Fields(this);
    uint32_t* words = reinterpret_cast<uint32_t*>(this);
    const uint8_t* thing = reinterpret_cast<const uint8_t*>(gStaffRoomPanelThing);
    if (thing == nullptr) {
        CallSlot(this, 0x34);
        return;
    }
    if (words[0x24 / 4] == 0) {
        words[0x24 / 4] = Word(thing, 0x1d8);
        if (words[0x24 / 4] != 0) {
            ShowStatusTag tag(0x18, static_cast<int32_t>(words[0x24 / 4]));
            Table(6)->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
            UIMain_SetTitleText(f->mWindow.get(), 0x3736, gUIMainConvertBuffer);
        }
    }
    InterfaceWindow* panel = f->mF10.get();
    if (panel != nullptr) {
        InterfaceWindow* window = panel->GetWindow(0x3724);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1a0)));

        window = f->mF10.get()->GetWindow(0x372d);
        if (window != nullptr) {
            uint32_t turns = reinterpret_cast<const CPerson*>(thing)->GetTimeInPark();
            TbTimeDiff span = gGameWorld->mParkTime.GetFunnyTimeForGameTurns(turns << 2);
            // The runtime's 64-bit divide by 36,000,000,000 (ticks per hour); the window gets the low word.
            visitor_callee::WindowSetValue(window, static_cast<int32_t>(span.mTicks / 36000000000LL));
        }

        window = f->mF10.get()->GetWindow(0x372e);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1c4)));
        window = f->mF10.get()->GetWindow(0x372f);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1c8)));
        window = f->mF10.get()->GetWindow(0x3730);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1cc)));
        window = f->mF10.get()->GetWindow(0x3731);
        if (window != nullptr) visitor_callee::WindowSetValue(window, static_cast<int32_t>(Word(thing, 0x1d0)));

        window = f->mF10.get()->GetWindow(0x3732);
        if (window != nullptr) {
            float happiness;
            std::memcpy(&happiness, thing + 0x19c, 4);
            int32_t scaled = static_cast<int32_t>((static_cast<uint32_t>(Fctiwz(happiness)) & 0xff) << 10);
            visitor_callee::WindowSetValue(window, scaled / 100);
        }

        // No test for NULL on this window.
        window = f->mF10.get()->GetWindow(0x3733);
        uint32_t state = Word(thing, 0x220);
        if (state == 0x10) {
            uint16_t index;
            std::memcpy(&index, thing + 0x1dc, 2);
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            char16_t name[0x40];
            reinterpret_cast<CObject*>(gThingArray[index].mThing)->GetUIName(name, 0x40);
            TbUniStringVTag tag(0, name);
            Table(9)->FormatString(gUIMainConvertBuffer, 0x3fff, 0x10, &tag);
        } else {
            Table(9)->FormatString(gUIMainConvertBuffer, 0x3fff, state);
        }
        visitor_callee::WindowSetText(window, gUIMainConvertBuffer);
    }

    uint32_t frame = Word(thing, 0x30);
    InterfaceWindow* child = f->mWindow.get()->GetWindow(0x3734)->GetWindow(0x3735);
    child->SetFrame(frame != 0 ? static_cast<int32_t>(frame - 1) : 999);
    char16_t* text = UIMain_ConvertMulti(Table(8)->GetConstString(frame));
    visitor_callee::WindowSetText(f->mWindow.get()->GetWindow(0x3734), text);
}

// Unit v5929: the visitor status panel (ui_visitorStatus.cpp): Show, InitWindow, UpdateWindow, the window handler VisitorStatusMsg and the
// file's static initialiser. The panel has the UIStatusPanelFields layout (Mac offsets), read through that struct.

CThing* gVisitorStatusThing = nullptr;               // 0x1037ec10
int16_t gVisitorStatusScript[] = {TPW_WINDOW_SCRIPT_102A4758};   // 0x102a4758: the image's words (generated, devtools/gen_game_tables.py)
int32_t gVisitorStatusWordEC3C = 0;                  // 0x1037ec3c
int32_t gVisitorStatusWordEC40 = 0;                  // 0x1037ec40
uint32_t* gVisitorStatusTable = nullptr;             // 0x102a4b5c

// 0x10142f44 (264 bytes)
void VisitorStatusPanel::InitWindow() {
    UIStatusPanelFields* fields = Fields(this);
    reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gVisitorStatusThing);
    InterfaceWindow* window = fields->mWindow->GetWindow(0x3734);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontColour(0, 0, 0, 0xff);
    window->SetClientDrawText(nullptr);
}

void VisitorStatusPanel::DeInitWindow() {}         // 0x10143084

// 0x101430c0 (276 bytes)
void VisitorStatusPanel::UpdateWindow() {
    int32_t* words = reinterpret_cast<int32_t*>(this);
    if (gStaffRoomPanelThing == nullptr) {
        CallSlot(this, 0x34);
        return;
    }
    CallSlot(this, 0x20);
    CallSlot(this, 0x1c);
    // The thing is read again after the calls (the global may have changed); its word at the Mac's +0x1d8 is kept at the panel's +0x24.
    words[0x24 / 4] = *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(gStaffRoomPanelThing) + 0x1d8);
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    if (words[0x24 / 4] == 0) {
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c9);
    } else {
        ShowStatusTag tag(0x18, words[0x24 / 4]);
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
    }
    UIMain_SetTitleText(Fields(this)->mWindow.get(), 0x3736, gUIMainConvertBuffer);
}

// 0x1014320c (432 bytes)
int32_t VisitorStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    VisitorStatusPanel* panel = &gVisitorStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) panel->UpdateStatusWindow();
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12e));
    } else if (message == 0x1e) {
        UIMain_UpdateSpritePreview();
    } else if (message == 0x100) {
        if (a == 0x3721) {
            base->GetNextObject();
        } else if (a < 0x3721) {
            if (a == -2) {
                panel->Remove();
                return 0;
            }
            if (a >= 0x3720) base->GetPreviousObject();
        } else {
            if (a == 0x3738) {
                CreateAllKidsPanel(gStaffRoomPanelThing);
                return 0;
            }
            if (a == 0x3737) {
                panel->Locate();
                return 0;
            }
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x101433f8 (112 bytes)
void VisitorStatusPanel::Show(CThing* thing) {
    gVisitorStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gVisitorStatusScript, VisitorStatusMsg, 0x3722, 0x3723, 0);
    SOUND_SendEvent(0, gSoundBanks[1], 0x81, 0, 0, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12e));
}

// 0x101434a0: the panel's window word is at the Mac's +0xc.
void VisitorStatusPanel::KidGone(CThing* thing) {
    if (reinterpret_cast<UIStatusPanelFields*>(this)->mWindow) reinterpret_cast<UIStatusPanel*>(this)->ObjectGone(thing);
}

// 0x1014350c
void VisitorStatusPanel::Locate() { ShowStaffPanel(gStaffRoomPanelThing); }

// 0x1014356c
void VisitorStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }

// 0x101435c4 (100 bytes)
void __sinit_ui_visitorStatus_cpp() {
    gVisitorStatusWordEC40 = 0;
    gVisitorStatusWordEC3C = 0x10000;
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gVisitorStatusPanel));
    // The Mac stores the address of its table of virtual slots (0x102a4b5c); the host's is an arena block (src/ui_StatusPanel_tables.cpp).
    if (gVisitorStatusTable == nullptr) gVisitorStatusTable = VisitorStatusPanelTable();
    UIStatusPanelFields* fields = Fields(&gVisitorStatusPanel);
    fields->mVtable.set(gVisitorStatusTable);
    fields->mMode = -2;
}
