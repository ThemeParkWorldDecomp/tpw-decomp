// CParkAnalyser::UpdateTurn (main app 0x100e4188, 4 bytes: a bare return) and CParkAnalyserCell::Init (0x100e6054, 24 bytes), unit w124.
#include "ThingParts2.h"
#include "analyser.h"

// The park analyser has nothing to do each turn.
void CParkAnalyser::UpdateTurn() {}

// Four stores and the answer 1; nothing else of the cell is written.
int32_t CParkAnalyserCell::Init(int32_t startX, int32_t startY, int32_t sizeX, int32_t sizeY) {
    mStartX = startX;
    mStartY = startY;
    mSizeX = sizeX;
    mSizeY = sizeY;
    return 1;
}
