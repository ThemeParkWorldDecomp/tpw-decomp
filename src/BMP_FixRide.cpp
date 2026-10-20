// Unit v5220: the ride commands of the bump world (halt, break, fix, begin), main app.
#include <cstdint>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

tagBUMPWORLD* World() { return reinterpret_cast<tagBUMPWORLD*>(&gColList); }

// The ride record of a ride id, or null when the world is not live; answers the id the record holds for it (kind << 8 | index).
tagBUMPRIDE* FindRide(int32_t ride, int32_t* id) {
    tagBUMPWORLD* world = World();
    if (world->magic != 0x4a454647u) {
        *id = 0;
        return nullptr;
    }
    uint32_t index = static_cast<uint32_t>(ride) & 0xffu;
    tagBUMPRIDE* record = reinterpret_cast<tagBUMPRIDE*>(reinterpret_cast<uint8_t*>(world->rides.get()) + index * 0xd0u);
    *id = static_cast<int32_t>((static_cast<uint32_t>(record->kind) << 8) | index);
    return record;
}

// The kinds the break and fix commands act on: -14 to -3 and -1.
bool KindActs(int32_t kind) {
    if (kind == -2) return false;
    if (kind < -2) return kind >= -0xe;
    return kind < 0;
}

// Sets the bump's animation state, flushes its set and starts the animation.
void StartAnim(tagBUMPSLOT* slot, uint32_t state, uint32_t loop) {
    slot->animState = state;
    if (slot->animSet != 0) FlushDeferredAnim(gRideAnimSets[slot->animSet], 0);
    if (slot->animSet != 0) SetCurRideAnim(gRideAnimSets[slot->animSet], state, 0, loop, 1.0f, 0);
}

}  // namespace

int32_t BMP_HaltRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    if (ride == id) {
        tagBUMPWORLD* world = World();
        tagBUMPSLOT* slot = world->bumps.get();
        if (slot != nullptr) {
            int32_t count = world->bumpCount;
            while (count != 0) {
                if (slot->flags & 1) {
                    --count;
                    if (slot->owner.get() == record) BMP_DOCK(reinterpret_cast<tagBUMP*>(slot));
                }
                slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
            }
        }
        BMP_CloseRide(ride);
    }
    return 0;
}

int32_t BMP_BreakRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    if (ride == id) {
        record->broken = 2;
        TRACE("Ride broken\n");
        if (!KindActs(record->kind)) return 0;
        tagBUMPWORLD* world = World();
        tagBUMPSLOT* slot = world->bumps.get();
        int32_t count = world->bumpCount;
        while (count != 0) {
            if (slot->flags & 1) {
                --count;
                if (slot->owner.get() == record && (slot->flags & 0x4000) && slot->animIndex != -1 && slot->emitter == -1) {
                    tag_sRideAnimSet* set = gRideAnimSets[slot->animSet];
                    tagBumpHeadTable* table = static_cast<tagBumpHeadTable*>(set->mHeads->mF28);
                    tagBumpHeadNode* node = table->entries[slot->animIndex].node;
                    float x = 0.0f, y = 0.0f, z = 0.0f;
                    if (node != nullptr) {
                        x = node->x;
                        y = node->y;
                        z = node->z;
                    }
                    float sx = 1024.0f * x;
                    float sy = 1024.0f * y;
                    float sz = 1024.0f * z;
                    slot->emitter = static_cast<int32_t>(PAR_InitEmitter(2, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz)));
                    if (slot->owner->kind == -1) StartAnim(slot, 0xc, 2);
                }
            }
            slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
        }
    }
    return 0;
}

int32_t BMP_FixRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    if (ride == id) {
        if (record->broken != 2) {
            record->broken = 0;
            return 0;
        }
        record->broken = 0;
        TRACE("Ride Fixed\n");
        if (!KindActs(record->kind)) return 0;
        tagBUMPWORLD* world = World();
        tagBUMPSLOT* slot = world->bumps.get();
        int32_t count = world->bumpCount;
        while (count != 0) {
            if (slot->flags & 1) {
                --count;
                if (slot->owner.get() == record && slot->emitter != -1) {
                    PAR_KillEmitter(slot->emitter);
                    slot->emitter = -1;
                    if (slot->owner->kind == -1 && (slot->flags & 0x4000)) StartAnim(slot, 5, 1);
                }
            }
            slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
        }
    }
    return 0;
}

int32_t BMP_BeginRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = FindRide(ride, &id);
    // The jump table at 0x1029c9ec: kinds -14 to -1; -13, -12, -8, -5 and -2 go to the return, the rest to the body.
    constexpr uint32_t kBodyKinds = (1u << 0) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 7) | (1u << 8) | (1u << 10) | (1u << 11) | (1u << 13);
    uint32_t tableIndex = ride == id ? static_cast<uint32_t>(record->kind + 0xe) : 0xffu;
    if (tableIndex < 0xe && ((kBodyKinds >> tableIndex) & 1u)) {
        if (record->state == 1) {
            record->state = 2;
            tagBUMPWORLD* world = World();
            tagBUMPSLOT* slot = world->bumps.get();
            int32_t count = world->bumpCount;
            while (count != 0) {
                if (slot->flags & 1) {
                    --count;
                    if (slot->owner.get() == record) {
                        slot->flags &= 0xfff7ffbfu;
                        slot->f88 = record->f04;
                        SetControlType(record, reinterpret_cast<tagBUMP*>(slot));
                        if (record->kind == -1 && (slot->flags & 0x4000)) StartAnim(slot, 5, 1);
                    }
                }
                slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
            }
        }
        TRACE("Start Bump Ride\n");
    }
    return 0;
}
