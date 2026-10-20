// Coaster train loading: peeps move between the entrance queue (instance +0xb0), the cars' head slots and the exit queue (instance +0xd0).
#include <cstdint>
#include <cstring>
#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "LevelInit.h"
#include "rsse.h"
#include "ppc_math.h"
#include "platform/arena.h"

// Defined here: no other file names them (bss on the Mac).
int32_t gCoasterCapacityFloor;   // 0x1029ce28
int32_t gKickPeepsFloor;         // 0x1029ce2c

namespace {

// The queue's words after the capacity and the block (the Mac's +0x08..+0x1f); the cursors are 32-bit addresses into the block.
struct QueueWords {
    uint32_t mF08;
    uint32_t mCount;
    uint32_t mCount2;
    uint32_t mRead;
    uint32_t mWrite;
    uint32_t mF1C;
};

QueueWords* Words(sWraparoundQueue* queue) { return reinterpret_cast<QueueWords*>(queue->mF08); }

// A cursor is the low 32 bits of an address in the arena (the arena lies within one 4 GB range).
uint32_t* Slot(uint32_t address) {
    const uintptr_t high = reinterpret_cast<uintptr_t>(tpw::ArenaBase()) & ~static_cast<uintptr_t>(0xFFFFFFFFu);
    return reinterpret_cast<uint32_t*>(high | address);
}

// The cursor one word on, wrapped: ((cursor + 4 - base) / 4) % capacity words from the base (srawi + addze, then divwu).
uint32_t Advance(uint32_t cursor, const sWraparoundQueue* queue) {
    const uint32_t base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(queue->mBlock));
    const int32_t d = static_cast<int32_t>(cursor + 4 - base);
    const uint32_t words = static_cast<uint32_t>((d >> 2) + ((d < 0 && (d & 3) != 0) ? 1 : 0));
    const uint32_t cap = queue->mCapacity;
    const uint32_t quotient = cap != 0 ? words / cap : 0;
    return base + (words - quotient * cap) * 4;
}

uint32_t Dequeue(sWraparoundQueue* queue) {
    QueueWords* w = Words(queue);
    if (w->mCount == 0) return 0;
    const uint32_t item = *Slot(w->mRead);
    w->mRead = Advance(w->mRead, queue);
    w->mCount--;
    return item;
}

void Enqueue(sWraparoundQueue* queue, uint32_t item) {
    QueueWords* w = Words(queue);
    if (w->mCount + w->mCount2 < queue->mCapacity) {
        *Slot(w->mWrite) = item;
        w->mWrite = Advance(w->mWrite, queue);
        w->mCount++;
    }
}

// mulhw by 0x2aaaaaab, srawi 4, plus the sign: a byte difference divided by 0x60.
uint32_t Div96(int32_t d) {
    int32_t hi = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * d) >> 32);
    hi >>= 4;
    return static_cast<uint32_t>(hi + static_cast<int32_t>(static_cast<uint32_t>(hi) >> 31));
}

float ReadFloat(const uint8_t* bytes) {
    float f;
    std::memcpy(&f, bytes, 4);
    return f;
}

}  // namespace

// 0x10049758
uint32_t CoasterSetCapacity(uint32_t coaster, int32_t capacity) {
    tag_sCoasterInstance* instance = static_cast<tag_sCoasterInstance*>(gObjectTable[coaster]);
    int32_t v = capacity;
    if (gCoasterCapacityFloor >= v) v = gCoasterCapacityFloor;
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    if (!(v < type->mF318)) v = type->mF318;
    const int32_t heads = instance->mTrains->mType->mF2F8;
    if (!(v < heads)) v = heads;
    int32_t current;
    std::memcpy(&current, instance->mFF0, 4);
    if (v != current) SetTrainCapacity(instance, static_cast<uint32_t>(v), 0, 1);
    // The original answers whatever r3 holds (the instance, or SetTrainCapacity's leftover): no caller reads it.
    return 0;
}

// 0x10049858
void KickPeepsFromEntranceQueue(tag_sCoasterInstance* instance) {
    const int32_t space = static_cast<int32_t>(instance->mQueueD0.mCapacity - Words(&instance->mQueueD0)->mCount) -
                          static_cast<int32_t>(Words(&instance->mQueueD0)->mCount2);
    int32_t n = static_cast<int32_t>(Words(&instance->mQueueB0)->mCount + Words(&instance->mQueueB0)->mCount2);
    if (!(n < space)) n = space;
    if (gKickPeepsFloor < n) {
        // n stays
    } else {
        n = gKickPeepsFloor;
    }
    for (int32_t i = 0; i < n; i++) {
        const uint32_t peep = Dequeue(&instance->mQueueB0);
        if (peep != 0) Enqueue(&instance->mQueueD0, peep);
    }
}

// 0x100499e4
void UnloadTrain(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t flag) {
    sCarInstance* car = train->mCars + train->mF08;
    for (uint32_t k = 0; k < train->mCarsInUse; k++, car++) {
        uint32_t* heads = flag != 0 ? car->mCurrentA : car->mCurrentB;
        uint32_t* count = flag != 0 ? &car->mCountA : &car->mCountB;
        for (uint32_t i = 0; i < *count; i++) {
            Enqueue(&instance->mQueueD0, heads[i]);
            DisableRideHead(gRideAnimSets[car->mRide]->mHeads, reinterpret_cast<const uint32_t*>(car->mType->mBlock)[i]);
            train->mF10--;
            heads[i] = 0;
        }
        *count = 0;
    }
}

// 0x10049bf4
void LoadTrain(tag_sCoasterInstance* instance, sTrainInstance* train, uint32_t first, uint32_t count) {
    for (;;) {
        if (train->mF14 == 0 || count == 0) return;
        const uint32_t peep = Dequeue(&instance->mQueueB0);
        if (peep == 0) return;
        const uint32_t seed = static_cast<uint32_t>(gLevelRandom) * 0x343fd + 0x269ec3;
        gLevelRandom = static_cast<int32_t>(seed);
        const uint32_t r = seed >> 16;
        sCarInstance* begin = train->mCars + first;
        sCarInstance* end = train->mCars + (count + first);
        sCarInstance* car = train->mCars + (first + (r - (r / count) * count));
        for (uint32_t i = 0; i < count; i++) {
            if (car->mCountB < car->mF44) {
                EnableRideHead(gRideAnimSets[car->mRide]->mHeads, reinterpret_cast<const uint32_t*>(car->mType->mBlock)[car->mCountB], peep);
                train->mF10++;
                car->mCurrentB[car->mCountB] = peep;
                car->mCountB++;
                break;
            }
            car++;
            if (car >= end) car = begin;
        }
        train->mF14--;
    }
}

// 0x10049ddc
void SetEntranceLinearSamples(tag_sCoasterInstance* instance) {
    tag_sTrackBrace* brace = instance->mLastBrace;
    const float a = brace->mF90->mControlsA->mF08;
    const float b = brace->mPrev->mPrev->mF90->mControlsA->mF08;
    const float t = reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF31C;
    const float oneMinus = 1.0f - t;
    const float mid = (a + b) * 0.5f;
    const float lo = ppc_fmadds(a, oneMinus, mid * t);
    std::memcpy(instance->mFF8, &lo, 4);
    const float t2 = reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF31C;
    instance->mFFC = ppc_fmadds(b, 1.0f - t2, mid * t2);
}

// 0x10049e8c
void GetCarsInStation(tag_sCoasterInstance* instance, sTrainInstance* train, uint32_t* first, uint32_t* last) {
    const sCarInstance* firstCar = nullptr;
    const sCarInstance* lastCar = nullptr;
    const sCarInstance* car = train->mCars + train->mF08;
    for (uint32_t i = 0; i < train->mCarsInUse; i++, car++) {
        const float fac = instance->mFAC;
        const float front = car->mF40 + car->mType->mF24 / fac;
        const float rear = car->mF40 - car->mType->mF28 / fac;
        if (front >= ReadFloat(instance->mFF8) && rear <= instance->mFFC) {
            if (firstCar == nullptr) firstCar = car;
            lastCar = car;
        }
    }
    // No car found: the original divides the null's distance to the car array (a 32-bit address difference).
    const uint32_t base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(train->mCars));
    *first = firstCar != nullptr ? static_cast<uint32_t>(firstCar - train->mCars) : Div96(static_cast<int32_t>(0 - base));
    *last = lastCar != nullptr ? static_cast<uint32_t>(lastCar - train->mCars) : Div96(static_cast<int32_t>(0 - base));
}

// 0x10049fbc
void HandleTrainLoading(tag_sCoasterInstance* instance, sTrainInstance* train) {
    const bool inStation = train->mFrontSample >= ReadFloat(instance->mFF8) && train->mRearSample <= instance->mFFC;
    if (!inStation && (train->mFlags & 0x20) == 0) return;
    if (inStation) {
        if ((train->mFlags & 0x20) == 0) UnloadTrain(instance, train, 1);
        if ((instance->mState & 4) != 0) {
            uint32_t a, b;
            GetCarsInStation(instance, train, &a, &b);
            const uint32_t first = train->mF18 < a ? train->mF18 : a;
            const uint32_t last = b < train->mF1C ? train->mF1C : b;
            train->mF14 = 0;
            const uint32_t count = (last - first) + 1;
            const sCarInstance* car = train->mCars + first;
            for (uint32_t i = 0; i < count; i++, car++) train->mF14 += car->mF44 - car->mCountB;
            LoadTrain(instance, train, first, count);
            train->mF18 = a;
            train->mF1C = b;
        }
        train->mFlags |= 0x20;
    } else {
        if ((instance->mState & 4) != 0) {
            train->mF14 = 0;
            const uint32_t count = train->mCarsInUse - train->mF18;
            const sCarInstance* car = train->mCars + train->mF18;
            for (uint32_t i = 0; i < count; i++, car++) train->mF14 += car->mF44 - car->mCountB;
            LoadTrain(instance, train, train->mF18, count);
        }
        train->mF18 = 0;
        train->mF1C = 0;
        sCarInstance* car = train->mCars + train->mF08;
        for (uint32_t i = 0; i < train->mCarsInUse; i++, car++) {
            uint32_t* tmp = car->mCurrentA;
            car->mCurrentA = car->mCurrentB;
            car->mCurrentB = tmp;
            car->mCountA = car->mCountB;
            car->mCountB = 0;
        }
        train->mFlags &= ~0x20u;
    }
}
