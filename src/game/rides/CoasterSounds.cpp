// CoasterSounds.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "Advisor.h"
#include "analyser.h"
#include "Debug.h"
#include "helpers/CoasterSoundsHelpers.h"
#include "helpers/CoasterTrainHelpers.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "tb_matrix.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "ui_iterators.h"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

// 0x10045a38 (108 bytes): the length of (x, z) in double, rounded to single, divides all three.
void NormaliseVectorXZ(s3DPoint& point) {
    double sum = static_cast<double>(point.x) * static_cast<double>(point.x) + static_cast<double>(point.z * point.z);
    float length = static_cast<float>(std::sqrt(sum));
    point.x = point.x / length;
    point.y = point.y / length;
    point.z = point.z / length;
}

// Unit v5205: StartTrainSounds (0x10046a18) and LayoutStartAllCoasterSounds (0x10046bc0).

namespace {

// The part both functions share, after StopTrainSounds: the four sound words and the two loops, unless the recorder runs.
void StartWords(tag_sCoasterInstance* instance, sTrainInstance* train) {
    if (gActionRecorder.mRecording == 0) {
        train->mF54[0] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 5));
        train->mF54[1] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 6));
        train->mF54[2] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 7));
        train->mF54[3] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 8));
        train->mF64 = SND_SendIndirectedEvent(instance->mScriptID, 0, gSoundBanks[7], 0,
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[0])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[1])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[2])));
        train->mF68 = SND_SendIndirectedEvent(instance->mScriptID, 0, gSoundBanks[7], 1,
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[0])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[1])),
                                              static_cast<int32_t>(ppc_fctiwz(train->mF48[2])));
    }
}

}  // namespace

void StartTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t noState) {
    StopTrainSounds(instance, train);
    StartWords(instance, train);
    if (noState == 0) SetTrainSoundState(train, 2, 100);
}

void LayoutStartAllCoasterSounds() {
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        sTrainInstance* train = instance->mTrains;
        for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); i++) {
            StopTrainSounds(instance, train);
            StartWords(instance, train);
            SetTrainSoundState(train, 2, 100);
            train++;
        }
    }
}

// 0x100471bc (184 bytes). Stops the train's two sound effects (mF64, mF68) and its scream (mF70) unless the action recorder is
// running, then clears the sound words either way.
void StopTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) {
    (void)instance;
    if (train->mF64 != 0 && gActionRecorder.mRecording == 0) SOUND_StopSoundEffect(train->mF64);
    if (train->mF68 != 0 && gActionRecorder.mRecording == 0) SOUND_StopSoundEffect(train->mF68);
    if (train->mF70 != 0 && gActionRecorder.mRecording == 0) SCREAM_Stop(static_cast<int32_t>(train->mF70));
    train->mF54[0] = 0;
    train->mF54[1] = 0;
    train->mF54[2] = 0;
    train->mF54[3] = 0;
    train->mF64 = 0;
    train->mF68 = 0;
    train->mF70 = 0;
    train->mF6C = 0;
}

// 0x100472c4 (220 bytes). Every train's four sound event codes (mF54[0..3]) that are 0 are looked up again with the coaster's
// script id and the kinds 5 to 8.
void RestoreCoasterSounds(tag_sCoasterInstance* instance) {
    if (instance == nullptr) return;
    sTrainInstance* train = instance->mTrains;
    for (uint32_t i = 0; i < static_cast<uint32_t>(instance->mTrainCount); i++) {
        if (train->mF54[0] == 0) train->mF54[0] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 5));
        if (train->mF54[1] == 0) train->mF54[1] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 6));
        if (train->mF54[2] == 0) train->mF54[2] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 7));
        if (train->mF54[3] == 0) train->mF54[3] = static_cast<uint32_t>(RSSE_GetSoundEventCode(instance->mScriptID, 8));
        train++;
    }
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

void GetLinearSamplePosn(tag_sCoasterInstance* instance, int32_t sample, float t, s3DPoint* out);

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

// ---- the bend angles' clamps (name invented): loop 2's yaw (lo, hi) and pitch (lo, hi), then loop 3's; the Mac's data words are all (0, 1)
float gBendAngleClamps[8] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};   // 0x1029cd8c

namespace {

const float kPi = 3.1415927f;        // Bits(0x40490fdb)
const float kHalfPi = 1.5707964f;    // Bits(0x3fc90fdb)
const float kTwoPi = 6.2831855f;     // Bits(0x40c90fdb)
// InterpAngles adds the float pi and 2 pi widened to double (0x400921fb60000000 at 0x102648b8, 0x401921fb60000000 at 0x102648b0).
const double kPiD = static_cast<double>(kPi);
const double kTwoPiD = static_cast<double>(kTwoPi);

// The engine's sine table at an angle: the index is the angle times the table's scale, truncated, masked. The sine of x is TableAt(x), the
// cosine of x is TableAt(pi / 2 + x).
float TableAt(float angle) {
    return gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(ppc_fctiwz(angle * gTPGlobals.mSinTableScale))];
}

// sqrt(a * a + b * b) as the original sums it: a's square exact in double (the fmadd form), b's the float product widened.
float Length(float a, float b) {
    return static_cast<float>(std::sqrt(static_cast<double>(a) * static_cast<double>(a) + static_cast<double>(b * b)));
}

// The arc cosine of a dot product held to [lo, hi] (the larger of lo and the value first, then the smaller of that and hi, each as the
// original's compare and branch: a NaN ends at lo, then at the smaller of lo and hi).
float ClampedAcos(float dot, float lo, float hi) {
    float value = lo < dot ? dot : lo;
    value = value < hi ? value : hi;
    return static_cast<float>(std::acos(static_cast<double>(value)));
}

// The car against the track at its own place, as AccumulateBendAngles' prologue leaves it.
struct BendFrame {
    s3DPoint point;      // the track's place under the car (samples base and base + 1 at the fraction)
    s3DPoint heading;    // the car's direction, normalised by its length on the ground (x and z)
    s3DPoint pitchDir;   // the car's direction turned by the track's yaw there, normalised by its length in y and z
    float behind;        // 1 - fraction: the weight of sample j
    float fraction;      // the weight of sample j + 1
};

// The rail's height at sample j: the type's word 0x5c when the sample climbs (flag 8), else 0x58, interpolated like the track's place.
float RailHeight(const tag_sCoasterType* coasterType, const sTrackSample* samples, uint32_t j, const BendFrame& frame) {
    const float h0 = (samples[j].mFlags & 8) != 0 ? coasterType->mF5C : coasterType->mF58;
    const float h1 = (samples[j + 1].mFlags & 8) != 0 ? coasterType->mF5C : coasterType->mF58;
    return ppc_fmadds(h0, frame.behind, h1 * frame.fraction);
}

// One pass of loops 2 and 3 (the clamps are the loop's own pair of pairs; `ahead` is loop 3): the place of sample j against the car's, taken
// as a yaw (D flattened onto the ground against the heading) and a pitch (D turned to face +z by GetVectorRotationAroundYAxisFromZ, against
// the pitch direction), each the arc cosine of a dot product signed by its cross product, and the rail's height there.
void BendStep(const tag_sCoasterType* coasterType, const sTrackSample* samples, uint32_t j, const BendFrame& frame, const float* clamps,
              bool ahead, float* yaw, float* pitch, float* height) {
    s3DPoint d;
    d.x = ppc_fmadds(samples[j].mPos.x, frame.behind, samples[j + 1].mPos.x * frame.fraction) - frame.point.x;
    d.y = ppc_fmadds(samples[j].mPos.y, frame.behind, samples[j + 1].mPos.y * frame.fraction) - frame.point.y;
    d.z = ppc_fmadds(samples[j].mPos.z, frame.behind, samples[j + 1].mPos.z * frame.fraction) - frame.point.z;
    const float flatLength = Length(d.x, d.z);
    s3DPoint flat;
    flat.x = d.x / flatLength;
    flat.y = d.y / flatLength;
    flat.z = d.z / flatLength;

    s3DPoint turned = d;
    const float rotation = GetVectorRotationAroundYAxisFromZ(turned);
    const float sinR = TableAt(-rotation);
    const float cosR = TableAt(kHalfPi - rotation);
    const float turnedX = ppc_fmadds(turned.x, cosR, turned.z * sinR);
    const float turnedZ = ppc_fmadds(ppc_fneg(turned.x), sinR, turned.z * cosR);
    const float turnedLength = Length(turned.y, turnedZ);
    turned.x = turnedX / turnedLength;
    turned.y = turned.y / turnedLength;
    turned.z = turnedZ / turnedLength;

    const float yawDot = ppc_fmadds(flat.x, frame.heading.x, flat.z * frame.heading.z);
    *yaw = ClampedAcos(yawDot, clamps[0], clamps[1]);
    const float yawCross = ppc_fmsubs(flat.z, frame.heading.x, flat.x * frame.heading.z);
    if (yawCross > 0.0f) *yaw = *yaw * -1.0f;

    const float pitchDot = ppc_fmadds(turned.y, frame.pitchDir.y, turned.z * frame.pitchDir.z);
    *pitch = ClampedAcos(pitchDot, clamps[2], clamps[3]);
    const float pitchCross = ppc_fmsubs(turned.z, frame.pitchDir.y, turned.y * frame.pitchDir.z);
    if (ahead ? pitchCross < 0.0f : pitchCross > 0.0f) *pitch = *pitch * -1.0f;

    *height = RailHeight(coasterType, samples, j, frame);
}

// The car's heading and pitch direction turned to the other way for the samples behind it (loop 2 runs with them negated, then they are put back).
void NegateFrame(BendFrame& frame) {
    frame.heading.x = frame.heading.x * -1.0f;
    frame.heading.y = frame.heading.y * -1.0f;
    frame.heading.z = frame.heading.z * -1.0f;
    frame.pitchDir.y = frame.pitchDir.y * -1.0f;
}

}  // namespace

float InterpAngles(float a, float b, float t);

// 0x10045ad8 (2,796 bytes). The car stands `position` samples along the track, between sample base and base + 1 at the fraction. The three
// arrays get one entry per sample from the car's tail to its nose, in track order: behindAsked entries before the car's own (the car's
// length behind it over the spacing, plus one; the ones before the start of the track are zero), the car's own (yaw and pitch 0, the rail's
// height), then aheadAsked (the same ahead; the ones past the end of the track are zero). *centre is behindAsked, the car's own index.
// The capacity (0x80, the arrays' length) is never read, and the arrays are not checked against it.
void AccumulateBendAngles(tag_sCoasterInstance* instance, sCarInstance* car, float* yaws, float* pitches, float* heights, uint32_t capacity,
                          float position, uint32_t* centre) {
    const sTrackSample* samples = instance->mSamples;
    const sCarType* type = car->mType;
    const tag_sCoasterType* coasterType = reinterpret_cast<const tag_sCoasterType*>(instance->mType);

    int32_t whole;
    const float fraction = SplitFraction(position, whole);
    const uint32_t base = static_cast<uint32_t>(whole);
    const float spacing = instance->mFAC;
    const uint32_t count = instance->mSampleCount;

    // How many samples the car's length asks for behind it and ahead of it, and how many the track has.
    const uint32_t behindAsked = cw_cvt_fp2unsigned(static_cast<double>(type->mF28 / spacing)) + 1;
    const uint32_t behind = behindAsked <= base ? behindAsked : base;
    const uint32_t first = base - behind;
    const uint32_t aheadAsked = cw_cvt_fp2unsigned(static_cast<double>(type->mF24 / spacing)) + 1;
    const uint32_t room = (count - 1) - base;
    const uint32_t ahead = aheadAsked <= room ? aheadAsked : room;
    const uint32_t last = base + ahead;

    BendFrame frame;
    frame.fraction = fraction;
    frame.behind = 1.0f - fraction;
    frame.point.x = ppc_fmadds(frame.behind, samples[base].mPos.x, fraction * samples[base + 1].mPos.x);
    frame.point.y = ppc_fmadds(frame.behind, samples[base].mPos.y, fraction * samples[base + 1].mPos.y);
    frame.point.z = ppc_fmadds(frame.behind, samples[base].mPos.z, fraction * samples[base + 1].mPos.z);
    const float trackYaw = InterpAngles(samples[base].mYaw, samples[base + 1].mYaw, fraction);

    // The car's direction: its x and z over their length is the heading; the same direction turned by the track's yaw, over its y and z
    // length, is the pitch direction.
    frame.heading.x = car->mF10[0];
    frame.heading.y = car->mF10[1];
    frame.heading.z = car->mF10[2];
    frame.pitchDir = frame.heading;
    const float headingLength = Length(frame.heading.x, frame.heading.z);
    frame.heading.x = frame.heading.x / headingLength;
    frame.heading.y = frame.heading.y / headingLength;
    frame.heading.z = frame.heading.z / headingLength;
    const float sinT = TableAt(-trackYaw);
    const float cosT = TableAt(kHalfPi - trackYaw);
    const float pitchX = ppc_fmadds(frame.pitchDir.x, cosT, frame.pitchDir.z * sinT);
    const float pitchZ = ppc_fmadds(ppc_fneg(frame.pitchDir.x), sinT, frame.pitchDir.z * cosT);
    const float pitchLength = Length(frame.pitchDir.y, pitchZ);
    frame.pitchDir.x = pitchX / pitchLength;
    frame.pitchDir.y = frame.pitchDir.y / pitchLength;
    frame.pitchDir.z = pitchZ / pitchLength;

    // Loop 1: the entries behind the start of the track.
    for (uint32_t i = behind; i < behindAsked; ++i) {
        *yaws++ = 0.0f;
        *pitches++ = 0.0f;
        *heights++ = 0.0f;
    }
    // Loop 2: the samples behind the car, seen from the other way round.
    if (behind != 0) {
        NegateFrame(frame);
        for (uint32_t j = first; j < base; ++j) {
            BendStep(coasterType, samples, j, frame, &gBendAngleClamps[0], false, yaws, pitches, heights);
            ++yaws;
            ++pitches;
            ++heights;
        }
        NegateFrame(frame);
    }
    // The car's own place.
    *yaws++ = 0.0f;
    *pitches++ = 0.0f;
    *heights++ = RailHeight(coasterType, samples, base, frame);
    // Loop 3: the samples ahead of the car.
    if (ahead != 0) {
        for (uint32_t j = base + 1; j <= last; ++j) {
            BendStep(coasterType, samples, j, frame, &gBendAngleClamps[4], true, yaws, pitches, heights);
            ++yaws;
            ++pitches;
            ++heights;
        }
    }
    // Loop 4: the entries past the end of the track.
    for (uint32_t i = ahead; i < aheadAsked; ++i) {
        *yaws++ = 0.0f;
        *pitches++ = 0.0f;
        *heights++ = 0.0f;
    }
    *centre = behindAsked;
}

// 0x10046620 (520 bytes). Turns `count` vertices, read from `source` and written to `dest` (the same sets work), four to a set. A vertex at z
// stands between entries k and k + 1 of the arrays, k = the whole part of (centre * spacing + z) / spacing, at the fraction left over (no
// bounds check on k): pitch, yaw and lift are interpolated there, the lift is added to y, the vertex is turned by the pitch about x and then
// by the yaw about y. `node` is never read.
void RotateNodeVertexPosns(tag_sNode* node, sVertexPosnSet* source, sVertexPosnSet* dest, uint32_t count, float* yaws, float* pitches,
                           float* heights, uint32_t centre, float spacing) {
    const float centreOffset = static_cast<float>(centre) * spacing;
    uint32_t lane = 0;
    while (count != 0) {
        const float x = source->x[lane];
        const float y = source->y[lane];
        const float z = source->z[lane];
        int32_t k;
        const float fraction = SplitFraction((centreOffset + z) / spacing, k);
        const float keep = 1.0f - fraction;
        const float pitch = ppc_fmadds(keep, pitches[k], fraction * pitches[k + 1]);
        const float yaw = ppc_fmadds(keep, yaws[k], fraction * yaws[k + 1]);
        const float lift = ppc_fmadds(keep, heights[k], fraction * heights[k + 1]);
        const float sinP = TableAt(pitch);
        const float cosP = TableAt(kHalfPi + pitch);
        const float sinY = TableAt(yaw);
        const float cosY = TableAt(kHalfPi + yaw);
        const float lifted = y + lift;
        const float turnedZ = ppc_fmadds(lifted, sinP, z * cosP);
        dest->y[lane] = ppc_fmsubs(lifted, cosP, z * sinP);
        dest->x[lane] = ppc_fmadds(x, cosY, turnedZ * sinY);
        dest->z[lane] = ppc_fmadds(ppc_fneg(x), sinY, turnedZ * cosY);
        --count;
        lane = (lane + 1) & 3;
        if (lane == 0) {
            ++source;
            ++dest;
        }
    }
}

// 0x1004688c (320 bytes). A car with a ride (mRide 0: nothing) is bent: the angles are gathered at the car's position (mF40) into three local
// arrays of 0x80, the ride's mesh (the set's mesh, whose header is the live copy) gets its body node's vertices turned from the master copy's
// (the header's mSelf) pristine ones, and the mesh's embedded data is processed again. The car type's mNodeIndex counts the nodes first and
// the mini nodes after them; a mini node without flag 0x200, and a node with it, are not turned.
void BendCarToTrack(tag_sCoasterInstance* instance, sCarInstance* car) {
    if (car->mRide == 0) return;
    float yaws[0x80];
    float pitches[0x80];
    float heights[0x80];
    uint32_t centre;
    AccumulateBendAngles(instance, car, yaws, pitches, heights, 0x80, car->mF40, &centre);
    const float spacing = instance->mFAC;
    tag_sRideAnimSet* set = gRideAnimSets[car->mRide];
    if (set != nullptr) {
        tag_sM3D2Header* header = set->mHeads->mHeader;
        const uint32_t index = car->mType->mNodeIndex;
        tag_sNode* node;
        if (index < header->mNodeCount)
            node = &header->mNodes[index];
        else
            node = static_cast<tag_sNode*>(&header->mMiniNodes[index - header->mNodeCount]);
        if ((node->mFlags & 0x200) == 0) {
            // The node's place among the header's nodes: the byte distance over the node size (0xa0), a signed division of the 32-bit
            // difference like the original's (the Mac's pointers are 32 bits; mulhw by 0x66666667, srawi 6, then the sign bit).
            const int32_t difference = static_cast<int32_t>(reinterpret_cast<intptr_t>(node) - reinterpret_cast<intptr_t>(header->mNodes.get()));
            const int32_t nodeIndex = difference / 160;
            sVertexPosnSet* source = header->mSelf->mNodes[nodeIndex].mVertices;
            RotateNodeVertexPosns(node, source, node->mVertices, node->mVertexCount, yaws, pitches, heights, centre, spacing);
        }
    }
    TPProcessEmbeddedData(gRideAnimSets[car->mRide]->mHeads);
}

// 0x10046d78 (612 bytes). The riders' screams of one train at the place (x, y, z): mF6C is the handle of the single scream that goes with a
// sample's flag 2, mF70 the handle of the group scream that goes with flag 4 (it counts mF10 / 2 + 1 riders), and train flag 0x100 says the
// group has begun. Nothing starts while the train's flag 0x10 or the instance's state 0x40 is set, and while the action recorder is running
// (mRecording) nothing starts or stops, but two calls on the single scream do not test it (the start of a screaming train that has no handle
// yet, and the move once the sample has lost flag 2). The train's flags are read at entry for the first tests and again for the later ones.
void HandleScreams(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t x, int32_t y, int32_t z) {
    const uint32_t flags = train->mFlags;
    const sTrackSample* sample = &instance->mSamples[ppc_fctiwz(train->mPosition)];
    if ((flags & 0x100) == 0) {
        if ((sample->mFlags & 2) != 0 && (flags & 0x10) == 0 && (instance->mState & 0x40) == 0 && gActionRecorder.mRecording == 0)
            train->mF6C = static_cast<uint32_t>(SCREAM_Single(1, 100, 100, x, y, z));
        if ((sample->mFlags & 4) != 0 && (train->mFlags & 0x10) == 0 && (instance->mState & 0x40) == 0) {
            if (train->mF70 == 0 && gActionRecorder.mRecording == 0)
                train->mF70 = static_cast<uint32_t>(SCREAM_Start(0, static_cast<int32_t>((train->mF10 >> 1) + 1), 0x4b, 0x4b, x, y, z));
            train->mFlags |= 0x100;
        }
    } else {
        if ((sample->mFlags & 2) != 0) {
            if (train->mF6C != 0) {
                if (gActionRecorder.mRecording == 0)
                    train->mF6C = static_cast<uint32_t>(SCREAM_Move(static_cast<int32_t>(train->mF6C), x, y, z));
            } else if ((flags & 0x10) == 0 && (instance->mState & 0x40) == 0) {
                train->mF6C = static_cast<uint32_t>(SCREAM_Single(1, 100, 100, x, y, z));
            }
        } else if (train->mF6C != 0) {
            train->mF6C = static_cast<uint32_t>(SCREAM_Move(static_cast<int32_t>(train->mF6C), x, y, z));
        }
        if ((sample->mFlags & 4) != 0 && (train->mFlags & 0x10) == 0 && (instance->mState & 0x40) == 0) {
            if (gActionRecorder.mRecording == 0) {
                SCREAM_Move(static_cast<int32_t>(train->mF70), x, y, z);
                SCREAM_SetLevel(static_cast<int32_t>(train->mF70), 0x4b, 0x4b);
            }
        } else {
            if (gActionRecorder.mRecording == 0) SCREAM_Stop(static_cast<int32_t>(train->mF70));
            train->mF70 = 0;
            train->mFlags &= ~0x100u;
        }
    }
}

// 0x1004702c (320 bytes). The train's two sound effects (mF64 and mF68): the first at the level mF78 / mF50 of the coaster type as a percent,
// the second at the rest of 100, both moved to the first-person car's place when the ride is the one being watched (mF3C, else mF48; the
// place truncated to ints). Nothing is set or moved while the action recorder is running. A train with mF10 set also runs its screams.
void TickTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train) {
    // With the recorder running the original passes whatever its caller left in the registers that hold the place (r29, r28, r27).
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    const uint32_t rideId = GetFirstPersonRideID();
    const float* place = train->mFirstPersonCar->mRide == rideId ? reinterpret_cast<const float*>(train->mF3C) : train->mF48;
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    const int32_t level = ppc_fctiwz(100.0f * (train->mF78 / type->mF50));
    if (gActionRecorder.mRecording == 0) {
        train->mF64 = SOUND_SetParamValue(train->mF64, train->mF54[1], static_cast<uint32_t>(level));
        train->mF68 = SOUND_SetParamValue(train->mF68, train->mF54[1], 100u - static_cast<uint32_t>(level));
        x = ppc_fctiwz(place[0]);
        y = ppc_fctiwz(place[1]);
        z = ppc_fctiwz(place[2]);
        train->mF64 = SOUND_MoveSoundEffect(train->mF64, x, y, z);
        train->mF68 = SOUND_MoveSoundEffect(train->mF68, x, y, z);
    }
    if (train->mF10 != 0) HandleScreams(instance, train, x, y, z);
}
