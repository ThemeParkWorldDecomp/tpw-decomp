// TrainingCentre.cpp: name proven by __sinit_TrainingCentre_cpp in the main application.

#include "analyser.h"
#include "CellBase.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/samsutils.h"
#include "RegionEffectCell.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"
#include "ThingMessages.h"
#include "ThingParts.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TrackCell.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// The world's cells (main app of SimTheme Park 2.04b3; the inventory's file names for these are unreliable; unit w113): the three
// cell constructors and the two grid registrations. The six grid globals are defined by the unit's test file and, in the launcher,
// by the generated zero definitions (nothing else reads them).

// 0x1012fb08 (68 bytes): all 0x28 bytes zeroed, then the type byte.
CTrackCell::CTrackCell() {
    std::memset(static_cast<void*>(this), 0, sizeof(CTrackCell));
    mKind = 2;
}

// CTrackCell (main app, SimTheme Park 2.04b3; the inventory files its code under TrainingCentre.cpp, as TrackCell.h line 1 says: unreliable; unit w179):
// HasBeenModifiedFrom 0x1012fb74, Serialise 0x1012fbc0 and the two GetNeighbouringCell 0x1012fcb8 (by offset) and 0x1012fddc (by compass direction).
//
// Quirks kept: Serialise throws away CCellBase::Serialise's answer (as CMapCell's); the track neighbour's cell lies at the SCALED offset (the
// listing reads the product's copy, 0x50(r1) and 0x54(r1), for the validity test AND the cell), where CMapCell::GetNeighbouringCell uses the
// unscaled delta for the cell; the cell is found from gWorld + 0x2d8 + 0x110000 (not gWorldMap); a direction byte that is not one single compass
// bit asserts and answers NULL.

namespace {

constexpr const char* kNameSegmentNumber = "mSegmentNumber";   // the Mac's string pool at 0x102469c8
constexpr const char* kBadDirection = "Incorrect use of function CTrackCell::GetNeighbouringCell( Direction , offset)";   // 0x102469d7 (no final newline)

}  // namespace

// 0x1012fb74, 8 bytes.
int32_t CTrackCell::HasBeenModifiedFrom(const CTrackCell& other) {
    (void)other;
    return 1;
}

// 0x1012fbc0, 196 bytes: the shared part first (its answer is ignored), then for 0 <= version < 2 the segment number, saved when `save` is nonzero.
int32_t CTrackCell::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    CCellBase::Serialise(file, save, version, unused);
    if (version >= 2 || version < 0) return 0;
    if (save != 0) {
        if (SAVEVAR(file, &mSegmentNumber, kNameSegmentNumber, 0) != 0) return 1;
    } else {
        if (LOADVAR(file, &mSegmentNumber, kNameSegmentNumber, 0) != 0) return 1;
    }
    return 0;
}

// 0x1012fcb8, 228 bytes. The product of the delta and the offset serves for the validity test and for the cell.
CTrackCell* CTrackCell::GetNeighbouringCell(MapDelta delta, int32_t offset) {
    const MapDelta scaled = delta * offset;
    const uint32_t i = static_cast<uint32_t>(mCellId) - 1u;
    if (!CMap::IsXYValid(static_cast<int32_t>((i & 0x7fu) + static_cast<uint32_t>(scaled.mX)),
                         static_cast<int32_t>((i >> 7) + static_cast<uint32_t>(scaled.mY)))) return nullptr;
    const uint16_t id = static_cast<uint16_t>(static_cast<uint32_t>(scaled.mX) + (static_cast<uint32_t>(scaled.mY) << 7) + mCellId);
    return reinterpret_cast<CTrackCell*>(reinterpret_cast<uint8_t*>(gWorld) + 0x2d8 + 0x110000 + (static_cast<int32_t>(id) - 1) * 0x28);
}

// 0x1012fddc, 412 bytes: the compass delta of the direction bit, then the offset form.
CTrackCell* CTrackCell::GetNeighbouringCell(uint8_t direction, int32_t offset) {
    const MapDelta* source;
    switch (direction) {
        case 1: source = &gMapDeltaMinusY; break;
        case 2: source = &gMapDeltaPlusXMinusY; break;
        case 4: source = &gMapDeltaPlusX; break;
        case 8: source = &gMapDeltaPlusXPlusY; break;
        case 0x10: source = &gMapDeltaPlusY; break;
        case 0x20: source = &gMapDeltaMinusXPlusY; break;
        case 0x40: source = &gMapDeltaMinusX; break;
        case 0x80: source = &gMapDeltaMinusXMinusY; break;
        default:
            GAME_ASSERTMSG(0, kBadDirection);
            return nullptr;
    }
    MapDelta local = *source;
    return GetNeighbouringCell(local, offset);
}

// CTrainingCentre::Serialise and UpdateMonthly (unit CTrainingCentre_Serialise).

int32_t gTrainingCounts[5] = {0, 0, 0, 0, 0};   // 0x102a4408
uint16_t gTrainingEnd1 = 0;                      // 0x102a441c
uint16_t gTrainingEnd2 = 0;                      // 0x102a441e: the null thing id, like gTrainingEnd1 (the data image's halfword is 0;
                                                 
// the 0x1013 that stood here is the next halfword, and the walk ran past the list's end)

// 0x10130008, 332 bytes.
int32_t CTrainingCentre::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    const char* name = "mBudget[i]";   // 0x10246afc; "mHaveEverTrained" follows at +0xb
    if (save != 0) {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 5; ++i) {
                if (SAVEVAR<int32_t>(file, reinterpret_cast<int32_t*>(&mF00[i]), name, 0) != 0) return 1;
            }
            if (SAVEVAR<bool>(file, reinterpret_cast<bool*>(&mF14), "mHaveEverTrained", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            for (int32_t i = 0; i < 5; ++i) {
                if (LOADVAR<int32_t>(file, reinterpret_cast<int32_t*>(&mF00[i]), name, 0) != 0) return 1;
            }
            if (LOADVAR<bool>(file, reinterpret_cast<bool*>(&mF14), "mHaveEverTrained", 0) != 0) return 1;
        }
    }
    return 0;
}

namespace {
const char* kAllocZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10246b18

// The index into the count words for a staff kind (5, 4, 6, 7, 8), or -1.
int32_t SlotOfKind(uint8_t kind) {
    switch (kind) {
        case 5: return 0;
        case 4: return 1;
        case 6: return 2;
        case 7: return 3;
        case 8: return 4;
    }
    return -1;
}
}  // namespace

// 0x1013018c, 928 bytes.
void CTrainingCentre::UpdateMonthly() {
    int32_t counts[5];
    for (int32_t i = 0; i < 5; ++i) counts[i] = gTrainingCounts[i];
    uint16_t cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gTrainingEnd1) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAllocZero);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        int32_t slot = SlotOfKind(thing->mKind);
        if (slot >= 0) counts[slot] += 1;
        cur = gGameWorld->GetNextUsedThing(thing).mIndex;
    }
    cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gTrainingEnd2) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAllocZero);
        CThing* thing = static_cast<CThing*>(gThingArray[cur].mThing);
        int32_t slot = SlotOfKind(thing->mKind);
        if (slot >= 0) {
            int32_t amount = static_cast<int32_t>(mF00[slot]) / counts[slot];
            reinterpret_cast<CStaff*>(thing)->TrainMe(amount);
        }
        cur = gGameWorld->GetNextUsedThing(thing).mIndex;
    }
}

// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.

uint32_t gTrainingCentreSinitHigh;  // 0x1034c6f8
uint32_t gTrainingCentreSinitLow;   // 0x1034c6fc
void EventHistType_Ctor(CThoughts::EventHistType*) {}   // 0x1012ed48: blr

void __sinit_TrainingCentre_cpp() {
    gTrainingCentreSinitLow = 0;
    gTrainingCentreSinitHigh = 0x10000;
}

int32_t gTbErrorZero;               // 0x102a4400

// 0x1012f5a8
template<>
int32_t LOADVARSTRUCT<CThoughts::EventHistType>(void* file, CThoughts::EventHistType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    TbError result = LbFile_Read(file, value, 4, &read);
    // The comparison against the TbError constant: (result >= 0) == (constant == 0).
    if (result.Succeeded() != (gTbErrorZero == 0) || read != 4) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should be a struct");
    // The two halves from the file's order (little-endian), as SAVEVARSTRUCT<EventHistType> writes them: the Mac swaps each, the
    // host reads them as they are (an unconditional swap here read a host-written save's history back byte-swapped; v5L15).
    value->mFirst = serialise::UnswapWord(value->mFirst);
    value->mSecond = serialise::UnswapWord(value->mSecond);
    return 0;
}

// SAVEVARSTRUCT<CThoughts::EventHistType> (0x1012f9a0, 260 bytes): saves a four-byte entry of two 16-bit fields, each swapped for the file.

template<> int32_t SAVEVARSTRUCT<CThoughts::EventHistType>(void* file, CThoughts::EventHistType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    CThoughts::EventHistType copy;
    copy.mFirst = value->mFirst;
    copy.mSecond = value->mSecond;
    GAME_ASSERTMSG(1, "should be a struct");
    copy.mFirst = serialise::SwapWord(copy.mFirst);
    copy.mSecond = serialise::SwapWord(copy.mSecond);
    if (!LbFile_Write(file, &copy, 4, &written).Succeeded() || written != 4) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}

// 0x1012f844, 260 B. The comparison against the data word at 0x102a43fc is the TbError idiom (see TbError::Succeeded).
template<> int32_t SAVEVAR<CThoughts::ThoughtType>(void* file, CThoughts::ThoughtType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(0));
    uint32_t written = 0;
    uint32_t copy = static_cast<uint32_t>(*value);
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where SAVEVARSTRUCT should be used instead");
    copy = serialise::SwapWord(copy);
    if (!LbFile_Write(file, &copy, 4, &written).Succeeded() || written != 4) {
        GAME_ASSERTMSG(0, "File write failed");
        return 1;
    }
    return 0;
}

// 0x1012f700, 236 B. The twin of SAVEVAR<CThoughts::ThoughtType>: one word read, swapped in place. The TbError comparison is the
// (result >= 0) idiom against the zero word at 0x102a4404.
template<> int32_t LOADVAR<CThoughts::ThoughtType>(void* file, CThoughts::ThoughtType* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, CSaveLoad::LogfileState(1));
    uint32_t read = 0;
    if (!LbFile_Read(file, value, 4, &read).Succeeded() || read != 4) {
        GAME_ASSERTMSG(0, "File read failed");
        return 1;
    }
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where LOADVARSTRUCT should be used instead");
    *value = serialise::UnswapWord(*value);
    return 0;
}
