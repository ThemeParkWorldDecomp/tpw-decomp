// Unit v5627: BMP_Move (main 0x10029b9c, 4464 bytes, TPAdvisor.cpp): one step of a bump car. Scales the velocity by the owner's speed,
// moves the car, then by the owner's kind (a jump table on kind + 14) steers it, starts and moves its particle emitters and updates its
// sound; finally turns the heading and opens or closes the gate of the collision object the car is in.
// The bump, its owner and the list at 0x102b56c0 are read by Mac offset (see include/BMP_InsideArea.h, unit v5627).
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "rsse.h"

int32_t gBumpTurnMax = 16;    // 0x1029cb30
int32_t gBumpTurnMin = -16;   // 0x1029cb34

namespace {

const uint32_t kBumpWorldMagic = 0x4a454647;

int32_t& Word(void* base, uint32_t offset) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(base) + offset); }

int32_t& ListWord(uint32_t offset) { return Word(&gColList, offset); }

// The list's random number (MSVC's rand step on the seed at +0x38).
int32_t NextRandom() {
    uint32_t seed = static_cast<uint32_t>(ListWord(0x38)) * 0x343fdu + 0x269ec3u;
    ListWord(0x38) = static_cast<int32_t>(seed);
    return static_cast<int32_t>(seed) >> 16;
}

bool TickGate() { return (ListWord(4) & 0x1f) == 0; }

tagBumpHeadEntry& HeadEntry(tag_sRideAnimSet* set, int32_t index) {
    return static_cast<tagBumpHeadTable*>(set->mHeads->mF28)->entries[index];
}

void HeadPosition(tag_sRideAnimSet* set, int32_t index, float& x, float& y, float& z) {
    tagBumpHeadNode* node = HeadEntry(set, index).node;
    if (node != nullptr) {
        x = node->x;
        y = node->y;
        z = node->z;
    } else {
        x = y = z = 0.0f;
    }
}

// The sound block the three steering cases share: places the sound and sets its parameter from the speed.
void UpdateSound(tagBUMP* b, tagBUMPRIDE* owner) {
    PossitionSound1(b);
    int32_t rideId;
    if (static_cast<uint32_t>(ListWord(0)) == kBumpWorldMagic) {
        int32_t diff = static_cast<int32_t>(b->owner.mOffset - static_cast<uint32_t>(ListWord(8)));
        rideId = static_cast<int32_t>((static_cast<uint32_t>(owner->kind) << 8) | static_cast<uint32_t>(diff / 0xd0));
    } else {
        rideId = 0;
    }
    uint16_t thing;
    TrackRideIdToThingId(&thing, rideId);
    int32_t value;
    if (thing != gAdvisorThingId) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        value = Word(gThingArray[thing].mThing, 0x20);   // the thing's word at Mac offset 0x20
    } else {
        value = 0;
    }
    int32_t code = RSSE_GetSoundEventCode(value, 10);
    Word(b, 0x20) = SOUND_SetParamValue(Word(b, 0x20), static_cast<uint32_t>(code), static_cast<uint32_t>(Word(b, 0x4c) / 3));
}

// The turn toward the target heading at 0x58, scaled by the speed.
int32_t Turn(tagBUMP* b) {
    int32_t d = (Word(b, 0x58) - Word(b, 0x54)) & 0x1ff;
    if (d > 0x100) d = -(0x200 - d);
    return (d * Word(b, 0x4c)) / 1000;
}

// The heading of the velocity: 256 - 256 * atan2(vx, -vy) / 3.14159, in 1/512 turns.
void AimAlongVelocity(tagBUMP* b) {
    double a = std::atan2(static_cast<double>(b->vx), static_cast<double>(static_cast<int32_t>(0u - static_cast<uint32_t>(b->vy))));
    Word(b, 0x58) = ppc_fctiwz(ppc_fnmsub(256.0, a / 3.14159, 256.0));
}

// Starts an emitter of `type` at the head entry named by the bump's word `field` (0x24 or 0x28), launched along the node's z axis.
void LaunchFromHead(tagBUMP* b, int32_t type, uint32_t field) {
    uint32_t setIndex = static_cast<uint32_t>(Word(b, 8));
    float x, y, z;
    HeadPosition(gRideAnimSets[setIndex], Word(b, field), x, y, z);
    uint32_t id = setIndex;
    tag_sRideAnimSet* ride = *GetRideFromID(id);
    float* matrix = GetDataNodeWorldMatrix(ride->mHeads, reinterpret_cast<uint32_t&>(Word(b, field)));
    s3DPoint dir;
    if (matrix != nullptr) {
        if (HeadEntry(ride, Word(b, field)).f00 & 0x10) {
            dir.x = -matrix[8];
            dir.y = -matrix[9];
            dir.z = -matrix[10];
        } else {
            dir.x = matrix[8];
            dir.y = matrix[9];
            dir.z = matrix[10];
        }
        NormaliseVector(&dir);
    } else {
        dir.x = 0.0f;
        dir.y = 0.0f;
        dir.z = 1.0f;
    }
    float sx = 1024.0f * x, sy = 1024.0f * y, sz = 1024.0f * z;
    float dx = 1024.0f * dir.x, dy = 1024.0f * dir.y, dz = 1024.0f * dir.z;
    PAR_InitEmitterLaunch(type, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz), ppc_fctiwz(dx), ppc_fctiwz(dy), ppc_fctiwz(dz));
}

// Moves the emitter at 0x2c to the head entry at 0x24 when both are set.
void FollowHead(tagBUMP* b) {
    if (Word(b, 0x2c) == -1 || Word(b, 0x24) == -1) return;
    float x, y, z;
    HeadPosition(gRideAnimSets[static_cast<uint32_t>(Word(b, 8))], Word(b, 0x24), x, y, z);
    float sx = 1024.0f * x, sy = 1024.0f * y, sz = 1024.0f * z;
    PAR_SetEmitterPos(Word(b, 0x2c), ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz));
}

void SetCarAnim(tagBUMP* b, uint32_t anim, uint32_t loop) {
    if (Word(b, 8) != 0) FlushDeferredAnim(gRideAnimSets[static_cast<uint32_t>(Word(b, 8))], 0);
    if (Word(b, 8) != 0) SetCurRideAnim(gRideAnimSets[static_cast<uint32_t>(Word(b, 8))], anim, 0, loop, 1.0f, 0);
}

// Case 0x10029ce0: the push decays and steers.
void SteerByPush(tagBUMP* b, tagBUMPRIDE* owner) {
    if (static_cast<int32_t>(std::labs(b->push)) > 0x30) {
        float push = static_cast<float>(b->push);
        float scaled = 99.8f * push;
        b->push = ppc_fctiwz(scaled / 100.0f);
    }
    int32_t wanted = b->push / 128;
    const int32_t* p = &gBumpTurnMax;
    if (wanted < *p) p = &wanted;
    if (!(*p > gBumpTurnMin)) p = &gBumpTurnMin;
    Word(b, 0x5c) = *p;
    if (owner->state == 2 && TickGate()) {
        if ((NextRandom() & 7) == 2) {
            tag_sNode* node = gRideAnimSets[static_cast<uint32_t>(Word(b, 8))]->mHeads->mHeader->mF78.get();
            float sx = 1024.0f * node->mMatrix.m[3][0];
            float sy = 1024.0f * node->mMatrix.m[3][1];
            float sz = 1024.0f * node->mMatrix.m[3][2];
            PAR_InitEmitter(0x60, ppc_fctiwz(sx), ppc_fctiwz(sy), ppc_fctiwz(sz));
        }
    }
    if (Word(b, 0x20) != 0) UpdateSound(b, owner);
}

// Case 0x10029f1c: steers toward the velocity (kind -1: toward the target at 0x50) and launches the head's emitter now and then.
void SteerByVelocity(tagBUMP* b, tagBUMPRIDE* owner) {
    if (owner->state == 2) {
        if (owner->kind != -1) {
            AimAlongVelocity(b);
        } else {
            Word(b, 0x58) = Word(b, 0x50);
        }
        Word(b, 0x5c) = Turn(b);
    } else {
        Word(b, 0x5c) = 0;
    }
    owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    if (owner->state == 2 && (b->flags & 0x4000) && TickGate() && Word(b, 0x28) != -1) {
        if ((NextRandom() & 3) == 2) {
            if (reinterpret_cast<tagBUMPRIDE*>(b->owner.get())->kind != -1 && Word(b, 0x14) == 0) {
                LaunchFromHead(b, 1, 0x28);
                Word(b, 0x14) = 4;
            }
        }
    }
    if (Word(b, 0x20) != 0) UpdateSound(b, reinterpret_cast<tagBUMPRIDE*>(b->owner.get()));
    FollowHead(b);
}

// Case 0x1002a3c0: the car's anim by speed, the head's emitters, then the default steering.
void Drive(tagBUMP* b, tagBUMPRIDE* owner) {
    if (owner->state == 2 && TickGate() && Word(b, 0x4c) > 0xe6 && Word(b, 0x14) == 0) Word(b, 0x14) = 2;
    if (Word(b, 0x4c) > 0x28) {
        if (Word(b, 0x10) == 0xc) {
            Word(b, 0x10) = 5;
            SetCarAnim(b, 5, 1);
        }
    } else if (Word(b, 0x10) == 5 && (b->flags & 0x400) == 0) {
        Word(b, 0x10) = 0xc;
        SetCarAnim(b, 0xc, 2);
    }
    owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    if (owner->state == 2 && owner->broken == 1 && TickGate() && Word(b, 0x4c) > 0x50 && Word(b, 0x24) != -1) {
        int32_t roll = NextRandom() & 7;
        if (roll == 2) {
            if (Word(b, 0x14) == 0) {
                LaunchFromHead(b, 10, 0x24);
                Word(b, 0x14) = 1;
            }
        } else if (roll < 2 && roll >= 0) {
            LaunchFromHead(b, 1, 0x24);
        }
    }
    if (Word(b, 0x20) != 0) UpdateSound(b, reinterpret_cast<tagBUMPRIDE*>(b->owner.get()));
    FollowHead(b);
}

// Case default (0x1002a9f4): steers toward the velocity and keeps the turn's change.
void SteerDefault(tagBUMP* b) {
    AimAlongVelocity(b);
    int32_t d = (Word(b, 0x58) - Word(b, 0x54)) & 0x1ff;
    if (d > 0x100) d = -(0x200 - d);
    Word(b, 0x60) = Word(b, 0x5c);
    Word(b, 0x5c) = (d * Word(b, 0x4c)) / 1000;
    Word(b, 0x60) = Word(b, 0x5c) - Word(b, 0x60);
}

}  // namespace

void BMP_Move(tagBUMP* b) {
    tagBUMPRIDE* owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    int32_t scale = Word(owner, 0x24);
    if (b->flags & 0x80000) {
        scale = (scale * 7) / 10;
    } else if (owner->broken == 2) {
        scale = 1000;
    }
    b->vx = (b->vx * scale) / 1024;
    b->dirX = b->vx;
    b->vy = (b->vy * scale) / 1024;
    b->dirY = b->vy;
    int32_t square = b->vx * b->vx + b->vy * b->vy;
    Word(b, 0x4c) = ppc_fctiwz(std::sqrt(static_cast<double>(square)));
    b->x = b->x + b->vx;
    b->y = b->y + b->vy;

    owner = reinterpret_cast<tagBUMPRIDE*>(b->owner.get());
    switch (owner->kind) {   // the table at 0x1029cb38 on kind + 14; -2 and the kinds outside -14..-1 take the default
    case -14:
    case -11:
    case -6:
    case -3:
    case -1:
        SteerByVelocity(b, owner);
        break;
    case -13:
    case -12:
    case -8:
    case -5:
        SteerByPush(b, owner);
        break;
    case -10:
    case -9:
    case -7:
    case -4:
        Drive(b, owner);
        SteerDefault(b);
        break;
    default:
        SteerDefault(b);
        break;
    }

    Word(b, 0x54) = (Word(b, 0x54) + Word(b, 0x5c)) & 0x1ff;
    tabCOLOBJECT* col = b->col.get();
    if (col == nullptr) return;
    if ((col->flags & 0x100) == 0) {
        if (b->flags & 0x200) BMP_MaskCar(b, 0);
        return;
    }
    Ptr32<tagBUMPTRIGGER> link;
    link.mOffset = col->pad14[1];   // the object's word 0x18
    tagBUMPTRIGGER* trigger = link.get();
    if (trigger == nullptr || trigger->magic != 0xdac) return;
    int32_t d = trigger->axis == 0xc ? b->x - col->x : b->y - col->y;
    uint32_t kind = 0;   // the Mac reads its stack slot unset when no anim set is named (index 0)
    uint32_t index = 0;
    if (trigger->open == 0) {
        uint32_t f = b->flags;
        if ((f & 0x200) == 0 && d < 0x4b0 && d > -0x4b0) {
            if (d < 0) b->flags = f | 0x800000;
            else b->flags = f & ~0x800000u;
            BMP_MaskCar(b, 1);
            if (trigger->animSet != 0) GetCurrentAnim(gRideAnimSets[trigger->animSet], &kind, &index, 0);
            if (kind != 5) {
                if (trigger->animSet != 0) FlushDeferredAnim(gRideAnimSets[trigger->animSet], 0);
                if (trigger->animSet != 0) SetCurRideAnim(gRideAnimSets[trigger->animSet], 5, 0, 0, 1.0f, 0);
                trigger->flags |= 2;
            }
        }
    } else {
        uint32_t f = b->flags;
        if (f & 0x200) {
            uint32_t right = f & 0x800000;
            if ((right != 0 && d > 0x4b0) || (right == 0 && d < -0x4b0)) BMP_MaskCar(b, 0);
        }
    }
    if (trigger->flags & 2) {
        if (trigger->animSet != 0) GetDeferredAnim(gRideAnimSets[trigger->animSet], &kind, &index, 0);
        if (kind == 0xc) {
            if (trigger->animSet != 0) SetCurRideAnim(gRideAnimSets[trigger->animSet], 2, 0, 1, 1.0f, 0);
            trigger->flags &= ~2u;
        }
    }
}
