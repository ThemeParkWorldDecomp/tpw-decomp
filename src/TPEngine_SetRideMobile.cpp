// SetRideMobile 0x10068280: marks ride `ride` as mobile: the mesh header gets flag 0x20, the header's node
// (mF78) gets bits 0x80800 in its first word, and the anim set gets flag 0x1000000. No NULL checks, like the original.
#include <cstdint>

#include "TPEngine.h"
#include "m3d2.h"
#include "MainWindowCallback.h"

void SetRideMobile(uint32_t& ride) {
    tag_sRideAnimSet* set = gRideAnimSets[ride];
    tag_sM3D2Header* header = set->mHeads->mHeader;
    header->mFlags |= 0x20;
    uint32_t* word = reinterpret_cast<uint32_t*>(set->mHeads->mHeader->mF78.get());
    *word |= 0x80800;
    set->mFlags |= 0x1000000;
}
