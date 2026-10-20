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
#include <cmath>
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"

// ---- the words of TPConfig.cpp: the first twelve are initialised data on the Mac (0x1029ce30 on), the other ten bss (0x102b7474 on)
float gTrackGravity = 1.0f;
int32_t gCoasterMinTrains = 1;
float gBankFadeStartMin = 0.0f;
float gBankFadeMax = 1.0f;
float gBankFadeMin = 0.0f;
float gExciteBankFloorA = 0.0f;
float gExciteBankFloorB = 0.0f;
float gExciteFloor = 0.0f;
float gRatingBMax = 100.0f;
float gRatingBMin = 0.0f;
float gRatingAMax = 100.0f;
float gRatingAMin = 0.0f;
int32_t gExciteRiseA;
int32_t gExciteRiseB;
float gExcitePrevB;
float gExciteMaxB;
float gExciteMaxA;
float gExcitePrevBank;
float gExcitePrevG[3];
int32_t gExciteWord;
float gExciteB;
float gExciteA;

namespace {

const float kPi = 3.1415927f;        // Bits(0x40490fdb)
const float kHalfPi = 1.5707964f;    // Bits(0x3fc90fdb)
const float kTwoPi = 6.2831855f;     // Bits(0x40c90fdb)
// The bank pass adds the float pi and 2 pi widened to double (0x400921fb60000000 and 0x401921fb60000000), not M_PI.
const double kPiD = static_cast<double>(kPi);
const double kTwoPiD = static_cast<double>(kTwoPi);

// The engine's sine table at the angle's negative: sin(-angle), and cos(-angle) as sin(pi / 2 - angle) (the index forms of the original).
float TableSin(float angle) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(-angle * gTPGlobals.mSinTableScale))];
}
float TableCos(float angle) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz((kHalfPi + -angle) * gTPGlobals.mSinTableScale))];
}

// The length of a vector as the original sums it: z squared + (x squared + y squared), the x square exact in double (the fmadd).
float Length3(float x, float y, float z) {
    return static_cast<float>(std::sqrt(static_cast<double>(z * z) + ppc_fmadd(static_cast<double>(x), static_cast<double>(x), static_cast<double>(y * y))));
}

}  // namespace

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

// 0x1004a75c (84 bytes). The MSL modf on the value widened to double: the answer is the fraction as a float and the whole part, truncated to an int
// (fctiwz), goes to *whole when that is given.
float ModF(float value, int32_t* whole) {
    double integral;
    double fraction = std::modf(static_cast<double>(value), &integral);
    if (whole != nullptr) *whole = ppc_fctiwz(integral);
    return static_cast<float>(fraction);
}

// 0x1004b320 (324 bytes). Walks the type's mF2E8 trains (the type is read again on each pass, the instance's train count too): the trains inside
// the count are switched on (flags 1 and 4, and activated when the instance has a train active, flag 0x800); the ones outside are switched off
// (flag 1 cleared) and, when the instance has a train active, their cars (from mF08, mCarsInUse of them) lose flag 1 and their ride's meshes are
// taken off the map: over the range mFC0..mFC4 when the set has flag 0x4000, else wholly; then the sounds stop and flag 2 is cleared.
void UpdateCoasterTrainsActive(tag_sCoasterInstance* instance) {
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(reinterpret_cast<const tag_sCoasterType*>(instance->mType)->mF2E8); ++i, ++train) {
        if (i < static_cast<uint32_t>(instance->mTrainCount)) {
            if ((train->mFlags & 1) == 0) {
                train->mFlags |= 5;
                if ((instance->mFlags & 0x800) != 0) ActivateCoasterTrain(instance, train);
            }
        } else if ((train->mFlags & 1) != 0) {
            train->mFlags &= ~1u;
            if ((instance->mFlags & 0x800) != 0) {
                sCarInstance* car = &train->mCars[train->mF08];
                for (uint32_t n = train->mCarsInUse; n != 0; --n, ++car) {
                    car->mFlags &= ~1u;
                    if (car->mRide != 0) {
                        tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
                        if ((set->mFlags & 0x4000) != 0) DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
                        else DisableMeshDraw(set->mHeads);
                    }
                }
                StopTrainSounds(instance, train);
                train->mFlags &= ~2u;
            }
        }
    }
}

// 0x1004b4ac (468 bytes). Places the cars of each train (mTrainCount of them, read again on each pass): the cars are laid from mCars[mCarsInUse - 1]
// down to mCars[0] (not from mF08) from the train's mF30, each the two cars' lengths (the front car's front, the next one's rear) over the sample
// spacing past the one before; the train's position is its centre car's; the front and rear samples come from the cars at mF08 and
// mF08 + mCarsInUse - 1; mF78 is the mean of the track's speed under the cars (mCars[0..], the speed interpolated between two samples).
void UpdateCoasterTrainPosns(tag_sCoasterInstance* instance) {
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); ++i, ++train) {
        float place = train->mF30;
        sCarInstance* car = &train->mCars[train->mCarsInUse - 1];
        for (uint32_t j = 0; j < train->mCarsInUse; ++j, --car) {
            car->mF40 = place;
            if (j < train->mCarsInUse - 1) place = place + (car->mType->mF24 + (car - 1)->mType->mF28) / instance->mFAC;
        }
        train->mPosition = train->mCars[train->mCentreCar].mF40;
        sCarInstance* front = &train->mCars[train->mF08];
        train->mFrontSample = (train->mPosition + front->mF38 / instance->mFAC) + front->mType->mF24 / instance->mFAC;
        sCarInstance* rear = front + (train->mCarsInUse - 1);
        train->mRearSample = (train->mPosition + rear->mF38 / instance->mFAC) - rear->mType->mF28 / instance->mFAC;
        float sum = 0.0f;
        sCarInstance* under = train->mCars;
        for (uint32_t n = train->mCarsInUse; n != 0; --n, ++under) {
            int32_t index;
            float fraction = ModF(under->mF40, &index);
            const sTrackSample* samples = instance->mSamples;
            sum = sum + ppc_fmadds(samples[index].mSpeed, 1.0f - fraction, samples[index + 1].mSpeed * fraction);
        }
        train->mF78 = sum / static_cast<float>(train->mCarsInUse);
        train->mFlags &= ~4u;
    }
}

// 0x1004b6c4 (580 bytes). The float argument is never read. Each train's mF30 becomes its last car's place (mCars[mF08 + mCarsInUse - 1]), moved
// onto the track when negative; then the trains are spread one train interval (the type's mF2EC milliseconds) apart in the time the track takes:
// from the previous train's place the samples are stepped (spacing over each sample's speed) until the interval is passed, and the place is
// interpolated between the last two samples, wrapped to the track's length. mLeadTrain is the first train. Zero trains divide by zero in
// the original (divwu): not tested.
void SpreadTrainsOnTrack(tag_sCoasterInstance* instance, float time) {
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    float interval = static_cast<float>(static_cast<uint32_t>(type->mF2EC)) / 1000.0f;
    float spacing = instance->mFAC;
    uint32_t n = static_cast<uint32_t>(instance->mTrainCount);
    sTrainInstance* train = instance->mTrains;
    while (n != 0) {
        train->mF30 = train->mCars[train->mF08 + train->mCarsInUse - 1].mF40;
        if (train->mF30 < 0.0f) train->mF30 = train->mF30 + instance->mLength;
        ++train;
        --n;
    }
    uint32_t j = 1 % static_cast<uint32_t>(instance->mTrainCount);
    instance->mLeadTrain = instance->mTrains;
    sTrainInstance* previous = instance->mTrains;
    uint32_t count = static_cast<uint32_t>(instance->mTrainCount) - 1;
    while (count != 0) {
        sTrainInstance* current = &instance->mTrains[j];
        double integral;
        float fraction = static_cast<float>(std::modf(static_cast<double>(previous->mF30), &integral));
        int32_t k = ppc_fctiwz(integral);
        float t = ((1.0f - fraction) * spacing) / instance->mSamples[k].mSpeed;
        float before = 0.0f;
        while (t < interval) {
            before = t;
            k = static_cast<int32_t>(static_cast<uint32_t>(k + 1) % instance->mSampleCount);
            t = t + spacing / instance->mSamples[k].mSpeed;
        }
        float f = (t - interval) / (t - before);
        current->mF30 = ppc_fmadds(static_cast<float>(k - 1), 1.0f - f, static_cast<float>(k) * f);
        current->mF30 = static_cast<float>(std::fmod(static_cast<double>(current->mF30) + static_cast<double>(instance->mLength), static_cast<double>(instance->mLength)));
        previous = current;
        j = (j + 1) % static_cast<uint32_t>(instance->mTrainCount);
        --count;
    }
}

// 0x1004ba48 (2,340 bytes). Over the inner samples (1 .. count - 2): the step to the previous and the next sample gives the g-forces, a(c) =
// ((previous - here) + (next - here)) / spacing * -gravity; the speed is carried along the track (gaining or losing it from the climb by the type's
// factors, floored at the type's minimum speed, scaled by mF54 where the track climbs, capped at the speed limit); the sample's g is then
// scaled by speed / 10 and UpdateExcitementAtSample adds its share of the excitement. With the type's flag 8 the sample's banking is then
// worked out from its g-forces by the sine table's rotations and faded out towards the end of the track; the stats are collated and, when the
// instance asks for it (flag 0x1000, cleared here) and is not the editor's preview (0x4000), the trains are set up for the new track.
void CalculateTrackGForcesSpeedAndExcitement(tag_sCoasterInstance* instance) {
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    sTrackSample* samples = instance->mSamples;
    sTrackSample* previous = &samples[0];
    sTrackSample* current = &samples[1];
    sTrackSample* next = &samples[2];
    float minimum = type->mF44;
    float speed = minimum;
    float limit = instance->mSpeedLimit;
    float spacing = instance->mFAC;
    float topSpeed = 0.0f;
    float topHeight = 0.0f;
    float elapsed = 0.0f;
    SetInitialExcitementValues(instance);
    for (uint32_t i = 1; i < instance->mSampleCount - 1; ++i, ++previous, ++current, ++next) {
        float height = current->mPos.y;
        s3DPoint back = {previous->mPos.x - current->mPos.x, previous->mPos.y - current->mPos.y, previous->mPos.z - current->mPos.z};
        s3DPoint ahead = {next->mPos.x - current->mPos.x, next->mPos.y - current->mPos.y, next->mPos.z - current->mPos.z};
        tag_sTrackBrace* brace = current->mPiece->mBrace;
        if ((type->mFlags & 0x100) != 0) {
            // The sample climbs (flag 8) when the normalised step ahead rises by more than 0.01745 (1 degree); the g-forces use the raw steps.
            s3DPoint step = ahead;
            float length = Length3(step.x, step.y, step.z);
            step.x = step.x / length;
            step.y = step.y / length;
            step.z = step.z / length;
            if (step.y > 0.01745f) current->mFlags |= 8;
            else current->mFlags &= ~8u;
        }
        float gravity = -gTrackGravity;
        s3DPoint force;
        force.x = ((back.x + ahead.x) / spacing) * gravity;
        force.y = ((back.y + ahead.y) / spacing) * gravity;
        force.z = ((back.z + ahead.z) / spacing) * gravity;
        float candidate;
        if (next->mPos.y > height) candidate = ppc_fmadds(type->mF28, type->mF24 * (height - next->mPos.y), speed);
        else candidate = ppc_fmadds(type->mF2C, type->mF24 * (height - next->mPos.y), speed);
        speed = (candidate < minimum) ? minimum : candidate;
        if ((current->mFlags & 8) != 0) speed = speed * type->mF54;
        speed = (speed < minimum) ? minimum : speed;
        if (speed == minimum) brace->mFlags |= 0x800;
        speed = (limit < speed) ? limit : speed;
        current->mSpeed = speed;
        current->mG = force;
        current->mG.x = current->mG.x * (speed / 10.0f);
        current->mG.y = current->mG.y * (speed / 10.0f);
        current->mG.z = current->mG.z * (speed / 10.0f);
        uint32_t word;
        std::memcpy(&word, brace->mF00, sizeof(word));
        UpdateExcitementAtSample(instance, i, word, speed, current->mBank, &current->mG, elapsed);
        topSpeed = (topSpeed < speed) ? speed : topSpeed;
        topHeight = (topHeight < height) ? height : topHeight;
        elapsed = elapsed + spacing / speed;
    }
    samples = instance->mSamples;
    samples[0].mG.x = 0.0f;
    samples[0].mG.y = 0.0f;
    samples[0].mG.z = 0.0f;
    samples[0].mSpeed = minimum;
    sTrackSample* last = &samples[instance->mSampleCount - 1];
    last->mG = (last - 1)->mG;
    last->mSpeed = (last - 1)->mSpeed;
    instance->mBankFadeStart = static_cast<float>(instance->mSampleCount - 1) - samples[instance->mBraces->mSample - 1].mSpeed / instance->mFAC;
    instance->mBankFadeStart = (instance->mBankFadeStart < gBankFadeStartMin) ? gBankFadeStartMin : instance->mBankFadeStart;
    type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    if ((type->mFlags & 8) != 0) {
        float previousAngle = kPi;
        float biasX = type->mF60;
        float biasY = type->mF64;
        float biasZ = type->mF68;
        type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
        sTrackSample* sample = instance->mSamples;
        for (uint32_t i = 0; i < instance->mSampleCount; ++i, ++sample) {
            s3DPoint v = {biasX + sample->mG.x, biasY + sample->mG.y, biasZ + sample->mG.z};
            // Three rotations by the negatives of the sample's yaw, pitch and bank, each from the values before it.
            float sinYaw = TableSin(sample->mYaw), cosYaw = TableCos(sample->mYaw);
            float x1 = ppc_fmadds(v.x, cosYaw, v.z * sinYaw);
            float z1 = ppc_fmadds(-v.x, sinYaw, v.z * cosYaw);
            float sinPitch = TableSin(sample->mPitch), cosPitch = TableCos(sample->mPitch);
            float y1 = ppc_fmsubs(v.y, cosPitch, z1 * sinPitch);
            float z2 = ppc_fmadds(v.y, sinPitch, z1 * cosPitch);
            float sinBank = TableSin(sample->mBank), cosBank = TableCos(sample->mBank);
            float x2 = ppc_fmsubs(x1, cosBank, y1 * sinBank);
            float y2 = ppc_fmadds(x1, sinBank, y1 * cosBank);
            v.x = x2;
            v.y = y2;
            v.z = z2;
            float angle = GetVectorRotationAroundZAxisFromY(&v);
            float turn = static_cast<float>(std::fmod(kTwoPiD + (kPiD + (static_cast<double>(angle) - static_cast<double>(previousAngle))), kTwoPiD)) - kPi;
            float length = Length3(v.x, v.y, v.z);
            float share = type->mF6C * length;
            if (share > 1.0f) share = 1.0f;
            float delta = turn * share;
            float wrapped = static_cast<float>(std::fmod((kTwoPiD + (kPiD + static_cast<double>(previousAngle))) + static_cast<double>(delta), kTwoPiD)) - kPi;
            sample->mBank = -(kPi - wrapped);
            if (sample->mBank < -kPi) sample->mBank = sample->mBank + kTwoPi;
            float index = static_cast<float>(static_cast<int32_t>(i));
            if (index > instance->mBankFadeStart) {
                float fade = (index - instance->mBankFadeStart) / (instance->mLength - instance->mBankFadeStart);
                fade = (gBankFadeMax < fade) ? gBankFadeMax : fade;
                fade = (fade < gBankFadeMin) ? gBankFadeMin : fade;
                sample->mBank = sample->mBank * (1.0f - fade);
            }
            previousAngle = wrapped;
        }
    }
    CollateCoasterStats(instance, topHeight, topSpeed, elapsed);
    if ((instance->mFlags & 0x1000) != 0) {
        if ((instance->mFlags & 0x4000) == 0) {
            type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
            int32_t wanted = ppc_fctiwz(elapsed / (static_cast<float>(static_cast<uint32_t>(type->mF2EC)) / 1000.0f));
            wanted = (gCoasterMinTrains < wanted) ? wanted : gCoasterMinTrains;
            uint32_t trains = (static_cast<uint32_t>(type->mF2E8) < static_cast<uint32_t>(wanted)) ? static_cast<uint32_t>(type->mF2E8) : static_cast<uint32_t>(wanted);
            SetCoasterCapacity(instance, trains, 0);
            UpdateCoasterTrainsActive(instance);
            SpreadTrainsOnTrack(instance, elapsed);
            UpdateCoasterTrainPosns(instance);
        }
        instance->mFlags = instance->mFlags & ~0x1000u;
    }
}

// 0x1004d338 (164 bytes). The excitement starts at the type's two starting values; the previous values and the running maxima are cleared.
void SetInitialExcitementValues(tag_sCoasterInstance* instance) {
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    gExciteA = type->mF320;
    gExciteB = type->mF324;
    gExciteWord = 0;
    std::memset(gExcitePrevG, 0, sizeof(gExcitePrevG));
    gExcitePrevBank = 0.0f;
    gExciteMaxB = 0.0f;
    gExciteMaxA = 0.0f;
    gExciteRiseB = 0;
    gExciteRiseA = 0;
    gExcitePrevB = gExciteB;
}

// 0x1004d424 (844 bytes). The word (r5) and the time (f3) are never read. The excitement at one sample: A (a running mean of the change of the
// g-forces from the previous values, which stay at the zeros SetInitialExcitementValues left) and B (the speed, the banking and the g-forces
// weighed by the type's factors) halve and take their new shares; the maxima follow; and from the fourth sample on the flags 4 and 2 of the
// sample say whether B rises by more than the type's thresholds (or has been rising and is still above its level).
//
// The original's f0 reaches the one use below untouched from the caller: where the banking-and-downward-pull term is not worked out, X is
// whatever f0 held at the call, which at the only call site (CalculateTrackGForcesSpeedAndExcitement, right after it stored g->z) is g->z.
void UpdateExcitementAtSample(tag_sCoasterInstance* instance, uint32_t sample, uint32_t word, float speed, float bank, s3DPoint* g, float time) {
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    sTrackSample* samples = instance->mSamples;
    float dx = static_cast<float>(std::fabs(static_cast<double>(gExcitePrevG[0]) - static_cast<double>(g->x)));
    float dy = static_cast<float>(std::fabs(static_cast<double>(gExcitePrevG[1]) - static_cast<double>(g->y)));
    float dz = static_cast<float>(std::fabs(static_cast<double>(gExcitePrevG[2]) - static_cast<double>(g->z)));
    float change = type->mF32C * (dz + (dx + dy));
    float over = std::fabs(bank) - kHalfPi;
    float bankA = (over < gExciteBankFloorA) ? gExciteBankFloorA : over;
    float stepA = (bankA == 0.0f) ? 0.0f : 1.0f;
    float bankShare = (bankA + stepA) * type->mF330;
    float gx = std::fabs(g->x);
    float gy = std::fabs(g->y);
    float gz = std::fabs(g->z);
    float bankChange = static_cast<float>(std::fabs(static_cast<double>(bank) - static_cast<double>(gExcitePrevBank)));
    float weighed = type->mF340 * bankChange + (speed * type->mF334 + type->mF338 * (gz + (gx + gy)));
    over = std::fabs(bank) - kHalfPi;
    float bankB = (over < gExciteBankFloorB) ? gExciteBankFloorB : over;
    float stepB = (bankB == 0.0f) ? 0.0f : 1.0f;
    float pull = bankB + stepB;
    float other;
    if (pull != 0.0f && g->y < 0.1f) {
        float k = pull * type->mF348;
        other = (g->y < 0.0f) ? 3.0f * k : k;
    } else {
        other = g->z;
    }
    float e = weighed - (type->mF344 * gx + other);
    e = (e < gExciteFloor) ? gExciteFloor : e;
    gExciteA = ppc_fmadds(gExciteA, 0.5f, change + bankShare);
    gExciteB = ppc_fmadds(gExciteB, 0.5f, e);
    gExciteMaxA = (gExciteMaxA < gExciteA) ? gExciteA : gExciteMaxA;
    gExciteMaxB = (gExciteMaxB < gExciteB) ? gExciteB : gExciteMaxB;
    samples[sample].mFlags &= ~7u;
    if (sample > 2) {
        float rise = gExciteB - gExcitePrevB;
        if (rise > type->mF34C) {
            gExciteRiseA = 1;
            samples[sample].mFlags |= 4;
        } else if (gExciteRiseA != 0 && gExciteB > type->mF354) {
            samples[sample].mFlags |= 4;
        } else {
            gExciteRiseA = 0;
        }
        if (rise > type->mF350) {
            gExciteRiseB = 1;
            samples[sample].mFlags |= 2;
        } else if (gExciteRiseB != 0 && gExciteB > type->mF358) {
            samples[sample].mFlags |= 2;
        } else {
            gExciteRiseB = 0;
        }
    }
    gExcitePrevB = gExciteB;
}

// 0x1004d7c8 (276 bytes). The fourth argument (time) is never read. The two ratings are the excitement's totals (A plus its maximum; half of B
// plus 1.7 times its maximum) clamped to their ranges (a NaN comes out as the upper bound); the marked braces are counted along the chain from
// the last brace back (both flags 0x40 and 0x80); the track's length is the sample count in world units and the top height and speed are the
// caller's.
void CollateCoasterStats(tag_sCoasterInstance* instance, float height, float speed, float time) {
    instance->mRatingA = gExciteA + gExciteMaxA;
    instance->mRatingB = ppc_fmadds(0.5f, gExciteB, 1.7f * gExciteMaxB);
    instance->mRatingB = (instance->mRatingB < gRatingBMax) ? instance->mRatingB : gRatingBMax;
    instance->mRatingB = (gRatingBMin < instance->mRatingB) ? instance->mRatingB : gRatingBMin;
    instance->mRatingA = (instance->mRatingA < gRatingAMax) ? instance->mRatingA : gRatingAMax;
    instance->mRatingA = (gRatingAMin < instance->mRatingA) ? instance->mRatingA : gRatingAMin;
    instance->mMarkedBraces = 0;
    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        if ((brace->mFlags & 0x40) != 0 && (brace->mFlags & 0x80) != 0) instance->mMarkedBraces = instance->mMarkedBraces + 1;
    }
    instance->mTrackLength = instance->mLength * instance->mFAC;
    instance->mTopHeight = height;
    instance->mTopSpeed = speed;
}
