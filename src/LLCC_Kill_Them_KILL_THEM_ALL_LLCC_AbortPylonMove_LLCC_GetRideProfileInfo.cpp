#include <cstdint>
#include "CellBase.h"
#include "LLCellSystem.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "ui_main.h"

int32_t gLLCCDB7C[0x102];  // 0x102ddb7c (name invented)

namespace {
// The jump table at 0x1029e0bc: the value the table gets for each cell kind 0 to 0x17 (0: nothing).
const int32_t kProfileKind[24] = {0, 4, 0, 0, 1, 0, 0, 0, 0, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
}  // namespace

// 0x1008f880 (280 bytes): copies the shape's size into the table, clears 16 x 16 words, then stores a value (1 to 4) at x * 16 + z for
// every cell of the shape whose kind (a word at 8 + z * 0xa0 + x * 8) is in the jump table. Answers the table.
int32_t* LLCC_GetRideProfileInfo(void* ride) {
    // The Mac reads the word at 0x18 (value slot 3 of the ride's balance); the host's slot holds a native pointer.
    const int32_t* s = static_cast<const int32_t*>(static_cast<RideInfo*>(ride)->Value(static_cast<RideBalanceSlot>(3)).mShape);
    int32_t* table = gLLCCDB7C;
    table[0x100] = s[0];
    table[0x101] = s[1];
    for (int32_t z = 0; z < 16; z++)
        for (int32_t x = 0; x < 16; x++) table[x * 16 + z] = 0;
    for (int32_t z = 0; z < s[1]; z++) {
        for (int32_t x = 0; x < s[0]; x++) {
            uint32_t kind = static_cast<uint32_t>(s[2 + z * 40 + x * 2]);
            if (kind > 0x17) continue;
            int32_t v = kProfileKind[kind];
            if (v != 0) table[x * 16 + z] = v;
        }
    }
    return table;
}

// 0x1008f0b4 (196 bytes): remembers the pylon's cell, takes the mesh instance word from the cell's track cell (clearing it there),
// deletes the map cell unless flag, and marks a brace drag pending.
void LLCC_StartPylonMove(int32_t x, int32_t z, int32_t flag) {
    gLLCCE0B4 = x;
    gLLCCE0B8 = z;
    uint16_t index = static_cast<uint16_t>(x + (z << 7) + 1);
    CMapCell* cell = reinterpret_cast<CMapCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + static_cast<int32_t>(static_cast<uint16_t>(index) - 1) * 0x44);
    CTrackCell* track = cell->GetTrackCellEquiv();
    gLLCCE08C = track->mMeshInstance;
    track->mMeshInstance = 0;
    if (flag == 0) cell->Delete(0, 0);
    gLLCCE090 = 1;
}
