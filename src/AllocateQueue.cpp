// Coaster queues and the script commands that use them (TPConfig.cpp, main app 0x10049180..0x10049724), unit v5269.
//
// The Mac's sWraparoundQueue is 0x20 bytes of words; the host's mCapacity and mBlock are the same, and mF08 (24 bytes, host +0x10) holds the
// six words the Mac keeps at +8..+0x1f: +8 (limit of CoasterGetQueueSpace), +0xc (count), +0x10 (second count), +0x14 (read place), +0x18
// (write place), +0x1c (compared with the read place by CoasterGetPeep; AllocateQueue clears it). The two places are the 32-bit
// truncations of addresses inside the block (the Mac's pointers); they are widened with the block's high half where read.
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

// 0x1029ce24: a limit read by CoasterGetQueueSpace (zero at start like the Mac's bss; its writer is not translated yet).
int32_t gCoasterQueueSpaceLimit;

namespace {

struct QueueWords {
    uint32_t mF08;     // Mac +0x08
    uint32_t mCount;   // Mac +0x0c
    uint32_t mCount2;  // Mac +0x10
    uint32_t mRead;    // Mac +0x14
    uint32_t mWrite;   // Mac +0x18
    uint32_t mF1C;     // Mac +0x1c
};

QueueWords* Words(sWraparoundQueue* queue) { return reinterpret_cast<QueueWords*>(queue->mF08); }

uint32_t* Place(sWraparoundQueue* queue, uint32_t place32) {
    uintptr_t high = reinterpret_cast<uintptr_t>(queue->mBlock) & ~static_cast<uintptr_t>(0xffffffffu);
    return reinterpret_cast<uint32_t*>(high | place32);
}

// The place after `place`: block + (((place + 4 - block) / 4) % capacity) * 4 (srawi + addze, then divwu; a capacity of 0 divides to 0).
uint32_t NextPlace(sWraparoundQueue* queue, uint32_t place32) {
    uint32_t block32 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(queue->mBlock));
    int32_t diff = static_cast<int32_t>(place32 + 4 - block32);
    int32_t shifted = diff >> 2;
    if (shifted < 0 && (diff & 3) != 0) shifted += 1;
    uint32_t n = static_cast<uint32_t>(shifted);
    uint32_t cap = queue->mCapacity;
    uint32_t q = cap != 0 ? n / cap : 0;
    return block32 + ((n - q * cap) << 2);
}

bool Push(sWraparoundQueue* queue, uint32_t value) {
    QueueWords* w = Words(queue);
    if (w->mCount + w->mCount2 >= queue->mCapacity) return false;
    *Place(queue, w->mWrite) = value;
    w->mWrite = NextPlace(queue, w->mWrite);
    w->mCount += 1;
    return true;
}

tag_sCoasterInstance* Instance(uint32_t coaster) { return static_cast<tag_sCoasterInstance*>(gObjectTable[coaster]); }

}  // namespace

void AllocateQueue(sWraparoundQueue* queue, uint32_t capacity) {
    queue->mCapacity = capacity;
    QueueWords* w = Words(queue);
    w->mCount = 0;
    w->mCount2 = 0;
    w->mF08 = 0;
    queue->mBlock = MReleaseAllocF(queue->mCapacity << 2, 1);
    w->mRead = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(queue->mBlock));
    w->mWrite = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(queue->mBlock));
    w->mF1C = 0;
}

// Note: the original's order of stores is mCapacity, +8, +0xc, +0x10 (all 0); here +8 is mF08 and the three clears are the same stores.

uint32_t CoasterAddPeep(uint32_t coaster, uint32_t peep) {
    tag_sCoasterInstance* instance = Instance(coaster);
    Push(&instance->mQueueB0, peep);
    return 0;
}

// The two queues in turn: the first with room takes the peep.
void HACKCoasterReturnLostPeep(tag_sCoasterInstance* instance, uint32_t peep) {
    if (Push(&instance->mQueueB0, peep)) return;
    Push(&instance->mQueueD0, peep);
}

uint32_t CoasterGetQueueSpace(uint32_t coaster) {
    tag_sCoasterInstance* instance = Instance(coaster);
    QueueWords* w = Words(&instance->mQueueB0);
    int32_t space = static_cast<int32_t>(w->mF08 - (w->mCount + w->mCount2));
    // The listing keeps the global's address when the global is >= the space, and takes the local's when it is smaller: a signed maximum.
    return static_cast<uint32_t>(gCoasterQueueSpaceLimit < space ? space : gCoasterQueueSpaceLimit);
}

uint32_t CoasterGetPeep(uint32_t coaster) {
    tag_sCoasterInstance* instance = Instance(coaster);
    sWraparoundQueue* queue = &instance->mQueueD0;
    QueueWords* w = Words(queue);
    if (w->mCount == 0) return 0;
    uint32_t read = w->mRead;
    if (read == w->mF1C) return 0;
    if (w->mCount == 0) return 0;
    uint32_t value = *Place(queue, read);
    w->mRead = NextPlace(queue, read);
    w->mCount -= 1;
    return value;
}

uint32_t CoasterSetRepairState(uint32_t coaster, uint32_t state) {
    tag_sCoasterInstance* instance = Instance(coaster);
    int32_t s = static_cast<int32_t>(state);
    if (s == 1) {
        SetCoasterState(instance, 0x40);
    } else if (s < 1) {
        if (s >= 0) SetCoasterState(instance, 0x10);
    } else if (s < 3) {
        SetCoasterState(instance, 0x20);
    }
    return 0;
}

void CoasterSetSpeed(uint32_t coaster, uint32_t speed) {
    tag_sCoasterInstance* instance = Instance(coaster);
    if (instance == nullptr) return;
    uint32_t current;
    std::memcpy(&current, instance->mF30, 4);
    if (speed == current) return;
    if (gActionRecorder.mRecording != 0 || (instance->mFlags & 0x8000) == 0) CoasterSetSpeed(instance, speed, 0);
}

uint32_t CoasterSetOpenClosedState(uint32_t coaster, int32_t state) {
    tag_sCoasterInstance* instance = Instance(coaster);
    if (state == 0) {
        if ((instance->mState & 0xa) != 0) SetCoasterState(instance, 4);
    } else if ((instance->mState & 4) != 0) {
        SetCoasterState(instance, 8);
    }
    return 0;
}
