// Unit v5962: the embedded-entry counters (0x101fff0c, 0x101ffe74), EnableRideHead (0x10200230), bthread_cond (0x10200e34, 0x10200ea8,
// 0x10200f30) and CalculateOrientFromTrackMorph (0x101fec54).
#include <cmath>

#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "TPCoaster.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "platform/macdoze.h"
#include "ppc_math.h"
#include "rsse.h"

// Name invented, only the address is used (0x1029fda8).
uint32_t gRideHeadSpawnBlock = 0;

// Weak defaults for callees that no translation provides yet (the test library defines the real fakes).
__attribute__((weak)) uint32_t CPerson::GetHeadSpriteID() { return 0; }
__attribute__((weak)) NS_MacDoze::InternalHandle* NS_MacDoze::CreateEvent(void*, int32_t, int32_t, const char*) { return nullptr; }
__attribute__((weak)) uint32_t NS_MacDoze::WaitForSingleObject(NS_MacDoze::InternalHandle*, uint32_t) { return 0; }
__attribute__((weak)) void bthread_callee::OperatorDelete(void* p) { ::operator delete(p); }

// 0x101fff0c: how many embedded entries of the set's mesh have a flag in `kind`.
uint32_t CountDataNodesByType(tag_sRideAnimSet* set, uint32_t kind) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t count = 0;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i) {
        if ((kind & header->mEmbedded[i].mFlags) != 0) count++;
    }
    return count;
}

// 0x101ffe74: the index of the n-th (counting from 1) embedded entry with a flag in `kind`, -1 when there are fewer.
int32_t FindNthDataNodeIndex(tag_sRideAnimSet* set, uint32_t kind, uint32_t n) {
    tag_sM3D2Header* header = set->mHeads->mHeader;
    uint32_t found = 0;
    for (uint32_t i = 0; i < header->mEmbeddedCount; ++i) {
        if ((kind & header->mEmbedded[i].mFlags) != 0) {
            found++;
            if (found == n) return static_cast<int32_t>(i);
        }
    }
    return -1;
}

// 0x10200230. The original also compares index + the header's halfword 0x46 with the one at 0x44 (the result is dropped); the node count
// (0x44) reaches the assert's argument list because its register is still live.
void EnableRideHead(tag_sMesh* mesh, uint32_t index, uint32_t peep) {
    sEmbeddedSet* set = static_cast<sEmbeddedSet*>(mesh->mF28);
    sEmbeddedRecord* record = set->mRecords + index;
    set->mFlags = set->mFlags | 4;
    set->mF08 = set->mF08 + 1;
    record->mFlags = record->mFlags | 2;
    tag_sM3D2Header* header = mesh->mHeader;
    uint32_t head;
    if (peep != 0) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n", peep,
                       static_cast<uint32_t>(header->mNodeCount));
        head = static_cast<CPerson*>(gThingArray[peep & 0xffff].mThing)->GetHeadSpriteID();
    } else {
        head = 0;
    }
    if ((head & 0x80000000u) != 0) {
        record->mF0C = static_cast<uint32_t>(ESP_SpawnScript(&gRideHeadSpawnBlock, static_cast<ESP_Sprites>(3),
                                                              static_cast<int32_t>(head & 0x7fffffffu), static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f));
    } else {
        record->mF0C = static_cast<uint32_t>(ESP_SpawnScript(&gRideHeadSpawnBlock, static_cast<ESP_Sprites>(1), static_cast<int32_t>(head),
                                                              static_cast<ACTS>(0), 0.0f, 0.0f, 0.0f));
    }
}

// 0x10200e34
bthread_cond::bthread_cond() { mHandle = NS_MacDoze::CreateEvent(nullptr, 0, 0, nullptr); }

// 0x10200ea8
bthread_cond* bthread_cond::Destroy(bthread_cond* self, int16_t flag) {
    if (self != nullptr) {
        NS_MacDoze::CloseHandle(self->mHandle);
        if (flag > 0) bthread_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10200f30
bool bthread_cond::wait(int32_t milliseconds) {
    uint32_t result;
    if (milliseconds < 0) {
        result = NS_MacDoze::WaitForSingleObject(mHandle, 0xffffffffu);
    } else {
        result = NS_MacDoze::WaitForSingleObject(mHandle, static_cast<uint32_t>(milliseconds));
    }
    return result == 0;
}

// 0x101fec54: the matrix of a track morph node: the third row is the direction from points to position (the normal's own turn when that is
// not a number), the second the normal, the first their cross product; then the morph's three angles (looked up in the sine table, the
// cosine taken half a turn on) turn it about z, y and x.
void CalculateOrientFromTrackMorph(s3DPoint* points, s3DPoint* position, s3DPoint* normal, sTrackMorphingParent* morph, sMatrix* matrix) {
    const uint8_t* angles = reinterpret_cast<const uint8_t*>(morph) - morph->mBack;   // three floats: x, y, z
    float dy = position->y - points->y;
    float dx = position->x - points->x;
    float dz = position->z - points->z;
    double sum =ppc_fmadd(dx, dx, static_cast<double>(dy * dy));
    double length = std::sqrt(static_cast<double>(dz * dz) + sum);
    float len = static_cast<float>(length);
    float fx = dx / len;
    float fy = dy / len;
    float fz = dz / len;
    if (std::isnan(fx)) {
        fx = normal->z;
        fy = normal->x;
        fz = normal->y;
    }
    float* m = &matrix->m[0][0];
    m[0] = ppc_fmsubs(normal->y, fz, normal->z * fy);
    m[1] = ppc_fmsubs(normal->z, fx, normal->x * fz);
    m[2] = ppc_fmsubs(normal->x, fy, normal->y * fx);
    m[4] = normal->x;
    m[5] = normal->y;
    m[6] = normal->z;
    m[8] = fx;
    m[9] = fy;
    m[10] = fz;
    sMatrix turn;
    SetMatrixIdentity(&turn);
    const float halfTurn = 1.5707963705062866f;
    float angle = reinterpret_cast<const float*>(angles)[2];
    int32_t s = ppc_fctiwz(angle * gTPGlobals.mSinTableScale);
    int32_t c = ppc_fctiwz((halfTurn + angle) * gTPGlobals.mSinTableScale);
    ZRotateLMatrix(&turn, gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(c)],
                   gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(s)]);
    angle = reinterpret_cast<const float*>(angles)[1];
    s = ppc_fctiwz(angle * gTPGlobals.mSinTableScale);
    c = ppc_fctiwz((halfTurn + angle) * gTPGlobals.mSinTableScale);
    YRotateLMatrix(&turn, gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(c)],
                   gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(s)]);
    angle = reinterpret_cast<const float*>(angles)[0];
    s = ppc_fctiwz(angle * gTPGlobals.mSinTableScale);
    c = ppc_fctiwz((halfTurn + angle) * gTPGlobals.mSinTableScale);
    XRotateLMatrix(&turn, gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(c)],
                   gTPGlobals.mSinTable[gTPGlobals.mSinTableMask & static_cast<uint32_t>(s)]);
    CombineMatricesNoWNoTransDest2nd(&turn, matrix);
}
