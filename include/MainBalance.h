// MainBalance.cpp: main app CMainBalance. Original offsets are PowerPC offsets.
#pragma once
#include "CBalanceTable.h"

class CMainBalance : public CBalanceTable {
public:
    CMainBalance() { Init(); }  // inline in 0x10021258
    ~CMainBalance();  // 0x1002136c, nonvirtual
    const BalanceDescription* GetBalanceDescription(int32_t row) override; // 0x100212dc
    BalanceValue* GetBalanceValuesStart() override; // 0x10021328
    BalanceValue mValues[504];  // 0x08, includes reserved slot zero
};
extern const BalanceDescription gMainBalanceDescriptions[284]; // 0x10294a00
// The NumSkyObjects count (the array group's size slot of the sky objects): the word at 0x6e4 of the Mac's gMainBalance, value slot (0x6e4 - 8) / 4.
constexpr int kMainBalanceNumSkyObjects = 439;
// The value slots CGame::InitLevel_Initialise (unit w117) reads: slot = (Mac offset - 8) / 4. The integer ones are `.mInteger`, the six
// floats `.mFloat`. The names are the description rows' (group names precede their members).
constexpr int kMainBalanceQueueCell = 365;               // Costs.QueueCell +0x5bc
constexpr int kMainBalancePathCell = 366;                // .PathCell +0x5c0
constexpr int kMainBalanceKartTrackCell = 367;           // .KartTrackCell +0x5c4
constexpr int kMainBalanceWaterTrackCell = 368;          // .WaterTrackCell +0x5c8
constexpr int kMainBalanceFogColour = 419;               // ThemeEngine.FogColour +0x694
constexpr int kMainBalanceAmbientLightLevel = 420;       // .AmbientLightLevel +0x698
constexpr int kMainBalanceDirectionalLightLevel = 421;   // .DirectionalLightLevel +0x69c
constexpr int kMainBalanceProcTexXOffset = 422;          // .ProceduralTexturingXOffset +0x6a0
constexpr int kMainBalanceProcTexYOffset = 423;          // .ProceduralTexturingYOffset +0x6a4
constexpr int kMainBalanceLightNormalX = 424;            // LightNormal.X +0x6a8 (Y 425, Z 426; floats)
constexpr int kMainBalanceCostumeAddOns = 427;           // ThemeAdvisorCostumes[i].AddOns +0x6b4 + 8i (slot 427 + 2i); .SubOffs is the next slot
constexpr int kMainBalanceWaterStartX = 436;             // WaterStartPos.X +0x6d8 (Y 437, Z 438; floats)
extern CMainBalance gMainBalance; // 0x102b4568, original symbol name not recovered

constexpr int kMainBalanceInitialCash = 100;   // InitialCash +0x198
constexpr int kMainBalanceInitialAdmissionFee = 101;   // +0x19c
constexpr int kMainBalanceLoanInfo = 102;   // +0x1a0: four slots per loan, first eight of ten used

// Unit w123a: Seasons slots, Mac +0x484/+0x488/+0x48c, stride 12 bytes.
constexpr int kMainBalanceAvgWeatherQuality = 287;
constexpr int kMainBalanceNormalTolerance = 288;
constexpr int kMainBalanceExceptionalWeatherChance = 289;
constexpr int kMainBalanceSeasonStride = 3;

// Unit w123b: the level's challenge rows, twenty signed row indices into challenges.sam's table (Mac +0x794 = (0x794 - 8) / 4). CChallengeHQ::Init visits
// all twenty and skips the ones <= 0; the array's own count (slot 503) is never read.
constexpr int kMainBalanceChallengesInThisLevel = 483;
constexpr int kMainBalanceChallengesInThisLevelCount = 20;

// Unit w124: the weather and challenge turn's slots (Mac offset - 8) / 4.
constexpr int kMainBalanceDaysOfWarning = 300;            // +0x4b8
constexpr int kMainBalanceDaysBetweenChanges = 301;       // +0x4bc
constexpr int kMainBalanceSpeedOfChange = 302;            // +0x4c0
constexpr int kMainBalanceLightningCountdownSpeed = 309;  // +0x4dc
constexpr int kMainBalanceLightningCountdownFrom = 310;   // +0x4e0
constexpr int kMainBalanceLightningTime = 311;            // +0x4e4
constexpr int kMainBalanceThunderStartsThisFarAway = 312; // +0x4e8
constexpr int kMainBalanceThunderRandomiser = 313;        // +0x4ec
constexpr int kMainBalanceMaxRaindrops = 314;             // +0x4f0
constexpr int kMainBalanceShortTimeLeftWarningAt = 482;   // +0x790

// Unit w129: the staff database's slots, (Mac offset - 8) / 4. CStaffDB::Update reads the five per-category staff targets (+0x280 to +0x290, one per
// staff type 0 to 4), the seconds between two top-ups of the pool (+0x2c4; the turns are four a second, so the limit is shifted left by 2) and the
// most staff made at once (+0x2c8). The names are inferred.
constexpr int kMainBalanceStaffTarget = 158;            // +0x280 (five slots, 158 to 162)
constexpr int kMainBalanceStaffTopUpInterval = 175;     // +0x2c4
constexpr int kMainBalanceStaffTopUpMax = 176;          // +0x2c8
// Unit w132: the slots the ride script system and the ride object read (Mac offset - 8) / 4.
constexpr int kMainBalanceDefaultReverb = 465;            // +0x74c  the reverb level RSSE_SetReverbOnEax falls back to
constexpr int kMainBalanceRideOffsetX = 339;              // +0x554  CObject::UpdateEngine's x when the ride record has none
constexpr int kMainBalanceRideOffsetZ = 340;              // +0x558  the same for z

// Unit w155 (names inferred from the rows MaxResearchersInPark and ResearcherConstsPerGrade of src/MainBalance.cpp): the researcher slots.
constexpr int kMainBalanceMaxResearchersInPark = 172;     // +0x2b8
constexpr int kMainBalanceResearchAbility = 261;          // +0x41c  ResearcherConstsPerGrade[grade].ResearchAbility
constexpr int kMainBalanceResearcherGradeStride = 3;      // 12 bytes a grade, five grades; slot 273 (+0x44c) is grade 4's
// Unit w188: the researcher's work time per grade (turns), read as mValues[slot + kMainBalanceResearcherGradeStride * grade].
constexpr int kMainBalanceResearcherWorkDuration = 260;   // +0x418  ResearcherConstsPerGrade[grade].WorkDuration

// Unit w178 (names inferred from the rows of src/MainBalance.cpp 107-124): the staff and entertainer constants per pay grade, 16 bytes a grade
// (four slots; grades 0 to 4). Each is read as mValues[slot + kMainBalanceStaffGradeStride * grade]. The listings' offsets win over the rows' own count
// for WorkDuration (the rows give 210; the code reads +0x358 = slot 212).
constexpr int kMainBalanceStaffIdleDuration = 186;               // +0x2f0  PerGradeStaffConsts[grade].IdleDuration
constexpr int kMainBalanceEntertainerWorkDuration = 212;         // +0x358  EntertainerConstsPerGrade[grade] (first member)
constexpr int kMainBalanceEntertainerActivationDistance = 214;   // +0x360  EntertainerConstsPerGrade[grade].ActivationDistance
constexpr int kMainBalanceStaffGradeStride = 4;                  // 16 bytes a grade, both tables

// Unit w182 (names inferred from the rows of src/MainBalance.cpp 140-142): GuardConstsPerGrade[grade], 8 bytes a grade (WorkDuration, PoundsPerTrainingPoint).
// Read as mValues[kMainBalanceGuardWorkDuration + kMainBalanceGuardGradeStride * grade]. ProcessIdle also reads kMainBalanceStaffIdleDuration (w178's).
constexpr int kMainBalanceGuardWorkDuration = 276;   // +0x458  GuardConstsPerGrade[grade].WorkDuration
constexpr int kMainBalanceGuardGradeStride = 2;      // 8 bytes a grade

// Unit w184 (names inferred; the rows are in src/MainBalance.cpp): the handyman's per-grade table, 12 bytes a grade, read as mValues[slot + kMainBalanceHandymanGradeStride * grade].
constexpr int kMainBalanceHandymanWorkDuration = 233;      // +0x3ac  how long he cleans litter or a toilet (turns)
constexpr int kMainBalanceHandymanDetectionRange = 234;    // +0x3b0  how far he looks for litter (cells) and the radius for a dirty toilet
constexpr int kMainBalanceHandymanGradeStride = 3;         // 12 bytes a grade

// Unit w187 (names inferred): MechanicConstsPerGrade[grade], 8 bytes a grade (WorkDuration, PoundsPerTrainingPoint).
constexpr int kMainBalanceMechanicWorkDuration = 249;   // +0x3ec  MechanicConstsPerGrade[grade].WorkDuration
constexpr int kMainBalanceMechanicGradeStride = 2;      // 8 bytes a grade

// Unit w190 (src/Staff_queries.cpp; names inferred from the rows of src/MainBalance.cpp): the staff pay and training slots, slot = (Mac offset - 8) / 4.
constexpr int kMainBalanceRestLevel = 183;                   // +0x2e4  the energy below which a staff member is too tired to work
constexpr int kMainBalanceBaseWage = 185;                    // +0x2ec  the wage of a grade, read as mValues[slot + kMainBalanceStaffGradeStride * grade]
constexpr int kMainBalancePayMultiplier = 206;               // +0x340  one slot a staff type (0 to 4)
constexpr int kMainBalanceEntertainerPoundsPerTrainingPoint = 215;   // +0x364  stride kMainBalanceEntertainerGradeStride
constexpr int kMainBalanceEntertainerGradeStride = 4;
constexpr int kMainBalanceHandymanPoundsPerTrainingPoint = 235;      // +0x3b4  stride kMainBalanceHandymanGradeStride
constexpr int kMainBalanceMechanicPoundsPerTrainingPoint = 250;      // +0x3f0  stride kMainBalanceMechanicGradeStride
constexpr int kMainBalanceResearcherPoundsPerTrainingPoint = 262;    // +0x420  stride kMainBalanceResearcherGradeStride
constexpr int kMainBalanceGuardPoundsPerTrainingPoint = 277;         // +0x45c  stride kMainBalanceGuardGradeStride

// Unit w191 (src/Staff_behaviour.cpp; names inferred from the rows of src/MainBalance.cpp 107-114 and 197-212), slot = (Mac offset - 8) / 4.
constexpr int kMainBalanceHappyHitCosNoRestArea = 184;            // +0x2e8  (integer) the happiness a staff member loses when he finds no rest area
constexpr int kMainBalanceStaffRecuperationRate = 187;            // +0x2f4  (FLOAT) PerGradeStaffConsts[grade], stride kMainBalanceStaffGradeStride: energy a turn at rest
constexpr int kMainBalanceStaffHappinessRecuperationRate = 188;   // +0x2f8  (FLOAT) the same, happiness a turn at rest
// The FixedItemInfo rows: the entrance A position is 12 slots before StrikeAreaStartX, counted back through the Crossing and Entrance rows.
constexpr int kMainBalanceEntranceAPosX = 349;                    // +0x57c
constexpr int kMainBalanceEntranceAPosY = 350;                    // +0x580
constexpr int kMainBalanceStrikeAreaStartX = 361;                 // +0x5ac
constexpr int kMainBalanceStrikeAreaStartY = 362;                 // +0x5b0
constexpr int kMainBalanceStrikeAreaSizeX = 363;                  // +0x5b4
constexpr int kMainBalanceStrikeAreaSizeY = 364;                  // +0x5b8

// Unit w226 (src/StaffDB_pool.cpp; names inferred from the rows of src/MainBalance.cpp 68-97), slot = (Mac offset - 8) / 4.
constexpr int kMainBalanceStaffAvgGrade = 148; // +0x258, five staff types
constexpr int kMainBalanceStaffBeginningNumber = 153; // +0x26c, five staff types
constexpr int kMainBalanceStaffMinInPool = 163; // +0x294, five staff types
constexpr int kMainBalanceStaffMaxInPark = 168; // +0x2a8, five staff types
constexpr int kMainBalanceStaffTimeoutTime = 177; // +0x2cc
