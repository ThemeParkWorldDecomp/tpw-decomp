// CPerson::CalculateOptionScore 0x101185f8 (2,612 bytes): how much the person wants to go on a ride, 0 to a few hundred.
#include <cstdint>
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "RideDB.h"
#include "ppc_math.h"

// Names invented, zero at start (declared in ThingReceivers.h).
uint16_t gNoBackOfQueue = 0;                 // 0x102a0fac
uint16_t gNoThingRide = 0;                   // 0x102a0fae
uint8_t gNeedMatchTable[0x79] = {};          // 0x102a0f1e
uint8_t gAmountMatchTable[0x15] = {};        // 0x102a0f97

namespace {

// The texts of the original sit in one pool at 0x102442be; the offsets are in the comments.
const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x000
const char kNoBoqText[] = "Calculating option score for object with no BOQ\n";                       // +0x228
const char kHeadText[] = "Person %d: COS on %ls %d:\n";                                               // +0x259
const char kDistFireText[] = "\t- dist (inc. nearby fireworks): %d\n";                                // +0x274
const char kDistText[] = "\t- dist: %d\n";                                                            // +0x299
const char kCloseQueueText[] = "\t- close to queue, q_match=%d\n";                                    // +0x2a6
const char kFarQueueText[] = "\t- far from queue, q_match=0\n";                                       // +0x2c5
const char kExText[] = "\t- base ex_diff=%d, base ex_match=%d, es=%d\n";                              // +0x2e3
const char kThirstText[] = "\t- thirst=%d, effect=%d, thirst_match=%d\n";                             // +0x310
const char kHungerText[] = "\t- hunger=%d, effect=%d, hunger_match=%d\n";                             // +0x33a
const char kToiletText[] = "\t- toilet=%d, effect=%d, toilet_match=%d\n";                             // +0x364
const char kIllnessText[] = "\t- illness=%d, effect=%d, illness_match=%d\n";                          // +0x38e
const char kBaseText[] = "\t- Base score = %d";                                                       // +0x3ba
const char kNewRideText[] = "\t- new ride, increasing score to %d\n";                                 // +0x3cd
const char kRainText[] = "\t- indoor ride in the rain, increasing score to %d\n";                     // +0x3f2
const char kGtText[] = "\t- GT-only ride, increasing score to %d\n";                                  // +0x426
const char kPriceText[] = "\t- expensive ride, increasing score to %d\n";                             // +0x44f
const char kUsedText[] = "\t- score after examining used-ride history=%d\n";                          // +0x47a
const char kTempText[] = "\t- score after examining temp history=%d\n";                               // +0x4a9
const char kFinalText[] = "\t- FINAL SCORE for ride %d = %d\n";                                       // +0x4d3

inline uint8_t* Bytes(void* p) { return static_cast<uint8_t*>(p); }
inline uint16_t Half(const uint8_t* base, uint32_t off) { return *reinterpret_cast<const uint16_t*>(base + off); }
inline uint32_t Word(const uint8_t* base, uint32_t off) { return *reinterpret_cast<const uint32_t*>(base + off); }
inline float Flt(const uint8_t* base, uint32_t off) { return *reinterpret_cast<const float*>(base + off); }

// gMainBalance's word at the Mac offset 8 + 4 * slot.
inline int32_t Bal(int32_t slot) { return gMainBalance.mValues[slot].mInteger; }

// The ride record's value at the Mac offset 0x0c + 4 * slot.
inline int32_t DbWord(const RideInfo* db, int32_t slot) { return db->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }
constexpr int32_t kSlot144 = 78, kSlot148 = 79, kSlot114 = 66, kSlotC4 = 46, kSlot1b8 = 107;

// mulhwu-and-shift division by ten, as the compiler wrote it: (x * 0xcccccccd >> 32) >> 3, kept to a byte.
inline uint32_t TenthByte(uint32_t x) { return static_cast<uint32_t>((static_cast<uint64_t>(0xcccccccdu) * x) >> 32 >> 3) & 0xffu; }

}  // namespace

// 0x101185f8
uint32_t CPerson::CalculateOptionScore(CObject* object) {
    uint8_t* self = Bytes(this);
    uint8_t* obj = Bytes(object);

    uint16_t queueA = object->GetBackOfQueue().mIndex;
    GAME_ASSERTMSG(static_cast<int32_t>((static_cast<uint32_t>(gNoBackOfQueue) - queueA) != 0 ? 1 : 0), kNoBoqText);
    uint16_t queueB = object->GetBackOfQueue().mIndex;
    int32_t rideX = static_cast<int32_t>((static_cast<uint32_t>(queueB) - 1u) & 0x7fu);
    uint16_t queueC = object->GetBackOfQueue().mIndex;
    uint16_t myCell = static_cast<uint16_t>(self[4] + (static_cast<uint32_t>(self[6]) << 7) + 1);
    int32_t rideY = static_cast<int16_t>((static_cast<uint32_t>(queueC) - 1u) >> 7);
    uint32_t myX = (static_cast<uint32_t>(myCell) - 1u) & 0x7fu;
    uint32_t myY = (static_cast<uint32_t>(myCell) - 1u) >> 7;
    uint32_t wDist = Bal(26) & 0xffff;
    uint32_t wToilet0 = static_cast<uint32_t>(Bal(29)) & 0xffff;   // thirst weight
    uint32_t wHunger = static_cast<uint32_t>(Bal(30)) & 0xffff;
    uint32_t wToilet = static_cast<uint32_t>(Bal(31)) & 0xffff;
    uint32_t wIllness = static_cast<uint32_t>(Bal(32)) & 0xffff;

    uint16_t objectId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
    uint16_t personId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    const uint8_t* db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    CTRACE(7, kHeadText, static_cast<uint32_t>(personId), reinterpret_cast<const RideInfo*>(db)->mItemName, static_cast<uint32_t>(objectId));

    if (Half(self, 0x1e0) != gNoThingRide) {
        uint16_t index = Half(self, 0x1e0);
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZeroText);
        const uint8_t* thing = static_cast<const uint8_t*>(gThingArray[index].mThing);
        if (thing != nullptr) {
            uint16_t type = Half(thing, 0xe);
            if (type == Half(obj, 0xe)) return 0;
        }
    }

    int32_t dx = static_cast<int32_t>(myX) - rideX;
    int32_t dy = static_cast<int32_t>(myY) - rideY;
    int32_t distA = dx * dx;
    int32_t distB = dy * dy;
    int32_t scaled = (distA + distB) * 100;
    uint32_t dist = static_cast<uint32_t>(scaled / 450);   // the compiler's multiply-high form of a signed divide by 450
    if (dist > 100u) dist = 100u;
    uint32_t distScore = 100u - dist;
    uint16_t cellId = static_cast<uint16_t>(rideX + (rideY << 7) + 1);
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellTag;
    cellTag.mIndex = cellId;
    CRegionEffectCell* effectCell = gWorldMap->GetRegionalEffectCell(cellTag);
    int32_t effect = effectCell->mpEffect[4];
    if (effect != 0) {
        distScore = distScore / static_cast<uint32_t>(effect);
        CTRACE(7, kDistFireText, distScore);
    } else {
        CTRACE(7, kDistText, distScore);
    }

    uint32_t queueWeight;
    uint32_t queueMatch;
    if (distA + distB <= 8) {
        uint32_t capacity = Word(obj, 0x3c);
        if (capacity == 0) capacity = 1;
        uint32_t divisor = capacity << 2;
        int32_t waiting = object->GetNumPeopleInQueue();
        queueWeight = static_cast<uint32_t>(Bal(27));
        uint32_t share = divisor == 0 ? 0u : (static_cast<uint32_t>(waiting) * 100u) / divisor;   // divwu by 0 answers 0 on the emulator
        queueMatch = 100u - share;
        CTRACE(7, kCloseQueueText, queueMatch);
    } else {
        queueMatch = 0;
        queueWeight = 0;
        CTRACE(7, kFarQueueText);
    }

    uint32_t preferred = this->GetPreferredExcitement();
    uint32_t level = object->GetExcitementLevel(false);
    int32_t exDiff = static_cast<int32_t>((preferred & 0xffu) - (level & 0xffu));
    if (exDiff < 0) exDiff = -exDiff;
    if (exDiff < 0) exDiff = 0;
    else if (exDiff > 0x32) exDiff = 0x32;
    int32_t exMatch = (0x32 - exDiff) * 2;
    uint32_t exWeight;
    if ((object->GetExcitementLevel(true) & 0xffu) == 0) exWeight = 0;
    else exWeight = static_cast<uint32_t>(Bal(28)) & 0xffff;
    CTRACE(700, kExText, exDiff, exMatch, exWeight);

    // thirst
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    int32_t thirstAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1a4)));
    uint32_t needWord = static_cast<uint32_t>(DbWord(reinterpret_cast<const RideInfo*>(db), kSlot144));
    uint32_t thirstMatch = gNeedMatchTable[TenthByte(needWord & 0xffu) * 11u + TenthByte(static_cast<uint32_t>(thirstAmount))];
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    CTRACE(7, kThirstText, static_cast<uint32_t>(thirstAmount) & 0xffu, DbWord(reinterpret_cast<const RideInfo*>(db), kSlot144), thirstMatch);

    // hunger
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    int32_t hungerAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1a8)));
    uint32_t hungerWord = static_cast<uint32_t>(DbWord(reinterpret_cast<const RideInfo*>(db), kSlot148));
    uint32_t hungerMatch = gNeedMatchTable[TenthByte(hungerWord & 0xffu) * 11u + TenthByte(static_cast<uint32_t>(hungerAmount))];
    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    CTRACE(7, kHungerText, static_cast<uint32_t>(hungerAmount) & 0xffu, DbWord(reinterpret_cast<const RideInfo*>(db), kSlot148), hungerMatch);

    // toilet
    bool indoor = (Half(obj, 0x2e) & 1u) != 0;
    int32_t toiletAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1ac)));
    uint32_t toiletMatch = indoor ? gAmountMatchTable[((static_cast<uint32_t>(toiletAmount) & 0xffu) + 4u) / 5u] : 0u;
    CTRACE(7, kToiletText, static_cast<uint32_t>(toiletAmount) & 0xffu, indoor ? 100 : 0, toiletMatch);

    // illness
    indoor = (Half(obj, 0x2e) & 1u) != 0;
    int32_t illnessAmount = ppc_fctiwz(static_cast<double>(Flt(self, 0x1b0)));
    uint32_t illnessMatch = indoor ? gAmountMatchTable[((static_cast<uint32_t>(illnessAmount) & 0xffu) + 4u) / 5u] : 0u;
    CTRACE(7, kIllnessText, static_cast<uint32_t>(illnessAmount) & 0xffu, indoor ? 100 : 0, illnessMatch);

    uint32_t sumWeights = wDist + queueWeight + exWeight + wToilet0 + wHunger + wToilet + wIllness;
    uint32_t sumScores = wDist * distScore + queueWeight * queueMatch + exWeight * static_cast<uint32_t>(exMatch) + wToilet0 * thirstMatch
                         + wHunger * hungerMatch + wToilet * toiletMatch + wIllness * illnessMatch;
    // (the sum is formed in the original's order; the wrap-around is the same either way)
    uint32_t scoreU = sumWeights == 0 ? 0u : sumScores / sumWeights;
    int32_t score = static_cast<int32_t>(scoreU);
    CTRACE(7, kBaseText, scoreU);

    uint32_t age = static_cast<uint32_t>(object->GetObjectAge());
    if (age <= static_cast<uint32_t>(Bal(18))) {
        score = score * Bal(19);
        CTRACE(7, kNewRideText, static_cast<uint32_t>(score));
    }

    CWeather* weather = gWorld->GetWeather();
    if (static_cast<int32_t>(weather->mF30) > 0) {
        db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
        if (DbWord(reinterpret_cast<const RideInfo*>(db), kSlot114) != 0) {
            score = score * Bal(20);
            CTRACE(7, kRainText, static_cast<uint32_t>(score));
        }
    }

    db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
    if (DbWord(reinterpret_cast<const RideInfo*>(db), kSlotC4) > 0) {
        db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
        int32_t extra = DbWord(reinterpret_cast<const RideInfo*>(db), kSlotC4);
        double scoreD = static_cast<double>(score);
        double extraD = static_cast<double>(static_cast<int32_t>(static_cast<uint32_t>(extra) + 1u));
        score = ppc_fctiwz(scoreD * (1.0 + extraD / 10.0));
        CTRACE(7, kGtText, static_cast<uint32_t>(score));
    } else {
        db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
        if (DbWord(reinterpret_cast<const RideInfo*>(db), kSlot1b8) != 0) {
            db = reinterpret_cast<const uint8_t*>(object->GetDBInfo());
            int32_t price = DbWord(reinterpret_cast<const RideInfo*>(db), kSlot1b8);
            float ratio = static_cast<float>(static_cast<double>(price)) / 3000.0f;
            if (static_cast<double>(ratio) > 1.0) {
                float factor = 1.0f + ratio / 10.0f;
                float scoreF = static_cast<float>(static_cast<double>(score));
                score = ppc_fctiwz(static_cast<double>(scoreF * factor));
                CTRACE(7, kPriceText, static_cast<uint32_t>(score));
            }
        }
    }

    // the rides the person already used, then the recent ones: each match divides the score
    if (Half(self, 0x1e0) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 5;
    } else if (Half(self, 0x1e2) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 4;
    } else if (Half(self, 0x1e4) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 3;
    } else if (Half(self, 0x1e6) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) {
        score = score / 2;
    }
    CTRACE(7, kUsedText, static_cast<uint32_t>(score));

    if (Half(self, 0x1e8) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 5;
    if (Half(self, 0x1ea) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 4;
    if (Half(self, 0x1ec) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 3;
    if (Half(self, 0x1ee) == reinterpret_cast<CThing*>(object)->GetThingId().mIndex) score = score / 2;
    CTRACE(7, kTempText, static_cast<uint32_t>(score));

    uint16_t finalId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
    CTRACE(7, kFinalText, static_cast<uint32_t>(score), static_cast<uint32_t>(finalId));
    return static_cast<uint32_t>(score);
}
