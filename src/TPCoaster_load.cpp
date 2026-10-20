// TPCoaster.cpp, main app: LoadCoasterType 0x1003ab7c (232 bytes).
#include "TPCoaster.h"
#include "CoasterBalance.h"
#include <cstdio>

void LoadCoasterType(uint32_t type, char* folder, CCoasterBalance& balance) {
    char texturePath[0x400];
    std::sprintf(texturePath, "%s\\GTexture", folder);
    auto* coaster = LoadCoasterTypeDataFile(type, folder, texturePath, balance);
    gRideTypes[type]->mFlags |= 0x40000;
    if (static_cast<int32_t>(gExtrudeFaceCountA) < coaster->mF3C) {
        gExtrudeFaceCountA = static_cast<uint32_t>(coaster->mF3C);
    }
    if (gExtrudeFaceCountB < coaster->mTextures->mCount) gExtrudeFaceCountB = coaster->mTextures->mCount;
    CreateTrackSectionMesh(coaster);
    SetupTrainType(coaster, folder);
    LoadCoasterPylons(coaster, &coaster->mPylonsA, &coaster->mPylonsB, folder);
    coaster->mF318 = static_cast<int32_t>(static_cast<uint32_t>(coaster->mF2F8) * static_cast<uint32_t>(coaster->mF2E8));
}
