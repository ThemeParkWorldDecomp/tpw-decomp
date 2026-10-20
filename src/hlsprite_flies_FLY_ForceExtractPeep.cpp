// The fly bases' peep handling and repair state (unit v5980): FLY_ForceExtractPeep 0x1007861c, FLY_ExtractPeep 0x1007848c,
// FLY_AddPeep 0x100787c8, FLY_SetRepairState 0x10078b40. A fly base is words only; offsets below are the Mac's.
#include "HLSprite.h"
#include "LbFile.h"
#include "Debug.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "rsse.h"

using processinstruction_callee::Labs;

namespace {

int32_t& W(uint8_t* p, uint32_t off) { return *reinterpret_cast<int32_t*>(p + off); }

constexpr uint32_t kSlotCount = 0x14;
constexpr uint32_t kStride = 0xe4;

// The head the slot's ride shows for peep number n: the mod of (n + offset) by the divisor, plus 1, found in the ride's set (0xffffffff: none).
// Answers true with the index when the slot has a divisor and a ride (the callers act only then).
bool HeadFor(uint8_t* slot, int32_t n, uint32_t* index, tag_sRideAnimSet** set) {
    const int32_t divisor = W(slot, 0x138);
    if (divisor == 0) return false;
    const int32_t sum = n + W(slot, 0x134);
    const int32_t rem = sum - (sum / divisor) * divisor;
    const uint32_t ride = static_cast<uint32_t>(W(slot, 0x11c));
    if (ride == 0) return false;
    *set = gRideAnimSets[ride];
    *index = FindDataNodeIndexByID(*set, 0x80, static_cast<uint32_t>(rem + 1));
    return *index != 0xffffffffu;
}

}  // namespace

int32_t FLY_ForceExtractPeep(int32_t fly) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr) {
            for (uint32_t i = 0; static_cast<int32_t>(i) < static_cast<int32_t>(kSlotCount); ++i) {
                const uint32_t s = i * kStride;
                if (W(base, s + 0xe8) > 0) {
                    const int32_t n = Labs(W(base, s + 0xe8));
                    uint32_t index;
                    tag_sRideAnimSet* set;
                    if (HeadFor(base + s, n, &index, &set)) DisableRideHead(set->mHeads, index);
                    int32_t& count = W(base, s + 0xe8);
                    if (count > 0) count = count - 1; else count = count + 1;
                    W(base, 0x48) -= 1;
                    const int32_t m = Labs(W(base, s + 0xe8));
                    return W(base, s + 0xec + static_cast<uint32_t>(m) * 4);
                }
            }
        }
    }
    return 0;
}

int32_t FLY_ExtractPeep(int32_t fly) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr) {
            for (uint32_t i = 0; i < kSlotCount; ++i) {
                const uint32_t s = i * kStride;
                if (W(base, s + 0xdc) == 1) {
                    const int32_t count = W(base, s + 0xe8);
                    if (count >= 0) return 0;
                    uint32_t index;
                    tag_sRideAnimSet* set;
                    if (HeadFor(base + s, -count, &index, &set)) DisableRideHead(set->mHeads, index);
                    W(base, s + 0xe8) = W(base, s + 0xe8) + 1;
                    W(base, 0x48) -= 1;
                    return W(base, s + 0xec + static_cast<uint32_t>(-W(base, s + 0xe8)) * 4);
                }
            }
        }
    }
    return 0;
}

int32_t FLY_AddPeep(int32_t fly, int32_t peep) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr) {
            for (uint32_t i = 0; i < kSlotCount; ++i) {
                const uint32_t s = i * kStride;
                if (W(base, s + 0xdc) == 1) {
                    const int32_t count = W(base, s + 0xe8);
                    if (count < 0) return 0;
                    W(base, s + 0xec + static_cast<uint32_t>(count) * 4) = peep;
                    W(base, s + 0xe8) = W(base, s + 0xe8) + 1;
                    uint32_t index;
                    tag_sRideAnimSet* set;
                    if (W(base, s + 0x138) != 0) {
                        // the head number uses the count after the increment, read back from the base
                        if (HeadFor(base + s, W(base, s + 0xe8), &index, &set)) EnableRideHead(set->mHeads, index, static_cast<uint32_t>(peep));
                    }
                    W(base, 0x48) += 1;
                    return 1;
                }
            }
        }
    }
    return 0;
}

namespace {

int32_t ThirdOf(int32_t v) { return v / 300; }
int32_t Scaled(int32_t v) { return static_cast<int32_t>(static_cast<uint32_t>(v) << 10) / 300; }

void StartCrash(uint8_t* base) {
    W(base, 0x114) = 1;
    W(base, 0xe0) = 0;
    if (W(base, 0x120) != 0) PAR_KillEmitter(W(base, 0x120));
    const int32_t a = Scaled(W(base, 0x60));
    const int32_t b = Scaled(W(base, 0x64));
    const int32_t c = Scaled(W(base, 0x68));
    W(base, 0x120) = static_cast<int32_t>(PAR_InitEmitter(0x28, a, b, c));
}

}  // namespace

int32_t FLY_SetRepairState(int32_t fly, int32_t state) {
    if (fly > 0 && fly < 100) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[fly]);
        if (base != nullptr && state != W(base, 0x34)) {
            W(base, 0x34) = state;
            if (state == 3) {
                StartCrash(base);
            } else if (state < 3) {
                if (state == 1) {
                    W(base, 0) = W(base, 4);
                    if (W(base, 0x114) != 0) {
                        const uint32_t ride = static_cast<uint32_t>(W(base, 0x11c));
                        if (ride != 0 && (W(base, 0x128) != 10 || W(base, 0x12c) != 0 || W(base, 0x130) != 0)) {
                            const int32_t answer = static_cast<int32_t>(SetCurRideAnimDuration(gRideAnimSets[ride], 10, 0, 0, 1.0f, 0));
                            W(base, 0x128) = 10;
                            W(base, 0x12c) = 0;
                            W(base, 0x130) = 0;
                            GAME_ASSERTMSG(answer, "FLY: Failed to trigger a flying car anim (%ld)", static_cast<int32_t>(10));
                        }
                        W(base, 0x114) = 0;
                        SOUND_SendEvent(0, gSoundBanks[2], 0xbb, ThirdOf(W(base, 0x60)), ThirdOf(W(base, 0x64)), ThirdOf(W(base, 0x68)));
                    }
                    if (W(base, 0xd8) != 0) {
                        const int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand()));
                        W(base, 0xd8) = W(base, 0x28) + (r - (r / 100) * 100);
                    }
                    if (W(base, 0x120) != 0) {
                        PAR_KillEmitter(W(base, 0x120));
                        W(base, 0x120) = 0;
                    }
                } else if (state > 1) {
                    W(base, 0) = W(base, 4);
                    if (W(base, 0x120) != 0) PAR_KillEmitter(W(base, 0x120));
                    W(base, 0x120) = static_cast<int32_t>(PAR_InitEmitter(0x27, W(base, 0x60), W(base, 0x64), W(base, 0x68)));
                }
            } else if (state < 5) {
                StartCrash(base);
                while (FLY_ForceExtractPeep(fly) != 0) {
                }
                W(base, 0) = 1;
            }
        }
    }
    return 0;
}
