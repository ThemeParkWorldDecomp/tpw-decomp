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
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "Ptr32.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

// The tick's function-local static: the first call writes the largest float here and sets the flag (nothing in this unit reads the float).
uint8_t gCarRideTickInit = 0;   // 0x102b746c
float gCarRideTickMax = 0.0f;   // 0x102b7470

namespace {

const float kHalfPi = 1.5707964f;   // Bits(0x3fc90fdb), 0x10264840

// The engine's sine table at an angle times the table's scale: the index is truncated (fctiwz) and masked. The sine of x is
// SineAt(x * scale), the cosine of x is SineAt((pi / 2 + x) * scale).
float SineAt(float scaled) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled)))];
}

// modf as the original expands it in place: the fraction of the value widened to double, as a float, and the whole part truncated to an
// int (fctiwz).
float SplitFraction(float value, int32_t& whole) {
    double integral;
    const double fraction = std::modf(static_cast<double>(value), &integral);
    whole = ppc_fctiwz(integral);
    return static_cast<float>(fraction);
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

// 0x100473e4 (236 bytes). Sets a train's sound state: when it differs from the train's current one (mF38), the state's parameter (the
// Mac's jump table at 0x1029cdac: 1 gives 0xf, 2 gives 0x1e, 4 gives 0x46, 8 gives 0x5a, 0x10 gives 0x32, every other state up to 0x10
// gives 0, above 0x10 gives 0) is set on the first sound effect (mF68, parameter mF54[2]) when it is not 0, and the state is kept. The
// value goes to the same sound effect's second parameter (mF54[3]) every time. Nothing is set while the action recorder is running.
void SetTrainSoundState(sTrainInstance* train, uint32_t state, uint32_t value) {
    if (state != train->mF38) {
        uint32_t parameter = 0;
        switch (state) {
            case 1: parameter = 0xf; break;
            case 2: parameter = 0x1e; break;
            case 4: parameter = 0x46; break;
            case 8: parameter = 0x5a; break;
            case 0x10: parameter = 0x32; break;
            default: break;
        }
        if (parameter != 0 && gActionRecorder.mRecording == 0) {
            train->mF68 = SOUND_SetParamValue(train->mF68, train->mF54[2], parameter);
        }
        train->mF38 = state;
    }
    if (gActionRecorder.mRecording == 0) train->mF68 = SOUND_SetParamValue(train->mF68, train->mF54[3], value);
}

// 0x10047510 (212 bytes). The sound of two trains colliding: sound event 0xc of bank 2 at the point of the track under the train's nose
// (its mFrontSample taken modulo the track's length), the point truncated to ints. Nothing is sent while the action recorder is running.
void PlayCollisionSound(tag_sCoasterInstance* instance, sTrainInstance* train) {
    s3DPoint point;
    const float place = static_cast<float>(std::fmod(static_cast<double>(train->mFrontSample), static_cast<double>(instance->mLength)));
    int32_t sample;
    const float fraction = SplitFraction(place, sample);
    GetLinearSamplePosn(instance, sample, fraction, &point);
    if (gActionRecorder.mRecording == 0) {
        SOUND_SendEvent(0, gSoundBanks[2], 0xc, ppc_fctiwz(point.x), ppc_fctiwz(point.y), ppc_fctiwz(point.z));
    }
}

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

// 0x1004f374 (92 bytes). sqrt of dx*dx + dy*dy + dz*dz, the reference overload of the one in src/rsse_math.cpp: the differences and dy*dy,
// dz*dz are single precision (fsubs, fmuls); dx*dx is fused (fmadd, double) with dy*dy, dz*dz is added (fadd, double); the root is the C
// library's, rounded to single.
float DistanceBetweenPoints(s3DPoint& a, s3DPoint& b) {
    const float dy = a.y - b.y;
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    double sum = ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(dy * dy));
    sum = static_cast<double>(dz * dz) + sum;
    return static_cast<float>(std::sqrt(sum));
}

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
