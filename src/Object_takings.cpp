// A ride's takings, costs, prizes and satisfaction (main app, Object.cpp range; unit w198): the 29 CObject members between 0x10107f50 and
// 0x101105f4 that count money and customers (GetObjectAge 0x10107f50, GetRemainingLife 0x10108024, WalkedAway 0x1010eec4, HadCustomer 0x1010ef0c,
// RecordNewTakings 0x1010ef54, RecordACost 0x1010f34c, HadAWinner 0x1010f4b8, GetPricePerUse 0x1010f4f4, GetPrizeValue 0x1010f52c, SetPrizeValue
// 0x1010f564, GetProspectiveCostOfGoods 0x1010f598, GetCostOfGoods 0x1010f6ec, GetTotalRevenue 0x1010f894, GetProfitability 0x1010f8d4,
// ExpressSatisfaction 0x1010f974, GetCustomerSatisfaction 0x1010f9d0, GetPercentageOfWinners 0x1010fa40, GetProductQuality 0x1010fb38,
// SetProductQuality 0x1010fb78, GetChanceOfWinning 0x1010fbb8, SetChanceOfWinning 0x1010fbf8, SetAmountOfSpecialIngredient 0x1010fc38,
// GetAmountOfSpecialIngredient 0x1010fc7c, GetSpecialIngredientName 0x1010fcc4, GetResidualValuePercentage 0x1010fd98, GetCurrentResidualValue
// 0x1010ffc8, PersonUsedToilet 0x10110070, WillIWin 0x101103e0 and SetPricePerUse 0x101105f4). src/Object.cpp, src/Object_turn.cpp,
// src/Object_queries.cpp and src/Object_queue.cpp hold other CObject members. The meanings of the fields are mine (the header says which).
#include <cstdint>

#include "Debug.h"
#include "FS_Online.h"
#include "History.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbTimeDiff.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {
constexpr int32_t kHistoryCount = 30;                       // every average and sum here asks for the last 30
constexpr int64_t kTicksPerDay = 864000000000LL;            // 100 ns ticks (src/Staff_queries.cpp)
constexpr int64_t kTicksPerMonth = 30 * kTicksPerDay;       // 0x1792f8648000: a month is 30 days here
constexpr int64_t kTicksPerYear = 365 * kTicksPerDay;       // 0x11ed178c6c000
constexpr int32_t kChallengeShopTakings = 12;               // the challenge counters AddToCounter is given (values from the listings)
constexpr int32_t kChallengeSideshowTakings = 13;
constexpr uint32_t kUpgradeStride = 16;                     // ride record slots between two upgrade levels
constexpr uint16_t kRegionEffectorDirty = 6;                // the effector a dirty toilet adds (no name in RegionEffector yet)
constexpr uint32_t kTextTableIndex = 4;                     // gText.mResourcePointers[4], the Mac's gText + 0x55c

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ObjectIdOf(uint16_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag;
    tag.mIndex = id;
    return tag;
}

// The ride record of the object's type (every use calls GetRideInfo again, like the original).
RideInfo* RecordOf(const CObject* object) { return gRideDatabase.GetRideInfo(ObjectIdOf(object->mObjectType)); }

// The cell the effectors are added to: the object's own cell as a 1 based map id.
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellOf(const CObject* object) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(object->mBase.mCellX + object->mBase.mCellY * 0x80 + 1);
    return tag;
}

// The null ids every AddToCounter call is given (the Mac loads two zero halfwords from the stack as whole words).
void AddToCounter(ChallengeType type, int32_t amount) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> noObject;
    noObject.mIndex = 0;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> noThing;
    noThing.mIndex = 0;
    gWorld->GetChallengeHQ()->AddToCounter(type, amount, noObject, noThing);
}

// The time a ride has been built, as the world's funny time minus the build stamp (the object is read as a CWorldSavvy: GetWorld ignores `this`).
int64_t BuiltFor(const CObject* object) {
    static_assert(sizeof(TbTimeStamp) == sizeof(object->mBuilt), "a time stamp is two words");
    const CWorldSavvy* savvy = reinterpret_cast<const CWorldSavvy*>(object);
    TbTimeStamp now = savvy->GetWorld().mParkTime.GetFunnyTime(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    // The original passes the member's own address (the stamp is read in place).
    TbTimeDiff span = now - *reinterpret_cast<const TbTimeStamp*>(object->mBuilt);
    return span.mTicks;
}

// The sum of the last 30 of a history, converted the way __cvt_fp2unsigned does.
uint32_t SumOf(const CHistory<uint32_t, 30>& history) {
    double sum;
    history.GetSumOfLast(sum, kHistoryCount);
    return cw_cvt_fp2unsigned(sum);
}

// Dirty: the flag for "provides relief" is set and the cleanliness, truncated to a byte, is below 25.
bool IsDirtyNow(const CObject* object) {
    if ((object->mFlags & 1) == 0) return false;
    const float cleanliness = static_cast<float>(static_cast<uint8_t>(ppc_fctiwz(static_cast<double>(object->mF40))));
    return cleanliness < 25.0f;
}

// Both cost-of-goods functions: each term is (value - 50) / 200 clamped to [-0.5, 0.5] (the compares are ordered ones that let a NaN through), the
// ingredient term changes sign for the record's special ingredients 1 and 3, and the cost scales the stored prize word by (1 + quality) + ingredient.
float ClampTerm(float term) {
    if (term < -0.5f) return -0.5f;
    if (term > 0.5f) return 0.5f;
    return term;
}

int32_t CostOfGoods(const CObject* object, float quality, float ingredient) {
    const uint32_t goods = object->mCostOfGoods;
    const float qualityTerm = ClampTerm((quality - 50.0f) / 200.0f);
    float ingredientTerm = ClampTerm((ingredient - 50.0f) / 200.0f);
    const int32_t special = RecordOf(object)->Value(kRideSlot_SpecialIngredient).mInteger;
    if (special == 1 || special == 3) ingredientTerm = ingredientTerm * -1.0f;
    const float scaled = static_cast<float>(goods) * ((1.0f + qualityTerm) + ingredientTerm);
    return ppc_fctiwz(static_cast<double>(scaled));
}

// The ride record's scrap value of one year, for the upgrade level the ride is at (the slot of level 0 plus 16 slots per level).
int32_t ScrapValue(const RideInfo* record, RideBalanceSlot year, uint8_t level) {
    return record->mBalance.mValues[year + kUpgradeStride * level].mInteger;
}
}  // namespace

// The whole days since the ride was built (the 64 bit span divided by a day, the low word kept).
int32_t CObject::GetObjectAge() const { return static_cast<int32_t>(BuiltFor(this) / kTicksPerDay); }

// mF44 (the remaining life) truncated.
int32_t CObject::GetRemainingLife() const { return ppc_fctiwz(static_cast<double>(mF44)); }

// One customer walked away: the total and the month's running count.
void CObject::WalkedAway() {
    mF1A0 += 1;
    mHistory22C.mTemp += 1;
}

// One customer used the object: the total and the month's running count.
void CObject::HadCustomer() {
    mF19C += 1;
    mHistory1A4.mTemp += 1;
}

// Money came in: deposited, counted in the takings and in the challenge counters of the ride's kind.
void CObject::RecordNewTakings(uint32_t amount) {
    gWorld->GetBankAccount()->MakeDeposit(amount);
    mHistory6C.mTemp += amount;
    mF17C += amount;
    if (RecordOf(this)->mUIType != kUIObjShop) {
        if (RecordOf(this)->mUIType != kUIObjSideshow) return;
        gWorld->GetParkAnalyser()->mHistory1F100[8].mTemp += static_cast<int32_t>(amount);
        AddToCounter(static_cast<ChallengeType>(kChallengeSideshowTakings), static_cast<int32_t>(amount));
        return;
    }
    gWorld->GetParkAnalyser()->mHistory1F100[7].mTemp += static_cast<int32_t>(amount);
    AddToCounter(static_cast<ChallengeType>(kChallengeShopTakings), static_cast<int32_t>(amount));
    // A shop also counts one sale of its kind: the shop type picks the counter, types 2 and 4 look at the special ingredient.
    const int32_t shopType = RecordOf(this)->Value(kRideSlot_ShopType).mInteger;
    int32_t counter = 0;
    switch (shopType) {
        case 1: counter = 5; break;
        case 3: counter = 8; break;
        case 5: counter = 7; break;
        case 6: counter = 6; break;
        case 2:
        case 4:
            switch (RecordOf(this)->Value(kRideSlot_SpecialIngredient).mInteger) {
                case 1: counter = 2; break;
                case 2: counter = 1; break;
                case 3: counter = 3; break;
                case 4: counter = 4; break;
                default: break;
            }
            break;
        default: break;
    }
    if (counter != 0) AddToCounter(static_cast<ChallengeType>(counter), 1);
}

// Money went out: counted in the costs, withdrawn and taken off the challenge counter of the ride's kind.
void CObject::RecordACost(uint32_t amount) {
    mHistoryF4.mTemp += amount;
    mF180 += amount;
    gWorld->GetBankAccount()->MakeWithdrawal(amount);
    if (RecordOf(this)->mUIType == kUIObjShop) {
        AddToCounter(static_cast<ChallengeType>(kChallengeShopTakings), static_cast<int32_t>(0u - amount));
        return;
    }
    if (RecordOf(this)->mUIType == kUIObjSideshow) AddToCounter(static_cast<ChallengeType>(kChallengeSideshowTakings), static_cast<int32_t>(0u - amount));
}

// A sideshow customer won.
void CObject::HadAWinner() { mHistory2B4.mTemp += 1; }

uint32_t CObject::GetPricePerUse() const { return mPricePerUse; }

uint32_t CObject::GetPrizeValue() const { return mCostOfGoods; }

void CObject::SetPrizeValue(int32_t value) { mCostOfGoods = static_cast<uint32_t>(value); }

// The cost of goods for a given quality and ingredient amount.
int32_t CObject::GetProspectiveCostOfGoods(float quality, float ingredient) const { return CostOfGoods(this, quality, ingredient); }

// The cost of goods for the ride's own quality and ingredient amount (the amount is asked first).
int32_t CObject::GetCostOfGoods() const {
    const float ingredient = static_cast<float>(GetAmountOfSpecialIngredient());
    const float quality = static_cast<float>(GetProductQuality());
    return CostOfGoods(this, quality, ingredient);
}

int32_t CObject::GetTotalRevenue() const { return static_cast<int32_t>(mF17C - mF180); }

// The last 30 months' takings minus the last 30 months' costs (the costs are summed first; the doubles are subtracted, then truncated).
int32_t CObject::GetProfitability() const {
    double costs;
    mHistoryF4.GetSumOfLast(costs, kHistoryCount);
    double takings;
    mHistory6C.GetSumOfLast(takings, kHistoryCount);
    return ppc_fctiwz(takings - costs);
}

// The first value is kept; later ones are averaged with the one before (signed, rounded toward zero).
void CObject::ExpressSatisfaction(int32_t value) {
    if (mHistory33C.mTemp == 0) {
        mHistory33C.mTemp = value;
        return;
    }
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(mHistory33C.mTemp) + static_cast<uint32_t>(value));
    mHistory33C.mTemp = sum / 2;
}

int32_t CObject::GetCustomerSatisfaction() const {
    int32_t average;
    mHistory33C.GetAverageOfLast(average, kHistoryCount);
    return average + 50;
}

// Winners as a percentage of the customers over the last 30 months; 0 when no customer was counted. (The Mac converts the customer sum twice
// in a row before the compare; the first conversion is dead, and it sums the customers a second time for the divisor.)
uint32_t CObject::GetPercentageOfWinners() const {
    if (SumOf(mHistory1A4) == 0) return 0;
    const uint32_t winners = SumOf(mHistory2B4);
    const uint32_t customers = SumOf(mHistory1A4);
    return (winners * 100u) / customers;
}

uint8_t CObject::GetProductQuality() const { return static_cast<uint8_t>(mF188); }

void CObject::SetProductQuality(uint8_t quality) { mF188 = quality; }

uint8_t CObject::GetChanceOfWinning() const { return static_cast<uint8_t>(mChanceOfNotLoosing); }

void CObject::SetChanceOfWinning(uint8_t chance) { mChanceOfNotLoosing = chance; }

void CObject::SetAmountOfSpecialIngredient(int32_t amount) { mF194 = static_cast<uint32_t>(amount); }

uint8_t CObject::GetAmountOfSpecialIngredient() const { return static_cast<uint8_t>(mF194); }

// The name of the record's special ingredient (ingredients 1 to 4 are strings 0 to 3 of the table at gText + 0x55c); none gives NULL.
const unsigned char* CObject::GetSpecialIngredientName() const {
    uint32_t id;
    switch (RecordOf(this)->Value(kRideSlot_SpecialIngredient).mInteger) {
        case 1: id = 0; break;
        case 2: id = 1; break;
        case 3: id = 2; break;
        case 4: id = 3; break;
        default: return nullptr;
    }
    return static_cast<const TbITable*>(gText.mResourcePointers[kTextTableIndex])->GetConstString(id);
}

// How much of its price the ride is worth, by its age: the record's scrap value of the first year (the second year, the third, the fourth for
// every other span, a negative one too), at the ride's upgrade level. In the first month with no customer yet a ride is worth all of it.
int32_t CObject::GetResidualValuePercentage() const {
    const int64_t span = BuiltFor(this);
    const int32_t year = static_cast<int32_t>(span / kTicksPerYear);
    if (year == 2) return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear3, mF4C);
    if (year < 2) {
        if (year == 0) {
            const int32_t month = static_cast<int32_t>(span / kTicksPerMonth);
            if (month == 0 && mF19C == 0) {
                if (RecordOf(this)->Value(kRideSlot_Upgrades_ScrapValueYear1).mInteger > 0) return 100;
            }
            return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear1, mF4C);
        }
        if (year >= 0) return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear2, mF4C);
    }
    return ScrapValue(RecordOf(this), kRideSlot_Upgrades_ScrapValueYear4, mF4C);
}

// That percentage of the record's cost of the upgrade (element 0), unsigned.
uint32_t CObject::GetCurrentResidualValue() const {
    const RideInfo* record = RecordOf(this);
    const int32_t percentage = GetResidualValuePercentage();
    return static_cast<uint32_t>(static_cast<uint32_t>(percentage) * static_cast<uint32_t>(record->Value(kRideSlot_Upgrades_CostOfUpgrade).mInteger)) / 100u;
}

// A person used the toilet. An online game cleans a dirty one; any other game wears the cleanliness down by amount / 20 (clamped to 0..100) and,
// when that turns the toilet dirty, swaps its map effect.
void CObject::PersonUsedToilet(uint32_t amount) {
    if (TheGameType().mType == kGameTypeOnline) {
        if (!IsDirtyNow(this)) return;
        // What follows is CleanFully (0x1010bda8) inlined: it tests for dirt again.
        if (IsDirtyNow(this)) {
            gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(kRegionEffectorDirty), CellOf(this));
            gWorldMap->AddRegionalEffector(kRegionEffectorRelief, CellOf(this));
        }
        RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 8, 0);
        mF40 = 100.0f;
        mF5A = 0;
        mF60 = 0;
        return;
    }
    const bool wasDirty = IsDirtyNow(this);
    const float wear = static_cast<float>(amount) / 20.0f;
    const float cleaner = -wear + mF40;
    if (cleaner > 100.0f) mF40 = 100.0f;
    else if (cleaner < 0.0f) mF40 = 0.0f;
    else mF40 = mF40 - wear;
    if (IsDirtyNow(this) && !wasDirty) {
        CTRACE(2, "Toilet has become dirty and smelly\n");
        gWorldMap->RemoveRegionalEffector(kRegionEffectorRelief, CellOf(this));
        gWorldMap->AddRegionalEffector(static_cast<RegionEffector>(kRegionEffectorDirty), CellOf(this));
    }
}

// The sideshow's draw: a person wins when the world's random number (modulo 100) is not above the chance. Only sideshows may have a chance below 100.
bool CObject::WillIWin(CPerson*) const {
    const bool allowed = RecordOf(this)->mUIType == kUIObjSideshow || static_cast<uint8_t>(mChanceOfNotLoosing) == 100;
    GAME_ASSERTMSG(allowed, "Non-sideshow object number %d has <100%% chance of winning\n");
    const uint32_t chance = static_cast<uint8_t>(mChanceOfNotLoosing);
    const uint32_t roll = gGameWorld->GetRand() % 100u;
    const bool wins = chance >= roll;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 11, wins ? 1 : 0);
    return wins;
}

void CObject::SetPricePerUse(int32_t price) { mPricePerUse = static_cast<uint32_t>(price); }
