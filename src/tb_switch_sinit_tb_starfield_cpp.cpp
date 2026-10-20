// Static initialiser of tb_starfield.cpp: constructs gStarfield and registers its destructor.
#include <cstdint>
#include <new>

#include "AdvisorScoring.h"
#include "tb_starfield.h"

uint32_t gStarfieldAtExitNode[4];   // engine data 0x10173174

void StarfieldDestructorThunk(void* object) { static_cast<CStarfield*>(object)->~CStarfield(); }

// 0x1003266c
void __sinit_tb_starfield_cpp() {
    new (&gStarfield) CStarfield();
    __register_global_object(&gStarfield, reinterpret_cast<void*>(&StarfieldDestructorThunk), gStarfieldAtExitNode);
}
