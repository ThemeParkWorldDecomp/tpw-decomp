// rsse.cpp (main app 0x100b7fa0, unit w114): the height field quads' orientation flags for procedural texturing on or off.
#include "TPEngine.h"
#include "m3d2.h"
#include "rsse.h"

// Walks the height field's quads (gHeightfieldHeader->mHField, mQuadCount of them, 4 bytes each). Every quad whose flag halfword has
// neither bit 0x80 nor bit 0x01 is changed: procedural textures on (`procedural` nonzero) clears the orientation bits 0x78; off (0) clears
// 0xf8 and puts back the same bits of the matching quad of the saved copy (gHeightfieldCopy, made by SaveHFieldCopy). The second halfword
// of a quad and the copy are never written. The copy's quad array is looked up in both modes, so a NULL gHeightfieldCopy faults even for
// mode 1 in the original (a load whose result the host drops); callers run after SaveHFieldCopy.
void CorrectOrientationFlagsForDiffProceduralMode(int32_t procedural) {
    sHField* field = gHeightfieldHeader->mHField;
    sHFieldQuad* quad = field->mQuads;
    const sHFieldQuad* copy = gHeightfieldCopy->mQuads;
    uint32_t count = field->mQuadCount;
    if (procedural != 0) {
        for (; count != 0; --count, ++quad) {
            if ((quad->mF00 & 0x81) == 0) quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff87);
        }
        return;
    }
    for (; count != 0; --count, ++quad, ++copy) {
        if ((quad->mF00 & 0x81) == 0) {
            quad->mF00 = static_cast<uint16_t>(quad->mF00 & 0xff07);
            quad->mF00 = static_cast<uint16_t>(quad->mF00 | (copy->mF00 & 0xf8));
        }
    }
}
