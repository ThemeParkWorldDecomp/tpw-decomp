// Unit v5L20: the coaster collision system (TPConfig.cpp on the Mac): the boxes of the track between braces, the chunks of two tracks
// inside the boxes' overlap, the test of two chunks' cross sections, the braces' collision lists, and GetPylonAtCell.
#include <cmath>
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

sTrackChunk* gCollisionChunksA;       // 0x102b7460
sTrackChunk* gCollisionChunksB;       // 0x102b745c
void* gCollisionBlockA;               // 0x102b7468
void* gCollisionBlockB;               // 0x102b7464
sTrackSample* gCollisionSampleA;      // 0x102b7330
sTrackSample* gCollisionSampleB;      // 0x102b732c
tag_sCoasterType* gCollisionTypeA;    // 0x102b7458
tag_sCoasterType* gCollisionTypeB;    // 0x102b7454
s3DPoint gCollisionRings[24];         // 0x102b7334 (ring B), 0x102b73c4 (ring A)
s3DPoint* gCollisionCorner0;          // 0x102b7328
s3DPoint* gCollisionCorner1;          // 0x102b7324
s3DPoint* gCollisionCorner2;          // 0x102b7320
s3DPoint* gCollisionCorner3;          // 0x102b731c

namespace {
const uint32_t kBraceNoCollide = 0x10000;     // the brace takes no part in the collisions
const uint32_t kBraceRecalc = 0x200000;       // the brace's collisions are to be worked out again
const uint32_t kBraceOverflow = 0x400000;     // the brace's track crosses more than 8 others
const uint32_t kMaxCollisions = 8;

// The pylon control's radius (word 0x58) of the first of the two braces' pylons that has a control, else 0.
float BraceRadius(const tag_sTrackBrace* a, const tag_sTrackBrace* b) {
    if (a->mData->mF18Table != nullptr) return reinterpret_cast<const sPylonControl*>(a->mData->mF18Table)->mF58;
    if (b->mData->mF18Table != nullptr) return reinterpret_cast<const sPylonControl*>(b->mData->mF18Table)->mF58;
    return 0.0f;
}

// The table's sine: the product with the scale is a single (fmuls), truncated as fctiwz does, masked.
float TableSin(float angle) {
    const int32_t index = ppc_fctiwz(static_cast<double>(angle * gTPGlobals.mSinTableScale));
    return gTPGlobals.mSinTable[static_cast<uint32_t>(index) & gTPGlobals.mSinTableMask];
}

// The strict overlap of two boxes, x then y then z.
bool Overlap(const sAlignedBox& a, const sAlignedBox& b) {
    for (int k = 0; k < 3; ++k) {
        if (!(a.mMin[k] < b.mMax[k]) || !(b.mMin[k] < a.mMax[k])) return false;
    }
    return true;
}

bool Inside(const s3DPoint& v, const float* lo, const float* hi) {
    return v.x >= lo[0] && v.x <= hi[0] && v.y >= lo[1] && v.y <= hi[1] && v.z >= lo[2] && v.z <= hi[2];
}

// The angle the four normalised vectors sweep (each acos of the double of DotProduct3D, rounded to single, summed in single).
bool InsideQuad(const s3DPoint& point) {
    s3DPoint v[4];
    const s3DPoint* corners[4] = {gCollisionCorner0, gCollisionCorner1, gCollisionCorner2, gCollisionCorner3};
    for (int k = 0; k < 4; ++k) {
        v[k].x = corners[k]->x - point.x;
        v[k].y = corners[k]->y - point.y;
        v[k].z = corners[k]->z - point.z;
    }
    for (int k = 0; k < 4; ++k) NormaliseVector(&v[k]);
    float sum = static_cast<float>(std::acos(static_cast<double>(DotProduct3D(&v[0], &v[1]))));
    sum = sum + static_cast<float>(std::acos(static_cast<double>(DotProduct3D(&v[1], &v[2]))));
    sum = sum + static_cast<float>(std::acos(static_cast<double>(DotProduct3D(&v[2], &v[3]))));
    sum = sum + static_cast<float>(std::acos(static_cast<double>(DotProduct3D(&v[3], &v[0]))));
    return sum >= 6.2821855f;   // 0x40c907aa
}

// The brace's list without `gone`, the kept entries moved down and the rest of the list cleared; the instance's count follows.
// A brace past 8 entries whose list lost one is flagged for a recalculation.
void DropFromList(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, const tag_sTrackBrace* gone) {
    tag_sTrackBrace** write = brace->mCollisions;
    tag_sTrackBrace** read = brace->mCollisions;
    uint32_t removed = 0;
    for (uint32_t k = 0; k < brace->mFB0; ++k, ++read) {
        if (*read == gone) {
            ++removed;
            *read = nullptr;
        } else {
            *write++ = *read;
        }
    }
    const uint32_t kept = static_cast<uint32_t>(write - brace->mCollisions);
    for (uint32_t k = kept; k < brace->mFB0; ++k) brace->mCollisions[k] = nullptr;
    brace->mFB0 = kept;
    instance->mCollisionRefs -= removed;
    if (removed != 0 && (brace->mFlags & kBraceOverflow)) brace->mFlags |= kBraceRecalc;
}

// One brace's list gains `other`, else (8 already) the brace is flagged.
void AddToList(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, tag_sTrackBrace* other) {
    if (brace->mFB0 < kMaxCollisions) {
        brace->mCollisions[brace->mFB0] = other;
        ++instance->mCollisionRefs;
        ++brace->mFB0;
    } else {
        brace->mFlags |= kBraceOverflow;
    }
}
}  // namespace

// 0x1003f954 (124 bytes): the first pylon of any coaster on the cell (not a preview, flags 1 and 4 clear); the cell's index is
// written to `out` when given.
tag_sPylon* GetPylonAtCell(uint32_t x, uint32_t z, uint32_t* out) {
    const uint32_t index = x + z * gHeightfieldHeader->mHField->mF18[0];
    sPylonRecord* found = nullptr;
    for (tag_sCoasterInstance* instance = gCoasterInstances; instance != nullptr; instance = instance->mNext) {
        sPylonRecord* node = reinterpret_cast<sPylonRecord*>(instance->mPylons);
        while (node != nullptr && ((node->mFlags & 5) != 0 || node->mIndex != index)) node = node->mNext;
        if (node != nullptr) {
            found = node;
            break;
        }
    }
    if (out != nullptr) *out = index;
    return reinterpret_cast<tag_sPylon*>(found);
}

// 0x10047634 (372 bytes): the box of the samples from a's to b's (b's index minus a's more after the first, none for a difference of
// -1), widened by the radius. Each axis is tested against the minimum and only then against the maximum.
void GetTrackBounds(tag_sCoasterInstance* instance, tag_sTrackBrace* a, tag_sTrackBrace* b, sAlignedBox* box) {
    const sTrackSample* sample = &instance->mSamples[a->mSample];
    const uint32_t count = b->mSample - a->mSample;
    box->mMin[0] = box->mMax[0] = sample->mPos.x;
    box->mMin[1] = box->mMax[1] = sample->mPos.y;
    box->mMin[2] = box->mMax[2] = sample->mPos.z;
    if (count != 0xffffffffu) {
        for (uint32_t i = 0; i < count; ++i) {
            ++sample;
            const float v[3] = {sample->mPos.x, sample->mPos.y, sample->mPos.z};
            for (int k = 0; k < 3; ++k) {
                if (v[k] < box->mMin[k]) {
                    box->mMin[k] = v[k];
                } else if (v[k] > box->mMax[k]) {
                    box->mMax[k] = v[k];
                }
            }
        }
    }
    const float radius = BraceRadius(a, b);
    for (int k = 0; k < 3; ++k) {
        box->mMin[k] = box->mMin[k] - radius;
        box->mMax[k] = box->mMax[k] + radius;
    }
}

// 0x10047818 (96 bytes): every brace's box, from the last brace back to the first.
void RecalcTrackBounds(tag_sCoasterInstance* instance) {
    tag_sTrackBrace* later = instance->mLastBrace;
    for (tag_sTrackBrace* brace = later->mPrev; brace != nullptr; brace = brace->mPrev) {
        GetTrackBounds(instance, later, brace, &later->mCollisionBounds);
        later = brace;
    }
}

// 0x100478b8 (800 bytes): the chunks from a's sample on (b's index minus a's of them) with either end inside the box widened by the
// radius; each chunk's box is its two ends' widened by the radius.
void GenerateSamplesInBoundArea(tag_sCoasterInstance* instance, tag_sTrackBrace* a, tag_sTrackBrace* b, const sAlignedBox* box,
                                sTrackChunk* chunks, uint32_t* count) {
    const float radius = BraceRadius(a, b);
    float lo[3], hi[3];
    for (int k = 0; k < 3; ++k) {
        lo[k] = box->mMin[k] - radius;
        hi[k] = box->mMax[k] + radius;
    }
    const uint32_t samples = b->mSample - a->mSample;
    uint32_t found = 0;
    for (uint32_t i = 0; i < samples; ++i) {
        const s3DPoint& p = instance->mSamples[a->mSample + i].mPos;
        const s3DPoint& q = instance->mSamples[a->mSample + i + 1].mPos;
        if (Inside(p, lo, hi) || Inside(q, lo, hi)) {
            sTrackChunk* chunk = &chunks[found];
            chunk->mSample = a->mSample + i;
            const float pv[3] = {p.x, p.y, p.z};
            const float qv[3] = {q.x, q.y, q.z};
            for (int k = 0; k < 3; ++k) {
                if (pv[k] < qv[k]) {
                    chunk->mBounds.mMin[k] = pv[k] - radius;
                    chunk->mBounds.mMax[k] = qv[k] + radius;
                } else {
                    chunk->mBounds.mMin[k] = qv[k] - radius;
                    chunk->mBounds.mMax[k] = pv[k] + radius;
                }
            }
            ++found;
        }
    }
    *count = found;
}

// 0x10047c64 (2,052 bytes): the chunks are those of gCollisionSampleA and gCollisionSampleB (the arguments are not read). Each track's
// two samples get a ring of its type's pairs turned by the first sample's bank (z), pitch (x) and yaw (y) and moved to the sample.
// Then every quad of ring A (two neighbours on each ring) is tested against the edges of ring B from each vertex to the next and to
// the one on the other ring: an edge crossing the quad's plane at a point the quad's corners go all the way round (the angles sum to
// 2 pi) is a collision. QUIRKS kept: both tracks' rings have type A's vertex count, but ring B is walked with type B's; the rings are
// 0x90 bytes each on the Mac (a count above 6 runs into the next globals), the host keeps them in one array.
int32_t DoProperChunkCollision(sTrackChunk* a, sTrackChunk* b) {
    const float halfPi = 1.57079637f;   // 0x3fc90fdb
    const sTrackSample* first = gCollisionSampleA;
    const tag_sCoasterType* type = gCollisionTypeA;
    s3DPoint* out = &gCollisionRings[12];
    for (int track = 0; track < 2; ++track) {
        const float yaw = first->mYaw;
        const float pitch = first->mPitch;
        const float bank = first->mBank;
        const sTrackSample* sample = first;
        for (int ring = 0; ring < 2; ++ring, ++sample) {
            const float* pair = type->mPairs;
            for (uint32_t n = gCollisionTypeA->mPairCount; n != 0; --n, pair += 2, ++out) {
                out->x = pair[0];
                out->y = pair[1];
                out->z = 0.0f;
                float s = TableSin(bank);
                float c = TableSin(halfPi + bank);
                float x = out->x;
                out->x = ppc_fmsubs(x, c, out->y * s);
                out->y = ppc_fmadds(x, s, out->y * c);
                s = TableSin(pitch);
                c = TableSin(halfPi + pitch);
                float y = out->y;
                out->y = ppc_fmsubs(y, c, out->z * s);
                out->z = ppc_fmadds(y, s, out->z * c);
                s = TableSin(yaw);
                c = TableSin(halfPi + yaw);
                x = out->x;
                out->x = ppc_fmadds(x, c, out->z * s);
                out->z = ppc_fmadds(ppc_fneg(x), s, out->z * c);
                out->x = out->x + sample->mPos.x;
                out->y = out->y + sample->mPos.y;
                out->z = out->z + sample->mPos.z;
            }
        }
        first = gCollisionSampleB;
        type = gCollisionTypeB;
        out = &gCollisionRings[0];
    }

    s3DPoint* ringA = &gCollisionRings[12];
    s3DPoint* ringB = &gCollisionRings[0];
    for (uint32_t i = 0; i < gCollisionTypeA->mPairCount; ++i) {
        uint32_t count = gCollisionTypeA->mPairCount;
        gCollisionCorner0 = &ringA[i];
        gCollisionCorner1 = &ringA[(i + 1) % count];
        gCollisionCorner2 = &ringA[count + i];
        gCollisionCorner3 = &ringA[count + (i + 1) % count];
        s3DPoint d = *gCollisionCorner1;
        d.x = d.x - gCollisionCorner0->x;
        d.y = d.y - gCollisionCorner0->y;
        d.z = d.z - gCollisionCorner0->z;
        s3DPoint e = *gCollisionCorner1;
        e.x = e.x - gCollisionCorner2->x;
        e.y = e.y - gCollisionCorner2->y;
        e.z = e.z - gCollisionCorner2->z;
        // The cross product in double (fmsub of exact products), each rounded to single.
        float ny = static_cast<float>(static_cast<double>(d.z) * e.x - static_cast<double>(d.x) * e.z);
        float nx = static_cast<float>(static_cast<double>(d.y) * e.z - static_cast<double>(d.z) * e.y);
        float nz = static_cast<float>(static_cast<double>(d.x) * e.y - static_cast<double>(d.y) * e.x);
        const float yy = ny * ny;
        const float zz = nz * nz;
        const float length = static_cast<float>(
            std::sqrt(static_cast<double>(zz) + ppc_fmadd(static_cast<double>(nx), static_cast<double>(nx), static_cast<double>(yy))));
        ny = ny / length;
        nx = nx / length;
        nz = nz / length;
        const s3DPoint* on = gCollisionCorner1;
        const float plane = ppc_fneg(ppc_fmadds(nz, on->z, ppc_fmadds(nx, on->x, ny * on->y)));
        for (uint32_t j = 0; j < gCollisionTypeB->mPairCount; ++j) {
            count = gCollisionTypeB->mPairCount;
            const s3DPoint& p = ringB[j];
            const s3DPoint& q = ringB[(j + 1) % count];
            const s3DPoint& w = ringB[count + j];
            const float dqx = q.x - p.x, dqy = q.y - p.y, dqz = q.z - p.z;
            const float dwx = w.x - p.x, dwy = w.y - p.y, dwz = w.z - p.z;
            const float distance = plane + ppc_fmadds(p.z, nz, ppc_fmadds(p.x, nx, p.y * ny));
            const float t1 = ppc_fneg(distance / ppc_fmadds(dqz, nz, ppc_fmadds(dqx, nx, dqy * ny)));
            const float t2 = ppc_fneg(distance / ppc_fmadds(dwz, nz, ppc_fmadds(dwx, nx, dwy * ny)));
            if (t1 >= 0.0f && t1 <= 1.0f) {
                const s3DPoint hit = {ppc_fmadds(t1, dqx, p.x), ppc_fmadds(t1, dqy, p.y), ppc_fmadds(t1, dqz, p.z)};
                if (InsideQuad(hit)) return 1;
            }
            if (t2 >= 0.0f && t2 <= 1.0f) {
                const s3DPoint hit = {ppc_fmadds(t2, dwx, p.x), ppc_fmadds(t2, dwy, p.y), ppc_fmadds(t2, dwz, p.z)};
                if (InsideQuad(hit)) return 1;
            }
        }
    }
    return 0;
}

// 0x100484b0 (40 bytes)
float DotProduct3D(const s3DPoint* a, const s3DPoint* b) {
    return ppc_fmadds(a->z, b->z, ppc_fmadds(a->x, b->x, a->y * b->y));
}

// 0x10048510 (1,196 bytes): the track from a1 to a0 against the one from b1 to b0. Neighbours on one track and a track's start against
// an end pylon are left out, as is a b0 kept out of the collisions. Within the overlap of the two boxes, the first pair of chunks whose
// boxes overlap and whose cross sections cross puts each brace in the other's list (or flags it), and the walk ends.
void CheckTrackToTrackCollisions(tag_sCoasterInstance* instanceA, tag_sCoasterInstance* instanceB, tag_sTrackBrace* a0,
                                 tag_sTrackBrace* a1, tag_sTrackBrace* b0, tag_sTrackBrace* b1) {
    if (instanceA == instanceB) {
        if (a1->mData == b0->mData || a0->mData == b1->mData) return;
        if ((a1->mData->mFlags & 1) && b0->mNumber == 0) return;
        if (a0->mNumber == 0 && (b1->mData->mFlags & 1)) return;
    }
    if (b0->mFlags & kBraceNoCollide) return;
    const sAlignedBox& boxA = a0->mCollisionBounds;
    const sAlignedBox& boxB = b0->mCollisionBounds;
    if (!Overlap(boxA, boxB)) return;
    sAlignedBox both;
    for (int k = 0; k < 3; ++k) {
        both.mMin[k] = boxA.mMin[k] < boxB.mMin[k] ? boxB.mMin[k] : boxA.mMin[k];
        both.mMax[k] = boxB.mMax[k] < boxA.mMax[k] ? boxB.mMax[k] : boxA.mMax[k];
    }
    uint32_t countA, countB;
    GenerateSamplesInBoundArea(instanceA, a0, a1, &both, gCollisionChunksA, &countA);
    GenerateSamplesInBoundArea(instanceB, b0, b1, &both, gCollisionChunksB, &countB);
    for (uint32_t i = 0; i < countA; ++i) {
        sTrackChunk* chunkA = &gCollisionChunksA[i];
        for (uint32_t j = 0; j < countB; ++j) {
            sTrackChunk* chunkB = &gCollisionChunksB[j];
            gCollisionSampleA = &instanceA->mSamples[chunkA->mSample];
            gCollisionSampleB = &instanceB->mSamples[chunkB->mSample];
            gCollisionTypeA = const_cast<tag_sCoasterType*>(reinterpret_cast<const tag_sCoasterType*>(instanceA->mType));
            gCollisionTypeB = const_cast<tag_sCoasterType*>(reinterpret_cast<const tag_sCoasterType*>(instanceB->mType));
            if (Overlap(chunkA->mBounds, chunkB->mBounds) && DoProperChunkCollision(chunkA, chunkB) != 0) {
                AddToList(instanceA, a0, b0);
                AddToList(instanceB, b0, a0);
                return;
            }
        }
    }
}

// 0x10048a64 (528 bytes): the brace leaves every list it is in and its own list is emptied. A brace past 8 entries does not know all
// the braces it crosses, so every brace of the instance drops it; else only those in its list.
void RemoveAllCollisionsFromBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) {
    if (brace->mFlags & kBraceOverflow) {
        for (tag_sTrackBrace* other = instance->mLastBrace; other != nullptr; other = other->mPrev) DropFromList(instance, other, brace);
        for (uint32_t k = 0; k < kMaxCollisions; ++k) brace->mCollisions[k] = nullptr;
        instance->mCollisionRefs -= brace->mFB0;
        brace->mFB0 = 0;
        brace->mFlags &= ~kBraceOverflow;
    } else {
        for (uint32_t i = 0; i < brace->mFB0; ++i) {
            DropFromList(instance, brace->mCollisions[i], brace);
            brace->mCollisions[i] = nullptr;
        }
        instance->mCollisionRefs -= brace->mFB0;
        brace->mFB0 = 0;
    }
}

// 0x10048cd0 (208 bytes): the brace's track (back to the brace before it) against every other track section of the instance but its
// neighbours: those ending two braces back and earlier, then those ending two braces on and later.
void CheckTrackToAllTracksCollisions(tag_sCoasterInstance* instance, tag_sTrackBrace* brace) {
    RemoveAllCollisionsFromBrace(instance, brace);
    brace->mFlags &= ~(kBraceRecalc | kBraceOverflow);
    for (tag_sTrackBrace* other = brace->mPrev->mPrev; other != nullptr && other->mPrev != nullptr; other = other->mPrev) {
        CheckTrackToTrackCollisions(instance, instance, brace, brace->mPrev, other, other->mPrev);
    }
    if (brace->mNext != nullptr) {
        for (tag_sTrackBrace* other = brace->mNext->mNext; other != nullptr; other = other->mNext) {
            CheckTrackToTrackCollisions(instance, instance, brace, brace->mPrev, other, other->mPrev);
        }
    }
}

// 0x10048e00 (280 bytes): the braces flagged for a recalculation, from the last back (the first brace has no track before it): one
// kept out of the collisions just leaves them. The chunk blocks hold the instance's mSegmentSpan chunks each.
void CheckAllTrackToTrackCollisions(tag_sCoasterInstance* instance) {
    const uint32_t span = instance->mSegmentSpan;
    gCollisionChunksA = static_cast<sTrackChunk*>(MReleaseAllocF(span * sizeof(sTrackChunk), 1));
    gCollisionChunksB = static_cast<sTrackChunk*>(MReleaseAllocF(span * sizeof(sTrackChunk), 1));
    gCollisionBlockA = MReleaseAllocF(span * 0x78, 1);
    gCollisionBlockB = MReleaseAllocF(span * 0x78, 1);
    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr && brace->mPrev != nullptr; brace = brace->mPrev) {
        if (brace->mFlags & kBraceRecalc) {
            if (brace->mFlags & kBraceNoCollide) {
                RemoveAllCollisionsFromBrace(instance, brace);
            } else {
                CheckTrackToAllTracksCollisions(instance, brace);
            }
        }
    }
    MReleaseFreeF(gCollisionBlockB);
    MReleaseFreeF(gCollisionBlockA);
    MReleaseFreeF(gCollisionChunksB);
    MReleaseFreeF(gCollisionChunksA);
}

// 0x10048f64 (32 bytes)
void DoCollisionChecking(tag_sCoasterInstance* instance) { CheckAllTrackToTrackCollisions(instance); }

// 0x10048fc4 (84 bytes): the brace and the one after it (whose track ends at it) leave the collisions.
void RemoveBraceFromCollisionSystem(tag_sTrackBrace* brace, tag_sCoasterInstance* instance) {
    RemoveAllCollisionsFromBrace(instance, brace);
    if (brace->mNext != nullptr) RemoveAllCollisionsFromBrace(instance, brace->mNext);
}

// 0x10049078 (92 bytes): every brace leaves the collisions and is flagged for a recalculation.
void FlagRecalcAllCoasterCollisions(tag_sCoasterInstance* instance) {
    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        RemoveAllCollisionsFromBrace(instance, brace);
        brace->mFlags |= kBraceRecalc;
    }
}
