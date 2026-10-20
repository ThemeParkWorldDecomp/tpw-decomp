// Unit v5344: the ride track counting, performance and teardown of the advisor world, main app.
//   BMP_CountTrackSections 0x10026f20, BMP_FreeTrack 0x100270e0, BMP_SetRidePerformance 0x10026c98, BMP_DestroyRide 0x10027274
#include <cstdint>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPAdvisor.h"
#include "rsse.h"

int32_t gRidePerfMax = 100;   // 0x1029ca24
int32_t gRidePerfMin = 0;     // 0x1029ca28

namespace {

constexpr uint32_t kWorldMagic = 0x4a454647u;

tagBUMPWORLD* WorldOf() { return reinterpret_cast<tagBUMPWORLD*>(&gColList); }

// The record of a ride id and the id the record holds (kind << 8 | index); NULL and 0 when the world is not live.
tagBUMPRIDE* LookupRide(int32_t ride, int32_t* id) {
    tagBUMPWORLD* world = WorldOf();
    if (world->magic == kWorldMagic) {
        tagBUMPRIDE* rides = world->rides.get();
        tagBUMPRIDE* record = rides + (static_cast<uint32_t>(ride) & 0xffu);
        int32_t diff = static_cast<int32_t>(reinterpret_cast<uint8_t*>(record) - reinterpret_cast<uint8_t*>(rides));
        *id = static_cast<int32_t>((static_cast<uint32_t>(record->kind) << 8) | static_cast<uint32_t>(diff / 0xd0));
        return record;
    }
    *id = 0;
    return nullptr;
}

// Docks every live bump owned by the record, then closes the ride.
void DockAndClose(int32_t ride, tagBUMPRIDE* owner) {
    tagBUMPWORLD* world = WorldOf();
    tagBUMPSLOT* slot = world->bumps.get();
    if (slot != nullptr) {
        int32_t count = world->bumpCount;
        while (count != 0) {
            if (slot->flags & 1) {
                --count;
                if (slot->owner.get() == owner) BMP_DOCK(reinterpret_cast<tagBUMP*>(slot));
            }
            slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
        }
    }
    BMP_CloseRide(ride);
}

uint32_t Word(const void* base, uint32_t offset) { return *reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(base) + offset); }

// A pointer word of a track node (an arena offset, 0 null).
const uint8_t* NodeAt(const void* base, uint32_t offset) { return Ptr32<const uint8_t>{Word(base, offset)}.get(); }

}  // namespace

int32_t BMP_CountTrackSections(int32_t ride, int32_t* a, int32_t* b, int32_t* c, int32_t* d) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide(ride, &id);
    if (ride != id) return 0;
    int32_t run = 0, ends = 0, longest = 0, crossings = 0, plain = 0, total = 0;
    for (int32_t pass = 2; pass != 0; --pass) {
        for (const uint8_t* node = NodeAt(record, 0xbc); node != nullptr; node = NodeAt(node, 0x24)) {
            uint32_t type = Word(node, 4) & 0xffffu;
            if (type == 11) {
                ++run;
                ++crossings;
            } else if (type < 11 && type >= 9) {
                ++run;
            } else if (type > 11 && type < 14) {
                if (Word(node, 8) == 0) ++plain;
                ++run;
            } else {
                if (run > longest) longest = run;
                run = 0;
                ++ends;
            }
            ++total;
        }
    }
    if (a != nullptr) *a = ends / 2;
    if (b != nullptr) *b = longest;
    if (c != nullptr) *c = crossings / 2;
    if (d != nullptr) *d = plain;
    return total / 2;
}

void BMP_FreeTrack(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide(ride, &id);
    if (ride == id) {
        tagBUMPRIDE* owner = LookupRide(ride, &id);
        if (ride == id) DockAndClose(ride, owner);
        FreeBuoysForRide(record, 6);
        FreeTrackForRide(record);
    }
}

void BMP_SetRidePerformance(int32_t ride, int32_t speed) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide(ride, &id);
    if (ride != id) return;
    uint8_t* r = reinterpret_cast<uint8_t*>(record);
    int32_t v = gRidePerfMax < speed ? gRidePerfMax : speed;
    v = gRidePerfMin < v ? v : gRidePerfMin;
    *reinterpret_cast<int32_t*>(r + 0x1c) = v;
    TRACE("Set ride performance to %d\n", *reinterpret_cast<int32_t*>(r + 0x1c));
    for (uint32_t i = 0; i < 4; ++i) {
        int32_t lo = *reinterpret_cast<int32_t*>(r + 0x30 + 4 * i);
        int32_t hi = *reinterpret_cast<int32_t*>(r + 0x40 + 4 * i);
        *reinterpret_cast<int32_t*>(r + 0x20 + 4 * i) = lo + static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(hi - lo)) / 100;
    }
}

void BMP_DestroyRide(int32_t ride) {
    int32_t id;
    tagBUMPRIDE* record = LookupRide(ride, &id);
    if (ride == id) {
        tagBUMPRIDE* owner = LookupRide(ride, &id);
        if (ride == id) DockAndClose(ride, owner);
        FreeBuoysForRide(record, 0);
        FreeTrackForRide(record);
        FreeColObjectsForRide(record);
        record->kind = 0;
        uint32_t* words = reinterpret_cast<uint32_t*>(&gColList);
        words[3] = words[3] - 1;
    }
}
