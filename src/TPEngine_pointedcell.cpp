// GetMapCellFromPointerWorldPosn (main app, SimTheme Park 2.04b3, TPEngine.cpp; unit w139), 0x10067084, 348 bytes.
//
// Turns the pointer's world position into the id of the map cell under it: with the height field's origin (mF08[0], mF08[1]) and cell size
// (mF08[2], mF08[3]), x = trunc((pos.x - origin x) / size x) and y = trunc((pos.z - origin y) / size y) (single precision subtract and
// divide, then fctiwz). Both are clamped, in the listing's order: negatives to 0, then (UNSIGNED compare) to the field's counts mF18 less one,
// negatives to 0 again, then (signed) to gHeightfieldWidth / Height less one. gIntrface.mF24 receives 0 when IsXYValid refuses the pair,
// else x + (y << 7) + 1 cut to 16 bits. Without a height field header or field nothing happens.
#include "TPEngine.h"

#include <cstdint>

#include "World.h"
#include "intrface.h"
#include "m3d2.h"
#include "ppc_math.h"

void GetMapCellFromPointerWorldPosn(s3DPoint* position) {
    const tag_sM3D2Header* const header = gHeightfieldHeader;
    if (header == nullptr) return;
    const sHField* const field = header->mHField;
    if (field == nullptr) return;

    const float dx = position->x - field->mF08[0];
    const float dz = position->z - field->mF08[1];
    int32_t x = ppc_fctiwz(static_cast<double>(dx / field->mF08[2]));
    int32_t y = ppc_fctiwz(static_cast<double>(dz / field->mF08[3]));
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (static_cast<uint32_t>(x) >= field->mF18[0]) x = static_cast<int32_t>(field->mF18[0]) - 1;
    if (static_cast<uint32_t>(y) >= field->mF18[1]) y = static_cast<int32_t>(field->mF18[1]) - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
    if (y >= gHeightfieldHeight) y = gHeightfieldHeight - 1;

    if (CMap::IsXYValid(x, y) == 0) {
        gIntrface.mF24 = 0;
    } else {
        gIntrface.mF24 = static_cast<uint16_t>(static_cast<uint32_t>(x) + (static_cast<uint32_t>(y) << 7) + 1u);
    }
}
