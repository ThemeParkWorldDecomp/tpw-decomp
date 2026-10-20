// The track end offsets of the ride tools (main app, LLCellRide.cpp range 0x1008c620 to 0x1008c6d8, SimTheme Park 2.04b3; unit w204):
// LLCR_OffsetStartOfTrack 0x1008c620 and LLCR_OffsetEndOfTrack 0x1008c6d8. Each moves a cell (two words the caller passes by address, read
// first and stored x then y at the end) by an amount that depends on the ride's rotation (0, 90, 180 or 270 degrees; a signed compare) and answers a
// compass bit. For any other rotation the cells stay and the Mac answers a register it never set (the caller's r0 in the first, the low byte of
// the link register in the second): the host answers 0 there, and the test compares only the cells.
#include <cstdint>

#include "LLCellSystem.h"
#include "ObjectParts.h"

// 0x1008c620, 132 bytes: the cell of the first track piece and the direction bit it points to.
uint8_t LLCR_OffsetStartOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    int32_t newX = *x;
    int32_t newY = *y;
    uint8_t direction = 0;
    switch (rotation) {
        case 0:
            direction = 4;
            newX += 4;
            newY += 1;
            break;
        case 0x5a:
            direction = 1;
            newX += 1;
            newY -= 5;
            break;
        case 0xb4:
            direction = 0x40;
            newX -= 5;
            newY -= 2;
            break;
        case 0x10e:
            direction = 0x10;
            newX -= 2;
            newY += 4;
            break;
        default:
            break;
    }
    *x = newX;
    *y = newY;
    return direction;
}

// 0x1008c6d8, 168 bytes: the cell of the last track piece; the answer is the OPPOSITE of the direction bit the first function would give for the
// same rotation (the bit of the end, which points back along the track).
uint8_t LLCR_OffsetEndOfTrack(int32_t* x, int32_t* y, int32_t rotation) {
    int32_t newX = *x;
    int32_t newY = *y;
    uint8_t direction = 0;
    switch (rotation) {
        case 0:
            direction = 4;
            newX -= 2;
            newY += 1;
            break;
        case 0x5a:
            direction = 1;
            newX += 1;
            newY += 1;
            break;
        case 0xb4:
            direction = 0x40;
            newX += 1;
            newY -= 2;
            break;
        case 0x10e:
            direction = 0x10;
            newX -= 2;
            newY -= 2;
            break;
        default:
            break;
    }
    *x = newX;
    *y = newY;
    return OPPOSITE(direction);
}
