// HLSprite.cpp (main app 0x100781f4, unit w114): FLY_RemoveBase, the removal of one fly base.
#include "HLSprite.h"

#include "LbFile.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "rsse.h"

// Removes the fly base `index` (1 to 99; anything else, or an empty slot of gFlyBases, does nothing). Each of the base's 20 slots with a
// nonzero active word is torn down: |count| times (counting down to 1) the ride's head with the number ((n + offset) mod divisor) + 1 is
// found in the ride set's embedded data (mask 0x80) and disabled, when the divisor and the ride are not 0; then the emitter is killed
// and cleared, the ride instance destroyed and cleared, the base's live count goes down by one and the sound is stopped and cleared.
// Then the base is freed and its slot cleared. A slot with a zero active word is skipped whole.
//
// Quirks kept: the emitter and the sound words are cleared twice (the second store of each is unconditional); the active word is never
// cleared; the head number's remainder is a signed one (divw), the sum wraps in 32 bits; |count| is the C library's labs, so INT_MIN
// loops 2^32 times (and the remainder of INT_MIN by -1 is undefined): neither is tested. The base pointer is read from the table again
// at each use, as the original does.
void FLY_RemoveBase(int32_t index) {
    if (index <= 0 || index >= 100 || gFlyBases[index] == nullptr) return;
    for (int32_t slot = 0; slot < kFlySlotCount; ++slot) {
        sFlyBase* base = static_cast<sFlyBase*>(gFlyBases[index]);
        sFlySlot* s = base->Slot(slot);
        if (s->mActive == 0) continue;
        uint32_t n = s->mCount < 0 ? 0u - static_cast<uint32_t>(s->mCount) : static_cast<uint32_t>(s->mCount);
        for (; n != 0; n = n - 1) {
            if (s->mDivisor != 0) {
                const int32_t sum = static_cast<int32_t>(n + static_cast<uint32_t>(s->mOffset));
                const int32_t quotient = sum / s->mDivisor;
                const int32_t remainder =
                    static_cast<int32_t>(static_cast<uint32_t>(sum) - static_cast<uint32_t>(quotient) * static_cast<uint32_t>(s->mDivisor));
                if (s->mRide != 0) {
                    const uint32_t head = FindDataNodeIndexByID(gRideAnimSets[s->mRide], 0x80, static_cast<uint32_t>(remainder) + 1);
                    if (head != 0xffffffff) DisableRideHead(gRideAnimSets[s->mRide]->mHeads, head);
                }
            }
        }
        if (s->mEmitter != 0) {
            PAR_KillEmitter(static_cast<int32_t>(s->mEmitter));
            s->mEmitter = 0;
        }
        s->mEmitter = 0;
        DestroyRideInstance(s->mRide);
        s->mRide = 0;
        base->mLive = base->mLive - 1;
        if (s->mSound != 0) SOUND_StopSoundEffect(static_cast<int32_t>(s->mSound));
        s->mSound = 0;
        s->mSound = 0;
    }
    LbMem_Free(gFlyBases[index]);
    gFlyBases[index] = nullptr;
}
