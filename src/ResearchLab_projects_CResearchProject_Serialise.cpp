#include <cstdint>
#include <new>
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "MainBalance.h"
#include "ObjectParts.h"
#include "ParkMessages.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"
#include "Serialise.h"

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short> ObjIdTag;
template<> int32_t SAVEVARSTRUCT<ObjIdTag>(void*, ObjIdTag*, const char*, int32_t);
template<> int32_t LOADVARSTRUCT<ObjIdTag>(void*, ObjIdTag*, const char*, int32_t);
template<> int32_t SAVEVAR<ResearchCat>(void*, ResearchCat*, const char*, int32_t);
template<> int32_t LOADVAR<ResearchCat>(void*, ResearchCat*, const char*, int32_t);
template<> int32_t SAVEVAR<unsigned char>(void*, unsigned char*, const char*, int32_t);
template<> int32_t LOADVAR<unsigned char>(void*, unsigned char*, const char*, int32_t);
template<> int32_t SAVEVAR<float>(void*, float*, const char*, int32_t);
template<> int32_t LOADVAR<float>(void*, float*, const char*, int32_t);

// 596 bytes. Saves (save != 0) or loads the six fields in order, stopping with 1 at the first failing step; versions 0 and 1 only
// (any other version does nothing and answers 0). The unused argument is ignored.
int32_t CResearchProject::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVARSTRUCT(file, reinterpret_cast<ObjIdTag*>(&mObjectId), "mObjId", 0) != 0) return 1;
            if (SAVEVAR(file, reinterpret_cast<ResearchCat*>(&mCategory), "mCat", 0) != 0) return 1;
            if (SAVEVAR(file, &mF08, "mResearchGroup", 0) != 0) return 1;
            if (SAVEVAR(file, &mUpgradeLevel, "mUpgradeLevel", 0) != 0) return 1;
            if (SAVEVAR(file, &mScore, "mCost", 0) != 0) return 1;
            if (SAVEVAR(file, &mProgress, "mResearchDone", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVARSTRUCT(file, reinterpret_cast<ObjIdTag*>(&mObjectId), "mObjId", 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<ResearchCat*>(&mCategory), "mCat", 0) != 0) return 1;
            if (LOADVAR(file, &mF08, "mResearchGroup", 0) != 0) return 1;
            if (LOADVAR(file, &mUpgradeLevel, "mUpgradeLevel", 0) != 0) return 1;
            if (LOADVAR(file, &mScore, "mCost", 0) != 0) return 1;
            if (LOADVAR(file, &mProgress, "mResearchDone", 0) != 0) return 1;
        }
    }
    return 0;
}
