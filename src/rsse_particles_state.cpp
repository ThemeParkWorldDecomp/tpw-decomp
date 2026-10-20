// The particle system's globals in the main app (the rsse.cpp range: 0x102e49f4 to 0x102f7bd0, see include/rsse.h). Kept
// apart from the functions (src/rsse_particles_init.cpp) so that a unit that only reads the system's records, such as
// the flying meshes' particle count, can carry them without the functions' callees.
#include "rsse.h"

PARState gParState;                 // 0x102edd68
PARLibrary gParLibrary;             // 0x102e5208
int32_t gParSinTable[512];          // 0x102e49f4
uint32_t gParLibraryWordB;          // 0x102e51f4
uint32_t gParLibraryWordA;          // 0x102e51f8
int32_t gParSystemRunning;          // 0x102e51fc
int32_t gParDrawingListBusy;        // 0x102e5200
PTCL* gParDrawingList;              // 0x102e5204

// 0x102edd94 on the Mac: the emitters of the state, indexed as far as 126 (see include/rsse.h).
PAREmitter (&gParEmitters)[126] = *reinterpret_cast<PAREmitter(*)[126]>(&gParState.mEmitters[0]);
