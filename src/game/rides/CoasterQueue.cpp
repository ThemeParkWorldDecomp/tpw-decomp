// CoasterQueue.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "FS_Online.h"
#include "helpers/CoasterTrainHelpers.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LevelInit.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "rsse.h"
#include "Serialise.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_main.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).

// 0x10049120: the object number of the instance with that script id, 0 when none.
int32_t GetCoasterInstanceID(int32_t handle) {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        if (instance->mScriptID == handle) return static_cast<int32_t>(instance->mObjectInstanceID);
    }
    return 0;
}

// Coaster queues and the script commands that use them (TPConfig.cpp, main app 0x10049180..0x10049724), unit v5269.
//
// The Mac's sWraparoundQueue is 0x20 bytes of words; the host's mCapacity and mBlock are the same, and mF08 (24 bytes, host +0x10) holds the
// six words the Mac keeps at +8..+0x1f: +8 (limit of CoasterGetQueueSpace), +0xc (count), +0x10 (second count), +0x14 (read place), +0x18
// (write place), +0x1c (compared with the read place by CoasterGetPeep; AllocateQueue clears it). The two places are the 32-bit
// truncations of addresses inside the block (the Mac's pointers); they are widened with the block's high half where read.

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

// 0x10049228 (the same machine code as CLBFlyingMeshSystem::Register).
void DestroyQueue(sWraparoundQueue* queue) {
    MReleaseFreeF(queue->mBlock);
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

// Coaster train loading: peeps move between the entrance queue (instance +0xb0), the cars' head slots and the exit queue (instance +0xd0).

// Defined here: no other file names them (bss on the Mac).
int32_t gCoasterCapacityFloor;   // 0x1029ce28
int32_t gKickPeepsFloor;         // 0x1029ce2c

namespace {

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

// The coaster's track statistics (TPConfig.cpp on the Mac, unit w212): ModF 0x1004a75c, SetCoasterCapacity 0x1004a248, UpdateCoasterTrainsActive
// 0x1004b320, UpdateCoasterTrainPosns 0x1004b4ac, SpreadTrainsOnTrack 0x1004b6c4, CalculateTrackGForcesSpeedAndExcitement 0x1004ba48,
// SetInitialExcitementValues 0x1004d338, UpdateExcitementAtSample 0x1004d424 and CollateCoasterStats 0x1004d7c8, with the 22 words of TPConfig.cpp
// they work on (gTrackGravity at 0x1029ce30 on, the excitement's running values at 0x102b7474 on).
//
// GenerateTrack (0x10050fcc, untranslated) is the one caller: it samples the track (include/TPCoaster.h, sTrackSample) and calls
// CalculateTrackGForcesSpeedAndExcitement, which works the g-forces, the speed and the excitement out sample by sample, banks the track and
// totals the stats; when the instance's flag 0x1000 is set it then sets the capacity, activates and spreads the trains over the track and
// places their cars.
//
// The compare directions follow the original's fcmpo / fcmpu + branch pairs, so a NaN takes the same way; the fused operations are the
// original's fmadds / fmsubs (ppc_math.h), and the project builds with -ffp-contract=off, so no other product-sum is fused. The sample
// stride is 0x38 bytes on the Mac and 0x40 on the host (the piece pointer): a sample index or a position past the track reads another place
// on either side, which is why the callers keep to [0, count - 2].

// 0x1004a248 (156 bytes). The third argument is never read. A capacity of 0 changes nothing. When the instance is in one of the loading states
// (mState & 0xc) the trains from the new count up to the old one are unloaded twice, flag 0 then flag 1 (the train base and the count are read
// again on each pass).
void SetCoasterCapacity(tag_sCoasterInstance* instance, uint32_t trains, uint32_t unused) {
    if (trains == 0) return;
    if ((instance->mState & 0xc) != 0) {
        for (uint32_t j = trains; j < static_cast<uint32_t>(instance->mTrainCount); ++j) {
            sTrainInstance* train = &instance->mTrains[j];
            UnloadTrain(instance, train, 0);
            UnloadTrain(instance, train, 1);
        }
    }
    instance->mTrainCount = static_cast<int32_t>(trains);
}

// Unit v5369: the trains' capacity and activation (TPConfig.cpp range of the main app).
//   RecalcDistancesFromCentreCar 0x1004a4e8, SetTrainCapacity 0x1004a328, ActivateCoasterTrain 0x1004a8a0

void RecalcDistancesFromCentreCar(tag_sCoasterInstance* instance, sTrainInstance* train);

// 0x1004a328 (380 bytes). The word at +0xf0 of the instance (inside mFF0) is the capacity; +0xb8 (inside mQueueB0) gets its copy at the end.
void SetTrainCapacity(tag_sCoasterInstance* instance, uint32_t a, uint32_t b, uint32_t c) {
    sTrainInstance* train = instance->mTrains;
    const tag_sCoasterType* instanceType = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    for (uint32_t t = 0; t < static_cast<uint32_t>(instanceType->mF2E8); ++t, ++train) {
        if ((train->mFlags & 4) != 0 || (b != 0 && b != train->mCarsInUse)) {
            if (b != 0) train->mCarsInUse = b;
            train->mF08 = 0;
            train->mCentreCar = ((train->mCarsInUse - 1) >> 1) + train->mF08;
            if (train->mCarsInUse != 0) {
                RecalcDistancesFromCentreCar(instance, train);
                GetTrainFrontAndRearSamples(instance, train);
            }
        }
        sCarInstance* car = train->mCars;
        if ((c & 1) != 0) {
            if (a == 0) a = train->mType->mF2F8;
            std::memcpy(instance->mFF0, &a, 4);
            uint32_t remaining = a;
            const tag_sCoasterType* type = train->mType;
            uint32_t slots = train->mCarsInUse;
            if (type->mCarTypeA != type->mCarTypeB) slots = slots - 1;
            if (type->mCarTypeC != type->mCarTypeB) slots = slots - 1;
            uint32_t used = 0;
            for (uint32_t i = 0; i < train->mCarsInUse; ++i, ++car) {
                if (car->mType == train->mType->mCarTypeB) {
                    uint32_t divisor = slots - used;
                    ++used;
                    uint32_t share = divisor != 0 ? remaining / divisor : remaining;   // divwu by 0 is undefined; the PowerPC emulator answers the dividend
                    car->mF44 = share;
                    remaining = remaining - share;
                } else {
                    car->mF44 = 0;
                }
            }
        }
    }
    std::memcpy(instance->mQueueB0.mF08, instance->mFF0, 4);
}
