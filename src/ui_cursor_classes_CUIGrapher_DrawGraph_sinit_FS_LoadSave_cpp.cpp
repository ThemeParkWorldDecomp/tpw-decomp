#include <cstdint>
#include <cstring>
#include "FS_LoadSave.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);

// gLoadSave's 16 bytes (0x10396a7c), the registration record (0x10396a8c) and the destructor stand-in.
uint32_t gLoadSaveWords[4];
uint8_t gLoadSaveNode[12];
char gLoadSaveDtorVector[8];

// 0x101fd668 (84 bytes): stores the 0x102b154c table word, zeroes words 1 to 3, overwrites word 0 with the 0x102b1540 table word
// (both stores are in the original) and registers the object for destruction.
void __sinit_FS_LoadSave_cpp() {
    gLoadSaveWords[0] = 0x102b154cu;
    gLoadSaveWords[1] = 0;
    gLoadSaveWords[2] = 0;
    gLoadSaveWords[3] = 0;
    gLoadSaveWords[0] = 0x102b1540u;
    __register_global_object(gLoadSaveWords, gLoadSaveDtorVector, gLoadSaveNode);
}
