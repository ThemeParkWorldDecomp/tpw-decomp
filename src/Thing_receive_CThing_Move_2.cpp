#include "AdvisorQueue.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "MapCell.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "World.h"

#include <cstring>

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdTag;

// 0x1012e5b4: moves the thing to the cells of two MapPos; a move inside the same cell only stores the positions.
void CThing::Move(MapPos x, MapPos y) {
    if (x.mCell == mCellX && y.mCell == mCellY) {
        mCellX = x.mCell;
        mCellXSub = x.mOffset;
        mCellY = y.mCell;
        mCellYSub = y.mOffset;
        return;
    }
    GAME_ASSERTMSG(static_cast<uint32_t>(x.mCell) + static_cast<uint32_t>(y.mCell) * 0x80 <= 0x4000,
                   "Attempt to move thing to invalid cell");
    ThingIdTag id;
    id.mIndex = mThingId;
    CMapCell* cells = reinterpret_cast<CMapCell*>(gWorldMap);
    uint16_t oldCell = static_cast<uint16_t>(mCellX + (mCellY * 0x80 + 1));
    cells[static_cast<int32_t>(oldCell) - 1].RemoveThing(id);
    mCellX = x.mCell;
    mCellXSub = x.mOffset;
    mCellY = y.mCell;
    mCellYSub = y.mOffset;
    uint16_t newCell = static_cast<uint16_t>(mCellX + (mCellY * 0x80 + 1));
    cells[static_cast<int32_t>(newCell) - 1].InsertThing(id);
}

// 0x1012e9f8: takes the thing out of its cell, frees a ride's instance, clears the selection global for people and staff
// (kinds 1, 4 to 8 and 0x12) and drops its subscriptions (the message centre is made on first use).
void CThing::FinalDestroy() {
    ThingIdTag id;
    id.mIndex = mThingId;
    CMapCell* cells = reinterpret_cast<CMapCell*>(gWorldMap);
    uint16_t cell = static_cast<uint16_t>(mCellX + (mCellY * 0x80 + 1));
    cells[static_cast<int32_t>(cell) - 1].RemoveThing(id);
    if (mKind == 3) DestroyRideInstance(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(this) + 0x1c));
    bool person = mKind == 1 || (mKind >= 4 && mKind <= 8) || mKind == 0x12;
    if (person && this == gESPF20) gESPF20 = nullptr;
    if (gMessageCentre == nullptr) {
        void* mem = staffhq_callee::OperatorNew(0x1d0);
        if (mem != nullptr) staffhq_callee::ConstructSubscriptions(mem);
        gMessageCentre = static_cast<CMessageCentre*>(mem);
    }
    gMessageCentre->UnsubscribeAll(*this);
}

namespace {

// The Mac's mX and mY are the byte pairs (cell, sub) read as one big-endian short; the templates take a host short. PairToShort turns
// the pair into that short in place and ShortToPair back, so the templates see the field's own address and a value they leave stays.
void PairToShort(uint8_t* b) {
    const int16_t v = static_cast<int16_t>((b[0] << 8) | b[1]);
    std::memcpy(b, &v, 2);
}

void ShortToPair(uint8_t* b) {
    int16_t v;
    std::memcpy(&v, b, 2);
    b[0] = static_cast<uint8_t>(static_cast<uint16_t>(v) >> 8);
    b[1] = static_cast<uint8_t>(v);
}

}  // namespace

// 0x1012e010: the position and the cell list links; versions 0 and 1 only; a load clears the dead flag.
int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    int16_t* px = reinterpret_cast<int16_t*>(&mCellX);
    int16_t* py = reinterpret_cast<int16_t*>(&mCellY);
    ThingIdTag* child = reinterpret_cast<ThingIdTag*>(&mCellNext);
    ThingIdTag* parent = reinterpret_cast<ThingIdTag*>(&mCellPrev);
    int32_t failed;
    if (save != 0) {
        if (version < 2 && version >= 0) {
            PairToShort(&mCellX);
            failed = SAVEVAR<int16_t>(file, px, "mX", 0);
            ShortToPair(&mCellX);
            if (failed != 0) return 1;
            PairToShort(&mCellY);
            failed = SAVEVAR<int16_t>(file, py, "mY", 0);
            ShortToPair(&mCellY);
            if (failed != 0) return 1;
            if (SAVEVARSTRUCT<ThingIdTag>(file, child, "mMapChild", 0) != 0) return 1;
            if (SAVEVARSTRUCT<ThingIdTag>(file, parent, "mMapParent", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            PairToShort(&mCellX);
            failed = LOADVAR<int16_t>(file, px, "mX", 0);
            ShortToPair(&mCellX);
            if (failed != 0) return 1;
            PairToShort(&mCellY);
            failed = LOADVAR<int16_t>(file, py, "mY", 0);
            ShortToPair(&mCellY);
            if (failed != 0) return 1;
            if (LOADVARSTRUCT<ThingIdTag>(file, child, "mMapChild", 0) != 0) return 1;
            if (LOADVARSTRUCT<ThingIdTag>(file, parent, "mMapParent", 0) != 0) return 1;
            mDead = 0;
        }
    }
    return 0;
}
