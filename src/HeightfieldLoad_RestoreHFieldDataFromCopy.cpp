#include <cstring>

#include "m3d2.h"

// 0x1005c050: puts the saved quad table back. The byte count is mQuadCount * 4 of the header's own field.
void RestoreHFieldDataFromCopy(tag_sM3D2Header* header, sHField* copy) {
    if (header == nullptr) return;
    sHField* field = header->mHField.get();
    if (field == nullptr) return;
    void* dest = field->mQuads.get();
    if (dest == nullptr) return;
    // The original reads copy->mQuads without a NULL check on copy; so does this.
    void* src = copy->mQuads.get();
    if (src == nullptr) return;
    uint32_t count = field->mQuadCount;
    if (count == 0) return;
    std::memcpy(dest, src, count << 2);
}
