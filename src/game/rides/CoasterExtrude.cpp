// CoasterExtrude.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "analyser.h"
#include "Debug.h"
#include "keytable_actions.h"
#include "LevelSystems.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Leaves the park's second-stage set-up calls (main app; unit w112a): the coaster track extruder's storage, the object instance
// table's set-up, the texture set loader and freer of the dynamic meshes (TPAdvisor.cpp range of the inventory) and the two
// static texture sets with their entries (initialised data of the image).

// ---- The extruder's storage (names invented) ----
uint32_t gExtrudeFaceStoreCount;
void* gExtrudeFaceStoreB;
void* gExtrudeFaceStoreA;

// 0x1004e60c (96 bytes): stores the count first, then allocates (zeroed) `b` records of 0x1c bytes and `a` points of 0x10 bytes.
// The products are 32-bit and wrap. Nothing is freed first: a second call leaks the old blocks.
void CreateExtrudeFaceStorage(uint32_t a, uint32_t b) {
    gExtrudeFaceStoreCount = b;
    gExtrudeFaceStoreB = MReleaseAllocF(b * 0x1c, 1);
    gExtrudeFaceStoreA = MReleaseAllocF(a << 4, 1);
}

// The coaster track's mesh (main app TPConfig.cpp, unit w213): the spline between two braces, the rings extruded along it, their faces and
// the track's mesh sets. ExtrudeTrackSection 0x1004f628 (1,816 bytes) builds one ring of vertices for each t it is given (a cross section
// of the extrusion table, blended and turned by the orientation the spline gives), skins the faces between the ring before and the new
// one, and calls GetNewTrackMeshSet 0x1004ea30 (816 bytes) for a fresh mesh set when the master set's room runs out. The faces are linked
// into one list per texture (the containers of the face store B, LinkFEStoreToFETextureCont 0x1004e790) and copied into the set's face
// sets when the set is finished (CopyExtrudedFaceData 0x1004e6a0). Also here: SkinCrossSectionFaces 0x1004f410, ExtrudeCrossSectionVertices
// 0x1004ed8c and the sTrackSample overload of InterpolateCoasterSpline 0x1004e318.
//
// Two bases, kept as the listing has them: the type, the track's mesh set, the samples and the vertices' V coordinate are read through
// gCoasterCurrent (re-read before each use), the ring's t, the V counter, the angles and the orientation through gCoasterCurrentData, the
// address of the +0x3c block, which is not cleared after a tick and so may belong to another instance. The mesh sets and the face lists
// live in the game's arena: the Ptr32 words hold arena offsets, so the stores of non-arena addresses stop the program.

// ---- The unit's globals (bss on the Mac; names invented, the addresses are in TPCoaster.h)
sExtrusionParams gExtrudeParamsStatic[2];
sExtrusionParams* gExtrudeParamsTable;
int32_t gExtrudeTextureKind;
sExtrudeFace* gExtrudeFaceNext;
int32_t gExtrudeKindFlag;
int32_t gExtrudeWinding;
float gExtrudeTOffset;
s3DPoint gExtrudeMax;
s3DPoint gExtrudeMin;
int32_t gExtrudeFaceSetCount;
int32_t gExtrudeFacesUsed;
int32_t gExtrudePointsUsed;
int32_t gExtrudeUVsUsed;
int32_t gExtrudeVertsUsed;
int32_t gExtrudeFacesLeft;
int32_t gExtrudePointsLeft;
int32_t gExtrudeUVsLeft;
int32_t gExtrudeVertsLeft;

namespace {

constexpr float kHalfPi = 1.57079637f;       // 0x3fc90fdb, the cosine's shift in the sine table
constexpr float kBoxReset = 1.0e6f;          // the box of the vertices starts at (+1e6) and (-1e6)
constexpr uint32_t kMinReset = 10000;        // a container's lowest vertex index when empty
constexpr int32_t kBlendFloats = 258;        // the Mac's local array of blended rows (5 floats a point): room for 50 points

// The instance whose +0x3c block gCoasterCurrentData points at: the Mac reads the angles and counters through that address.
inline tag_sCoasterInstance* DataInstance() {
    return reinterpret_cast<tag_sCoasterInstance*>(static_cast<uint8_t*>(gCoasterCurrentData) - offsetof(tag_sCoasterInstance, mBlock3C));
}
inline sCoasterTrackParams* Data() { return static_cast<sCoasterTrackParams*>(gCoasterCurrentData); }
inline sCoasterOrientation* Orientation() { return reinterpret_cast<sCoasterOrientation*>(&DataInstance()->mF78); }
inline const tag_sCoasterType* TypeOf(const tag_sCoasterInstance* instance) { return reinterpret_cast<const tag_sCoasterType*>(instance->mType); }

// The sine table's index of a (float) product: fctiwz, then the mask.
inline uint32_t SineIndex(float product) {
    return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(product))) & gTPGlobals.mSinTableMask;
}

// The counters a ring of n vertices moves (the points are never touched).
inline void CountVertices(int32_t n) {
    gExtrudeVertsUsed += n;
    gExtrudeVertsLeft -= n;
    gExtrudeUVsUsed += n;
    gExtrudeUVsLeft -= n;
}

// The box of a vertex coordinate: both limits, each only when passed.
inline void WidenBoth(float value, float& low, float& high) {
    if (value < low) low = value;
    if (value > high) high = value;
}
// The box of a vertex coordinate, the later rings' way: the high limit is only looked at when the low one is not passed.
inline void WidenEither(float value, float& low, float& high) {
    if (value < low) {
        low = value;
    } else if (value > high) {
        high = value;
    }
}

}  // namespace

void InterpolateCoasterSpline(sTrackSample* samples, sCoasterTrackParams* params, tag_sBezierControl* a, tag_sBezierControl* b,
                              tag_sBezierControl* c, tag_sBezierControl* d, float t, s3DPoint* point, sCoasterOrientation* orient);

// 0x1004e6a0. Copies the linked faces of every container (in the order of the textures) into the set's faces and fills each container's
// face set: the texture slot, the first face, the count and the lowest and highest vertex index. A container without a face set is skipped.
void CopyExtrudedFaceData() {
    const tag_sCoasterInstance* instance = gCoasterCurrent;
    sFaceExtrudeTextureCont* cont = static_cast<sFaceExtrudeTextureCont*>(gExtrudeFaceStoreB);
    tag_sDMeshSet* set = instance->mF38;
    uint32_t textures = set->mMesh->mHeader->mTextures.mOffset;
    sFace* faces = set->mFaces;
    uint32_t first = 0;
    for (uint32_t remaining = TypeOf(instance)->mTextures->mCount; remaining != 0; --remaining) {
        if (cont->mFaceSet.mOffset != 0) {
            cont->mFaceSet->mF08 = static_cast<uint16_t>(cont->mMin);
            cont->mFaceSet->mF0A = static_cast<uint16_t>(cont->mMax);
            cont->mFaceSet->mTexture.mOffset = textures;
            cont->mFaceSet->mFaceCount = static_cast<uint16_t>(cont->mCount);
            cont->mFaceSet->mFirstFace = static_cast<uint16_t>(first);
            for (sExtrudeFace* entry = cont->mHead; entry != nullptr; entry = entry->mNext) {
                *faces = entry->mFace;
                ++faces;
            }
            first += cont->mCount;
        }
        ++cont;
        textures += 8;
    }
}

// 0x1004e790. Links the next free entry of the face store A to the end of the container's list and returns it. The container's first link
// takes the node's next face set (the face set count so far) and counts it in the header and the node. gExtrudeFaceNext moves on.
sExtrudeFace* LinkFEStoreToFETextureCont(sFaceExtrudeTextureCont* cont) {
    sExtrudeFace* entry = gExtrudeFaceNext;
    tag_sM3D2Header* header = gCoasterCurrent->mF38->mMesh->mHeader;
    entry->mNext = nullptr;
    if (cont->mHead.mOffset == 0) cont->mHead = entry;
    sExtrudeFace* tail = cont->mTail;
    if (tail != nullptr) tail->mNext = entry;
    entry->mPrev = cont->mTail;
    cont->mTail = entry;
    if (cont->mFaceSet.mOffset == 0) {
        cont->mFaceSet.mOffset = header->mF78->mFaceSets.mOffset + (static_cast<uint32_t>(gExtrudeFaceSetCount) << 4);
        cont->mFaceSet->mF0C[0] = ((cont->mFlags & 1) != 0) ? 1 : 0;
        header->mFaceSetCount = static_cast<uint16_t>(header->mFaceSetCount + 1);
        header->mF78->mFaceSetCount = static_cast<uint16_t>(header->mF78->mFaceSetCount + 1);
        ++gExtrudeFaceSetCount;
    }
    ++cont->mCount;
    ++gExtrudeFaceNext;
    return entry;
}

// GenerateTrackSubdivisions 0x1004e8c0, CoasterSetSpeed (instance overload) 0x1004b94c, GetBankAtBrace 0x1004b1f8,
// FlashEndPylons 0x10052158, DecCoasterConst1 0x100524a0.

// 0x1004e8c0. Fills `out` with the sample positions of the section: for each of `count` steps (the brace's spline count when a == b, else 11)
// the position t0 + span * i / (count - 1), then every key of the params array that lies in [position, next position) and differs from it.
// Answers how many floats it wrote. The params array is walked on (stride sizeof(sExtrusionParams)).
int32_t GenerateTrackSubdivisions(tag_sTrackBrace* a, tag_sTrackBrace* b, sExtrusionParams* params, float* out, float span) {
    int32_t written = 0;
    while (params->mF00 < gExtrudeTOffset) ++params;
    uint32_t count;
    if (a == b) count = b->mF90->mCount;
    else count = 11;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t last = count - 1;
        float t0 = gExtrudeTOffset;
        float start = ppc_fmadds(span, static_cast<float>(i) / static_cast<float>(last), t0);
        float end = ppc_fmadds(span, static_cast<float>(i + 1) / static_cast<float>(last), t0);
        out[written++] = start;
        while (i < last) {
            float key = params->mF00;
            if (!(key >= start)) break;
            if (!(key < end)) break;
            if (key != start) out[written++] = key;
            ++params;
        }
    }
    return written;
}

// 0x1004ea30. Closes the track's current mesh set (if any): the box of the vertices into the header and the node, the counts of what was
// built, the faces copied, the render switched on. Then clears the containers and the face list and brings the V counter to its fraction.
// With flag 0 it also makes the next set from the type's master set (chained to the one before) and resets the room and the box.
void GetNewTrackMeshSet(int32_t flag) {
    const tag_sCoasterType* type = TypeOf(gCoasterCurrent);
    tag_sDMeshSet* masterSet = type->mMeshSet;
    const sDMeshEntry* entry = static_cast<const sDMeshEntry*>(type->mF14);
    if (gCoasterCurrent->mF38 != nullptr) {
        tag_sM3D2Header* header = gCoasterCurrent->mF38->mMesh->mHeader;
        header->mBox.mMin[0] = gExtrudeMin.x;
        header->mBox.mMin[1] = gExtrudeMin.y;
        header->mBox.mMin[2] = gExtrudeMin.z;
        header->mBox.mMax[0] = gExtrudeMax.x;
        header->mBox.mMax[1] = gExtrudeMax.y;
        header->mBox.mMax[2] = gExtrudeMax.z;
        tag_sNode* node = header->mF78;
        node->mBox.mMin[0] = gExtrudeMin.x;
        node->mBox.mMin[1] = gExtrudeMin.y;
        node->mBox.mMin[2] = gExtrudeMin.z;
        node->mBox.mMax[0] = gExtrudeMax.x;
        node->mBox.mMax[1] = gExtrudeMax.y;
        node->mBox.mMax[2] = gExtrudeMax.z;
        header->mFlags |= 0x400;
        header->mFA8[3] = static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(gExtrudeMin.x / 10.0f)));
        header->mFA8[2] = static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(gExtrudeMax.x / 10.0f)));
        header->mFA8[1] = static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(gExtrudeMin.z / 10.0f)));
        header->mFA8[0] = static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(gExtrudeMax.z / 10.0f)));
        header->mFlags |= 0x800;
        CopyExtrudedFaceData();
        header = gCoasterCurrent->mF38->mMesh->mHeader;
        node = header->mF78;
        header->mVertexSetCount = static_cast<uint16_t>(static_cast<int32_t>(static_cast<uint32_t>(gExtrudeVertsUsed) + 3u) >> 2);
        header->mUVSetCount = static_cast<uint16_t>(static_cast<int32_t>(static_cast<uint32_t>(gExtrudeUVsUsed) + 3u) >> 2);
        header->mFaceCount = static_cast<uint16_t>(gExtrudeFacesUsed);
        node->mVertexCount = static_cast<uint16_t>(gExtrudeVertsUsed);
        node->mF5C = static_cast<uint16_t>(gExtrudeFacesUsed);
        node->mF5E = static_cast<uint16_t>(gExtrudeUVsUsed);
        DMEnableMeshRender(gCoasterCurrent->mF38);
    }
    sFaceExtrudeTextureCont* cont = static_cast<sFaceExtrudeTextureCont*>(gExtrudeFaceStoreB);
    for (uint32_t remaining = TypeOf(gCoasterCurrent)->mTextures->mCount; remaining != 0; --remaining, ++cont) {
        cont->mCount = 0;
        cont->mHead.mOffset = 0;
        cont->mTail.mOffset = 0;
        cont->mFaceSet.mOffset = 0;
        cont->mMin = kMinReset;
        cont->mMax = 0;
    }
    gExtrudeFaceNext = static_cast<sExtrudeFace*>(gExtrudeFaceStoreA);
    double whole;
    DataInstance()->mF74 = static_cast<float>(std::modf(static_cast<double>(DataInstance()->mF74), &whole));
    if (flag == 0) {
        tag_sDMeshSet* previous = gCoasterCurrent->mF38;
        InstanceDynamicMeshSet(masterSet, nullptr, 0x21ef, &gCoasterCurrent->mF38);
        gCoasterCurrent->mF38->mF38 = previous;
        SetMeshPosition(gCoasterCurrent->mF38->mMesh, 0.0f, 0.0f, 0.0f);
        tag_sM3D2Header* header = gCoasterCurrent->mF38->mMesh->mHeader;
        header->mFaceSetCount = 0;
        header->mF78->mFaceSetCount = 0;
        gExtrudeVertsLeft = entry->mVertexCount;
        gExtrudeUVsLeft = entry->mUVCount;
        gExtrudePointsLeft = entry->mPointCount;
        gExtrudeFacesLeft = entry->mFaceCount;
        gExtrudeVertsUsed = 0;
        gExtrudeUVsUsed = 0;
        gExtrudePointsUsed = 0;
        gExtrudeFacesUsed = 0;
        gExtrudeFaceSetCount = 0;
        gExtrudeMin.x = kBoxReset;
        gExtrudeMin.y = kBoxReset;
        gExtrudeMin.z = kBoxReset;
        gExtrudeMax.x = -kBoxReset;
        gExtrudeMax.y = -kBoxReset;
        gExtrudeMax.z = -kBoxReset;
    }
}

// 0x1004ed8c. The ring of the section's points at t: the section's row, or (when the section has a second row) the two rows blended by
// the params' weight, each point turned by the three angles (a4, then a3, then a2) through the sine table and moved by the track's point;
// its normal is turned the same way and made unit. The vertex, its normal and its U (the point's +8) and V (the instance's +0x74) go to
// the track's mesh set at gExtrudeVertsUsed on (the caller counts them). The box min/max take every vertex: with `first` both limits are
// looked at for every axis, without it only the high one when the low one is not passed.
void ExtrudeCrossSectionVertices(float t, float a2, float a3, float a4, s3DPoint& point, s3DPoint& min, s3DPoint& max, int32_t first,
                                 sCrossSection* section, sExtrusionParams* params) {
    uint32_t index = static_cast<uint32_t>(gExtrudeVertsUsed);
    const int32_t count = static_cast<int32_t>(section->mF00);
    const float* row = reinterpret_cast<const float*>(section->mPoints);
    float blended[kBlendFloats];
    if (section->mPointsB != nullptr) {
        float marker;
        std::memcpy(&marker, &section->mF14, sizeof(marker));
        const float before = params[-1].mF04;
        const float key = params->mF04;
        const float weight = params->mF08;
        float w;
        if (marker > 0.0f) {
            const float elapsed = t - before;
            const float span = key - before;
            const float remainder = 1.0f - weight;
            const float ratio = elapsed / span;
            w = ppc_fmadds(weight, ratio, remainder);
        } else {
            w = weight * ((t - before) / (key - before));
        }
        w = (w < 0.0f) ? 0.0f : w;
        w = (1.0f < w) ? 1.0f : w;
        const float rest = 1.0f - w;
        const float* rowA = reinterpret_cast<const float*>(section->mPoints);
        const float* rowB = reinterpret_cast<const float*>(section->mPointsB);
        float* out = blended;
        for (uint32_t n = section->mF00; n != 0; --n) {
            for (int k = 0; k < 5; ++k) out[k] = ppc_fmadds(rowA[k], rest, rowB[k] * w);
            rowA += 5;
            rowB += 5;
            out += 5;
        }
        row = blended;
    }
    for (int32_t i = 0; i < count; ++i) {
        const float scale = gTPGlobals.mSinTableScale;
        const float* table = gTPGlobals.mSinTable;
        const uint32_t i4 = SineIndex(a4 * scale);
        const uint32_t j4 = SineIndex((kHalfPi + a4) * scale);
        const uint32_t i3 = SineIndex(a3 * scale);
        const uint32_t j3 = SineIndex((kHalfPi + a3) * scale);
        const uint32_t i2 = SineIndex(a2 * scale);
        const uint32_t j2 = SineIndex((kHalfPi + a2) * scale);
        const float s4 = table[i4];
        const float c4 = table[j4];
        const float s3 = table[i3];
        const float c3 = table[j3];
        const float s2 = table[i2];
        const float c2 = table[j2];
        const float p0 = row[0];
        const float p1 = row[1];
        const float p3 = row[3];
        const float p4 = row[4];
        const float zero = 0.0f;
        const float y1 = ppc_fmadds(p0, s4, p1 * c4);
        const float x1 = ppc_fmsubs(p0, c4, p1 * s4);
        const float z1 = ppc_fmadds(y1, s3, zero * c3);
        const float y2 = ppc_fmsubs(y1, c3, zero * s3);
        const float x2 = ppc_fmadds(x1, c2, z1 * s2);
        const float z2 = ppc_fmadds(ppc_fneg(x1), s2, z1 * c2);
        const float ny1 = ppc_fmadds(p3, s4, p4 * c4);
        const float nx1 = ppc_fmsubs(p3, c4, p4 * s4);
        const float nz1 = ppc_fmadds(ny1, s3, zero * c3);
        const float ny2 = ppc_fmsubs(ny1, c3, zero * s3);
        const float nx2 = ppc_fmadds(nx1, c2, nz1 * s2);
        const float nz2 = ppc_fmadds(ppc_fneg(nx1), s2, nz1 * c2);
        const double lengthA = ppc_fmadd(static_cast<double>(nx2), static_cast<double>(nx2), static_cast<double>(ny2 * ny2));
        const float lengthB = nz2 * nz2;
        const float length = static_cast<float>(std::sqrt(static_cast<double>(lengthB) + lengthA));
        const float x = x2 + point.x;
        const float nx = nx2 / length;
        const float ny = ny2 / length;
        const float nz = nz2 / length;
        const float y = y2 + point.y;
        const float z = z2 + point.z;
        if (first != 0) {
            WidenBoth(x, min.x, max.x);
            WidenBoth(y, min.y, max.y);
            WidenBoth(z, min.z, max.z);
        } else {
            WidenEither(x, min.x, max.x);
            WidenEither(y, min.y, max.y);
            WidenEither(z, min.z, max.z);
        }
        const uint32_t lane = index & 3;
        sVertexPosnSet& vertices = gCoasterCurrent->mF38->mVertexSets[index >> 2];
        vertices.x[lane] = x;
        vertices.y[lane] = y;
        vertices.z[lane] = z;
        s3DPoint& normal = gCoasterCurrent->mF38->mPoints[index];
        normal.x = nx;
        normal.y = ny;
        normal.z = nz;
        const float u = row[2];
        row += 5;
        const tag_sCoasterInstance* instance = gCoasterCurrent;
        float* uv = instance->mF38->mUVSets[index >> 2].mF00;
        uv[lane] = u;
        uv[4 + lane] = instance->mF74;
        ++index;
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

// 0x1004f410. Two faces for each edge of the section, between the ring before (pts vertices back) and the new one, linked to the edge's
// texture container; the container's lowest and highest vertex index follow. The winding turns over with every edge.
void SkinCrossSectionFaces(sCrossSection* section, int32_t edges, int32_t pts) {
    const sCrossSectionEdge* edge = static_cast<const sCrossSectionEdge*>(section->mBlock);
    for (int32_t i = 0; i < edges; ++i, ++edge) {
        const uint32_t base = static_cast<uint32_t>(gExtrudeVertsUsed);
        const uint32_t a = edge->mF06 + base - static_cast<uint32_t>(pts);
        sFaceExtrudeTextureCont* cont = static_cast<sFaceExtrudeTextureCont*>(gExtrudeFaceStoreB) + edge->mF04;
        const uint32_t c = edge->mF08 + base - static_cast<uint32_t>(pts);
        const uint32_t b = edge->mF06 + base;
        const uint32_t d = edge->mF08 + base;
        cont->mMin = (a < cont->mMin) ? a : cont->mMin;
        cont->mMin = (c < cont->mMin) ? c : cont->mMin;
        cont->mMax = (cont->mMax < b) ? b : cont->mMax;
        cont->mMax = (cont->mMax < d) ? d : cont->mMax;
        if (gExtrudeWinding == 0) {
            sExtrudeFace* first = LinkFEStoreToFETextureCont(cont);
            first->mFace.mF02[0] = static_cast<uint16_t>(a);
            first->mFace.mF02[1] = static_cast<uint16_t>(b);
            first->mFace.mF02[2] = static_cast<uint16_t>(c);
            sExtrudeFace* second = LinkFEStoreToFETextureCont(cont);
            second->mFace.mF02[0] = static_cast<uint16_t>(d);
            second->mFace.mF02[1] = static_cast<uint16_t>(c);
            second->mFace.mF02[2] = static_cast<uint16_t>(b);
        } else {
            sExtrudeFace* first = LinkFEStoreToFETextureCont(cont);
            first->mFace.mF02[0] = static_cast<uint16_t>(a);
            first->mFace.mF02[1] = static_cast<uint16_t>(b);
            first->mFace.mF02[2] = static_cast<uint16_t>(d);
            sExtrudeFace* second = LinkFEStoreToFETextureCont(cont);
            second->mFace.mF02[0] = static_cast<uint16_t>(d);
            second->mFace.mF02[1] = static_cast<uint16_t>(c);
            second->mFace.mF02[2] = static_cast<uint16_t>(a);
        }
        gExtrudeWinding = (gExtrudeWinding == 0) ? 1 : 0;
    }
}

// 0x1004f628. Extrudes the cross sections along the spline from brace1 to brace2: one ring for the track's first point (when brace1 is the
// first brace or force is set), then one ring per further t of ts (count of them in all, the first being the start's). The params table
// is walked to the record the t has reached, a new ring of vertices is made when the walk moved, the mesh set is full or the params were
// just chosen, and the faces between the ring before and the new one are skinned. The track's length so far (divided by 6) is the V of the
// texture. Which params and which texture the edges use depends on the brace: branch A (a fixed extrusion, the first brace's +0xb0, the
// braces' flags 0x1000000 or 0x802000) rewrites the section's edges to one texture; C is for flag 0x800 braces of a type with flag 0x40.
void ExtrudeTrackSection(tag_sTrackBrace* brace1, tag_sTrackBrace* brace2, int32_t count, float* ts, int32_t force) {
    const tag_sCoasterType* type = TypeOf(gCoasterCurrent);
    bool restart;
    sExtrusionParams* params;
    if (brace1->mFB0 != 0 || (brace1->mFlags & 0x1000000) != 0 || (brace2->mFlags & 0x802000) != 0) {
        int16_t kind;
        if ((brace1->mFlags & 0x1000000) == 0) {
            kind = (brace1->mFB0 != 0 || (brace2->mFlags & 0x4000) != 0) ? 2 : 3;
        } else if ((brace1->mFlags & 0x4000) == 0) {
            kind = static_cast<int16_t>(gExtrudeTextureKind);
        } else {
            kind = 2;
        }
        const uint32_t textures = type->mTextureCount;
        const sCoasterRecord* select = type->mSelectA;
        for (uint32_t i = 0; i < select->mPointCount; ++i) {
            static_cast<sCrossSectionEdge*>(select->mBlock)[i].mF04 = static_cast<uint16_t>(textures + kind - 1);
        }
        restart = true;
        params = &gExtrudeParamsStatic[1];
    } else if ((brace1->mFlags & 0x800) != 0 && (type->mFlags & 0x40) != 0) {
        restart = (gExtrudeKindFlag == 0);
        gExtrudeKindFlag = 1;
        params = &gExtrudeParamsStatic[0];
    } else {
        restart = (gExtrudeKindFlag != 0);
        gExtrudeKindFlag = 0;
        params = gExtrudeParamsTable;
    }
    s3DPoint previous;
    if (brace1 == brace2) {
        if ((brace1->mData->mFlags & 1) != 0) return;
        previous = brace1->mF90->mControlsA[0].mF40;
    } else {
        previous = brace1->mF90->mControlsA[static_cast<int32_t>(brace1->mF90->mCount) - 1].mF40;
    }
    if (brace1->mNumber == 0 || force != 0) {
        const sBraceSpline* spline = brace1->mF90;
        tag_sBezierControl* controlsA = spline->mControlsA;
        const float t0 = controlsA[0].mF08;
        if (brace1 == brace2) {
            InterpolateCoasterSpline(gCoasterCurrent->mSamples, Data(), controlsA, brace2->mF90->mControlsA + 1, spline->mControlsB,
                                     brace2->mF90->mControlsB + 1, 0.0f, &previous, Orientation());
        } else {
            const int32_t last = static_cast<int32_t>(spline->mCount) - 1;
            InterpolateCoasterSpline(gCoasterCurrent->mSamples, Data(), controlsA + last, brace2->mF90->mControlsA, spline->mControlsB + last,
                                     brace2->mF90->mControlsB, 0.0f, &previous, Orientation());
        }
        const float a2 = DataInstance()->mF7C;
        const float a3 = DataInstance()->mF80;
        const float a4 = DataInstance()->mF84;
        GetNewTrackMeshSet(0);
        ExtrudeCrossSectionVertices(t0, a2, a3, a4, previous, gExtrudeMin, gExtrudeMax, 1, params->mSection, params);
        restart = false;
        CountVertices(static_cast<int32_t>(params->mSection->mF00));
    }
    float* t = ts + 1;
    for (int32_t i = 0; i < count - 1; ++i, ++t) {
        const float b2 = DataInstance()->mF88;
        const float b3 = DataInstance()->mF8C;
        const float b4 = DataInstance()->mF90;
        s3DPoint current;
        float currentT;
        if (brace1 == brace2) {
            double whole;
            float fraction = static_cast<float>(std::modf(static_cast<double>(*t - gExtrudeTOffset), &whole));
            int32_t which = ppc_fctiwz(whole);
            if (which == static_cast<int32_t>(brace1->mF90->mCount) - 1) {
                --which;
                fraction = fraction + 1.0f;
            }
            tag_sBezierControl* a = brace1->mF90->mControlsA + which;
            tag_sBezierControl* b = brace2->mF90->mControlsA + (which + 1);
            InterpolateCoasterSpline(gCoasterCurrent->mSamples, Data(), a, b, brace1->mF90->mControlsB + which,
                                     brace2->mF90->mControlsB + (which + 1), fraction, &current, Orientation());
            currentT = ppc_fmadds(a->mF08, 1.0f - fraction, b->mF08 * fraction);
        } else {
            const sBraceSpline* spline = brace1->mF90;
            const int32_t last = static_cast<int32_t>(spline->mCount) - 1;
            tag_sBezierControl* a = spline->mControlsA + last;
            tag_sBezierControl* b = brace2->mF90->mControlsA;
            InterpolateCoasterSpline(gCoasterCurrent->mSamples, Data(), a, b, spline->mControlsB + last, brace2->mF90->mControlsB,
                                     *t - gExtrudeTOffset, &current, Orientation());
            const float place = *t - gExtrudeTOffset;
            currentT = ppc_fmadds(a->mF08, 1.0f - place, b->mF08 * place);
        }
        bool moved = false;
        while (params->mF00 < *t) {
            moved = true;
            ++params;
        }
        const int32_t vertices = static_cast<int32_t>(params->mSection->mF00);
        bool newSet = true;
        if (static_cast<int32_t>(static_cast<uint32_t>(vertices) << 1) <= gExtrudeVertsLeft &&
            static_cast<int32_t>(params->mSection->mPointCount << 1) <= gExtrudeFacesLeft) {
            newSet = false;
        }
        if (newSet || moved || restart) {
            const float ringT = DataInstance()->mF70;
            if (newSet) GetNewTrackMeshSet(0);
            ExtrudeCrossSectionVertices(ringT, b2, b3, b4, previous, gExtrudeMin, gExtrudeMax, newSet ? 1 : 0, params->mSection, params);
            CountVertices(static_cast<int32_t>(params->mSection->mF00));
        }
        const float dx = previous.x - current.x;
        const float dy = previous.y - current.y;
        const float dz = previous.z - current.z;
        const double flat = ppc_fmadd(static_cast<double>(dx), static_cast<double>(dx), static_cast<double>(dy * dy));
        const float distance = static_cast<float>(std::sqrt(static_cast<double>(dz * dz) + flat));
        DataInstance()->mF74 = DataInstance()->mF74 + distance / 6.0f;
        ExtrudeCrossSectionVertices(currentT, DataInstance()->mF7C, DataInstance()->mF80, DataInstance()->mF84, current, gExtrudeMin,
                                    gExtrudeMax, 0, params->mSection, params);
        SkinCrossSectionFaces(params->mSection, static_cast<int32_t>(params->mSection->mPointCount), vertices);
        const int32_t n = static_cast<int32_t>(params->mSection->mF00);
        const int32_t edges = static_cast<int32_t>(params->mSection->mPointCount);
        CountVertices(n);
        gExtrudeFacesUsed += edges * 2;
        gExtrudeFacesLeft -= edges * 2;
        DataInstance()->mF70 = currentT;
        previous = current;
    }
}

// 0x1004fd94. The type holds at 0x78 the table and at 0x7c the count (Mac offsets). Entries are 0x2c bytes: eight floats (low, high
// for the angle and the orientation's three angles) and the answer word at 0x20. The last entry is the fallback and is never tested.
int32_t GetCrossSectionFromParams(sCoasterType* type, float unused, sCoasterOrientation* orientation, float angle) {
    (void)unused;
    const float values[4] = {angle, orientation->mF04, orientation->mF08, orientation->mF0C};
    int32_t found = 0;
    const sCoasterCrossSectionEntry* entry = type->mTable;
    for (uint32_t i = 0; i < type->mCount - 1u; ++i, ++entry) {
        const float* r = entry->mRange;
        if (values[0] >= r[0] && values[0] <= r[1] && values[1] >= r[2] && values[1] <= r[3] && values[2] >= r[4] && values[2] <= r[5] &&
            values[3] >= r[6] && values[3] <= r[7]) {
            found = entry->mAnswer;
            break;
        }
    }
    if (found != 0) return found;
    return type->mTable[type->mCount - 1u].mAnswer;
}

float gExtrudeBlendMin = 0.0f;         // 0x1029ce70
float gExtrudeBlendMax = 1.0f;         // 0x1029ce74
sExtrusionParams gExtrudeParamsBuf[2560];   // 0x102b7674

namespace {

// The words the type's table keeps are Mac addresses: on the host they are arena offsets (0 is null); the out records keep native pointers.
sCoasterRecord* AsRecord(uint32_t word) {
    return word != 0 ? reinterpret_cast<sCoasterRecord*>(tpw::gArenaBase + word) : nullptr;
}

float RecordLength(const sCoasterRecord* record) {   // the record's word +0x14 (mF14), a float
    float value;
    std::memcpy(&value, &record->mF14, sizeof(value));
    return value;
}

// The key of a track position: the sample number (the whole part, indexing the samples) plus how far the position is between the sample's
// control and the next one's; the control list is walked while the next control's place is below the position.
float KeyOf(float position) {
    sTrackSample* samples = gCoasterCurrent->mSamples;
    // The call stays a call (the optimiser merges calls of modf with the same argument, and the original makes every one).
    static double (*volatile callModf)(double, double*) = static_cast<double (*)(double, double*)>(&std::modf);
    double whole;
    callModf(static_cast<double>(position), &whole);
    const int32_t index = ppc_fctiwz(whole);
    tag_sBezierControl* node = reinterpret_cast<tag_sBezierControl*>(samples[index].mPiece);
    tag_sBezierControl* next;
    for (;;) {
        next = node->mNext;
        if (next == nullptr) break;
        if (!(next->mF08 < position)) break;
        node = next;
    }
    float ratio;
    if (next != nullptr) {
        const float from = node->mF08;
        const float a = position - from;
        const float b = next->mF08 - from;
        ratio = a / b;
    } else {
        ratio = 0.0f;
    }
    uint32_t number;
    std::memcpy(&number, node, sizeof(number));
    return ratio + static_cast<float>(number);
}

}  // namespace

// 0x1004ff18 (2,536 bytes). Mac registers: r23 out, r22 branch, r19 inBranch, r25 previous answer, r26 new answer, r24 the word the out record keeps.
void GenerateExtrusions() {
    SetInitialCoasterInterpParams(static_cast<sCoasterTrackParams*>(gCoasterCurrentData));
    sCoasterType* type = reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterCurrent->mType));
    sCoasterOrientation* orient = reinterpret_cast<sCoasterOrientation*>(
        static_cast<char*>(gCoasterCurrentData) + (offsetof(tag_sCoasterInstance, mF78) - offsetof(tag_sCoasterInstance, mBlock3C)));
    float pos = 0.0f;
    bool inBranch = false;
    sExtrusionParams* branch = nullptr;
    sExtrusionParams* out = gExtrudeParamsBuf;
    out->mF00 = 0.0f;
    out->mF04 = 0.0f;
    uint32_t previous = static_cast<uint32_t>(type->mTable[0].mAnswer);
    out->mSection = AsRecord(previous);
    sExtrusionParams& second = gExtrudeParamsStatic[1];
    second.mF04 = 10000000.0f;
    second.mF00 = 10000000.0f;
    second.mSection = *reinterpret_cast<sCoasterRecord**>(reinterpret_cast<char*>(type) + kCoasterTypeSectionB);
    second.mF08 = 0.0f;
    sExtrusionParams& first = gExtrudeParamsStatic[0];
    first.mF04 = 10000000.0f;
    first.mF00 = 10000000.0f;
    first.mF08 = 0.0f;
    first.mSection = *reinterpret_cast<sCoasterRecord**>(reinterpret_cast<char*>(type) + kCoasterTypeSectionA);

    while (!IsEndOfSampleBuffer(gCoasterCurrent, pos)) {
        s3DPoint point;
        InterpolateCoasterSpline(gCoasterCurrent, pos, &point, orient);
        // The call stays a call of fmod (the double one): the optimiser would narrow it to fmodf, as both arguments are floats.
        static double (*volatile callFmod)(double, double) = static_cast<double (*)(double, double)>(&std::fmod);
        orient->mF0C = static_cast<float>(callFmod(static_cast<double>(orient->mF0C), 3.1415927410125732));
        const uint32_t answer = static_cast<uint32_t>(GetCrossSectionFromParams(type, pos, orient, 0.0f));
        uint32_t keep = answer;
        if (answer != previous) {
            uint32_t flags;
            std::memcpy(&flags, type->mF00 + 0x74, sizeof(flags));
            if ((flags & 1) == 0) {
                out->mF04 = pos;
                out->mF00 = KeyOf(out->mF04);
                ++out;
            } else {
                sCoasterRecord* transition = nullptr;
                uint32_t transitionWord = 0;
                for (uint32_t i = 0; i < type->mCount; ++i) {
                    const sCoasterCrossSectionEntry* entry = &type->mTable[i];
                    uint32_t from, to;
                    std::memcpy(&from, entry->mF24, sizeof(from));
                    std::memcpy(&to, entry->mF24 + 4, sizeof(to));
                    if (from == previous && to == answer) {
                        transitionWord = static_cast<uint32_t>(entry->mAnswer);
                        transition = AsRecord(transitionWord);
                        break;
                    }
                }
                out->mF04 = pos;
                out->mF00 = KeyOf(out->mF04);
                ++out;
                if (transition != nullptr) {
                    const float length = RecordLength(transition);
                    if (length > 0.0f) {
                        keep = transitionWord;
                        if (inBranch) {
                            branch->mF04 = pos;
                            branch->mF00 = KeyOf(branch->mF04);
                        }
                        inBranch = true;
                        out->mF04 = pos + RecordLength(transition);
                        out->mF00 = KeyOf(out->mF04);
                        out->mF08 = 1.0f;
                        branch = out;
                    } else {
                        out->mSection = transition;
                        const float end = pos + length;
                        if (branch != nullptr && branch->mF04 > end) {
                            if (inBranch) {
                                const float lengthB = std::fabs(RecordLength(branch->mSection));
                                const float lengthA = std::fabs(RecordLength(transition));
                                const float lowest = branch->mF04;
                                const float f1 = lowest - lengthB;
                                const float denominator = lengthB + lengthA;
                                const float f1b = pos - f1;
                                float ratio = f1b / denominator;
                                ratio = (ratio < gExtrudeBlendMin) ? gExtrudeBlendMin : ratio;
                                ratio = (gExtrudeBlendMax < ratio) ? gExtrudeBlendMax : ratio;
                                branch->mF08 = ratio;
                                out->mF08 = ratio;
                                branch->mF04 = ppc_fnmsubs(1.0f - ratio, lengthB, branch->mF04);
                                branch->mF00 = KeyOf(branch->mF04);
                                out->mF04 = pos;
                                out->mF00 = KeyOf(out->mF04);
                                ++out;
                            } else {
                                out->mF04 = pos;
                                out->mF00 = KeyOf(out->mF04);
                                out->mF08 = 1.0f;
                                ++out;
                            }
                        } else {
                            out[-1].mF04 = pos + RecordLength(transition);
                            out[-1].mF00 = KeyOf(out[-1].mF04);
                            out->mF04 = pos;
                            out->mF00 = KeyOf(out->mF04);
                            out->mF08 = 1.0f;
                            ++out;
                        }
                        inBranch = false;
                    }
                    branch = out;
                }
            }
            previous = answer;
            out->mSection = AsRecord(keep);
        }
        if (inBranch) {
            const float limit = branch->mF04;
            if (pos > limit || pos == limit) {
                out->mF04 = pos;
                out->mF00 = KeyOf(out->mF04);
                inBranch = false;
                out[1].mSection = AsRecord(answer);
                ++out;
            }
        }
        pos = pos + 0.25f;
    }
    out->mF04 = (1.0f + pos) + 0.25f;
    out->mF00 = 100000.0f;
}
