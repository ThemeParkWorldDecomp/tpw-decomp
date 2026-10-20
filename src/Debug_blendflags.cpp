// Debug.cpp range of the main app (SimTheme Park 2.04b3), unit w133: SetBaseBlendFlags (0x1006ebec, 116 bytes).
#include "TPEngine.h"
#include "m3d2.h"

// Sets (on != 0) or clears bit 0x800 of the first halfword of the texture record of every face set of the node, unless the node has
// flag 0x200. The original answers the last texture record (or the mesh) in r3; the callers drop it.
void SetBaseBlendFlags(tag_sMesh* mesh, tag_sNode* node, int32_t on) {
    static_cast<void>(mesh);
    if ((node->mFlags & 0x200) != 0) return;
    sFaceSet* set = node->mFaceSets.get();
    if (on != 0) {
        for (uint32_t n = node->mFaceSetCount; n != 0; --n) {
            sTexture* texture = set->mTexture.get();
            ++set;
            texture->mF00 = static_cast<uint16_t>(texture->mF00 | 0x800);
        }
        return;
    }
    for (uint32_t n = node->mFaceSetCount; n != 0; --n) {
        sTexture* texture = set->mTexture.get();
        ++set;
        texture->mF00 = static_cast<uint16_t>(texture->mF00 & 0xf7ff);
    }
}
