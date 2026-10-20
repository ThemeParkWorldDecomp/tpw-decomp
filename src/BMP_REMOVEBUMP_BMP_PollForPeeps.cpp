// Unit v5139: the peep lists of a bump ride and BMP_LeaveBump (main app). The list at 0x102b56c0 is read by Mac offset:
// +0 magic 0x4a454647, +8 the ride records (0xd0 bytes each), +0x34 the head of the free list of peep nodes (0x14 bytes, next at +0x10).
// The pointer words of the list, the records and the nodes are Ptr32 (offsets from the arena base).
#include <cstdint>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "platform/arena.h"
#include "rsse.h"

namespace {

uintptr_t ListBase() { return reinterpret_cast<uintptr_t>(&gColList); }
uint32_t& R(uintptr_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }
uintptr_t P(uintptr_t base, uint32_t off) {
    const uint32_t v = R(base, off);
    return v != 0 ? reinterpret_cast<uintptr_t>(tpw::gArenaBase) + v : 0;
}
uint32_t Enc(uintptr_t p) { return p != 0 ? tpw::ArenaOffset(reinterpret_cast<void*>(p)) : 0; }

constexpr uint32_t kMagic = 0x4a454647u;

// The ride record of a ride id (0 when the magic is wrong), and the id the record answers for itself (the original's check).
uintptr_t RideRecord(int32_t ride) {
    const uintptr_t g = ListBase();
    if (R(g, 0) != kMagic) return 0;
    return P(g, 8) + (static_cast<uint32_t>(ride) & 0xffu) * 0xd0u;
}

int32_t RideIdOf(uintptr_t rec) {
    const uintptr_t g = ListBase();
    if (R(g, 0) != kMagic) return 0;
    const int32_t diff = static_cast<int32_t>(rec - P(g, 8));
    return static_cast<int32_t>((R(rec, 0) << 8) | static_cast<uint32_t>(diff / 0xd0));
}

}  // namespace

// 0x1002e198, 168 bytes: pops the first peep node of the ride's leaving list onto the free list; answers its peep (0 when none).
int32_t BMP_PollForPeeps(int32_t ride) {
    const uintptr_t g = ListBase();
    const uintptr_t rec = RideRecord(ride);
    if (R(g, 0) != kMagic) return 0;
    if (ride != RideIdOf(rec)) return 0;
    const uintptr_t node = P(rec, 0xc8);
    if (node == 0) return 0;
    R(rec, 0xc8) = R(node, 0x10);
    R(node, 0x10) = R(g, 0x34);
    R(g, 0x34) = Enc(node);
    return static_cast<int32_t>(R(node, 0));
}

// 0x1002e268, 208 bytes: moves a free node onto the ride's leaving list holding the peep; warns when there is none.
// (A wrong magic with ride 0 reads through a null record, as the original does.)
void BMP_AddPeepToRideLeavingList(int32_t ride, uint32_t peep) {
    const uintptr_t g = ListBase();
    const uintptr_t rec = RideRecord(ride);
    if (ride == RideIdOf(rec)) {
        const uintptr_t node = P(g, 0x34);
        if (node != 0) {
            R(g, 0x34) = R(node, 0x10);
            R(node, 0x10) = R(rec, 0xc8);
            R(rec, 0xc8) = Enc(node);
            R(node, 0) = peep;
            R(node, 4) = static_cast<uint32_t>(ride);
            return;
        }
    }
    TRACE("###WARNING### Peep List full. You've just lost a Peep\n");
}

// 0x1002e070, 240 bytes: moves a free node onto the ride's joining list (when the ride word +0x50 is set); answers 1 when added.
int32_t BMP_AddPeepToRideJoiningList(int32_t ride, uint32_t peep) {
    const uintptr_t g = ListBase();
    const uintptr_t rec = RideRecord(ride);
    if (ride == RideIdOf(rec) && static_cast<int32_t>(R(rec, 0x50)) != 0) {
        const uintptr_t node = P(g, 0x34);
        if (node != 0) {
            R(g, 0x34) = R(node, 0x10);
            R(node, 0x10) = R(rec, 0xc4);
            R(rec, 0xc4) = Enc(node);
            R(node, 0) = peep;
            R(node, 4) = static_cast<uint32_t>(ride);
            R(node, 8) = 0xffffffffu;
            TRACE("Peep %d added to next car to be launched on Ride %d\n");
            return 1;
        }
    }
    return 0;
}

// 0x1002e370, 200 bytes: a bump leaves: when no peeps remain, the ride is flagged and its anim goes to state 0xc.
void BMP_LeaveBump(tagBUMP* bump) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    if (RemovePeepsFromBump(bump, 1) != 0) return;
    R(P(b, 0x9c), 0x50) = 1;
    R(b, 0) &= 0xffffff81u;
    if (static_cast<int32_t>(R(b, 0x10)) == 0xc) return;
    R(b, 0x10) = 0xc;
    if (R(b, 8) != 0) FlushDeferredAnim(gRideAnimSets[R(b, 8)], 0);
    if (R(b, 8) != 0) SetCurRideAnim(gRideAnimSets[R(b, 8)], 0xc, 0, 2, 1.0f, 0);
}
