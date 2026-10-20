#include <cmath>
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "analyser.h"
#include "m3d2.h"
#include "rsse.h"
#include "ui_iterators.h"
#include "keytable_actions.h"
#include "platform/samsutils.h"

namespace {
// A person (reached by Mac offset, as the other sources do).
uint16_t PersonRide(const CThing* person) { return *reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(person) + 0x1dc); }
int32_t PersonState(const CThing* person) { return *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(person) + 0x220); }
}  // namespace

// 0x10044b10 (552 bytes). r28/r29 stay the caller's when the coaster's script is NULL; here they start 0.
void HACKRestoreCoasterQueues(uint32_t instance) {
    tag_sCoasterInstance* found = nullptr;
    for (tag_sCoasterInstance* it = gCoasterInstances; it != nullptr; it = it->mNext) {
        if (it->mEntranceID == instance) {
            found = it;
            break;
        }
    }
    if (found == nullptr) return;
    uint16_t coasterId = static_cast<const CThing*>(found->mCObject)->GetThingId().mIndex;
    uint32_t var0 = 0;
    uint32_t var1 = 0;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(found->mScriptID);
    if (script != nullptr) {
        var0 = static_cast<uint32_t>(RSSE_Getvar(script, 0));
        var1 = static_cast<uint32_t>(RSSE_Getvar(script, 1));
    }
    for (CThing* person = UI_IterateThingNext(kUIIterVisitors, nullptr); person != nullptr;
         person = UI_IterateThingNext(kUIIterVisitors, person)) {
        if (PersonRide(person) != coasterId || PersonState(person) != 0x10) continue;
        uint16_t peep = person->GetThingId().mIndex;
        if (var0 == peep || var1 == peep) continue;
        bool present = false;
        sTrainInstance* train = found->mTrains;
        for (uint32_t t = 0; t < static_cast<uint32_t>(found->mTrainCount) && !present; ++t, ++train) {
            sCarInstance* car = train->mCars;
            for (uint32_t c = 0; c < train->mCarsInUse && !present; ++c, ++car) {
                for (uint32_t k = 0; k < car->mCountA; ++k) {
                    if (static_cast<uint16_t>(car->mCurrentA[k]) == peep) {
                        present = true;
                        break;
                    }
                }
                if (present) break;
                for (uint32_t k = 0; k < car->mCountB; ++k) {
                    if (static_cast<uint16_t>(car->mCurrentB[k]) == peep) {
                        present = true;
                        break;
                    }
                }
            }
        }
        if (!present) HACKCoasterReturnLostPeep(found, peep);
    }
}

// 0x10045a38 (108 bytes): the length of (x, z) in double, rounded to single, divides all three.
void NormaliseVectorXZ(s3DPoint& point) {
    double sum = static_cast<double>(point.x) * static_cast<double>(point.x) + static_cast<double>(point.z * point.z);
    float length = static_cast<float>(std::sqrt(sum));
    point.x = point.x / length;
    point.y = point.y / length;
    point.z = point.z / length;
}

// 0x1004a648 (224 bytes)
void DisableCoasterCarDrawSafe(uint32_t ride) {
    tag_sRideAnimSet* set = gRideAnimSets[ride];
    if (set == nullptr) return;
    tag_sMesh* mesh = set->mHeads;
    if ((mesh->mF00 & 1) != 0) {
        if ((set->mFlags & 0x4000) != 0) DisableMeshDraw(mesh, set->mFC0, set->mFC4);
        else DisableMeshDraw(mesh);
    }
    for (uint32_t i = 0; i < 0x100; ++i) {
        int32_t index = FindNthDataNodeIndex(set, 0x80, i + 1);
        if (index == -1) continue;
        sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(set->mHeads->mF28);
        if ((embedded->mRecords[index].mFlags & 2) != 0) DisableRideHead(set->mHeads, static_cast<uint32_t>(index));
    }
}

// 0x10052564 (156 bytes): the coaster's type words +8 and +0xc are floats.
int32_t IncCoasterConst1() {
    if (gCoasterEditInstance != nullptr) {
        uint8_t* type = reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
        if (type != nullptr) {
            float* a = reinterpret_cast<float*>(type + 8);
            *a = *a * 1.05f;
            type = reinterpret_cast<uint8_t*>(const_cast<uint32_t*>(gCoasterEditInstance->mType));
            CTRACE(0, "Curviness %.2f - Straight Divisor %.2f\n", static_cast<double>(*reinterpret_cast<float*>(type + 8)),
                   static_cast<double>(*reinterpret_cast<float*>(type + 0xc)));
            FlagGenerateTrack(gCoasterEditInstance, 1);
        }
    }
    return 1;
}
