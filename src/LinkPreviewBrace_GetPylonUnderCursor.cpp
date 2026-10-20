#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"

// 0x1003f848 (216 bytes): the pylon of the cell under the pointer. The cell is the pointer's world x and z over 10
// (fctiwz); its key is x + z * the height field's row stride (word 0x18 of the height field). The instances are
// walked in list order, each one's pylon list (head at +0x4c, linked at +0x28); pylons with flag 1 or 4 are skipped.
tag_sPylon* GetPylonUnderCursor(uint32_t* a, uint32_t* b, uint32_t* c) {
    float fz = gPointerWorldPosn.z / 10.0f;
    float fx = gPointerWorldPosn.x / 10.0f;
    uint32_t stride = gHeightfieldHeader->mHField->mF18[0];
    uint32_t cellZ = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fz)));
    uint32_t cellX = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fx)));
    uint32_t key = cellX + cellZ * stride;

    sCoasterPylon* found = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        sCoasterPylon* p = instance->mPylons;
        while (p != nullptr && ((p->mFlags & 5) != 0 || p->mF08 != key)) p = p->mNext;
        if (p != nullptr) {
            found = p;
            break;
        }
    }
    if (a != nullptr) *a = cellX;
    if (b != nullptr) *b = cellZ;
    if (c != nullptr) *c = key;
    return reinterpret_cast<tag_sPylon*>(found);
}
