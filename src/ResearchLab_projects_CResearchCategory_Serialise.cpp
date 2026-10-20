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
#include "ThingParts.h"

// 436 bytes: saves (save != 0) or loads the four fields, stopping at the first callee that answers non-zero.
// Versions 0 and 1 are handled; any other version does nothing and answers 0.
int32_t CResearchCategory::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (save != 0) {
        if (version < 2 && version >= 0) {
            if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mEffort), "effort", 0) != 0) return 1;
            if (SAVEVAR(file, reinterpret_cast<MacULong*>(&mOutput), "output", 0) != 0) return 1;
            if (SAVEVAR(file, &mF08, "currentProject", 0) != 0) return 1;
            if (SAVEVAR(file, &mF0C, "highestGroupAvailable", 0) != 0) return 1;
        }
    } else {
        if (version < 2 && version >= 0) {
            if (LOADVAR(file, reinterpret_cast<MacULong*>(&mEffort), "effort", 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<MacULong*>(&mOutput), "output", 0) != 0) return 1;
            if (LOADVAR(file, &mF08, "currentProject", 0) != 0) return 1;
            if (LOADVAR(file, &mF0C, "highestGroupAvailable", 0) != 0) return 1;
        }
    }
    return 0;
}
