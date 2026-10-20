// The park analyser's staff queries (unit w155): HiredSomeone 0x100e4750, GetStaffHappinessLevel 0x100e4ebc, GetDaysSinceLastObjectBuilt 0x100e7098,
// GetDaysSinceLastStaffHired 0x100e7188, NumberOfStaff 0x100e8324, NumberOfStaffTooTiredToWork 0x100e84d4, NumberOfStaffTypeWithPatrolArea 0x100e86a4,
// AverageQualityOfStaffType 0x100e88a0, AverageHappinessOfStaffType 0x100e89fc, AverageHappinessOfStaff 0x100e8b64, CurrentNumberOfStaffTypeForHire
// 0x100e8d4c, CurrentResearcherAbility 0x100e8dd8 and MaxResearcherAbility 0x100e8ed8.
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"

namespace {
const char kEntryZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10240242
constexpr uint16_t kNoThing = 0;           // the end id of every walk: a halfword of initialised data (0x102a04da and the others), all 0
constexpr int32_t kFirstStaffKind = 4;     // the listings test 4, 5, 6, 8, 7 one after the other: 4 to 8
constexpr int32_t kLastStaffKind = 8;
constexpr uint32_t kStaffGradeOffset = 0x1e4;        // int32_t: the staff's grade (names mine)
constexpr uint32_t kStaffHappinessOffset = 0x1f4;    // float: the happiness, read as fctiwz(f) & 0xff
constexpr uint32_t kStaffNextResearcherOffset = 0x20c;   // uint16_t: the id of the next researcher of the world's list
constexpr int64_t kTicksPerDay = 864000000000LL;

template <class T>
T StaffField(const CThing* thing, uint32_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(thing) + offset, sizeof(T));
    return value;
}

bool IsStaffKind(int32_t kind) { return kind >= kFirstStaffKind && kind <= kLastStaffKind; }

// The happiness byte: fctiwz of the float, the low byte of the word.
uint32_t HappinessByte(const CThing* thing) {
    return static_cast<uint32_t>(ppc_fctiwz(StaffField<float>(thing, kStaffHappinessOffset))) & 0xff;
}

// The used list's walk (NumberOfStaffType's): per thing GetWorld, the assertion on row 0, the body, then GetWorld().GetNextUsedThing(thing).
template <class Body>
void WalkThings(const CParkAnalyser* analyser, Body body) {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    uint16_t cur = savvy->GetWorld().GetUsedThingHead().mIndex;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        CThing* const thing = static_cast<CThing*>(gThingArray[cur].mThing);
        body(thing);
        cur = savvy->GetWorld().GetNextUsedThing(thing).mIndex;
    }
}

// Days from `stamp` to the funny time of the world CWorldSavvy answers (0x10237e1c divides the 64 bit span by a day, the low word is kept).
int32_t DaysSince(const CParkAnalyser* analyser, const uint32_t (&stampWords)[2]) {
    static_assert(sizeof(TbTimeStamp) == sizeof(stampWords), "a time stamp is two words");
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(analyser);
    TbTimeStamp now = savvy->GetWorld().mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    // The original passes the member's own address (the stamp is read in place).
    TbTimeDiff span = now - *reinterpret_cast<const TbTimeStamp*>(stampWords);
    return static_cast<int32_t>(span.mTicks / kTicksPerDay);
}
}  // namespace

// The park's funny time of the GLOBAL world (gWorld, not GetWorld) is the time the last staff member was hired.
void CParkAnalyser::HiredSomeone() {
    TbTimeStamp stamp = gWorld->mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    static_assert(sizeof(stamp) == sizeof(mLastStaffHired), "a time stamp is two words");
    std::memcpy(mLastStaffHired, &stamp, sizeof(mLastStaffHired));
}

// The argument is not used. The sum of the happiness bytes is carried through single precision: each step converts the sum to a float, adds
// the byte as a float and truncates back (so a sum above 2^24 loses its low bits). The answer is sum / count as floats, 0.0f when no staff.
float CParkAnalyser::GetStaffHappinessLevel(uint32_t) const {
    int32_t sum = 0;
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind)) {
            const uint32_t byte = HappinessByte(thing);
            count++;
            sum = ppc_fctiwz(static_cast<float>(static_cast<float>(sum) + static_cast<float>(byte)));
        }
    });
    if (count > 0) return static_cast<float>(sum) / static_cast<float>(count);
    return 0.0f;
}

int32_t CParkAnalyser::GetDaysSinceLastObjectBuilt() const { return DaysSince(this, mLastObjectBuilt); }

int32_t CParkAnalyser::GetDaysSinceLastStaffHired() const { return DaysSince(this, mLastStaffHired); }

// Dead staff count too (no mDead test, unlike NumberOfStaffType).
int32_t CParkAnalyser::NumberOfStaff() const {
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind)) count++;
    });
    return count;
}

int32_t CParkAnalyser::NumberOfStaffTooTiredToWork() const {
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind) && reinterpret_cast<const CStaff*>(thing)->TooTiredToWork() != 0) count++;
    });
    return count;
}

// The model is compared as a signed word with the kind byte; 0 means any model.
int32_t CParkAnalyser::NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const {
    int32_t count = 0;
    WalkThings(this, [&](CThing* thing) {
        const int32_t kind = thing->mKind;
        if (IsStaffKind(kind) && (static_cast<int32_t>(model) == kind || static_cast<int32_t>(model) == 0) &&
            reinterpret_cast<const CStaff*>(thing)->HasPatrolArea() != 0) count++;
    });
    return count;
}

// No staff test: any thing whose kind byte equals the model. 32 bit wrap in the products, a signed divide.
int32_t CParkAnalyser::AverageQualityOfStaffType(CThing::ThingModel model) const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing) {
        if (static_cast<int32_t>(model) == static_cast<int32_t>(thing->mKind)) {
            count++;
            sum += static_cast<uint32_t>(StaffField<int32_t>(thing, kStaffGradeOffset));
        }
    });
    if (count == 0) return 0;
    const int32_t numerator = static_cast<int32_t>(sum * 100u);
    const int32_t denominator = static_cast<int32_t>(static_cast<uint32_t>(count) * 5u);
    return numerator / denominator;
}

int32_t CParkAnalyser::AverageHappinessOfStaffType(CThing::ThingModel model) const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing) {
        if (static_cast<int32_t>(model) == static_cast<int32_t>(thing->mKind)) {
            count++;
            sum += HappinessByte(thing);
        }
    });
    if (count == 0) return 0;
    return static_cast<int32_t>(sum) / count;
}

int32_t CParkAnalyser::AverageHappinessOfStaff() const {
    int32_t count = 0;
    uint32_t sum = 0;
    WalkThings(this, [&](CThing* thing) {
        if (IsStaffKind(thing->mKind)) {
            count++;
            sum += HappinessByte(thing);
        }
    });
    if (count == 0) return 0;
    return static_cast<int32_t>(sum) / count;
}

// The type is a 32 bit enum on the Mac; the database's declaration takes an int32_t.
int32_t CParkAnalyser::CurrentNumberOfStaffTypeForHire(StaffType type) const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    return savvy->GetWorld().mStaffDB.GetNumStaffOfType(static_cast<int32_t>(type));
}

// The world's list of researchers (the head halfword at +0x1da742, the link at +0x20c of each): the balance's ResearchAbility of each one's grade.
int32_t CParkAnalyser::CurrentResearcherAbility() const {
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(this);
    uint32_t sum = 0;
    uint16_t cur = savvy->GetWorld().mF1DA742;
    while (cur != kNoThing) {
        savvy->GetWorld();
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kEntryZeroText);
        const CThing* const thing = static_cast<const CThing*>(gThingArray[cur].mThing);
        const int32_t grade = StaffField<int32_t>(thing, kStaffGradeOffset);
        const uint16_t next = StaffField<uint16_t>(thing, kStaffNextResearcherOffset);
        const int32_t slot = kMainBalanceResearchAbility + static_cast<int32_t>(static_cast<uint32_t>(grade) * 3u);
        sum += static_cast<uint32_t>(gMainBalance.mValues[slot].mInteger);
        cur = next;
    }
    return static_cast<int32_t>(sum);
}

// Slot 172 times grade 4's research ability (slot 273), 32 bit wrap.
int32_t CParkAnalyser::MaxResearcherAbility() const {
    const uint32_t a = static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceMaxResearchersInPark].mInteger);
    const uint32_t b = static_cast<uint32_t>(gMainBalance.mValues[kMainBalanceResearchAbility + kMainBalanceResearcherGradeStride * 4].mInteger);
    return static_cast<int32_t>(a * b);
}
