#include "TPEngine.h"
#include "TPMain.h"
#include "Main.h"
#include "TPMovie.h"
#include "FS_System.h"
#include "Quickload.h"
#include "rsse.h"
#include <cstdio>
#include <cstring>

tag_sRideAnimSet* gHeightfieldRideSet;
tag_sM3D2Header* gHeightfieldHeader;
sHFieldQuad* gHeightfieldQuads;
sHField* gHeightfieldCopy;

int32_t LoadLevelHeightfield(int32_t useLevel, int32_t mode) {
    char folder[1024], shared[1024];
    std::sprintf(folder, "%s:Terrain", useLevel ? gTPFileInfo.GetLevelPath().mData : get_theme_path());
    BuildSharedTexturePath(shared);
    SetupDirectoryForMeshLoad(const_cast<char*>("base.md2"), folder, shared);
    LoadRideAnimSet(const_cast<char*>("Base"), 8, &gHeightfieldRideSet, nullptr, 1);
    if (gHeightfieldRideSet == nullptr) LoadRideAnimSet(const_cast<char*>("TestBase"), 8, &gHeightfieldRideSet, nullptr, 1);
    if (gHeightfieldRideSet == nullptr) { gHeightfieldHeader = nullptr; gHeightfieldQuads = nullptr; return 0; }
    gHeightfieldHeader = gHeightfieldRideSet->mHeads->mHeader;
    gHeightfieldQuads = gHeightfieldHeader->mHField->mQuads;
    if (gHeightfieldHeader != nullptr) {
        BuildHFieldTextureTable(gHeightfieldHeader);
        SetupHFieldNodeGamuts(gHeightfieldHeader);
        DetermineHFieldFlatness(gHeightfieldHeader);
        SaveHFieldCopy(gHeightfieldHeader, &gHeightfieldCopy);
        if ((gGameOptions & 0x2000) != 0 && useLevel == 0) LoadProceduralTextureSet(mode);
        CreateMeshTypeEngineMesh(gHeightfieldRideSet->mHeads);
        CreateEngineMesh(gHeightfieldRideSet->mHeads);
        if ((gHeightfieldRideSet->mFlags & 0x4000) != 0)
            EnableMeshDraw(gHeightfieldRideSet->mHeads, gHeightfieldRideSet->mFC0, gHeightfieldRideSet->mFC4);
        else EnableMeshDraw(gHeightfieldRideSet->mHeads);
    }
    return 1;
}

void SaveHFieldCopy(tag_sM3D2Header* header, sHField** copy) {
    if (header == nullptr || header->mHField == nullptr) { *copy = nullptr; return; }
    sHField* source = header->mHField;
    const uint32_t heightBytes = source->mHeightCount * sizeof(float);
    const uint32_t quadBytes = source->mQuadCount * sizeof(sHFieldQuad);
    auto* field = static_cast<sHField*>(MReleaseAllocF(sizeof(sHField) + heightBytes + quadBytes, 1));
    if (field != nullptr) std::memcpy(field, source, sizeof(sHField));
    field->mHeights = reinterpret_cast<float*>(field + 1);
    if (source->mHeights != nullptr && source->mHeightCount != 0) std::memcpy(field->mHeights.get(), source->mHeights.get(), heightBytes);
    field->mQuads = reinterpret_cast<sHFieldQuad*>(reinterpret_cast<uint8_t*>(field + 1) + heightBytes);
    if (source->mQuads != nullptr && source->mQuadCount != 0) std::memcpy(field->mQuads.get(), source->mQuads.get(), quadBytes);
    *copy = field;
}

void DestroyLevelHeightfield() {
    if (gHeightfieldCopy != nullptr) { MReleaseFreeF(gHeightfieldCopy); gHeightfieldCopy = nullptr; }
    if (gHeightfieldRideSet != nullptr) { DestroyRideAnimSet(gHeightfieldRideSet, 0); gHeightfieldRideSet = nullptr; }
    FreeHFieldTextureTable();
}
