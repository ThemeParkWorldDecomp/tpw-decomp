// Unit v5087: the bump removal of the advisor world (main app): BMP_REMOVEBUMP 0x1002e614, BMP_DOCK 0x1002e740,
// RemovePeepsFromBump 0x1002e464, FindTargetBump 0x1002df10.
// The list (gColList), the ride records (0xd0 bytes) and the bumps (0xac bytes apart) are read by Mac offset, like in
// SetupHeadIDs.cpp; pointer fields inside them are Ptr32 offsets from the arena base.
#include <cstddef>
#include <cstdint>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "Ptr32.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "m3d2.h"
#include "rsse.h"

uint16_t gAdvisorThingId;   // 0x1029c960 (name invented): the advisor's own row in gThingArray

namespace {

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kPeepText[] = "Peep %d added to ride %d leaving list\n";
const char kOverText[] = "Ride Over - reset to loading\n";

inline uint32_t& R(uintptr_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }
inline int32_t& S(uintptr_t base, uint32_t off) { return *reinterpret_cast<int32_t*>(base + off); }
inline uintptr_t Dec(uint32_t v) { return v != 0 ? reinterpret_cast<uintptr_t>(tpw::gArenaBase) + v : 0; }
inline uint32_t Enc(uintptr_t p) { return p != 0 ? tpw::ArenaOffset(reinterpret_cast<const void*>(p)) : 0; }
inline uintptr_t P(uintptr_t base, uint32_t off) { return Dec(R(base, off)); }

inline uintptr_t ListBase() { return reinterpret_cast<uintptr_t>(&gColList); }
inline bool ListIsLive() { return R(ListBase(), 0) == 0x4a454647u; }

// fctiwz as the hardware does it
int32_t F2I(float f) {
    const double d = f;
    if (d != d) return INT32_MIN;
    if (d >= 2147483648.0) return INT32_MAX;
    if (d <= -2147483649.0) return INT32_MIN;
    return static_cast<int32_t>(d);
}

// The ride's id as the list computes it: the ride's first word shifted up, or'ed with its index; 0 when the list is not live.
int32_t RideId(uintptr_t ride) {
    if (!ListIsLive()) return 0;
    const int32_t index = static_cast<int32_t>(static_cast<uint32_t>(ride - P(ListBase(), 8))) / 0xd0;
    return static_cast<int32_t>((R(ride, 0) << 8) | static_cast<uint32_t>(index));
}

// The first node of the ride's mesh set's header: the three floats the emitters are placed at.
void EmitterAt(uintptr_t bump, int32_t type) {
    tag_sRideAnimSet* set = gRideAnimSets[S(bump, 8)];
    const uintptr_t node = reinterpret_cast<uintptr_t>(set->mHeads->mHeader->mF78.get()) + 0x10;
    const float k = 1024.0f;
    const float x = *reinterpret_cast<const float*>(node + 0x30);
    const float y = *reinterpret_cast<const float*>(node + 0x34);
    const float z = *reinterpret_cast<const float*>(node + 0x38);
    const int32_t a = F2I(k * x);
    const int32_t b = F2I(k * y);
    const int32_t c = F2I(k * z);
    PAR_InitEmitter(type, a, b, c);
}

}  // namespace

// 0x1002df10, 288 bytes: picks a random other live bump of the same ride whose area meets bump's, chains them through +0x94.
tagBUMP* FindTargetBump(tagBUMP* bump, tagBUMPRIDE* ride, int32_t* index) {
    const uintptr_t g = ListBase();
    const uintptr_t self = reinterpret_cast<uintptr_t>(bump);
    if (!(S(reinterpret_cast<uintptr_t>(ride), 0x5c) > 1)) return nullptr;
    const uintptr_t first = P(g, 0x10);
    uintptr_t p = first;
    int32_t count = S(g, 0x14);
    uint32_t found = 0;
    uintptr_t last = 0;
    while (count != 0) {
        const uint32_t flags = R(p, 0);
        if ((flags & 1) != 0) {
            if (self != p && (flags & 0x20) == 0 && R(self, 0x9c) == R(p, 0x9c) &&
                DoAreasIntersect(reinterpret_cast<tabCOLOBJECT*>(P(self, 0x98)),
                                 reinterpret_cast<tabCOLOBJECT*>(P(p, 0x98))) != 0) {
                R(p, 0x94) = Enc(last);
                last = p;
                found++;
            }
            count--;
        }
        p += 0xac;
    }
    if (found == 0) return nullptr;
    const uint32_t rnd = gGameWorld->GetRand();
    int32_t n = static_cast<int32_t>(rnd - (rnd / found) * found);
    uintptr_t cur = last;
    while (--n >= 0) cur = P(cur, 0x94);
    if (index != nullptr) *index = static_cast<int32_t>(static_cast<uint32_t>(cur - first)) / 0xac;
    return reinterpret_cast<tagBUMP*>(cur);
}

// 0x1002e464, 380 bytes: gives the bump's peep nodes back to the ride's free list; answers the ride's count (+0x60).
int32_t RemovePeepsFromBump(tagBUMP* bump, int32_t emit) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    uintptr_t node;
    while ((node = P(b, 0x30)) != 0) {
        R(b, 0x30) = R(node, 0x10);
        const uintptr_t ride = P(b, 0x9c);
        R(node, 0x10) = R(ride, 0xc8);
        R(ride, 0xc8) = Enc(node);
        S(P(b, 0x9c), 0x60) = S(P(b, 0x9c), 0x60) - 1;
        if (R(node, 8) != 0xffffffffu) {
            DisableRideHead(gRideAnimSets[S(b, 8)]->mHeads, R(node, 8));
            R(node, 8) = 0xffffffffu;
            if (emit != 0) EmitterAt(b, 0x13);
        }
        const int32_t id = RideId(P(b, 0x9c));
        TRACE(kPeepText, static_cast<int32_t>(R(node, 0)), id);
    }
    return S(P(b, 0x9c), 0x60);
}

// 0x1002e740, 836 bytes: removes a bump: its peeps, its sub ride instances, sound and emitter; answers the ride's +0x5c.
int32_t BMP_DOCK(tagBUMP* bump) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    const uintptr_t g = ListBase();
    const uintptr_t ride = P(b, 0x9c);
    RemovePeepsFromBump(bump, 0);
    if ((R(b, 0) & 0x400000u) != 0) {
        R(ride, 0x58) = 0;
        R(b, 0) = R(b, 0) & 0xffbfffffu;
    }
    if (S(ride, 0x50) != 4) EmitterAt(b, 0xf);
    DestroyRideInstance(R(b, 8));
    if ((R(b, 0) & 0x02002000u) != 0) {
        DestroyRideInstance(R(b, 0xc));
        R(b, 0) = R(b, 0) & 0xfdffdfffu;
    }
    if (R(b, 0x20) != 0 && S(ride, 0x50) != 4) {
        const int32_t kind = S(ride, 0);
        if (kind == -10 || kind == -9 || kind == -7 || kind == -4) {
            const int32_t id = RideId(P(b, 0x9c));
            uint16_t thingId;
            TrackRideIdToThingId(&thingId, id);
            int32_t thingHandle;
            if (thingId == gAdvisorThingId) {
                thingHandle = 0;
            } else {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZero);
                thingHandle = S(reinterpret_cast<uintptr_t>(gThingArray[thingId].mThing), 0x20);
            }
            R(b, 0x20) = static_cast<uint32_t>(SND_SendIndirectedEvent(thingHandle, S(b, 0x20), gSoundBanks[7], 1, 0, 0, 0));
            if (R(b, 0x20) != 0) {
                const float fx = static_cast<float>(S(b, 0x34));
                const float fy = static_cast<float>(S(b, 0x38));
                const float q = 302.7000122070312f;
                const int32_t ax = F2I(fx / q);
                const int32_t az = F2I(fy / q);
                R(b, 0x20) = static_cast<uint32_t>(SOUND_MoveSoundEffect(S(b, 0x20), ax, 0, az));
            }
            R(b, 0x20) = 0;
        } else {
            SOUND_FadeSoundEffect(S(b, 0x20));
        }
    }
    if (S(b, 0x2c) != -1) {
        PAR_KillEmitter(S(b, 0x2c));
        S(b, 0x2c) = -1;
    }
    R(b, 0) = R(b, 0) & 0xfffffffeu;
    S(g, 0x14) = S(g, 0x14) - 1;
    S(ride, 0x5c) = S(ride, 0x5c) - 1;
    if (S(ride, 0x5c) == 0) {
        const int32_t kind = S(ride, 0);
        if (!(kind == -8 || kind == -5 || kind == -12 || kind == -13)) {
            TRACE(kOverText);
            S(ride, 0x50) = 1;
        }
    }
    return S(ride, 0x5c);
}

// 0x1002e614, 260 bytes: docks the bump of the ride with this id (with a: only one without peeps); answers 1 when one was found.
int32_t BMP_REMOVEBUMP(int32_t rideId, int32_t a) {
    const uintptr_t g = ListBase();
    uintptr_t ride = 0;
    if (ListIsLive()) ride = P(g, 8) + static_cast<uint32_t>(rideId & 0xff) * 0xd0;
    const int32_t id = RideId(ride);
    if (rideId == id) {
        uintptr_t p = P(g, 0x10);
        int32_t count = S(g, 0x14);
        while (count != 0) {
            if ((R(p, 0) & 1) != 0) {
                count--;
                if (P(p, 0x9c) == ride && (a == 0 || R(p, 0x30) == 0)) {
                    BMP_DOCK(reinterpret_cast<tagBUMP*>(p));
                    return 1;
                }
            }
            p += 0xac;
        }
    }
    if (a != 0) BMP_REMOVEBUMP(rideId, 0);
    return 0;
}
