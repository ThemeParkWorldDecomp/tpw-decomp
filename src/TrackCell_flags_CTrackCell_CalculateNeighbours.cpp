// CTrackCell::CalculateNeighbours (0x100a0bd4, 1,732 bytes): after a cell's mode changed, joins it to the neighbours that are track
// (modes 0xb ... ) or hoarding cells (type 0x19) in the four compass directions, when the argument offsets say 2 or -2.
#include "TrackCell.h"

#include "LLCellSystem.h"
#include "ObjectParts.h"

int32_t gTrackNbrOn485C;
uint16_t gTrackNbr290;
uint16_t gTrackNbr292;
uint16_t gTrackNbr294;
uint16_t gTrackNbr296;
uint16_t gTrackNbr298;
uint16_t gTrackNbr29A;
uint16_t gTrackNbr29C;
uint16_t gTrackNbr29E;

namespace {

// One direction. `hoarding` selects the hoarding test (type 0x19) instead of IsTrack.
void Join(CTrackCell* self, const MapDelta& d, bool hoarding, const uint16_t& alt, uint8_t mine, uint8_t theirs) {
    MapDelta delta = d;
    CTrackCell* n = self->GetNeighbouringCell(delta, 2);
    if (n == nullptr) return;
    if (hoarding) {
        if (static_cast<int32_t>(n->mType) != 0x19) return;
    } else {
        if (n->IsTrack() == 0) return;
    }
    if (self->NumberOfNeighbours() >= 4) return;
    if (n->NumberOfNeighbours() >= 4) return;
    const uint16_t parent = n->mParentID;
    if (parent == gLLCCDB68 || parent == alt) {
        self->ModifyNeighbours(mine);
        n->ModifyNeighbours(theirs);
    }
}

// The jump table at 0x1029e2a0, indexed by mode - 0xb: 1 track cells, 2 hoarding cells, 0 nothing.
const uint8_t kKind[15] = {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 2};

}  // namespace

void CTrackCell::CalculateNeighbours(CELL_MODES mode, int32_t a, int32_t b) {
    int32_t x, y;
    if (gTrackNbrOn485C != 0) {
        if (gLLCCE288 == 0x80 || gLLCCE28C == 0x80) {
            x = 0;
            y = 0;
        } else {
            x = gLLCCE288;
            y = gLLCCE28C;
        }
    } else {
        x = a;
        y = b;
    }

    const uint32_t index = static_cast<uint32_t>(static_cast<int32_t>(mode) - 0xb);
    if (index <= 0xe && kKind[index] != 0) {
        const bool h = kKind[index] == 2;
        if (y == 2) Join(this, gMapDeltaMinusY, h, h ? gTrackNbr298 : gTrackNbr290, 1, 0x10);
        if (x == -2) Join(this, gMapDeltaPlusX, h, h ? gTrackNbr29A : gTrackNbr292, 4, 0x40);
        if (y == -2) Join(this, gMapDeltaPlusY, h, h ? gTrackNbr29C : gTrackNbr294, 0x10, 1);
        if (x == 2) Join(this, gMapDeltaMinusX, h, h ? gTrackNbr29E : gTrackNbr296, 0x40, 4);
    }

    gLLCCE288 = a;
    gLLCCE28C = b;
}
