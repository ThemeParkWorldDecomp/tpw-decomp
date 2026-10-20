// CPerson::UpdateTurn (0x1011f7d0, 1464 bytes) and CPerson::FindNearestFireworks (0x1011ed54, 396 bytes).
// The person is reached by Mac offset (+0x4 and +0x6 the cell bytes, +0x30 the CThoughts, +0x19c / +0x1a4 / +0x1a8 / +0x1ac / +0x1b0 floats
// clamped to 0..100, +0x1bc, +0xc2 the sprite halfword, +0x1dc, +0x214, +0x220 state, +0x228 and +0x22a the queue links).
#include <cstdint>
#include <cstdlib>

#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"

uint16_t gThingListEnd;   // 0x102a1008
uint16_t gQueueLinkA;     // 0x102a100a
uint16_t gQueueLinkB;     // 0x102a100c

namespace {

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10244aa8
const char kQueueText[] = "Person %d is in queue for object %d (next %d, prev %d) but doesn't think he is\n";   // 0x10244aa8 + 0x899

uint8_t* Bytes(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }
uint16_t& H(uint8_t* base, int32_t off) { return *reinterpret_cast<uint16_t*>(base + off); }
float& F(uint8_t* base, int32_t off) { return *reinterpret_cast<float*>(base + off); }

// fctiwz's low word (saturating)
uint32_t Fctiwz(float value) {
    if (value != value) return 0x80000000u;
    if (value >= 2147483648.0f) return 0x7fffffffu;
    if (value < -2147483648.0f) return 0x80000000u;
    return static_cast<uint32_t>(static_cast<int32_t>(value));
}

// field = clamp(value, 0, 100) as the listing's compare chain does (a NaN is stored as it is)
void Store(float& field, float value) {
    if (value > 100.0f) field = 100.0f;
    else if (value < 0.0f) field = 0.0f;
    else field = value;
}

void AddCell(uint8_t* self, int32_t off, const int16_t* cell, int32_t index) {
    float cur = F(self, off);
    float value = static_cast<float>(cell[index]);
    Store(F(self, off), value + cur);
}

void CheckHundred(uint8_t* self, int32_t off) {
    if ((Fctiwz(F(self, off)) & 0xffu) == 100u) Store(F(self, 0x19c), -1.0f + F(self, 0x19c));
}

}  // namespace

// 0x1011f7d0
void CPerson::UpdateTurn() {
    uint8_t* self = Bytes(this);
    reinterpret_cast<CPeep*>(this)->UpdateTurn();
    uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    if ((Bytes(gWorld) != nullptr) && ((gWorld->mTurn & 3u) == (static_cast<uint32_t>(id) & 3u))) {
        *reinterpret_cast<int32_t*>(self + 0x1bc) -= 1;
        uint16_t cellId = static_cast<uint16_t>(self[4] + (static_cast<uint32_t>(self[6]) << 7) + 1);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellTag;
        cellTag.mIndex = cellId;
        const int16_t* cell = reinterpret_cast<const int16_t*>(gWorldMap->GetRegionalEffectCell(cellTag));
        AddCell(self, 0x19c, cell, 0);
        AddCell(self, 0x1b0, cell, 1);
        AddCell(self, 0x1a8, cell, 2);
        if ((gWorld->mTurn & 0xfu) == 0) {
            Store(F(self, 0x1ac), 1.0f + F(self, 0x1ac));
            Store(F(self, 0x1a8), 2.0f + F(self, 0x1a8));
            Store(F(self, 0x1a4), 2.0f + F(self, 0x1a4));
        }
        CheckHundred(self, 0x1b0);
        CheckHundred(self, 0x1a8);
        CheckHundred(self, 0x1a4);
        CheckHundred(self, 0x1ac);
        if ((Fctiwz(F(self, 0x1ac)) & 0xffu) > 0x50u) H(self, 0xc2) = gPersonTable[1];
        else H(self, 0xc2) = gPersonTable[0];
        uint32_t state = *reinterpret_cast<uint32_t*>(self + 0x220);
        if (state != 0x10 && state != 0x11) {
            if (reinterpret_cast<CPeep*>(this)->IsInsidePark() != 0) {
                uint32_t rand = gWorld->GetRand();
                if (rand % 10u == 0) DoSomeThinking();
                uint32_t balloon = *reinterpret_cast<uint32_t*>(self + 0x214);
                if (balloon != 0) {
                    balloon -= 1;
                    *reinterpret_cast<uint32_t*>(self + 0x214) = balloon;
                    if (balloon == 0) MakeBalloonPop();
                }
            }
        }
        reinterpret_cast<CThoughts*>(self + 0x30)->ProcessThoughtBubbles();
    }
    if (InQueueingState() == 0) {
        uint16_t prev = H(self, 0x22a);
        uint16_t next = H(self, 0x228);
        uint16_t object = H(self, 0x1dc);
        uint16_t thingId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        int32_t ok = 0;
        if (gQueueLinkA == H(self, 0x228) && gQueueLinkB == H(self, 0x22a)) ok = 1;
        GAME_ASSERTMSG(ok, kQueueText, static_cast<uint32_t>(thingId), static_cast<uint32_t>(object), static_cast<uint32_t>(next),
                       static_cast<uint32_t>(prev));
        H(self, 0x228) = 0;
        H(self, 0x22a) = 0;
    }
    UpdateTemporaryHistory();
}

// 0x1011ed54: the hidden result pointer is r3, `this` r4 in the listing.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CPerson::FindNearestFireworks() {
    uint8_t* self = Bytes(this);
    uint32_t bestDistance = 0xffffffffu;
    uint16_t current = H(Bytes(gWorld), 0x1da746);
    uint16_t best = 0;
    while (current != gThingListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZero);
        uint8_t* thing = Bytes(gThingArray[current].mThing);
        if ((H(thing, 0x2e) & 0x80u) != 0) {
            RSSE_SCRIPT* script = RSSE_GetScriptPointer(*reinterpret_cast<int32_t*>(thing + 0x20));
            if (RSSE_Getvar(script, 0) == 0) {
                uint32_t a = static_cast<uint16_t>(thing[4] + (static_cast<uint32_t>(thing[6]) << 7) + 1) - 1u;
                uint32_t b = static_cast<uint16_t>(self[4] + (static_cast<uint32_t>(self[6]) << 7) + 1) - 1u;
                int32_t dx = static_cast<int32_t>(std::labs(static_cast<int32_t>((a & 0x7fu) - (b & 0x7fu))));
                int32_t dy = static_cast<int32_t>(std::labs(static_cast<int32_t>((a >> 7) - (b >> 7))));
                uint32_t distance = static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
                if (distance <= 4u && distance < bestDistance) {
                    best = current;
                    bestDistance = distance;
                }
            }
        }
        current = H(thing, 0xc);
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = best;
    return result;
}
