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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"

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

// 0x1004e318. The point and the orientation of the track between the controls a and b at t (the stand-in callees write them), then the
// bank: from the samples the controls c and d name when the params' word 0 has bit 8, else blended from the braces' +0xe4 by the
// fraction of the controls' brace numbers. orient is optional: the position alone is wanted then.
void InterpolateCoasterSpline(sTrackSample* samples, sCoasterTrackParams* params, tag_sBezierControl* a, tag_sBezierControl* b,
                              tag_sBezierControl* c, tag_sBezierControl* d, float t, s3DPoint* point, sCoasterOrientation* orient) {
    BezierPosition(a, b, t, point);
    if (orient == nullptr) return;
    s3DPoint tangent;
    BezierTangent(a, b, t, &tangent);
    const float u = 1.0f - t;
    const float bTwist = b->mBrace->mF6C + b->mF0C;
    const float aTwist = a->mBrace->mF6C + a->mF0C;
    const float twist = ppc_fmadds(aTwist, u, bTwist * t);
    GetAlignmentParamsQuick(tangent, twist, orient, nullptr);
    GetBankParams(point, c, d, t, orient);
    if ((*reinterpret_cast<const uint32_t*>(params) & 8) != 0) {
        const uint32_t second = cw_cvt_fp2unsigned(d->mF08);
        const float fromSecond = t * samples[second].mBank;
        const uint32_t first = cw_cvt_fp2unsigned(c->mF08);
        orient->mF0C = orient->mF0C + ppc_fmadds(u, samples[first].mBank, fromSecond);
    } else {
        const float dPlace = static_cast<float>(d->mBrace->mNumber) + d->mF0C;
        const float cPlace = static_cast<float>(c->mBrace->mNumber) + c->mF0C;
        float place = ppc_fmadds(cPlace, u, dPlace * t);
        double whole;
        std::modf(static_cast<double>(cPlace), &whole);
        place = place - static_cast<float>(whole);
        place = (place < 0.0f) ? 0.0f : place;
        place = (1.0f < place) ? 1.0f : place;
        orient->mF0C = orient->mF0C + ppc_fmadds(c->mBrace->mFE4, 1.0f - place, d->mBrace->mFE4 * place);
    }
    orient->mF18 = orient->mF0C;
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
