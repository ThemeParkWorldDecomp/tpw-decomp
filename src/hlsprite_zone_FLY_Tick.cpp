// FLY_Tick 0x1007567c (9288 bytes, unit v5880): one tick of every fly of every fly base 1 to 99 (HLSprite.cpp).
// A base is LbMem_Alloc(0x122c) bytes of 32-bit words (include/HLSprite.h, sFlyBase); fly i starts at +0x5c + i * 0xe4 (the
// FLYBOY that GetNextWaypoint and GetNextHoldingWaypoint take; its state word at +0x80 is sFlySlot::mActive). Fields are read by
// their Mac offsets. The state is a jump table at 0x1029d2d0 (state 0 and above 9 do nothing); then the common motion, the
// avoidance of the other flies and the steering, and the sound position.
// Kept from the original: the turn delta folds by 0xfff (not 0x1000), the avoidance loop starts at the fly itself (only the
// flies after it are looked at), the landed test is made twice, the GAME_ASSERTMSG condition is SetCurRideAnim's r3.
#include <cmath>
#include <cstdint>
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "Debug.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "Advisor.h"
#include "World.h"
#include "ppc_math.h"
#include "rsse.h"

// Initialised data of HLSprite.cpp (names invented): the lowest target heights of the approach (state 4 to 5), of the turn
// towards the base (state 4 to 6) and of the take-off (state 1 to 2).
int32_t gFlyApproachMinHeight = 0x36b0;   // 0x1029d2c4
int32_t gFlyTurnMinHeight = 0x36b0;       // 0x1029d2c8
int32_t gFlyTakeOffMinHeight = 0x36b0;    // 0x1029d2cc

namespace {

// The assertion text at 0x1023dc85.
const char kFlyAnimText[] = "FLY: Failed to trigger a flying car anim (%ld)";

int32_t& FW(uint8_t* p, uint32_t off) { return *reinterpret_cast<int32_t*>(p + off); }

int32_t SinT(int32_t a) { return static_cast<const int32_t*>(gFlySinTable)[static_cast<uint32_t>(a) & 0xfffu]; }
int32_t CosT(int32_t a) { return static_cast<const int32_t*>(gFlySinTable)[(static_cast<uint32_t>(a) + 0x400u) & 0xfffu]; }

int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t Shl(int32_t a, int n) { return static_cast<int32_t>(static_cast<uint32_t>(a) << n); }

// The Mac's labs on a 32-bit long (the most negative number stays negative).
int32_t Labs(int32_t v) { return v < 0 ? Sub(0, v) : v; }

// (int)sqrt((double)(a * a + b * b)), the sum in 32 bits.
int32_t Root2(int32_t a, int32_t b) {
    return ppc_fctiwz(std::sqrt(static_cast<double>(Add(Mul(a, a), Mul(b, b)))));
}

// The distance to the target in the ground plane, by 32nds, as the original: ((x - tx) / 32, (z - tz) / 32), root, times 32.
int32_t TargetDistance(uint8_t* fly) {
    return Shl(Root2(Sub(FW(fly, 4), FW(fly, 0x4c)) / 32, Sub(FW(fly, 0xc), FW(fly, 0x54)) / 32), 5);
}

// 652 * atan2(dz, dx), truncated: the heading in 4096ths of a turn.
int32_t Heading(int32_t dz, int32_t dx) {
    return ppc_fctiwz(652.0 * std::atan2(static_cast<double>(dz), static_cast<double>(dx)));
}

// The animation change the original inlines: only when the fly has a ride and its current one differs.
void SetFlyAnim(uint8_t* fly, uint32_t anim, uint32_t loop) {
    if (static_cast<uint32_t>(FW(fly, 0xc0)) == 0) return;
    if (FW(fly, 0xcc) == static_cast<int32_t>(anim) && FW(fly, 0xd0) == 0 && FW(fly, 0xd4) == static_cast<int32_t>(loop)) return;
    const uint32_t ride = static_cast<uint32_t>(FW(fly, 0xc0));
    int32_t done = 0;
    if (ride != 0) done = static_cast<int32_t>(SetCurRideAnimDuration(gRideAnimSets[ride], anim, 0, loop, 1.0f, 0));
    FW(fly, 0xcc) = static_cast<int32_t>(anim);
    FW(fly, 0xd0) = 0;
    FW(fly, 0xd4) = static_cast<int32_t>(loop);
    GAME_ASSERTMSG(done, kFlyAnimText, anim);
}

// The engine sound's parameter 10 (the event code asked even when the fly has no sound).
void SetFlySound(uint8_t* base, uint8_t* fly, uint32_t value) {
    const int32_t code = RSSE_GetSoundEventCode(FW(base, 0x54), 10);
    if (FW(fly, 0xe0) != 0) FW(fly, 0xe0) = SOUND_SetParamValue(FW(fly, 0xe0), static_cast<uint32_t>(code), value);
}

// Flying: animation 9 with sound 0x3c when +0xb8 is set, else 5 with 0x1e.
void FlyingAnim(uint8_t* base, uint8_t* fly) {
    if (FW(fly, 0xb8) != 0) {
        SetFlyAnim(fly, 9, 1);
        SetFlySound(base, fly, 0x3c);
    } else {
        SetFlyAnim(fly, 5, 1);
        SetFlySound(base, fly, 0x1e);
    }
}

int32_t Rand() { return Labs(static_cast<int32_t>(gGameWorld->GetRand())); }

}  // namespace

void FLY_Tick() {
    for (int32_t index = 1; index < 100; ++index) {
        uint8_t* const base = static_cast<uint8_t*>(gFlyBases[index]);
        if (base == nullptr) continue;
        if (FW(base, 0xc) < FW(base, 0)) EmployEric(index);
        for (int32_t i = 0; i < 20; ++i) {
            uint8_t* const fly = base + 0x5c + static_cast<uint32_t>(i) * 0xe4u;
            if (FW(fly, 0x80) == 0) continue;
            FW(fly, 0x1c) = FW(fly, 4);
            FW(fly, 0x20) = FW(fly, 8);
            FW(fly, 0x24) = FW(fly, 0xc);
            FW(fly, 0x28) = FW(fly, 0x10);
            FW(fly, 0x2c) = FW(fly, 0x14);
            FW(fly, 0x30) = FW(fly, 0x18);
            if (i > 0 && FW(base, 0xc) > FW(base, 0) && FW(base, 0xc) > 1) FW(fly, 0xb4) = 1;
            FLYBOY* const boy = reinterpret_cast<FLYBOY*>(fly);
            switch (static_cast<uint32_t>(FW(fly, 0x80))) {
            case 8:   // 0x100757b4: holding near the base
                FlyingAnim(base, fly);
                if (FW(fly, 0x8c) == 0 && FW(fly, 0xb4) != 0) {
                    FW(fly, 0x80) = 4;
                    GetNextHoldingWaypoint(boy);
                    FW(fly, 0x50) = 0x36b0;
                }
                if (TargetDistance(fly) < 5000) GetNextHoldingWaypoint(boy);
                if (FW(base, 0x2c) != 0) break;
                FW(base, 0x2c) = 1;
                if (FW(base, 0x44) != 0) {
                    FW(fly, 0xbc) = FW(base, 0x40);
                    FW(fly, 0x80) = 5;
                    FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(Mul(Sub(0, FW(base, 0x30)), 6), CosT(FW(fly, 0xbc))) / 1024);
                    const int32_t h = Add(FW(base, 0x18), 5000);
                    FW(fly, 0x50) = gFlyApproachMinHeight < h ? h : gFlyApproachMinHeight;
                    FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(Mul(Sub(0, FW(base, 0x30)), 6), SinT(FW(fly, 0xbc))) / 1024);
                } else {
                    FW(fly, 0xbc) = Heading(Sub(FW(base, 0x1c), FW(fly, 0xc)), Sub(FW(base, 0x14), FW(fly, 4)));
                    FW(fly, 0x80) = 6;
                    FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(Sub(0, FW(base, 0x30)), CosT(FW(fly, 0xbc))) / 1024);
                    const int32_t h = Add(FW(base, 0x18), 5000);
                    FW(fly, 0x50) = gFlyTurnMinHeight < h ? h : gFlyTurnMinHeight;
                    FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(Sub(0, FW(base, 0x30)), SinT(FW(fly, 0xbc))) / 1024);
                }
                break;
            case 5:   // 0x10075c40: the approach
                FlyingAnim(base, fly);
                if (TargetDistance(fly) < 5000) {
                    FW(fly, 0x80) = 6;
                    FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(Sub(0, FW(base, 0x30)), CosT(FW(fly, 0xbc))) / 1024);
                    FW(fly, 0x50) = Add(FW(base, 0x18), 5000);
                    FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(Sub(0, FW(base, 0x30)), SinT(FW(fly, 0xbc))) / 1024);
                }
                if (FW(fly, 0xb4) != 0 && FW(fly, 0x8c) == 0) {
                    FW(fly, 0x80) = 4;
                    FW(fly, 0x50) = 0x36b0;
                    FW(base, 0x2c) = 0;
                }
                break;
            case 1: {   // 0x10075f38: landed
                FW(fly, 0x14) = FW(fly, 0x14) / 2;
                SetFlySound(base, fly, 0x50);
                if (FW(base, 0x3c) != 0) break;
                if (FW(fly, 0xb8) != 0) break;
                const int32_t keep = FW(base, 0x38);
                if (keep == 0 && i == 0 && FW(base, 0xc) == 1) break;
                if (keep == 0 && i == 0 && FW(base, 0xc) == 1) break;   // the original tests it twice
                FW(fly, 0x80) = 2;
                const int32_t r = Rand();
                FW(fly, 0x7c) = Add(FW(base, 0x28), r % 100);
                FW(fly, 0x84) = FW(base, 0x10);
                const int32_t a = FW(base, 0x44) != 0 ? FW(base, 0x40) : FW(fly, 0x10);
                FW(fly, 0x4c) = Add(FW(base, 0x14), Mul(CosT(a), 20000) / 1024);
                FW(fly, 0x54) = Add(FW(base, 0x1c), Mul(SinT(a), 20000) / 1024);
                const int32_t tx = FW(fly, 0x4c);
                const int32_t tz = FW(fly, 0x54);
                int32_t ground;
                if (tx >= gFlyZoneD || tz >= gFlyZoneA || tx < gFlyZoneB || tz < gFlyZoneC) {
                    ground = 0;
                } else {
                    int32_t cx = tx / 300 / 10;
                    int32_t cz = tz / 300 / 10;
                    if (cx < 0) cx = 0;
                    if (cz < 0) cz = 0;
                    if (cx >= gHeightfieldWidth) cx = gHeightfieldWidth - 1;
                    if (cz >= gHeightfieldHeight) cz = gHeightfieldHeight - 1;
                    ground = Mul(ppc_fctiwz(GetWorldGSquareHeight(cx, cz, nullptr)), 300);
                }
                const int32_t above = Add(ground, 5000);
                const int32_t climb = Add(FW(base, 0x18), 5000);
                const int32_t h = climb < above ? above : climb;
                FW(fly, 0x50) = gFlyTakeOffMinHeight < h ? h : gFlyTakeOffMinHeight;
                break;
            }
            case 9:   // 0x10076260: leaving; gone at the target
                FlyingAnim(base, fly);
                if (TargetDistance(fly) >= 5000) break;
                FW(fly, 0x80) = 0;
                if (static_cast<uint32_t>(FW(fly, 0xc4)) != 0) {
                    PAR_KillEmitter(FW(fly, 0xc4));
                    FW(fly, 0xc4) = 0;
                }
                FW(fly, 0xc4) = 0;
                DestroyRideInstance(static_cast<uint32_t>(FW(fly, 0xc0)));
                FW(fly, 0xc0) = 0;
                FW(base, 0xc) = FW(base, 0xc) - 1;
                if (FW(fly, 0xe0) != 0) SOUND_StopSoundEffect(FW(fly, 0xe0));
                FW(fly, 0xe0) = 0;
                FW(fly, 0xe0) = 0;
                break;
            case 2: {   // 0x10076548: the take-off climb
                SetFlyAnim(fly, 4, 1);
                SetFlySound(base, fly, 0xf);
                if (FW(fly, 8) < FW(fly, 0x50)) break;
                FW(base, 0x2c) = 0;
                if (FW(fly, 0xb4) != 0) {
                    FW(fly, 0x80) = 4;
                    break;
                }
                FW(fly, 0x80) = 3;
                const int32_t x = FW(fly, 0x4c);
                int32_t gx = Mul(x / 20000, 20000);
                if (x < 0) gx = Sub(gx, 20000);
                FW(fly, 0x58) = Sub(gx, 20000);
                FW(fly, 0x5c) = FW(fly, 0x50);
                const int32_t z = FW(fly, 0x54);
                int32_t gz = Mul(z / 20000, 20000);
                if (z < 0) gz = Sub(gz, 20000);
                FW(fly, 0x60) = gz;
                break;
            }
            case 3:   // 0x10076718: wandering over the park
                FlyingAnim(base, fly);
                if (FW(fly, 0xb8) != 0 && FW(base, 0x34) == 3) FW(fly, 0x84) = 0;
                if (FW(fly, 0xb8) != 0) FW(fly, 0x7c) = FW(base, 0x28) / 2;
                if (FW(fly, 0x84) <= 0 || FW(fly, 0xb4) != 0) {
                    FW(fly, 0x80) = 4;
                    GetNextWaypoint(boy);
                    FW(fly, 0x50) = 0x36b0;
                    break;
                }
                if (TargetDistance(fly) < 5000) GetNextWaypoint(boy);
                break;
            case 4:   // 0x100769e0: the turn towards the base
                FlyingAnim(base, fly);
                if (TargetDistance(fly) >= 5000) break;
                if (FW(fly, 0xb4) != 0 && FW(fly, 0x8c) == 0) {
                    FW(fly, 0x80) = 9;
                    FW(fly, 0x50) = 0x36b0;
                    if ((Rand() & 1) != 0) {
                        const int32_t m = Mul(Rand(), 100);
                        const int32_t span = Sub(gFlyZoneD, gFlyZoneB);
                        FW(fly, 0x4c) = Add(gFlyZoneB, Sub(m, Mul(m / span, span)));
                        if ((Rand() & 1) != 0)
                            FW(fly, 0x54) = Add(gFlyZoneA, 0x10000 - 0x5038);
                        else
                            FW(fly, 0x54) = Add(gFlyZoneC, -0x10000 + 0x5038);
                    } else {
                        const int32_t m = Mul(Rand(), 100);
                        const int32_t span = Sub(gFlyZoneA, gFlyZoneC);
                        FW(fly, 0x54) = Add(gFlyZoneC, Sub(m, Mul(m / span, span)));
                        if ((Rand() & 1) != 0)
                            FW(fly, 0x4c) = Add(gFlyZoneD, 0x10000 - 0x5038);
                        else
                            FW(fly, 0x4c) = Add(gFlyZoneB, -0x10000 + 0x5038);
                    }
                    FW(fly, 0x7c) = Shl(FW(base, 0x28), 1);
                } else {
                    FW(fly, 0x80) = 8;
                    GetNextHoldingWaypoint(boy);
                }
                break;
            case 6:   // 0x10076db4: the final approach; then down to the base
                FlyingAnim(base, fly);
                if (TargetDistance(fly) >= 5000) break;
                FW(fly, 0x80) = 7;
                FW(fly, 0x7c) = FW(base, 0x4c) != 0 ? FW(base, 0x28) / 2 : FW(base, 0x28);
                FW(fly, 0x4c) = FW(base, 0x14);
                FW(fly, 0x50) = FW(base, 0x18);
                FW(fly, 0x54) = FW(base, 0x1c);
                break;
            case 7:   // 0x10077044: landing
                if (FW(fly, 8) <= FW(base, 0x18)) {
                    FW(fly, 8) = FW(base, 0x18);
                    FW(fly, 0x80) = 1;
                    FW(base, 0x3c) = 1;
                    FW(fly, 0x18) = 0;
                    FW(fly, 0x7c) = 0;
                    FW(fly, 0x74) = 0;
                    if (FW(fly, 0x8c) > 0) FW(fly, 0x8c) = Sub(0, FW(fly, 0x8c));
                    SetFlyAnim(fly, 3, 0);
                    SetFlySound(base, fly, 0x50);
                } else {
                    SetFlyAnim(fly, 6, 0);
                    SetFlySound(base, fly, FW(fly, 0xb8) != 0 ? 0x3c : 0x1e);
                }
                break;
            default:
                break;
            }

            // 0x1007728c: the motion.
            FW(fly, 0x78) = Mul(FW(fly, 0x78), FW(fly, 0x80) == 1 ? 500 : 900) / 1024;
            FW(fly, 0x70) = Mul(FW(fly, 0x70), 31) / 32;
            FW(fly, 0x84) = Sub(FW(fly, 0x84), 1);
            {
                const int32_t s = FW(fly, 0x7c);
                const int32_t t = Mul(s, FW(base, 0x50)) / 10;
                FW(fly, 0x78) = Add(FW(fly, 0x78), Add(s, t) / 10);
            }
            FW(fly, 0x64) = Mul(FW(fly, 0x78), CosT(FW(fly, 0x10))) / 1024;
            FW(fly, 0x6c) = Mul(FW(fly, 0x78), SinT(FW(fly, 0x10))) / 1024;
            FW(fly, 4) = Add(FW(fly, 4), FW(fly, 0x64));
            FW(fly, 0xc) = Add(FW(fly, 0xc), FW(fly, 0x6c));

            if (FW(fly, 0x80) == 1) {
                if (FW(fly, 0x14) > 0) FW(fly, 0x14) = Sub(FW(fly, 0x14), 0x32);
            } else {
                // The other flies (from this one on) that are flying: push apart in height and speed.
                for (int32_t j = i; j < 20; ++j) {
                    if (j == i) continue;
                    uint8_t* const other = base + 0x5c + static_cast<uint32_t>(j) * 0xe4u;
                    const int32_t st = FW(other, 0x80);
                    if (st == 0 || st == 1) continue;
                    const int32_t d2 = Shl(Root2(Sub(FW(fly, 4), FW(other, 4)) / 32, Sub(FW(fly, 0xc), FW(other, 0xc)) / 32), 5);
                    if (d2 < Mul(FW(base, 0x20), 15000) / 1024 &&
                        Labs(Sub(FW(other, 8), FW(fly, 8))) < Mul(FW(base, 0x20), 0xc80) / 1024) {
                        if (FW(fly, 0x80) == 7) {
                            FW(other, 0x74) = Add(FW(other, 0x74), 0x28);
                        } else if (FW(other, 0x80) == 7) {
                            FW(fly, 0x74) = Add(FW(fly, 0x74), 0x28);
                        } else {
                            if (FW(fly, 0x78) > FW(other, 0x78))
                                FW(other, 0x78) = Sub(FW(other, 0x78), FW(other, 0x78) / 16);
                            else
                                FW(fly, 0x78) = Sub(FW(fly, 0x78), FW(fly, 0x78) / 16);
                            if (FW(fly, 8) > FW(other, 8))
                                FW(fly, 0x74) = Add(FW(fly, 0x74), 0x28);
                            else if (i < j)
                                FW(other, 0x74) = Add(FW(other, 0x74), 0x28);
                        }
                    }
                    const int32_t dx = Sub(FW(fly, 4), FW(other, 4)) / 32;
                    const int32_t dy = Sub(FW(fly, 8), FW(other, 8)) / 32;
                    const int32_t dz = Sub(FW(fly, 0xc), FW(other, 0xc)) / 32;
                    const int32_t sum = Add(Mul(dx, dx), Add(Mul(dy, dy), Mul(dz, dz)));
                    const int32_t d3 = Shl(ppc_fctiwz(std::sqrt(static_cast<double>(sum))), 5);
                    if (d3 < Mul(FW(base, 0x20), 3000) / 1024) {
                        if (FW(fly, 8) > FW(other, 8))
                            FW(fly, 8) = Add(FW(fly, 8), 500);
                        else
                            FW(other, 8) = Add(FW(other, 8), 500);
                        FW(fly, 0x18) = Add(FW(fly, 0x18), 1000);
                        FW(other, 0x18) = Sub(FW(other, 0x18), 1000);
                    }
                }

                // Steering towards the target.
                const int32_t heading = Heading(Sub(FW(fly, 0x54), FW(fly, 0xc)), Sub(FW(fly, 0x4c), FW(fly, 4)));
                int32_t turn = static_cast<int32_t>(static_cast<uint32_t>(Sub(heading, FW(fly, 0x10))) & 0xfffu);
                if (turn >= 0x800) turn -= 0xfff;
                FW(fly, 0x70) = Add(turn, FW(fly, 0x70)) / 2;
                FW(fly, 0x10) = static_cast<int32_t>(static_cast<uint32_t>(Add(FW(fly, 0x10), FW(fly, 0x70) / 10)) & 0xfffu);

                if (FW(fly, 0x80) == 7) {
                    if (FW(fly, 0x14) < 0x3c) FW(fly, 0x14) = FW(fly, 0x14) + 2;
                    const int32_t d = Root2(Sub(FW(fly, 4), FW(fly, 0x4c)) / 32, Sub(FW(fly, 0xc), FW(fly, 0x54)) / 32);
                    const int32_t lift = Shl(d, FW(base, 0x4c) != 0 ? 6 : 5);
                    FW(fly, 8) = Add(FW(base, 0x18), Sub(Add(Mul(FW(fly, 8), 7), lift), 3000)) / 8;
                } else {
                    const int32_t k = Labs(Sub(FW(fly, 0x50), FW(fly, 8))) > 10000 ? 3 : 1;
                    int32_t v = Sub(FW(fly, 0x50), FW(fly, 8)) / 20;
                    if (v > 0x10) v = 0x10;
                    if (v < -0x10) v = -0x10;
                    FW(fly, 0x74) = Add(FW(fly, 0x74), Mul(v, k));
                    if (FW(fly, 0x74) > 0x200) FW(fly, 0x74) = 0x200;
                    if (FW(fly, 0x74) < -0x200) FW(fly, 0x74) = -0x200;
                    FW(fly, 0x74) = Mul(FW(fly, 0x74), 7) / 8;
                    FW(fly, 8) = Add(FW(fly, 8), Mul(FW(fly, 0x74), 3));
                    FW(fly, 0x14) = Mul(FW(base, 0x24), FW(fly, 0x74)) / 1024;
                }

                // The roll follows the turn rate, eased by eighths on the short way round.
                const int32_t rate = FW(fly, 0x70);
                int32_t roll = static_cast<int32_t>(
                    static_cast<uint32_t>(Mul(FW(base, 0x24), Sub(Sub(0, rate) / 2, rate / 4)) / 1024) & 0xfffu);
                const int32_t cur = static_cast<int32_t>(static_cast<uint32_t>(FW(fly, 0x18)) & 0xfffu);
                const int32_t diff = roll - cur;
                if (diff > 0x800)
                    roll -= 0x1000;
                else if (diff < -0x800)
                    roll += 0x1000;
                FW(fly, 0x18) = static_cast<int32_t>(static_cast<uint32_t>((roll + cur * 7) / 8) & 0xfffu);
            }

            if (FW(fly, 0xe0) != 0)
                FW(fly, 0xe0) = SOUND_MoveSoundEffect(FW(fly, 0xe0), FW(fly, 4) / 300, FW(fly, 8) / 300, FW(fly, 0xc) / 300);
        }
    }
}
