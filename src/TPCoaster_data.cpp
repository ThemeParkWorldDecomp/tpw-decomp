// TPCoaster.cpp, main app: LoadCoasterTypeDataFile 0x10039a28 (192 bytes).
#include "TPCoaster.h"
#include "CoasterBalance.h"

namespace {
const char kSharedTextureFolder[] = "Data\\Generic\\Dynamic\\Textures"; // 0x1023cdc5
}

tag_sCoasterType* LoadCoasterTypeDataFile(uint32_t type, char*, char* texturePath, CCoasterBalance& balance) {
    CCoasterBalance local;
    local.Init();
    // The Mac asks for 0x35c bytes; the host's record holds native pointers and is bigger, so it asks for its own size (as
    // PAR_BuildDrawingList sizes its blocks by sizeof(PTCL)). The test maps the one size to the other.
    auto* coaster = static_cast<tag_sCoasterType*>(MReleaseAllocF(sizeof(tag_sCoasterType), 1));
    gCoasterTypes[gCoasterTypeCount] = coaster;
    gCoasterTypeCount = gCoasterTypeCount + 1u;
    coaster->mRideTypeNumber = type;
    SetCoasterTypeDefaults(coaster);
    CreateCoasterFromSamFile(coaster, texturePath, &balance);
    coaster->mTextures->mSharedFolder = kSharedTextureFolder;
    // The return runs ~CCoasterBalance (0x10039b30), which is the UnInit.
    return coaster;
}
