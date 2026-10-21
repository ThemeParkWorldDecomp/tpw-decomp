// CoasterTrain.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "FS_Online.h"
#include "helpers/CoasterSoundsHelpers.h"
#include "helpers/CoasterTrainHelpers.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "tb_matrix.h"
#include "TPAdvisor.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v5369: the trains' capacity and activation (TPConfig.cpp range of the main app).
//   RecalcDistancesFromCentreCar 0x1004a4e8, SetTrainCapacity 0x1004a328, ActivateCoasterTrain 0x1004a8a0

// 0x1004a4e8 (260 bytes): the centre car is at distance 0; the cars before it count up, the cars after it count down (negative), by the type's
// spacing (first car, last car, or a middle car's); mF3C is the distance divided by the instance's mFAC.
void RecalcDistancesFromCentreCar(tag_sCoasterInstance* instance, sTrainInstance* train) {
    const tag_sCoasterType* type = train->mType;
    sCarInstance* centre = &train->mCars[train->mCentreCar];
    float zero = 0.0f;
    float distance = zero;
    centre->mF38 = zero;
    StoreFloat(centre->mF3C, zero);
    if (train->mCentreCar != 0) {
        for (int32_t i = static_cast<int32_t>(train->mCentreCar) - 1; i >= 0; --i) {
            sCarInstance* car = &train->mCars[i];
            float step;
            if (car->mIndex == 0) step = type->mF30C;
            else if (car->mIndex == train->mCarsInUse - 1) step = type->mF314;
            else step = type->mF310;
            distance = distance + step;
            car->mF38 = distance;
            StoreFloat(car->mF3C, distance / instance->mFAC);
        }
    }
    distance = 0.0f;
    for (int32_t i = static_cast<int32_t>(train->mCentreCar) + 1; i < static_cast<int32_t>(train->mCarsInUse); ++i) {
        sCarInstance* car = &train->mCars[i];
        float step;
        if (car->mIndex == 0) step = type->mF30C;
        else if (car->mIndex == train->mCarsInUse - 1) step = type->mF314;
        else step = type->mF310;
        distance = distance - step;
        car->mF38 = distance;
        StoreFloat(car->mF3C, distance / instance->mFAC);
    }
}

// 0x1004a648 (224 bytes)
void DisableCoasterCarDrawSafe(uint32_t ride) {
    tag_sRideAnimSet* set = gRideAnimSets[ride];
    if (set == nullptr) return;
    tag_sMesh* mesh = set->mHeads;
    if ((mesh->mF00 & 1) != 0) {
        if ((set->mFlags & 0x4000) != 0) DisableMeshDraw(mesh, set->mFC0, set->mFC4);
        else DisableMeshDraw(mesh);
    }
    for (uint32_t i = 0; i < 0x100; ++i) {
        int32_t index = FindNthDataNodeIndex(set, 0x80, i + 1);
        if (index == -1) continue;
        sEmbeddedSet* embedded = static_cast<sEmbeddedSet*>(set->mHeads->mF28);
        if ((embedded->mRecords[index].mFlags & 2) != 0) DisableRideHead(set->mHeads, static_cast<uint32_t>(index));
    }
}

// A coaster car bent to the track, and a running train's sounds and screams (TPConfig.cpp on the Mac, unit w214): AccumulateBendAngles
// 0x10045ad8, RotateNodeVertexPosns 0x10046620, BendCarToTrack 0x1004688c, HandleScreams 0x10046d78, TickTrainSounds 0x1004702c and
// InterpAngles 0x1004dd5c, with the eight clamps of the bend angles (gBendAngleClamps, 0x1029cd8c).
//
// CoasterCarRideTrackTick (0x1004c4f0, untranslated) calls BendCarToTrack once per car: AccumulateBendAngles reads the sampled track
// around the car's place (include/TPCoaster.h, sTrackSample) and leaves, for every sample from the car's tail to its nose, the yaw and the
// pitch the track turns by there against the car's own direction, and the rail's height; RotateNodeVertexPosns then turns every vertex of
// the car's body node by the angles interpolated at the vertex's z. The sample stride is 0x38 bytes on the Mac and 0x40 on the host (the
// piece pointer), which the array indexing here follows. TickTrainSounds keeps a train's two sound effects at the ride's level and place
// and calls HandleScreams, which starts, moves and stops the riders' screams.
//
// The compare directions follow the original's fcmpo / fcmpu + branch pairs, so a NaN takes the same way; the fused operations are the
// original's fmadds / fmsubs (ppc_math.h), and the project builds with -ffp-contract=off, so no other product-sum is fused. The angles use
// the engine's sine table (gTPGlobals), not the C library's sin and cos; the arc cosine, the square root, fmod and modf are the C library's.

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

void SetCoasterCapacity(tag_sCoasterInstance* instance, uint32_t trains, uint32_t unused);

// 0x1004a75c (84 bytes). The MSL modf on the value widened to double: the answer is the fraction as a float and the whole part, truncated to an int
// (fctiwz), goes to *whole when that is given.
float ModF(float value, int32_t* whole) {
    double integral;
    double fraction = std::modf(static_cast<double>(value), &integral);
    if (whole != nullptr) *whole = ppc_fctiwz(integral);
    return static_cast<float>(fraction);
}

// 0x1004a7d0 (116 bytes). The train's front sample: position plus the first car's sample over the coaster's scale plus its type's mF24
// over the scale; the rear: position plus the last car's sample over the scale minus its type's mF28 over the scale.
void GetTrainFrontAndRearSamples(tag_sCoasterInstance* instance, sTrainInstance* train) {
    sCarInstance* first = train->mCars + static_cast<int32_t>(train->mF08);
    sCarInstance* last = first + static_cast<int32_t>(train->mCarsInUse - 1);
    float front = train->mPosition + first->mF38 / instance->mFAC;
    front = front + first->mType->mF24 / instance->mFAC;
    train->mFrontSample = front;
    float rear = train->mPosition + last->mF38 / instance->mFAC;
    rear = rear - last->mType->mF28 / instance->mFAC;
    train->mRearSample = rear;
}

// 0x1004a8a0 (380 bytes): only when the instance's word +0x3c has bit 4.
void ActivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    if ((instance->mBlock3C & 4) == 0) return;
    instance->mFlags |= 0x800;
    GetRideAnimTime(0);
    train->mFlags |= 2;
    sCarInstance* car = train->mCars;
    for (uint32_t i = 0; i < train->mCarsInUse; ++i, ++car) car->mF1C = 0;
    car = train->mCars + (train->mCarsInUse - 1);
    for (uint32_t i = 0; i < train->mCarsInUse; ++i, --car) {
        car->mFlags |= 1;
        if (car->mRide != 0) {
            tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
            if ((set->mFlags & 0x4000) != 0) EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
            else EnableMeshDraw(set->mHeads);
        }
        car->mCurrentA = car->mHeadsA;
        car->mCurrentB = car->mHeadsB;
        car->mCountA = 0;
        car->mCountB = 0;
        if ((car->mType->mFlags & 1) == 0 && car->mRide != 0)
            SetCurRideAnim(gRideAnimSets[car->mRide], 5, 0, 1, 1.0f, 0);
    }
    StartTrainSounds(instance, train, 0);
}

// 0x1004aa70 (200 bytes). Clears the drawn bit of each car in use and disables the draw of its ride's mesh (the animated kind,
// flag 0x4000, with the two HFieldXZ), then stops the train's sounds and clears the train's active bit (2).
void DeactivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train) {
    sCarInstance* car = train->mCars + static_cast<int32_t>(train->mF08);
    for (uint32_t count = train->mCarsInUse; count != 0; count--) {
        car->mFlags &= ~1u;
        if (car->mRide != 0) {
            tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
            if ((set->mFlags & 0x4000) != 0) {
                DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
            } else {
                DisableMeshDraw(set->mHeads);
            }
        }
        car++;
    }
    StopTrainSounds(instance, train);
    train->mFlags &= ~2u;
}

// A coaster's cars ridden along the track each frame (TPConfig.cpp on the Mac, unit w215): CoasterCarRideTrackTick 0x1004c4f0,
// SetCarOrientation 0x1004ab8c, SetTrainSoundState 0x100473e4, HandleCollisions 0x1004c3c0, InterpolateCoasterSpline 0x1004b0b0 (the
// instance overload), PlayCollisionSound 0x10047510, GetLinearSamplePosn 0x1004afa4 and DistanceBetweenPoints 0x1004f374 (the reference
// overload; the pointer overload at 0x100c3198 is in src/rsse_math.cpp).
//
// CoasterCarRideTrackTick runs once a frame for a running coaster (src/TPCoaster_ticks.cpp calls it). It works in four passes over the
// instance's trains. The first moves every drawn car's point (mF48) to the track's sample point at the train's place. The second is the
// train physics: the trains are kept in a list ordered by the time they have left in the frame (mTrainWords, one word per train), and the
// train at the head is advanced half a sample at a time (its speed pulled towards the speed the track under its cars asks for, the lift
// hill's floor speed while a car stands on a brace of kind 0x800, the banking fade at the end of the lap, the collision of HandleCollisions
// against the train ahead) until its time is spent; a train that has spent its time leaves the list. The third pass puts every car on the
// spline (InterpolateCoasterSpline), turns its ride node (SetCarOrientation), bends bent cars (BendCarToTrack, unit w214), sets the
// train's sound state (SetTrainSoundState) and keeps the lead train; the fourth, for a train with flag 0x80 (a train that has just left
// the station), clears the flag once the time to ride the stretch up to the next train is over the type's limit.
//
// The compare directions follow the original's fcmpo / fcmpu + branch pairs, so a NaN takes the same way ((v < floor) ? floor : v keeps a
// NaN); the fused operations are the original's fmadds / fmsubs / fmadd (ppc_math.h), and the project builds with -ffp-contract=off, so no
// other product-sum is fused. The angles of SetCarOrientation use the engine's sine table (gTPGlobals). The order list is a list of
// 32-bit words on the Mac; here the words hold Ptr32 offsets (the trains come from the arena), as InstanceCoasterTrains (unit w192) sized
// them.

// The tick's function-local static: the first call writes the largest float here and sets the flag (nothing in this unit reads the float).
uint8_t gCarRideTickInit = 0;   // 0x102b746c
float gCarRideTickMax = 0.0f;   // 0x102b7470

namespace {

// The engine's sine table at an angle times the table's scale: the index is truncated (fctiwz) and masked. The sine of x is
// SineAt(x * scale), the cosine of x is SineAt((pi / 2 + x) * scale).
float SineAt(float scaled) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)))];
}

// The train in a word of the order list (0: none) and the word of a train.
sTrainInstance* TrainOf(uint32_t word) {
    Ptr32<sTrainInstance> train;
    train.mOffset = word;
    return train.get();
}

uint32_t WordOf(sTrainInstance* train) {
    Ptr32<sTrainInstance> word;
    word.set(train);
    return word.mOffset;
}

// The point of the track at a place in samples: the straight line between the two samples around it, as the tick's first two passes
// compute it (the fused form of GetLinearSamplePosn).
void LinearPoint(const sTrackSample* samples, int32_t sample, float t, float* out) {
    const float u = 1.0f - t;
    const s3DPoint& a = samples[sample].mPos;
    const s3DPoint& b = samples[sample + 1].mPos;
    out[0] = ppc_fmadds(a.x, u, b.x * t);
    out[1] = ppc_fmadds(a.y, u, b.y * t);
    out[2] = ppc_fmadds(a.z, u, b.z * t);
}

}   // namespace

void SetTrainSoundState(sTrainInstance* train, uint32_t state, uint32_t value);

void PlayCollisionSound(tag_sCoasterInstance* instance, sTrainInstance* train);

// 0x1004ab8c (760 bytes). Turns the ride node of a car's ride (the first node of the ride's mesh, gRideAnimSets[mRide]) to the
// orientation: the node's matrix is the identity turned about z by the third angle (mF0C, the bank), about x by the second (mF08) and
// about y by the first (mF04), with the engine's sine table. When a point is given it is turned by the same three turns in the same
// order (the original reads back, from memory, the coordinate it has just stored: the value is the same). The car's mF10 is the matrix's
// third row, the direction the car faces.
void SetCarOrientation(sCarInstance* car, sCoasterOrientation* orient, s3DPoint* point) {
    sMatrix* matrix = &gRideAnimSets[car->mRide]->mHeads->mHeader->mF78->mMatrix;
    SetMatrixIdentity(matrix);
    ZRotateLMatrixNoTrans(matrix, SineAt((kHalfPi + orient->mF0C) * gTPGlobals.mSinTableScale), SineAt(orient->mF0C * gTPGlobals.mSinTableScale));
    XRotateLMatrixNoTrans(matrix, SineAt((kHalfPi + orient->mF08) * gTPGlobals.mSinTableScale), SineAt(orient->mF08 * gTPGlobals.mSinTableScale));
    YRotateLMatrixNoTrans(matrix, SineAt((kHalfPi + orient->mF04) * gTPGlobals.mSinTableScale), SineAt(orient->mF04 * gTPGlobals.mSinTableScale));
    if (point != nullptr) {
        // About z: x and y.
        float cosine = SineAt((kHalfPi + orient->mF0C) * gTPGlobals.mSinTableScale);
        float sine = SineAt(orient->mF0C * gTPGlobals.mSinTableScale);
        float y = point->y;
        float x = point->x;
        point->x = ppc_fmsubs(x, cosine, y * sine);
        point->y = ppc_fmadds(x, sine, y * cosine);
        // About x: y and z.
        cosine = SineAt((kHalfPi + orient->mF08) * gTPGlobals.mSinTableScale);
        sine = SineAt(orient->mF08 * gTPGlobals.mSinTableScale);
        const float z = point->z;
        y = point->y;
        point->y = ppc_fmsubs(y, cosine, z * sine);
        point->z = ppc_fmadds(y, sine, z * cosine);
        // About y: z and x.
        cosine = SineAt((kHalfPi + orient->mF04) * gTPGlobals.mSinTableScale);
        sine = SineAt(orient->mF04 * gTPGlobals.mSinTableScale);
        x = point->x;
        const float z2 = point->z;
        const float negated = ppc_fneg(x);
        point->x = ppc_fmadds(x, cosine, z2 * sine);
        point->z = ppc_fmadds(negated, sine, z2 * cosine);
    }
    car->mF10[0] = matrix->m[2][0];
    car->mF10[1] = matrix->m[2][1];
    car->mF10[2] = matrix->m[2][2];
}

// 0x1004aedc (132 bytes). The speed a car wants at a height: a plus (the type's +0x28 going up, +0x2c going down, times the type's
// +0x24 times (c - d)); at least b. A NaN gives b.
float GetCarWantedSpeedAtHeight(sCoasterType* type, float a, float b, float c, float d) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(type);
    float f24, f28, f2c;
    std::memcpy(&f24, bytes + 0x24, 4);
    std::memcpy(&f28, bytes + 0x28, 4);
    std::memcpy(&f2c, bytes + 0x2c, 4);
    float slope = (d > c) ? f28 : f2c;
    float diff = c - d;
    float scaled = f24 * diff;
    float wanted = ppc_fmadds(slope, scaled, a);
    return !(wanted < b) ? wanted : b;   // bge is "not less": a NaN keeps wanted
}

// 0x1004afa4 (96 bytes). The point a fraction t of the way from sample `sample` to the next one: samples[i] * (1 - t) + samples[i + 1] * t,
// the first product fused with the sum.
void GetLinearSamplePosn(tag_sCoasterInstance* instance, int32_t sample, float t, s3DPoint* out) {
    const float u = 1.0f - t;
    const s3DPoint& a = instance->mSamples[sample].mPos;
    const s3DPoint& b = instance->mSamples[sample + 1].mPos;
    out->x = ppc_fmadds(a.x, u, b.x * t);
    out->y = ppc_fmadds(a.y, u, b.y * t);
    out->z = ppc_fmadds(a.z, u, b.z * t);
}

// Unit v51069: Advisor.cpp's static initialiser and save templates, and small leaves of the bump rides (TPAdvisor.cpp), the camera,
// the coasters and the lobby (main app).

// 0x1004b050: position >= the track's length (false for a NaN).
int32_t IsEndOfSampleBuffer(tag_sCoasterInstance* instance, float position) {
    return position < instance->mLength ? 0 : 1;   // the original's compare falls through to 1 for a NaN
}

// 0x1004b0b0 (224 bytes). The instance overload: the track's point and orientation at a place in samples. The sample the place lies in
// (its whole part) names the two lists of controls (the piece, mPiece, and the second list, mPieceB) at the start of that sample's stretch; both
// lists are walked, in step, to the first control of the first list at or past the place, and the sample overload (0x1004e318) interpolates
// between that control and the one before it, at the fraction of the place between their mF08.
void InterpolateCoasterSpline(tag_sCoasterInstance* instance, float position, s3DPoint* point, sCoasterOrientation* orient) {
    double whole;
    std::modf(static_cast<double>(position), &whole);
    const sTrackSample* sample = &instance->mSamples[ppc_fctiwz(whole)];
    // The sample's piece pointer (sTrackSamplePiece) is the start of a control: the host's sTrackSamplePiece is a prefix view of it.
    tag_sBezierControl* a = reinterpret_cast<tag_sBezierControl*>(sample->mPiece);
    tag_sBezierControl* c = sample->mPieceB;
    tag_sBezierControl* b = a->mNext;
    tag_sBezierControl* d = c->mNext;
    while (b->mF08 < position) {
        a = b;
        c = d;
        b = b->mNext;
        d = d->mNext;
    }
    const float t = (position - a->mF08) / (b->mF08 - a->mF08);
    InterpolateCoasterSpline(instance->mSamples, reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C), a, b, c, d, t, point, orient);
}

// GenerateTrackSubdivisions 0x1004e8c0, CoasterSetSpeed (instance overload) 0x1004b94c, GetBankAtBrace 0x1004b1f8,
// FlashEndPylons 0x10052158, DecCoasterConst1 0x100524a0.

// 0x1004b1f8. The bank change per unit of travel at the brace (0 when it has no bank or flag 0x100).
float GetBankAtBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, float f) {
    float zero = 0.0f;
    if (brace->mF94w != zero && (brace->mFlags & 0x100) == 0) {
        float d1 = DistanceBetweenPoints(reinterpret_cast<const s3DPoint*>(&brace->mNext->mX4C), reinterpret_cast<const s3DPoint*>(&brace->mX4C));
        float d2 = DistanceBetweenPoints(reinterpret_cast<const s3DPoint*>(&brace->mPrev->mX4C), reinterpret_cast<const s3DPoint*>(&brace->mX4C));
        float bank = brace->mF94w;
        float sum = d1 + d2;
        float limit;
        if (bank >= zero) limit = 3.1415927410125732f;
        else limit = -3.1415927410125732f;
        float diff = limit - bank;
        const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
        float scaled = f * diff;
        float per = scaled / type->mF10;
        return per / sum;
    }
    return 0.0f;
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

// 0x1004b94c. Sets the coaster's speed (a percentage): blends the type's two speed limits, flags the track for generation.
void CoasterSetSpeed(tag_sCoasterInstance* instance, uint32_t speed, int32_t flag) {
    if (flag == 0) LayoutRecordCoasterSpeed(instance, speed);
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    float fraction = static_cast<float>(speed) / 100.0f;
    float one = 1.0f;
    float high = type->mF50;
    float low = type->mF4C;
    float inverse = one - fraction;
    float scaled = high * fraction;
    instance->mSpeedLimit = ppc_fmadds(low, inverse, scaled);
    std::memcpy(instance->mF30, &speed, 4);
    FlagGenerateTrack(instance, 1);
    type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    if ((type->mFlags & 8) != 0) FlagRecalcAllCoasterCollisions(instance);
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

// 0x1004c3c0 (224 bytes). Two trains that have run into each other: the train behind the given one in the list (the next in the array,
// wrapping), when it is slower than the given train and the given train's nose is past the other's tail (the tail taken one lap on when the
// other's place is behind the given one's), take a third of each other's speed twice their own (2 * slow + fast goes to the fast one, 2 *
// fast + slow to the slow one, over 3), the slow one marked (flag 0x40); the collision sound plays when the speeds are then over 1 apart.
void HandleCollisions(tag_sCoasterInstance* instance, sTrainInstance* train) {
    const uint32_t count = static_cast<uint32_t>(instance->mTrainCount);
    if (count <= 1) return;
    sTrainInstance* next = &instance->mTrains[(static_cast<uint32_t>(train - instance->mTrains) + 1) % count];
    float rear = next->mRearSample;
    if (next->mPosition < train->mPosition) rear = rear + instance->mLength;
    if (!(train->mFrontSample > rear)) return;
    const float slow = next->mF78;
    const float fast = train->mF78;
    if (!(slow < fast)) return;
    train->mF78 = ppc_fmadds(2.0f, slow, fast) / 3.0f;
    next->mF78 = ppc_fmadds(2.0f, fast, next->mF78) / 3.0f;
    next->mFlags |= 0x40;
    const float difference = static_cast<float>(std::fabs(static_cast<double>(train->mF78) - static_cast<double>(next->mF78)));
    if (difference > 1.0f) PlayCollisionSound(instance, train);
}

float DistanceBetweenPoints(s3DPoint& a, s3DPoint& b);

// 0x1004c4f0 (3,512 bytes). See the head of the file for the four passes.
void CoasterCarRideTrackTick(tag_sCoasterInstance* instance) {
    if (gCarRideTickInit == 0) {
        gCarRideTickMax = FLT_MAX;
        gCarRideTickInit = 1;
    }
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    uint32_t* order = instance->mTrainWords;
    const uint32_t now = GetRideAnimTime(0);
    instance->mLeadTrain = instance->mTrains;
    const float minSpeed = type->mF48 * instance->mF2C;    // the speed a train keeps on the plain track
    const float boost = type->mF44 * instance->mF2C;       // the speed it keeps on a lift (a brace of kind 0x800)
    const float maxSpeed = instance->mSpeedLimit;
    uint32_t count = static_cast<uint32_t>(instance->mTrainCount);
    const float length = instance->mLength;
    const float fac = instance->mFAC;
    float fade = 0.0f;   // the last banking fade made (a train's x^4 over the fade stretch); the Mac keeps it in a register across trains
    if (!(instance->mFlags & 0x10)) {
        instance->mF120 = now;
        instance->mFlags |= 0x10;
    }
    const float frameTime = static_cast<float>(now - instance->mF120) / 1000.0f;

    // Pass 1: every train gets the frame's time as its time left, every drawn car the track's point at its place, and the train is entered
    // in the order list.
    for (uint32_t k = 0; k < static_cast<uint32_t>(instance->mTrainCount); k++) {
        sTrainInstance* train = &instance->mTrains[k];
        train->mF7C = frameTime;
        for (uint32_t j = 0; j < train->mCarsInUse; j++) {
            sCarInstance* car = &train->mCars[j];
            if (!(car->mFlags & 1)) continue;
            float place = train->mPosition + car->mF38 / instance->mFAC;
            if (place < 0.0f) {
                place = place + instance->mLength;
            } else {
                place = static_cast<float>(std::fmod(static_cast<double>(place), static_cast<double>(instance->mLength)));
            }
            int32_t sample;
            const float fraction = SplitFraction(place, sample);
            LinearPoint(instance->mSamples, sample, fraction, car->mF48);
        }
        order[k] = WordOf(train);
    }

    // Pass 2: advance the trains, the one with the least time left first.
    while (count != 0) {
        sTrainInstance* train = TrainOf(order[0]);
        float start = (length < train->mPosition) ? length : train->mPosition;
        bool lifted = false;
        int32_t ahead = 0;
        float sum = 0.0f;
        sCarInstance* centre = &train->mCars[train->mCentreCar];
        const float floorSpeed = ((train->mFlags & 0x80) && (train->mFlags & 0x20)) ? 0.5f * boost : boost;
        for (uint32_t j = 0; j < train->mCarsInUse; j++) {
            sCarInstance* car = &train->mCars[j];
            if (!(car->mFlags & 1)) continue;
            float place = train->mPosition + car->mF38 / instance->mFAC;
            if (place < 0.0f) place = place + length;
            float nose = 0.5f + place;
            if (nose < 0.0f) {
                nose = nose + length;
            } else {
                nose = static_cast<float>(std::fmod(static_cast<double>(nose), static_cast<double>(length)));
            }
            int32_t sample;
            const float fraction = SplitFraction(nose, sample);
            LinearPoint(instance->mSamples, sample, fraction, car->mF54);
            ahead += (place > instance->mBankFadeStart) ? 1 : 0;
            const tag_sTrackBrace* brace = instance->mSamples[sample].mPiece->mBrace;
            const float drop = car->mF48[1] - car->mF54[1];
            const float gravity = (car->mF54[1] > car->mF48[1]) ? type->mF28 : type->mF2C;
            float speed = ppc_fmadds(gravity, type->mF24 * drop, train->mF78);
            if (brace->mFlags & 0x800) {
                speed = (speed < floorSpeed) ? floorSpeed : speed;
                sum = sum + speed;
                lifted = true;
                car->mFlags |= 2;
            } else {
                speed = (speed < minSpeed) ? minSpeed : speed;
                sum = sum + speed;
                car->mFlags &= ~2u;
            }
        }
        sum = sum / static_cast<float>(train->mCarsInUse);
        const float distance = DistanceBetweenPoints(*reinterpret_cast<s3DPoint*>(centre->mF48), *reinterpret_cast<s3DPoint*>(centre->mF54));
        if (ahead != 0 && !(train->mFlags & 0x40)) {
            // Cars past the start of the banking fade: the speed fades to the floor speed over the rest of the lap, x^4 of the way.
            const sCarInstance* first = &train->mCars[train->mF08];
            const float stretch = length - instance->mBankFadeStart;
            const float along = (train->mPosition + first->mF38 / instance->mFAC) - instance->mBankFadeStart;
            float x = along / stretch;
            if (x < 0.0f) {
                x = 1.0f;
            } else if (x > 1.0f) {
                x = 1.0f;
            }
            fade = x * x;
            fade = fade * fade;
            sum = ppc_fmadds(sum, 1.0f - fade, floorSpeed * fade);
            train->mFlags |= 0x10;
        } else {
            const int32_t at = ppc_fctiwz(static_cast<double>(centre->mF40));
            if (instance->mSamples[at].mFlags & 8) sum = sum * type->mF54;
            train->mFlags &= ~0x10u;
        }
        if (lifted) {
            sum = (sum < floorSpeed) ? floorSpeed : sum;
            train->mFlags |= 8;
        } else {
            sum = (sum < minSpeed) ? minSpeed : sum;
            train->mFlags &= ~8u;
        }
        sum = (maxSpeed < sum) ? maxSpeed : sum;
        const float step = distance / ((train->mF78 + sum) * 0.5f);
        if (!(step >= train->mF7C)) {
            // The step takes less than the time left (or is NaN): the train takes it, half a sample on, its cars' points moved to the
            // half-sample points, and goes back in the list after the trains with less time left.
            train->mF78 = sum;
            train->mF7C = train->mF7C - step;
            train->mPosition = train->mPosition + 0.5f;
            for (uint32_t j = 0; j < train->mCarsInUse; j++) {
                std::memcpy(train->mCars[j].mF48, train->mCars[j].mF54, sizeof(train->mCars[j].mF48));
            }
            bool placed = false;
            for (uint32_t k = 1; k < count; k++) {
                if (!(TrainOf(order[k])->mF7C > train->mF7C)) {
                    order[k - 1] = WordOf(train);
                    placed = true;
                    break;
                }
                order[k - 1] = order[k];
            }
            if (!placed) order[count - 1] = WordOf(train);
        } else {
            // The time left ends inside the step: the train rides the part of the step the time left covers at the mean of its speed and
            // the speed it was heading for, and leaves the list.
            const float portion = train->mF7C / step;
            const float blended = ppc_fmadds(1.0f - portion, train->mF78, portion * sum);
            train->mPosition = ppc_fmadds(0.5f, (train->mF7C * ((train->mF78 + blended) * 0.5f)) / distance, start);
            // The original stores the speed clamped below by minSpeed and then, over it, the speed clamped above by maxSpeed.
            train->mF78 = (blended < minSpeed) ? minSpeed : blended;
            train->mF78 = (maxSpeed < blended) ? maxSpeed : blended;
            count--;
            for (uint32_t k = 0; k < count; k++) order[k] = order[k + 1];
            order[count] = 0;
        }
        const float factor = (train->mF78 < 2.0f) ? 0.95f : 1.0f;   // 0x3f733333: a train under speed 2 loses 5% a frame
        train->mF78 = train->mF78 * factor;
        if (train->mF78 < 0.01f) train->mF78 = 0.0f;
        sCarInstance* head = &train->mCars[train->mF08];
        train->mFrontSample = (train->mPosition + head->mF38 / instance->mFAC) + head->mType->mF24 / instance->mFAC;
        sCarInstance* tail = head + (static_cast<int32_t>(train->mCarsInUse) - 1);   // counted from the first car's place, as the original does
        train->mRearSample = (train->mPosition + tail->mF38 / instance->mFAC) - tail->mType->mF28 / instance->mFAC;
        HandleCollisions(instance, train);
        if (!(train->mPosition < instance->mLength)) {
            train->mPosition = static_cast<float>(std::fmod(static_cast<double>(train->mPosition), static_cast<double>(length)));
        }
        if (train->mPosition > instance->mFFC && train->mPosition < instance->mBankFadeStart) train->mFlags &= ~0x40u;
    }

    // Pass 3: the cars on the spline, the ride nodes, the bent cars, the trains' sounds and the lead train; pass 4 inside it (flag 0x80).
    for (uint32_t k = 0; k < static_cast<uint32_t>(instance->mTrainCount); k++) {
        sTrainInstance* train = &instance->mTrains[k];
        uint32_t lifts = 0;
        for (uint32_t j = 0; j < train->mCarsInUse; j++) {
            sCarInstance* car = &train->mCars[j];
            if (!(car->mFlags & 1)) {
                car->mF40 = 0.0f;
                continue;
            }
            car->mF40 = train->mPosition + car->mF38 / instance->mFAC;
            if (car->mF40 < 0.0f) {
                car->mF40 = car->mF40 + length;
            } else {
                car->mF40 = static_cast<float>(std::fmod(static_cast<double>(car->mF40), static_cast<double>(length)));
            }
            s3DPoint point;
            sCoasterOrientation* orient = reinterpret_cast<sCoasterOrientation*>(&instance->mF78);
            InterpolateCoasterSpline(instance, car->mF40, &point, orient);
            if (car->mRide != 0) {
                if (car->mType->mF18 == 0.0f) {
                    SetCarOrientation(car, orient, nullptr);
                } else {
                    // The car's height above the rail, turned with the car.
                    s3DPoint offset = {0.0f, car->mType->mF18, 0.0f};
                    SetCarOrientation(car, orient, &offset);
                    point.x = point.x + offset.x;
                    point.y = point.y + offset.y;
                    point.z = point.z + offset.z;
                }
                tag_sNode* node = gRideAnimSets[car->mRide]->mHeads->mHeader->mF78;
                node->mMatrix.m[3][0] = point.x;
                node->mMatrix.m[3][1] = point.y;
                node->mMatrix.m[3][2] = point.z;
                if (!(car->mType->mFlags & 1)) {
                    UpdateRideAnimSpeedImmediate(gRideAnimSets[car->mRide], train->mF78 * car->mType->mF1C, 0);
                }
            }
            if (j == train->mCentreCar) std::memcpy(train->mF48, &point, sizeof(train->mF48));
            if (j == train->mF08) std::memcpy(train->mF3C, &point, sizeof(train->mF3C));
            if (car->mType->mFlags & 1) BendCarToTrack(instance, car);
            lifts += (car->mFlags & 2) != 0 ? 1u : 0u;
        }
        if (train->mFlags & 0x10) {
            SetTrainSoundState(train, 8, static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(fade))) * 50u + 50u);
        } else if (lifts == 0 || instance->mF2C == 0.0f) {
            SetTrainSoundState(train, 0x10, 100);
        } else {
            const float ratio = train->mF78 / instance->mSpeedLimit;
            const uint32_t state = (ratio > 0.3f) ? 4 : 2;
            SetTrainSoundState(train, state, (lifts * 100) / static_cast<uint32_t>(train->mType->mF2F4));
        }
        if (train->mPosition < instance->mLeadTrain->mPosition) instance->mLeadTrain = train;
        if (train->mFlags & 0x80) {
            sTrainInstance* next = &instance->mTrains[(static_cast<uint32_t>(train - instance->mTrains) + 1) % static_cast<uint32_t>(instance->mTrainCount)];
            const uint32_t nextWhole = cw_cvt_fp2unsigned(static_cast<double>(next->mPosition));
            uint32_t end = nextWhole;
            if (static_cast<float>(nextWhole) < train->mPosition) end = instance->mSampleCount + nextWhole - 1;
            float time = 0.0f;
            // The original indexes the samples by the train's number k, not by the sample being passed.
            for (uint32_t sample = cw_cvt_fp2unsigned(static_cast<double>(train->mPosition)); sample < end; sample++) {
                time = time + fac / instance->mSamples[k % instance->mSampleCount].mSpeed;
            }
            if (1000.0f * time >= static_cast<float>(static_cast<uint32_t>(type->mF2EC))) train->mFlags &= ~0x80u;
        }
    }
    instance->mF120 = now;
}

// 0x1004d2ec (the same machine code as ThemeParkWorld::HandleUpdateWindow): returns at once.
void DoImmediatePhysicsFeedback(tag_sCoasterInstance*) {}

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

// 0x1004d920
void GetCoasterPeepStats(uint32_t id, float* ratingA, float* ratingB) {
    tag_sCoasterInstance* instance = static_cast<tag_sCoasterInstance*>(gObjectTable[id]);
    *ratingA = instance->mRatingA;
    *ratingB = instance->mRatingB;
}

// TPConfig.cpp (main app), unit v5275: RoundUp 0x1004da1c (92 bytes) and GetCoasterTrackStats 0x1004d970 (116 bytes).

// 0x1004d970: copies the track length, the top height, the top speed and the marked-brace count of the coaster of the object.
void GetCoasterTrackStats(void* object, float* a, float* height, float* c, uint32_t* loops) {
    tag_sCoasterInstance* instance = GetCoasterInstanceFromCObject(object);
    *a = instance->mTrackLength;       // 0x10c
    *height = instance->mTopHeight;    // 0x110
    *c = instance->mTopSpeed;          // 0x114
    *loops = instance->mMarkedBraces;  // 0x108
}
