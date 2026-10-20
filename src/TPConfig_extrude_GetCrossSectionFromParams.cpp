#include "TPCoaster.h"

// 0x1004fd94. The type holds at 0x78 the table and at 0x7c the count (Mac offsets). Entries are 0x2c bytes: eight floats (low, high
// for the angle and the orientation's three angles) and the answer word at 0x20. The last entry is the fallback and is never tested.
int32_t GetCrossSectionFromParams(sCoasterType* type, float unused, sCoasterOrientation* orientation, float angle) {
    (void)unused;
    const float values[4] = {angle, orientation->mF04, orientation->mF08, orientation->mF0C};
    int32_t found = 0;
    const sCoasterCrossSectionEntry* entry = type->mTable;
    for (uint32_t i = 0; i < type->mCount - 1u; ++i, ++entry) {
        const float* r = entry->mRange;
        if (values[0] >= r[0] && values[0] <= r[1] && values[1] >= r[2] && values[1] <= r[3] && values[2] >= r[4] && values[2] <= r[5] &&
            values[3] >= r[6] && values[3] <= r[7]) {
            found = entry->mAnswer;
            break;
        }
    }
    if (found != 0) return found;
    return type->mTable[type->mCount - 1u].mAnswer;
}
