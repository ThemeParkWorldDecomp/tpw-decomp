#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "BMP_InsideArea.h"
#include "RideDB.h"
#include "analyser.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "effect_manager.h"
#include "m3d2.h"
#include "rsse.h"
#include "Debug.h"
#include "TPSound.h"
#include "Advisor.h"
#include "World.h"
#include "ppc_math.h"

// 0x1002d404, 2768 bytes: sets up the bump's behaviour for the kind of its ride (the kind -14 to -1 go through a jump table at
// 0x1029cba0, the rest take the default). The ride record (0xd0 bytes) and the bump (0xac bytes) are read by Mac offset; the
// list is the one at 0x102b56c0 (+0 magic, +8 rides, +0x10 bumps, +0x14 their count, +0x20 the table of 0x1c bytes, +0x24 its
// count). Pointer words inside the list, the rides and the bumps are Ptr32 offsets from the arena base.
namespace {

inline uint32_t& R(uintptr_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }
inline int32_t& S(uintptr_t base, uint32_t off) { return *reinterpret_cast<int32_t*>(base + off); }
inline uintptr_t P(uintptr_t base, uint32_t off) {
    const uint32_t v = R(base, off);
    return v != 0 ? reinterpret_cast<uintptr_t>(tpw::gArenaBase) + v : 0;
}
inline uint32_t Off(uintptr_t p) { return p != 0 ? tpw::ArenaOffset(reinterpret_cast<const void*>(p)) : 0; }

inline int32_t Labs(int32_t x) { return x < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(x)) : x; }
inline uint32_t Mod(uint32_t a, uint32_t b) { return b != 0 ? a % b : a; }   // divwu by 0 answers 0 in the emulator
// srawi 8 then addze: the division by 256 toward zero
inline int32_t Div256(int32_t v) { return (v >> 8) + (v < 0 && (v & 0xff) != 0 ? 1 : 0); }

}  // namespace

void SetControlType(tagBUMPRIDE* rideArg, tagBUMP* bumpArg) {
    const uintptr_t ride = reinterpret_cast<uintptr_t>(rideArg);
    const uintptr_t bump = reinterpret_cast<uintptr_t>(bumpArg);
    const uintptr_t list = reinterpret_cast<uintptr_t>(&gColList);

    auto rideIdOf = [&](uintptr_t r) -> int32_t {
        if (R(list, 0) != 0x4a454647u) return 0;
        const int32_t index = static_cast<int32_t>(Off(r) - R(list, 8)) / 0xd0;
        return static_cast<int32_t>((R(r, 0) << 8) | static_cast<uint32_t>(index));
    };
    // the sound of the bump: the advisor's own thing gets no entity
    auto startSound = [&]() {
        const uintptr_t owner = P(bump, 0x9c);
        uint16_t id = 0;
        TrackRideIdToThingId(&id, rideIdOf(owner));
        int32_t entity;
        if (id != gAdvisorThingId) {
            const uint16_t keep = id;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            entity = static_cast<int32_t>(R(reinterpret_cast<uintptr_t>(gThingArray[keep].mThing), 0x20));
        } else {
            entity = 0;
        }
        R(bump, 0x20) = static_cast<uint32_t>(SND_SendIndirectedEvent(entity, 0, gSoundBanks[7], 0, 0, 0, 0));
        const int32_t handle = static_cast<int32_t>(R(bump, 0x20));
        if (handle != 0) {
            const float fy = static_cast<float>(S(bump, 0x38));
            const float fx = static_cast<float>(S(bump, 0x34));
            const float qx = fx / 302.7f;
            const float qy = fy / 302.7f;
            const int32_t ix = ppc_fctiwz(static_cast<double>(qx));
            const int32_t iy = ppc_fctiwz(static_cast<double>(qy));
            R(bump, 0x20) = static_cast<uint32_t>(SOUND_MoveSoundEffect(handle, ix, 0, iy));
        }
    };
    auto instanceSub = [&]() {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type;
        type.mIndex = static_cast<uint16_t>(R(ride, 0x10) & 0xffff);
        RideInfo* info = gRideDatabase.GetRideInfo(type);
        R(bump, 0xc) = InstanceRide(info->mSubRideTypes[0], 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0);
    };
    auto rand = [&]() -> uint32_t { return gGameWorld->GetRand(); };
    auto nextNode = [&](uintptr_t node) -> uintptr_t {
        const uintptr_t n = P(node, 0x14);
        return n != 0 ? n : P(ride, 0xa8);
    };
    auto tableIndexOk = [&](int32_t i) { return !(i < 0 || i >= S(list, 0x24)); };

    uintptr_t node = 0;
    int32_t count = 0;

    const uint32_t sel = static_cast<uint32_t>(S(ride, 0)) + 14u;
    if (sel > 13u) goto kDefault;
    // the jump table at 0x1029cba0, for the kinds -14 to -1
    switch (sel) {
        case 0: case 3: case 8: case 11: case 13: goto kA;
        case 1: case 2: case 6: case 9: goto kD;
        case 4: case 5: goto kC;
        case 7: case 10: goto kC2;
        case 12: goto kE;
        default: goto kDefault;
    }

kA:
    if ((R(bump, 0) & 0x100000u) != 0) {
        int32_t tries = 100;
        for (;;) {
            const uint32_t modulus = R(P(ride, 0xc0), 0xc);
            int32_t dist = static_cast<int32_t>(Mod(rand(), modulus));
            const uint32_t rnd = rand();
            const int32_t sinv = gHostSineTable[rnd & 0x1ff];
            const int32_t cosv = gHostSineTable[((rnd & 0x1ff) + 0x80) & 0x1ff];
            const int32_t dx = static_cast<int32_t>(static_cast<uint32_t>(dist) * static_cast<uint32_t>(cosv));
            const int32_t dy = -static_cast<int32_t>(static_cast<uint32_t>(dist) * static_cast<uint32_t>(sinv));
            S(bump, 0x34) = S(ride, 0x74) + Div256(dx);
            S(bump, 0x38) = S(ride, 0x78) + Div256(dy);
            if (--tries == 0) break;
            uintptr_t other = P(list, 0x10);
            int32_t left = S(list, 0x14);
            bool hit = false;
            while (left != 0) {
                if ((R(other, 0) & 1u) != 0) {
                    left--;
                    if (other != bump) {
                        int32_t shift = 0;
                        int32_t ax = Labs(S(other, 0x34) - S(bump, 0x34));
                        int32_t ay = Labs(S(other, 0x38) - S(bump, 0x38));
                        while (ax > 0x8000 || ay > 0x7fff) {
                            ax >>= 1;
                            shift++;
                            ay >>= 1;
                        }
                        const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(ax) * static_cast<uint32_t>(ax) +
                                                                  static_cast<uint32_t>(ay) * static_cast<uint32_t>(ay));
                        const int32_t root = ppc_fctiwz(std::sqrt(static_cast<double>(sum)));
                        const int32_t reach = shift < 32 ? static_cast<int32_t>(static_cast<uint32_t>(root) << shift) : 0;
                        if (S(other, 0x64) + S(bump, 0x64) > reach) {
                            hit = true;
                            break;
                        }
                    }
                }
                other += 0xac;
            }
            if (!hit) break;
        }
        R(bump, 0) &= ~0x4000u;
        const uint32_t rnd = rand();
        R(bump, 0x54) = rnd & 0x1ff;
        R(bump, 0x50) = rnd & 0x1ff;
        R(bump, 0x24) = FindDataNodeIndexByID(gRideAnimSets[R(bump, 8)], 0x100, 2);
        R(bump, 0x28) = FindDataNodeIndexByID(gRideAnimSets[R(bump, 8)], 0x100, 1);
        if (S(ride, 0) == -1) {
            R(bump, 0) |= 0x3000000u;
            instanceSub();
        }
        S(bump, 0xa4) = -1;
    }
kB:
    if (R(bump, 0x20) == 0 && (R(bump, 0) & 0x404000u) == 0x404000u && S(P(bump, 0x9c), 0x50) == 2 &&
        (R(bump, 0) & 0x20u) == 0) {
        startSound();
    }
    {
        bool done = false;
        if ((R(bump, 0) & 0x40004u) == 0) {
            if ((rand() & 0xf) < 3) {
                int32_t index = 0;
                tagBUMP* target = FindTargetBump(bumpArg, rideArg, &index);
                if (target != nullptr) {
                    R(bump, 0) &= 0xfffbff81u;
                    R(bump, 0) |= 4u;
                    S(bump, 0x80) = index;
                    S(bump, 0x8c) = 0x5a;
                    done = true;
                }
            }
        }
        if (!done) {
            uint32_t steps;
            if ((R(bump, 0) & 8u) != 0 && (rand() & 0xf) < 4) {
                steps = 1;
                node = P(list, 0x20) + static_cast<uint32_t>(S(bump, 0x7c)) * 0x1c;
            } else {
                const uint32_t rnd = rand();
                steps = Mod(rnd, R(ride, 0xac));
                node = P(ride, 0xa8);
            }
            R(bump, 0) &= 0xfffbff81u;
            R(bump, 0) |= 0x10008u;
            while (steps != 0) {
                node = nextNode(node);
                steps--;
            }
            S(bump, 0x7c) = S(node, 4);
            S(bump, 0x8c) = 3;
        }
        R(bump, 0) |= 0x8000u;
    }
    goto kEnd;

kC:
    if ((R(bump, 0) & 0x100000u) != 0) R(bump, 0) |= 0x3c00u;
kC2:
    R(bump, 0) &= 0xfffbff81u;
    S(bump, 0x8c) = 0;
    if ((R(bump, 0) & 0x100000u) != 0) {
        startSound();
        R(bump, 0x24) = FindDataNodeIndexByID(gRideAnimSets[R(bump, 8)], 0x100, 1);
        if ((R(bump, 0) & 0x2000u) != 0) instanceSub();
    }
    if (S(ride, 0x50) == 1) {
        node = P(ride, 0xa8);
        count = S(ride, 0x5c);
        while (count != 0) {
            if ((R(node, 0) & 2u) != 0) {
                if (--count == 0) break;
                node = P(node, 0x14);
            }
        }
        S(bump, 0x7c) = S(node, 4);
        R(bump, 0) |= 0x10048u;
        goto kEnd;
    }
    if (S(ride, 0x50) == 2) {
        R(bump, 0) |= 0x10008u;
        if (!tableIndexOk(S(bump, 0x7c))) S(bump, 0x7c) = S(P(ride, 0xa4), 4);
        S(bump, 0x8c) = 0;
        const uint32_t rnd = rand();
        S(bump, 0x68) = (S(ride, 0x20) / 9) * (3 - static_cast<int32_t>(rnd & 7));
    }
    goto kEnd;

kD:
    R(bump, 0) &= 0xfffbff81u;
    S(bump, 0x8c) = 0;
    if ((R(bump, 0) & 0x100000u) != 0) {
        startSound();
        S(bump, 0x7c) = S(P(ride, 0xa4), 4);
    }
    R(bump, 0) |= 0x10008u;
    if (!tableIndexOk(S(bump, 0x7c))) S(bump, 0x7c) = S(P(ride, 0xa4), 4);
    S(bump, 0x8c) = 0;
    S(bump, 0x68) = 0;
    goto kEnd;

kE:
    if ((R(bump, 0) & 8u) != 0)
        node = P(list, 0x20) + static_cast<uint32_t>(S(bump, 0x7c)) * 0x1c;
    else
        node = P(ride, 0xa8);
    R(bump, 0) &= 0xfffbff81u;
    R(bump, 0) |= 0x10008u;
    node = nextNode(node);
    S(bump, 0x7c) = S(node, 4);
    S(bump, 0x8c) = 0;
    goto kEnd;

kDefault:
    R(bump, 0) &= 0xffffff81u;
    R(bump, 0) |= 0x10010u;
    S(bump, 0x7c) = static_cast<int32_t>(Mod(rand(), R(list, 0x24)));

kEnd:
    R(bump, 0) &= ~0x100000u;
}
