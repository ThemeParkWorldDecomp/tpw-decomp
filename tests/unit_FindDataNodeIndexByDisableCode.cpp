// Test library for FindDataNodeIndexByDisableCode and CTRACE (unit v5L21). The other functions of src/game/TPEmbed.cpp, src/game/TPMesh.cpp, src/game/rides/RideAnim.cpp
// are linked but not called; NS_MacDoze::_stricmp (FindDataNodeIndexByIDAdvisorTempHack's) is a fake that is never reached.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LLCellSystem.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "rsse.h"

namespace NS_MacDoze {
int32_t _stricmp(const char*, const char*) { return 1; }
}  // namespace NS_MacDoze

extern "C" {

// The set, its mesh and the mesh's header are built here; `words` holds three words per entry (flags, 0x04, 0x08).
int32_t tpw_dc_find(uint32_t count, const uint32_t* words, uint32_t code, uint32_t id) {
    sEmbeddedData* entries = static_cast<sEmbeddedData*>(tpw::ArenaAllocZeroed(sizeof(sEmbeddedData) * (count + 1)));
    for (uint32_t i = 0; i < count; ++i) {
        entries[i].mFlags = words[3 * i];
        entries[i].mF04 = words[3 * i + 1];
        entries[i].mF08 = words[3 * i + 2];
    }
    tag_sM3D2Header* header = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    header->mEmbeddedCount = static_cast<uint16_t>(count);
    header->mEmbedded = entries;
    tag_sMesh* mesh = static_cast<tag_sMesh*>(tpw::ArenaAllocZeroed(sizeof(tag_sMesh)));
    mesh->mHeader = header;
    tag_sRideAnimSet* set = static_cast<tag_sRideAnimSet*>(tpw::ArenaAllocZeroed(sizeof(tag_sRideAnimSet)));
    set->mHeads = mesh;
    return FindDataNodeIndexByDisableCode(set, code, id);
}

void tpw_dc_ctrace(uint32_t level, const char* format, uint32_t a, uint32_t b) { CTRACE(level, format, a, b); }

}  // extern "C"
