// ui_main.cpp (main app), unit w142: ContextMouse (0x10182548, 3,508 bytes), the park pointer. Every frame the game window asks it what
// the pointer is over: it picks the help text and the cursor from the mouse mode, the thing under the pointer (gIntrface's staff or
// guest), the shift keys and the map cell (gIntrface.mF24, the 1 based cell id), and highlights the ride the pointer is on.
//
// Words the original keeps (names invented, ui_main.h): gGameWinWordC38 is the KIND of the last help text shown (0 none, 2 grass, 1 and 3
// the cell kinds, 4 an upgradable, 5 a ride, 6 a cell type 0x15, 7 a staff member, 8 to 0xc a guest or such by type, 0xd the ticket booth,
// 0xe a park entrance); gGameWinWordC34 is the thing (or 0) the text is about. A text is only built again when the kind or the thing
// changed since the last call (`old`, the kind at entry; the kind is cleared to 0 at the start of every call). Where a path ends with
// "finish" below, the Mac ends the function without asking for a cursor.
//
// The things are reached at the Mac's offsets (kind byte +2, a staff member's word +0x1d8, a guest's +0x28), as src/rsse_ride_tick.cpp does.
#include <cstdint>
#include <cstring>
#include <new>

#include "Debug.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbAudioSystem.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "intrface.h"
#include "ui_main.h"
#include "ui_tooltip.h"

namespace {
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024d8b2 + 6
char16_t gEmptyHelp[1] = {0};   // 0x1024da3e

uint8_t ByteAt(const void* base, std::size_t offset) {
    return static_cast<const uint8_t*>(base)[offset];
}
uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}
// The Mac word for a thing's address (gGameWinWordC34 is a 32 bit word).
int32_t PtrWord(const void* p) {
    return static_cast<int32_t>(reinterpret_cast<uintptr_t>(p));
}

// The mouse, made as UIMain_SetPanel makes it (operator new(1), the default mode operator new(8), the mode assigned, the two words -1).
CMouse* MakeMouse() {
    CMouse* mouse = new (std::nothrow) CMouse;
    if (mouse != nullptr) {
        CMMDefault* mode = new (std::nothrow) CMMDefault;
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
    }
    return mouse;
}

// The thing of a map cell's thing id (0 when there is none; the null id of the Mac reads 0). The asserts are the Mac's.
void* ThingOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return gThingArray[id].mThing;
}

CMapCell* MapCellOf(uint32_t id) {   // the 1 based cell id
    return reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + static_cast<uint32_t>((id - 1) * 0x44u));
}
CTrackCell* TrackCellOf(uint32_t id) {
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld->GetMap()) + 0x110000 + static_cast<uint32_t>((id - 1) * 0x28u));
}

TbITable* HelpTable() {
    return static_cast<TbITable*>(gText.mResourcePointers[7]);
}

void ShowHelp() {
    SetHelpTextString(gUIMainConvertBuffer);
}
void ShowConst(uint32_t id) {
    SetHelpTextString(UIMain_ConvertMulti(const_cast<unsigned char*>(HelpTable()->GetConstString(id))));
}
}  // namespace

void ContextMouse(int32_t context) {
    // The shift keys: the keyboard object is made on first use (0xc bytes on the Mac; NULL when it could not be made).
    uint32_t keys = 0;
    if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new (std::nothrow) SamsUtilities::UKeyboard;
    const uint32_t bits = SamsUtilities::gKeyboard->GetModifiers();
    if ((bits & 0x200) != 0) keys |= 3;
    if ((bits & 0x1100) != 0) keys |= 0xc;
    if ((bits & 0x800) != 0) keys |= 0x30;

    const int32_t old = gGameWinWordC38;
    const int32_t staff = StaffPanelUp();
    gGameWinWordC38 = 0;

    int32_t mode;
    if (context != 0) {
        mode = 1;
    } else {
        if (gMouse == nullptr) gMouse = MakeMouse();
        mode = (gMouseMode != nullptr) ? gMouseMode->GetMode() : 0;
    }
    const int32_t cellMode = LLCC_SystemGetMode();

    char16_t* const buffer = gUIMainConvertBuffer;
    TbITable* const table = HelpTable();
    const uint16_t id = gIntrface.mF24;

    if (mode == 8 || (mode == 3 && static_cast<uint32_t>(cellMode - 0x15) <= 1u)) {
        // The ride under the pointer in the ride, or ride deleting, modes.
        if (gRideHighlightKeep != 0) {
            HighlightRide();
            return;
        }
        if (id == 0) return;
        const CMapCell* cell = MapCellOf(id);
        if (static_cast<int32_t>(cell->mType) != 0x15) return;
        const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing = MapCellOf(cell->mParentID)->GetObjectThingId();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        if (gThingArray[thing.mIndex].mThing == UI_GetSelectedObject()) HighlightRide();
        return;
    }

    if (mode == 9) {
        if (id == 0) return;
        uint32_t cellId = static_cast<uint32_t>(id) - 1;
        uint16_t found;
        LLCR_OverRide(&found, static_cast<int32_t>(cellId & 0x7f), static_cast<int32_t>(cellId >> 7));
        if (found == 0) {
            if (gGameWinWordC34 == 0 && old == gGameWinWordC38) return;
            gGameWinWordC34 = 0;
            table->FormatString(buffer, 0x3fff, 0x243);
            ShowHelp();
            return;
        }
        void* const obj = ThingOf(found);
        if (ByteAt(obj, 2) != 3) return;
        CObject* const object = reinterpret_cast<CObject*>(obj);
        if (object->UpgradesWhat().mIndex != 0) return;
        if (object->GetDBInfo()->mUIType != 0) return;
        if (gGameWinWordC34 != PtrWord(obj) || old != gGameWinWordC38) {
            gGameWinWordC34 = PtrWord(obj);
            object->GetUIName(buffer, 0x3fff);
            TbUniStringVTag tag(2, buffer);
            table->FormatString(buffer, 0x3fff, 0x244, &tag);
            ShowHelp();
        }
        HighlightRide();
        return;
    }

    if (static_cast<uint32_t>(mode) > 1u) {
        // Any other mode: no help text, no cursor.
        if (old != gGameWinWordC38) SetHelpTextString(gEmptyHelp);
        gGameWinWordC34 = 0;
        return;
    }

    // Mode 0 or 1: the thing under the pointer, then the cell.
    void* const pointed = gIntrface.mPointedStaffOrGuest;
    int16_t cursor = 0;
    bool done = false;
    const bool staffCase = (staff == 2 && keys == 0) || (keys == 3 && staff == 0);

    if (staffCase) {
        if (pointed != nullptr && ByteAt(pointed, 2) == 1) {
            gGameWinWordC38 = 7;
            cursor = 2;
            if (!(old == 7 && gGameWinWordC34 == PtrWord(pointed))) {
                const uint32_t word = WordAt(pointed, 0x1d8);
                if (word == 0) {
                    table->FormatString(buffer, 0x3fff, 0x22a);
                } else {
                    TbIntegerVTag tag(3, static_cast<int32_t>(word));
                    table->FormatString(buffer, 0x3fff, 0x229, &tag);
                }
                ShowHelp();
            }
            gGameWinWordC34 = PtrWord(pointed);
            UI_SetGameWindowCursor(2);
            return;
        }
        if (old != gGameWinWordC38) SetHelpTextString(gEmptyHelp);
        UI_SetGameWindowCursor(0);
        return;
    }

    if (pointed != nullptr) {
        const uint8_t kind = ByteAt(pointed, 2);
        int32_t shown = 0;
        switch (kind) {
            case 4: shown = 9; break;
            case 5: shown = 8; break;
            case 6: shown = 0xa; break;
            case 7: shown = 0xb; break;
            case 8: shown = 0xc; break;
            default: break;
        }
        if (shown != 0) {
            cursor = 2;
            done = true;
            gGameWinWordC38 = shown;
            if (!(old == gGameWinWordC38 && gGameWinWordC34 == PtrWord(pointed))) {
                CStaff* const person = reinterpret_cast<CStaff*>(pointed);
                const char16_t* name = person->GetName();
                TbUniStringVTag tag(2, name);
                table->FormatString(buffer, 0x3fff, 0x22b, &tag);
                ShowHelp();
                person->BeingPointedAt();
            }
            gGameWinWordC34 = PtrWord(pointed);
        }
    }
    if (staff != 0) done = true;
    if (done || id == 0) {
        UI_SetGameWindowCursor(cursor);
        return;
    }

    // The park entrances and the ticket booths.
    const CMap* const map = gGameWorld->GetMap();
    const uint32_t e0 = map->GetParkEntrancePos(0).mIndex;
    const uint32_t e1 = map->GetParkEntrancePos(1).mIndex;
    const uint32_t b0 = map->GetTicketBoothPos(0).mIndex;
    const uint32_t b1 = map->GetTicketBoothPos(1).mIndex;
    if (id == e0 || id == e1 || id == ((e0 - 1) & 0xffff) || id == ((e1 + 1) & 0xffff)) {
        gGameWinWordC38 = 0xe;
        if (old != 0xe) ShowConst(0x1cb);
        gGameWinWordC34 = 0;
        HighlightRide();
        UI_SetGameWindowCursor(2);
        return;
    }
    if (id == b0 || id == b1) {
        gGameWinWordC38 = 0xd;
        if (old != 0xd) ShowConst(0x1cc);
        gGameWinWordC34 = 0;
        UI_SetGameWindowCursor(2);
        return;
    }

    // The cell.
    CMapCell* const cell = MapCellOf(id);
    CTrackCell* track = TrackCellOf(id);
    const uint32_t cellId = static_cast<uint32_t>(id) - 1;
    uint16_t found;
    LLCR_OverRide(&found, static_cast<int32_t>(cellId & 0x7f), static_cast<int32_t>(cellId >> 7));
    void* thing = nullptr;   // r27
    if (found != 0) thing = ThingOf(found);

    enum { kCursorCall, kF40, kFA8, kG090, kF7C } next = kG090;
    const uint32_t trackType = static_cast<uint32_t>(track->mType);
    if (trackType == 0xb || trackType == 0x10) {
        next = kF40;
    } else if (trackType == 0xc) {
        CTrackCell* const parent = TrackCellOf(track->mParentID);
        if (static_cast<uint32_t>(parent->mType) == 0x19) {
            done = true;
            next = kG090;
        } else {
            track = parent;
            next = kF40;
        }
    }
    if (next == kF40) next = (cell->IsRide() != 0 || cell->IsExit() != 0 || cell->IsEntrance() != 0) ? kF7C : kFA8;
    if (next == kF7C) {
        if (thing == nullptr) return;
        next = (reinterpret_cast<CObject*>(thing)->UpgradesWhat().mIndex == 0) ? kG090 : kFA8;
    }
    if (next == kFA8) {
        const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> owner = MapCellOf(track->mParentID)->GetObjectThingId();
        if (owner.mIndex == 0) {
            gGameWinWordC34 = 0;
            return;
        }
        void* const ride = ThingOf(owner.mIndex);
        cursor = (WordAt(ride, 0x28) != 0) ? 0xa : 6;
        gGameWinWordC38 = 5;
        if (old != 5) ShowConst(0x1be);
        gGameWinWordC34 = 0;
        done = true;
        HighlightRide();
    }
    // G090
    if (!done && cell->IsGrass() != 0) {
        gGameWinWordC38 = 2;
        done = true;
        cursor = 3;
        if (old != 2) ShowConst(0x1b9);
        gGameWinWordC34 = 0;
    }
    if (done) {
        UI_SetGameWindowCursor(cursor);
        return;
    }

    // The cell's type: 1 and 3 name the cell, 4, 9 and 10 an upgradable ride, 21 a ride.
    const uint32_t cellType = static_cast<uint32_t>(cell->mType);
    int32_t kind = 0;
    uint32_t textId = 0;
    switch (cellType) {
        case 1: kind = 1; cursor = 3; textId = 0x1ba; break;
        case 3: kind = 3; cursor = 4; textId = 0x1bc; break;
        case 4:
        case 9:
        case 10: kind = -1; break;
        case 0x15: kind = 6; cursor = 0xb; textId = 0x1c0; break;
        default: break;
    }
    if (kind == -1) {
        if (thing != nullptr) {
            CObject* const object = reinterpret_cast<CObject*>(thing);
            if (object->UpgradesWhat().mIndex == 0 || object->GetDBInfo()->Value(kRideSlot_UpgradeType).mInteger == 2) {
                gGameWinWordC38 = 4;
                if (PtrWord(thing) != gGameWinWordC34) {
                    gGameWinWordC34 = PtrWord(thing);
                    object->GetUIName(buffer, 0x3fff);
                    TbUniStringVTag tag(2, buffer);
                    table->FormatString(buffer, 0x3fff, 0x226, &tag);
                    ShowHelp();
                }
                HighlightRide();
                cursor = 2;
            }
        }
        UI_SetGameWindowCursor(cursor);
        return;
    }
    if (kind == 0) {
        if (old != gGameWinWordC38) SetHelpTextString(gEmptyHelp);
        gGameWinWordC34 = 0;
        UI_SetGameWindowCursor(0);
        return;
    }
    gGameWinWordC38 = kind;
    if (old != kind) ShowConst(textId);
    gGameWinWordC34 = 0;
    if (kind == 6) HighlightRide();
    UI_SetGameWindowCursor(cursor);
}
