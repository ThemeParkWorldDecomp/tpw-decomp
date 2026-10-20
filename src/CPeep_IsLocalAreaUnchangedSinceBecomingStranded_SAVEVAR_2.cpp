// Unit SAVEVAR_2: SAVEVAR<ESP_Sprites> (0x101162d4) and __sinit_Peep_cpp (0x10116418), both from Peep.cpp.
#include <cstdint>

#include "CPeep_SAVEVAR_2.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TbError.h"
#include "ThingReceivers.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

int32_t gPeepTbErrorZero;          // 0x102a0f04
int32_t gPeepC498;                 // 0x1034c498
int32_t gPeepC49C;                 // 0x1034c49c
uint32_t gPeepSinitNodes[9][3];    // 0x1034c3f4 .. 0x1034c454
char gPeepMapDeltaDtorVector[8];   // stand-in for 0x102663d0

// 0x101162d4: writes the sprite id as one word in file order (little-endian: the Mac reverses its bytes, the host's are already in that
// order, as SaveVar's SwapWord; an unconditional swap here wrote 8 as 0x08000000, which LOADVAR<ESP_Sprites> read back as a set out of
// range, v5L15). The result is tested as (result >= 0) == (zero word == 0), then 4 bytes written.
template<>
int32_t SAVEVAR<ESP_Sprites>(void* file, ESP_Sprites* value, const char* name, int32_t noLog) {
    if (noLog == 0) gSaveLoad.WriteSerialiseLog(4, name, static_cast<CSaveLoad::LogfileState>(0));
    uint32_t written = 0;
    uint32_t word = static_cast<uint32_t>(*reinterpret_cast<int32_t*>(value));
    GAME_ASSERTMSG(1, "should not be a struct");
    GAME_ASSERTMSG(1, "dummy assert: should pick up cases where SAVEVARSTRUCT should be used instead");
    word = serialise::SwapWord(word);
    TbError result = LbFile_Write(file, &word, 4, &written);
    if (result.Succeeded() == (gPeepTbErrorZero == 0) && written == 4) return 0;
    GAME_ASSERTMSG(0, "File write failed");
    return 1;
}

// 0x10116418: the nine compass steps (the cell, then the eight neighbours), each registered with a destructor.
void __sinit_Peep_cpp() {
    static const int32_t kSteps[9][2] = {{-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {0, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    gPeepC49C = 0;
    gPeepC498 = 0x10000;
    for (int32_t i = 0; i < 9; ++i) {
        gNeighbourSteps[i].mDx = kSteps[i][0];
        gNeighbourSteps[i].mDy = kSteps[i][1];
        __register_global_object(&gNeighbourSteps[i], gPeepMapDeltaDtorVector, gPeepSinitNodes[8 - i]);
    }
}
