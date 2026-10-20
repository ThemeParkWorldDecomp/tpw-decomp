// The all-kids panel (unit v5078): FillAllKidsListCallback 0x10194880, UpdateListEntries 0x101949cc (the all-kids one; the
// all-staff twin of the same name is at 0x10198944), AllKidsPanelMsg 0x10194b4c, LCFormat 0x10194df0.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "AdvisorScoring.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ParkTime.h"
#include "Ptr32.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_controls.h"
#include "ui_main.h"

void RemoveAllKidsPanel();   // 0x101956a4 (56 bytes, untranslated)

// Globals of this panel (the Mac's addresses in the comments).
InterfaceWindow* gAllKidsMsgWindow;                // 0x103947c8  the panel window
InterfaceListControl2* gAllKidsListControl;        // 0x103947c4  the list in it
int32_t gAllKidsListFormat;                        // 0x103947bc  the row height LCFormat keeps
int32_t gAllKidsFontIndex = 7;                     // 0x102a8878
int32_t gAllKidsSelectedIndex = 1;                 // 0x102a8880

namespace {

// A list entry's data word is a pointer on the Mac; on the host it is the arena offset (Ptr32.h).
int32_t DataOf(const CThing* thing) { return thing != nullptr ? static_cast<int32_t>(tpw::ArenaOffset(thing)) : 0; }
CThing* ThingOf(int32_t data) {
    Ptr32<CThing> pointer;
    pointer.mOffset = static_cast<uint32_t>(data);
    return pointer.get();
}

uint32_t Word(const void* base, uint32_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 4);
    return v;
}

// The six cells one person gives the list (the stack block at 0x38 of FillAllKidsListCallback, 0x44 of UpdateListEntries).
void MakeCells(CThing* thing, UI_LCAdd* cells, uint32_t turnShift) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(thing);
    CPerson* person = reinterpret_cast<CPerson*>(thing);
    for (int i = 0; i < 6; ++i) cells[i].mRaw = 0;
    cells[0].mInt = static_cast<int32_t>(Word(p, 0x1d8));
    cells[1].mInt = static_cast<int32_t>(Word(p, 0x1a0));
    uint32_t turns = person->GetTimeInPark();
    CParkTime* parkTime = &gGameWorld->mParkTime;
    TbTimeDiff span = parkTime->GetFunnyTimeForGameTurns(turns << turnShift);   // the update asks for four times as many turns (slwi 2)
    cells[2].mInt = static_cast<int32_t>(span.mTicks / 0x861c46800LL);   // 0x10237e1c: 64-bit division by 36,000,000,000
    cells[3].mInt = static_cast<int32_t>(Word(p, 0x1c4));
    int32_t lives = static_cast<int32_t>(Word(p, 0x30));
    cells[4].mInt = lives != 0 ? lives - 1 : 999;
    float level;
    std::memcpy(&level, p + 0x19c, 4);
    int32_t whole = ppc_fctiwz(static_cast<double>(level));
    cells[5].mInt = static_cast<int32_t>((static_cast<uint32_t>(whole) & 0xffu) << 10) / 100;
}

}  // namespace

// 0x10194880 (276 bytes): adds the person to the list.
int32_t FillAllKidsListCallback(CThing* thing) {
    UI_LCAdd cells[6];
    MakeCells(thing, cells, 0);
    gAllKidsListControl->AddItem(cells, DataOf(thing), -1, 0);
    return 1;
}

namespace AllKids {

// 0x101949cc (344 bytes): refreshes every row of the list from its person (last row first).
void UpdateListEntries() {
    if (gAllKidsMsgWindow == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAllKidsMsgWindow->GetWindow(0x1e497));
    for (int32_t position = list->mItemCount - 1; position >= 0; --position) {
        int32_t data;
        list->GetData(position, &data);
        CThing* thing = ThingOf(data);
        UI_LCAdd cells[6];
        MakeCells(thing, cells, 2);
        list->ChangeItem(position, cells, 0x3f, position == 0, -1);
    }
}

}  // namespace AllKids

// 0x10194b4c (616 bytes): the panel's message handler.
int32_t AllKidsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        switch (a) {
        case -2:
            RemoveAllKidsPanel();
            return 0;
        case 0x1e499:
            CreateParkStatusPanel();
            return 0;
        case 0x1e49a:
            CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr);
            return 0;
        case 0x1e49b:
            CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr);
            return 0;
        default:
            break;
        }
    } else if (message == 5) {
        RemoveAllKidsPanel();
        return 1;
    } else if (message == 0x10) {
        if (a == 0x80083) AllKids::UpdateListEntries();
    } else if (message == 0x14) {
        gAllKidsMsgWindow = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11e));
    } else if (message == 0x15) {
        UIMain_SetTitleText(window, 0x1e498, const_cast<char16_t*>(UIMain_GetString(0x70)));
    } else if (message == 0x400) {
        InterfaceWindow* list = window->GetWindow(a);
        if (list != nullptr && b != -1) {
            int32_t data = 0;
            static_cast<InterfaceListControl2*>(list)->GetData(b, &data);
            gVisitorStatusPanel.Show(ThingOf(data));
        }
        return 0;
    } else if (message == 0x402) {
        InterfaceWindow* list = window->GetWindow(a);
        if (list != nullptr && b != -1) {
            int32_t data = 0;
            static_cast<InterfaceListControl2*>(list)->GetData(b, &data);
            CThing* thing = ThingOf(data);
            UIMain_ZoomToThing(thing->GetThingId());
            RemoveAllKidsPanel();
            return 0;
        }
    } else if (message == 0x406) {
        gAllKidsSelectedIndex = b;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x10194df0 (408 bytes): makes the list-format window (the row height is always 0x38 in the end).
InterfaceWindow* LCFormat() {
    if (gAllKidsListFormat == 0) {
        // The Mac calls a virtual through the font entry's table here (__ptr_glue); the class is not recovered, so the height getter stands in.
        InterfaceFont* font = GetFontTableEntry(static_cast<uint32_t>(gAllKidsFontIndex));
        int32_t height = static_cast<int16_t>(font->GetHeight());
        int32_t screen = gUIRealScreenHeight;
        gAllKidsListFormat = screen != 0 ? height * 0x600 / screen + 6 : height * 0x600;
    }
    gAllKidsListFormat = 0x38;
    void* memory = ::operator new(sizeof(InterfaceWindow));   // 0x138 bytes on the Mac; the host window is larger
    InterfaceWindow* window = new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gAllKidsListFormat));
    window->CreateObject(1, 1, 3, 0, 0x2c, 0, 0x2c);
    window->CreateObject(1, 1, 0x3e7, 0, 0, 0, 0);
    window->CreateObject(1, 0x11, 4, 0, 0, 0, 0);
    window->CreateObject(6, 1, 4, 0, 0, 0, 0);
    window->CreateObject(9, 0x11, 5, 0, 0, 0, 0);
    return window;
}
