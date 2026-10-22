// AdvisorScoring.cpp (main app): the advisor's message scoring. A message is one of 351 (kAdvisorMessageCount) things the advisor may
// say; each has a record in the static table gAdvisorMessages (src/AdvisorScoring_table.cpp, generated from the file image at 0x1027efa4,
// 0x30 bytes a record) and a scoring function, one of the _ADVISOR_* members, that the record points to. The queries of this file
// read the record, or the per-category numbers of the advisor balance table (data:advisor:advisor.sam, loaded by the constructor).
// Translated here: the constructor 0x10010054, the destructor 0x1000adb0 and the ten queries 0x1001019c..0x10010f38; the other members (GetFirstResponseFor,
// LastMinuteCheck, GetHighestScoringFromNext, Load, Save and the scorers) belong to later units.
#pragma once
#include <cstdint>

#include "CBalanceTable.h"

// The messages are numbered 0 .. 350; the names of the real ones live in the scorers' symbols (_ADVISOR_NO_SCORE is 0, _ADVISOR_WELCOME 1).
// Two numbers past the end have a meaning of their own: kAdvisorMessageCount (0x15f, the number of messages) and kAdvisorNoMessage
// (0x160, what GetHighestScoringFromNext answers when nothing qualifies and what every record's last word holds). Every query answers
// "no record" for these two without a trace line. Names invented.
constexpr int32_t kAdvisorMessageCount = 0x15f;
enum AdvisorMessage : int32_t {
    kAdvisorNoMessage = 0x160,
};

class CAdvisorScoring;
template<class Tag, class T> struct tag_IDX_TYPE;
struct IDXTYPEUniqueNameThingId;
// A scorer: _ADVISOR_* members, called through the record's pointer to member (the runtime glue at 0x1023ba70 takes the
// {this delta, virtual index, function} triple at record + 0x10; the host stores a real pointer to member).
typedef int32_t (CAdvisorScoring::*AdvisorScorer)();

// A record of the message table. The Mac's is 0x30 bytes; the host's holds the scorer as a pointer to member (16 bytes, not the Mac's 12),
// so the sizes differ and the offsets in the comments are the Mac's.
struct AdvisorMessageRecord {
    int32_t mSkipped;          // 0x00  1 only for message 0 (read by GetHighestScoringFromNext: a message that is not considered)
    int32_t mMessage;          // 0x04  the record's own number (the table is in order, so it equals the index)
    int32_t mCategory;         // 0x08  the message's group, 0 .. 9 (10 is "none"); the group indexes the balance table's MessageGroups array
    int32_t mMode;             // 0x0c  where it may be said: 2 anywhere, 0 only in the normal game, 1 only in the easy-mode game
    AdvisorScorer mScorer;     // 0x10  (12 bytes on the Mac) empty in the file; the static initialiser of the scorers fills it
    int32_t mMaxDuplicates;    // 0x1c  how many copies of the message the queue may hold (the low byte is used)
    int32_t mFirstResponse;    // 0x20  the first of its advisor lines
    int32_t mNumResponses;     // 0x24  how many lines
    int32_t mCycling;          // 0x28  how the lines are cycled
    int32_t mWord2c;           // 0x2c  0x160 (kAdvisorNoMessage) in every record; not read by this file
};

// 0x1027efa4. The block holds the 351 records with two more around them: record -1 first and a zero record last. On the Mac the 0x30 bytes
// before the table are a block of pointers to member whose second word is -1, so the lookup of message -1 FINDS them as a record (its
// "category" is the address 0x10265858, its cycling -1, its number of lines 0, its mode 0); no other number outside the table matches. Its
// scorer cannot be written as a pointer to member here, so it is null: GetScoreFor(-1), GetMinTimeFor(-1), SayOnlyOnce(-1) and
// HasThisBeenKilledTooManyTimes(-1) run wild on the Mac (a call through garbage, a read at this + 12 * 0x10265858) and on the host. The
// spare record behind the table is zero: on the Mac the data after the table has a 0 where its number would be, so a search that ran one
// record past the end would meet a 0. Not const: the scorer unit and the tests fill the scorer slots.
extern AdvisorMessageRecord gAdvisorMessageBlock[kAdvisorMessageCount + 2];
inline AdvisorMessageRecord* const gAdvisorMessages = gAdvisorMessageBlock + 1;   // 0x1027efa4

// The balance table of data:advisor:advisor.sam (CAdvisorBalance, Advisor.cpp 0x1000e93c and 0x1000e98c; its description rows are
// generated into src/AdvisorBalance_table.cpp from 0x1028318c). 393 value slots (the walk of CBalanceTable::Init over the 775 rows; the
// last slot any scorer reads is 392). The array group MessageGroups[10] has three slots a group from slot 4: MinTimeSameMessage,
// SayOnlyOnce and DiscardAfterSlaps, and its count in slot 34.
constexpr int32_t kAdvisorBalanceSlots = 393;
constexpr int32_t kAdvisorGroupSlot = 4;       // the first MessageGroups slot
constexpr int32_t kAdvisorBalanceMinScore = 3;   // unit w124: the lowest score that gets a message queued (Mac +0x20 in the scoring object)
constexpr int32_t kAdvisorGroupStride = 3;
extern BalanceDescription gAdvisorBalanceDescriptions[775];

class CAdvisorBalance : public CBalanceTable {
public:
    const BalanceDescription* GetBalanceDescription(int32_t row) override;   // 0x1000e93c
    BalanceValue* GetBalanceValuesStart() override;                          // 0x1000e98c
    BalanceValue mValues[kAdvisorBalanceSlots];   // 0x08 on the Mac, slot 0 reserved
};

class CAdvisorScoring {
public:
    CAdvisorScoring();     // 0x10010054: makes the balance table (Init) and loads data:advisor:advisor.sam
    ~CAdvisorScoring();    // 0x1000adb0
    int32_t Load(void*& file);   // 0x100111f0 (116 bytes, stand-in; unit v5206): 0 when it failed
    int32_t Save(void*& file) const;   // 0x10011294 (148 bytes, stand-in; unit v5628): 0 when it failed

    // -1 when the message has no record or is not valid in the current game; else the answer of the record's scorer.
    int32_t GetScoreFor(AdvisorMessage message);                          // 0x1001019c
    // 0x10010f88 (unit v5971; launcher/pending_w124.cpp, its interim stand-in, since deleted). The best message of the next `count` records, kAdvisorNoMessage for none.
    AdvisorMessage GetHighestScoringFromNext(int32_t count);              // 0x10010f88
    int32_t GetNumResponsesFor(AdvisorMessage message) const;             // 0x1001055c  0 without a record
    uint32_t GetMaxDuplicatesInQueueFor(AdvisorMessage message) const;    // 0x10010654  0 without a record
    int32_t GetMinTimeFor(AdvisorMessage message) const;                  // 0x10010758  the category's MinTimeSameMessage; 0 without a record
    bool SayOnlyOnce(AdvisorMessage message) const;                       // 0x10010864  the category's SayOnlyOnce is not 0
    int32_t GetCyclingFor(AdvisorMessage message) const;                  // 0x10010980  5 without a record
    int32_t GetMessageCategory(AdvisorMessage message) const;             // 0x10010a74  10 without a record
    // True when `killed` slaps are at least the category's DiscardAfterSlaps (which must not be 0, nor the category 10).
    bool HasThisBeenKilledTooManyTimes(AdvisorMessage message, int32_t killed) const;   // 0x10010b6c
    bool IsThisValidInTheCurrentMode(AdvisorMessage message) const;       // 0x10010cc8
    bool IsThisATutorialMessage(AdvisorMessage message) const;            // 0x10010e78  the category is 1

    // Three of the scorers, for the table's pointers (the scorer unit declares and translates the rest, 0x10011358 on).
    int32_t _ADVISOR_NO_SCORE();      // 0x10011358
    int32_t _ADVISOR_WELCOME();       // 0x1001139c
    int32_t _ADVISOR_OPEN_PARK();     // 0x100113dc

    // Unit v5040 (src/game/advisor/AdvisorScoring.cpp, src/game/advisor/Advisor.cpp). Mac offsets of the balance values read are in the source.
    int32_t _ADVISOR_TOILETS_CLOSED();      // 0x1001aa20
    int32_t _ADVISOR_STAFFROOM_CLOSED();    // 0x1001aafc
    int32_t _ADVISOR_SIDESHOWS_CLOSED();    // 0x1001a934
    int32_t _ADVISOR_LOTS_OF_PRANKS();      // 0x1001abdc
    int32_t _ADVISOR_SHOPS_CLOSED();        // 0x1001a84c
    int32_t _ADVISOR_RIDES_CLOSED();        // 0x1001a728
    int32_t _ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_SATISFACTION();   // 0x1001a5e4
    int32_t _ADVISOR_GOLD_TICKET_NEAR_TO_FIRST_ONE();                  // 0x1001aef8

    // Unit v5081 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_FEATURES_DIRTY_TOILETS_1();         // 0x10015628
    int32_t _ADVISOR_FEATURES_DIRTY_TOILETS_2();         // 0x10015798
    int32_t _ADVISOR_FEATURES_DIRTY_TOILETS_3();         // 0x10015908
    int32_t _ADVISOR_FEATURES_DIRTY_TOILETS_4();         // 0x10015a64
    int32_t _ADVISOR_FEATURES_POOR_SELECTION();          // 0x10015018
    int32_t _ADVISOR_SIDESHOWS_BUILD_MORE_SIDESHOWS();   // 0x10014ea0

    // Unit v5106 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_TUTORIAL_EXPLAIN_TOOLTIPS();                // 0x1001b604
    int32_t _ADVISOR_GOLD_TICKET_EXPLANATION();                  // 0x1001b6d0
    int32_t _ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_a();   // 0x1001b8ec
    int32_t _ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_b();   // 0x1001b9d0
    int32_t _ADVISOR_GOLD_TICKET_AMOUNT_ALL_RIDES_RESEARCHED_c();   // 0x1001bab4
    int32_t _ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_a();            // 0x1001bb8c
    int32_t _ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_b();            // 0x1001bcc0
    int32_t _ADVISOR_GOLD_TICKET_AMOUNT_VISITORS_c();            // 0x1001bdf4

    // Unit v5108 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_MAX_HANDYMEN_V2();        // 0x10016e68
    int32_t _ADVISOR_MAX_ENTERTAINERS();       // 0x10016f2c
    int32_t _ADVISOR_MAX_ENTERTAINERS_V2();    // 0x10016fe8
    int32_t _ADVISOR_MAX_MECHANICS_V2();       // 0x10017168
    int32_t _ADVISOR_MAX_GUARDS();             // 0x10017230
    int32_t _ADVISOR_MAX_GUARDS_V2();          // 0x100172e4
    int32_t _ADVISOR_MAX_RESEARCHERS();        // 0x100173a8
    int32_t _ADVISOR_MAX_RESEARCHERS_V2();     // 0x10017460

    // Unit v5118 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_STAFF_GUARD_BUILD_CAMERA();          // 0x10012b0c
    int32_t _ADVISOR_STAFF_SET_GUARD_AREA();              // 0x10012bec
    int32_t _ADVISOR_STAFF_SET_ENTERTAINER_AREA();        // 0x10012dd4
    int32_t _ADVISOR_STAFF_ENTERTAINER_COVERAGE();        // 0x10012f18
    int32_t _ADVISOR_STAFF_TRAIN_MECHANICS();             // 0x10013044
    int32_t _ADVISOR_STAFF_TRAIN_HANDYMEN();              // 0x10013144
    int32_t _ADVISOR_STAFF_TRAIN_GUARDS();                // 0x10013244
    int32_t _ADVISOR_STAFF_TRAIN_ENTERTAINERS();          // 0x10013340

    // Unit v5122 (src/game/advisor/AdvisorScoring.cpp). Limit and multiplier are balance slots at the Mac offsets in the source.
    int32_t _ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_VALUE();             // 0x1001a010
    int32_t _ADVISOR_CONGRAT_PRICING_SIDESHOWS_GREAT_VALUE();          // 0x1001a104
    int32_t _ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_VALUE();         // 0x10019f18
    int32_t _ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_VALUE();              // 0x10019e28
    int32_t _ADVISOR_CONGRAT_PRICING_FOOD_GREAT_SATISFACTION();        // 0x1001a1f8
    int32_t _ADVISOR_CONGRAT_PRICING_FOOD_GREAT_VALUE();               // 0x10019d34
    int32_t _ADVISOR_CONGRAT_PRICING_SHOPS_GREAT_SATISFACTION();       // 0x1001a2f0
    int32_t _ADVISOR_CONGRAT_PRICING_RESTAURANT_GREAT_SATISFACTION();  // 0x1001a3e8

    // Unit v5136 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_VISITORS_THIRSTY();           // 0x10011640
    int32_t _ADVISOR_VISITORS_HUNGRY();            // 0x100116d8
    int32_t _ADVISOR_STAFF_HIRE_MECHANICS_1();     // 0x1001176c
    int32_t _ADVISOR_STAFF_HIRE_MECHANICS_2();     // 0x10011838
    int32_t _ADVISOR_STAFF_HIRE_MECHANICS_3();     // 0x10011900
    int32_t _ADVISOR_STAFF_HIRE_HANDYMEN_1();      // 0x10011a08
    int32_t _ADVISOR_STAFF_HIRE_HANDYMEN_2();      // 0x10011b0c
    int32_t _ADVISOR_STAFF_HIRE_HANDYMEN_3();      // 0x10011c0c

    // Unit v5252 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_IN_THE_RED_SIX_MONTHS();       // 0x10016420
    int32_t _ADVISOR_TAKE_LOAN();                   // 0x1001636c
    int32_t _ADVISOR_IN_THE_RED_THREE_MONTHS();     // 0x10016524
    int32_t _ADVISOR_IN_THE_RED_MONTH_LEFT();       // 0x10016630
    int32_t _ADVISOR_WEALTH_HIGH();                 // 0x10016160
    int32_t _ADVISOR_GENERAL_LAX_SECURITY();        // 0x10015f68
    int32_t _ADVISOR_LOTS_HANDYMAN();               // 0x100168e8
    int32_t _ADVISOR_LOTS_ENTERTAINERS();           // 0x100169b4

    // Unit v5256 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_CONGRAT_PRICING_DRINKS_GREAT_SATISFACTION();   // 0x1001a4e8
    int32_t _ADVISOR_PRICING_SIDESHOWS_POOR_SATISFACTION();         // 0x10019c24
    int32_t _ADVISOR_PRICING_DRINKS_POOR_SATISFACTION();            // 0x10019b10
    int32_t _ADVISOR_PRICING_RESTAURANT_POOR_SATISFACTION();        // 0x100199f8
    int32_t _ADVISOR_GOLD_TICKET_NEAR_TO_XPEEPS();                  // 0x1001b054
    int32_t _ADVISOR_PRICING_SHOPS_POOR_SATISFACTION();             // 0x100198ec
    int32_t _ADVISOR_PRICING_FOOD_POOR_SATISFACTION();              // 0x100197dc
    int32_t _ADVISOR_PRICING_SIDESHOWS_POOR_VALUE();                // 0x100196d0

    // Unit v5265 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_STAFF_HIRE_GUARDS_1();        // 0x10011d4c
    int32_t _ADVISOR_STAFF_HIRE_GUARDS_2();        // 0x10011e64
    int32_t _ADVISOR_STAFF_HIRE_GUARDS_3();        // 0x10011f54
    int32_t _ADVISOR_STAFF_HIRE_ENTERTAINERS_1();  // 0x10012090
    int32_t _ADVISOR_STAFF_HIRE_RESEARCHERS_2();   // 0x10012328
    int32_t _ADVISOR_STAFF_SET_MECHANIC_AREA();    // 0x1001260c
    int32_t _ADVISOR_STAFF_SET_HANDYMAN_AREA();    // 0x100128a4

    // Unit v5282 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_TRAIN_STAFF_GENERAL();               // 0x10018f4c
    int32_t _ADVISOR_SET_PATROLS_GENERAL();               // 0x10018e80
    int32_t _ADVISOR_HIRE_STAFF_GENERAL();                // 0x10018d90
    int32_t _ADVISOR_NEED_STAFF_ROOM();                   // 0x10018c78
    int32_t _ADVISOR_PRICING_FOOD_POOR_VALUE();           // 0x100192a4
    int32_t _ADVISOR_PRICING_SHOPS_POOR_VALUE();          // 0x100193ac
    int32_t _ADVISOR_PRICING_RESTAURANT_POOR_VALUE();     // 0x100194b4
    int32_t _ADVISOR_CONGRAT_COSTUMES();                  // 0x100188f4

    // Unit v5767 (src/game/advisor/AdvisorScoring.cpp).
    int32_t GetFirstResponseFor(AdvisorMessage message) const;     // 0x10010464: the record's first line, 0x266 without a record
    int32_t _ADVISOR_TUTORIAL_UI_PANEL();                          // 0x1001b540
    int32_t _ADVISOR_GOLD_TICKET_NEAR_TO_XPROFIT();                // 0x1001b30c
    int32_t _ADVISOR_PRICING_DRINKS_POOR_VALUE();                  // 0x100195c4

    // Unit v5892 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_GOLD_TICKET_NEAR_TO_XHAPPINESS();        // 0x1001b18c
    int32_t _ADVISOR_CONGRAT_BALLOONS();                      // 0x10018854
    int32_t _ADVISOR_CONGRAT_SEND_POSTCARD();                 // 0x1001873c
    int32_t _ADVISOR_CONGRAT_PUBLISH_PARK();                  // 0x10018624
    int32_t _ADVISOR_CONGRAT_MECHANICS_TRAINED();             // 0x100183ac
    int32_t _ADVISOR_CONGRAT_ENTERTAINERS_TRAINED();          // 0x100182b0
    int32_t _ADVISOR_CONGRAT_HANDYMEN_TRAINED();              // 0x100181b8
    int32_t _ADVISOR_CONGRAT_RESEARCHERS_TRAINED();           // 0x100180c0
    int32_t _ADVISOR_CONGRAT_SECURITY_TRAINED();              // 0x10017fc8
    int32_t _ADVISOR_CONGRAT_VISITORS_HAPPY();                // 0x10017edc
    int32_t _ADVISOR_CONGRAT_STAFF_HAPPY();                   // 0x10017df4
    int32_t _ADVISOR_VISITORS_TOO_SCARED();                   // 0x10017c40

    // Unit v5896 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_WEALTH_HIGH_REPAY_LOAN();              // 0x10016218
    int32_t _ADVISOR_PRICING_TICKET_TOO_CHEAP();            // 0x10015cec
    int32_t _ADVISOR_WAGES_HIGH();                          // 0x10016778
    int32_t _ADVISOR_PRICING_TICKET_TOO_EXPENSIVE();        // 0x10015c14
    int32_t _ADVISOR_LOTS_MECHANICS();                      // 0x10016a84
    int32_t _ADVISOR_LOTS_SECURITY();                       // 0x10016b50
    int32_t _ADVISOR_LOTS_RESEARCHERS();                    // 0x10016c1c
    int32_t _ADVISOR_QUEUE_UNCONNECTED();                   // 0x10016cec
    int32_t _ADVISOR_MAX_HANDYMEN();                        // 0x10016db0
    int32_t _ADVISOR_FEATURES_NOT_ENOUGH_TOILETS();         // 0x10015564
    int32_t _ADVISOR_FEATURES_NO_TOILETS();                 // 0x100153e0
    int32_t _ADVISOR_FEATURES_BUILD_FEATURES();             // 0x10015298

    // Unit v5937 (src/game/advisor/AdvisorScoring.cpp): 0x100102d8, 276 bytes. -1 when the message has no record or is not valid in
    // the current mode; else the scorer's answer. Copies mWord4 / mWord8 out to the two references (not when there is no record).
    int32_t LastMinuteCheck(AdvisorMessage message, tag_IDX_TYPE<struct IDXTYPEUniqueNameThingId, uint16_t>& thingId, uint32_t& value);

    // Unit v5939 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_RIDES_RESEARCH_MORE();        // 0x10013f28
    int32_t _ADVISOR_RIDES_BUILD_RIDES();          // 0x10014078
    int32_t _ADVISOR_RIDES_NO_UPGRADES();          // 0x10014160
    int32_t _ADVISOR_RIDES_MORE_UPGRADES();        // 0x100142a4
    int32_t _ADVISOR_SHOPS_POOR_SELECTION();       // 0x10014558
    int32_t _ADVISOR_SHOPS_RESEARCH_MORE();        // 0x100146a8
    int32_t _ADVISOR_SHOPS_BUILD_SHOPS();          // 0x100147f8
    int32_t _ADVISOR_SHOPS_BUILD_MORE_SHOPS();     // 0x1001493c
    int32_t _ADVISOR_SIDESHOWS_POOR_SELECTION();   // 0x10014aac

    // Unit v5973 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_RIDES_POOR_SELECTION();         // 0x10013dd8
    int32_t _ADVISOR_SIDESHOWS_RESEARCH_MORE();      // 0x10014c00
    int32_t _ADVISOR_SIDESHOWS_BUILD_SIDESHOWS();    // 0x10014d54
    int32_t _ADVISOR_FEATURES_RESEARCH_MORE();       // 0x10015168
    int32_t _ADVISOR_HYPE_NEWPARK();                 // 0x10017528
    int32_t _ADVISOR_BIG_BUSLOAD_ARRIVED();          // 0x10017b2c

    // Unit v5971 (src/game/advisor/AdvisorScoring.cpp).
    int32_t _ADVISOR_CLOSE_PARK();                    // 0x1001151c
    int32_t _ADVISOR_STAFF_HIRE_RESEARCHERS_1();      // 0x100121b0
    int32_t _ADVISOR_STAFF_HIRE_RESEARCHERS_3();      // 0x10012468
    int32_t _ADVISOR_STAFF_MECHANIC_COVERAGE();       // 0x1001274c
    int32_t _ADVISOR_STAFF_HANDYMAN_COVERAGE();       // 0x100129e4
    int32_t _ADVISOR_STAFF_TRAIN_RESEARCHERS();       // 0x10013444

    // Unit v51055 (src/game/advisor/AdvisorScoring.cpp):
    // each returns the word at the Mac offset from this (balance values).
    int32_t _ADVISOR_STAFF_UNHAPPY_MECHANICS();            // 0x10013544 +0x1bc
    int32_t _ADVISOR_STAFF_UNHAPPY_HANDYMEN();             // 0x10013594 +0x1c0
    int32_t _ADVISOR_STAFF_UNHAPPY_GUARDS();               // 0x100135e4 +0x1c4
    int32_t _ADVISOR_STAFF_UNHAPPY_ENTERTAINERS();         // 0x10013634 +0x1c8
    int32_t _ADVISOR_STAFF_UNHAPPY_RESEARCHERS();          // 0x10013688 +0x1cc
    int32_t _ADVISOR_STAFF_VERY_UNHAPPY_MECHANICS();       // 0x100136dc +0x1d0
    int32_t _ADVISOR_STAFF_VERY_UNHAPPY_HANDYMEN();        // 0x10013734 +0x1d4
    int32_t _ADVISOR_STAFF_VERY_UNHAPPY_GUARDS();          // 0x10013788 +0x1d8
    int32_t _ADVISOR_STAFF_VERY_UNHAPPY_ENTERTAINERS();    // 0x100137dc +0x1dc
    int32_t _ADVISOR_STAFF_VERY_UNHAPPY_RESEARCHERS();     // 0x10013834 +0x1e0
    int32_t _ADVISOR_STAFF_STRIKING_MECHANICS();           // 0x1001388c +0x1e4
    int32_t _ADVISOR_STAFF_STRIKING_HANDYMEN();            // 0x100138e0 +0x1e8
    int32_t _ADVISOR_STAFF_STRIKING_GUARDS();              // 0x10013930 +0x1ec
    int32_t _ADVISOR_STAFF_STRIKING_ENTERTAINERS();        // 0x10013980 +0x1f0
    int32_t _ADVISOR_STAFF_STRIKING_RESEARCHERS();         // 0x100139d4 +0x1f4
    int32_t _ADVISOR_STAFF_HAPPIER_MECHANICS();            // 0x10013a28 +0x1f8
    int32_t _ADVISOR_STAFF_HAPPIER_HANDYMEN();             // 0x10013a78 +0x1fc
    int32_t _ADVISOR_STAFF_HAPPIER_GUARDS();               // 0x10013ac8 +0x200
    int32_t _ADVISOR_STAFF_HAPPIER_ENTERTAINERS();         // 0x10013b18 +0x204
    int32_t _ADVISOR_STAFF_HAPPIER_RESEARCHERS();          // 0x10013b6c +0x208
    int32_t _ADVISOR_STAFF_STRIKE_END_BAD_MECHANICS();     // 0x10013bc0 +0x20c
    int32_t _ADVISOR_STAFF_STRIKE_END_BAD_HANDYMEN();      // 0x10013c18 +0x210
    int32_t _ADVISOR_STAFF_STRIKE_END_BAD_GUARDS();        // 0x10013c70 +0x214
    int32_t _ADVISOR_STAFF_STRIKE_END_BAD_ENTERTAINERS();  // 0x10013cc4 +0x218
    int32_t _ADVISOR_STAFF_STRIKE_END_BAD_RESEARCHERS();   // 0x10013d20 +0x21c
    int32_t _ADVISOR_RIDES_LONG_QUEUES_BORED_VISITORS();   // 0x10013d7c returns 0 (int32_t like every scorer: the table is one type)
    int32_t _ADVISOR_RIDES_USE_UPGRADES();                 // 0x100143b0 returns 0 (int32_t like every scorer: the table is one type)
    int32_t _ADVISOR_RIDES_BREAKDOWN_IMMINENT();           // 0x100143fc +0x248
    int32_t _ADVISOR_RIDES_BREAKDOWN_NO_MECHANICS();       // 0x10014450 +0x24c
    int32_t _ADVISOR_RIDES_BREAKDOWN_BUSY_MECHANICS();     // 0x100144a8 +0x250

    // Unit v51068 (src/game/advisor/AdvisorScoring.cpp).
    // Each returns the word at the Mac offset from this (balance values), or a constant.
    int32_t _ADVISOR_PRANK_SBOMB();                        // 0x100176b8 +0x3d4
    int32_t _ADVISOR_PRANK_LITTER();                       // 0x100176fc +0x3d8
    int32_t _ADVISOR_PRANK_BALLOON();                      // 0x10017744 +0x3dc
    int32_t _ADVISOR_PARK_NOW_OPEN();                      // 0x1001778c +0x3e0
    int32_t _ADVISOR_PARK_NOW_CLOSED();                    // 0x100177d4 +0x3e4
    int32_t _ADVISOR_UPGRADE_HIRE_MECHANIC();              // 0x1001781c +0x3e8
    int32_t _ADVISOR_UPGRADE_STIKING_MECHANICS();          // 0x1001786c +0x3ec
    int32_t _ADVISOR_NEW_UPGRADE_NO_MECHANIC();            // 0x100178c0 +0x3f0
    int32_t _ADVISOR_NEW_RESEARCH_GROUP_RIDE();            // 0x10017910 +0x3f4
    int32_t _ADVISOR_NEW_RESEARCH_GROUP_SHOP();            // 0x10017960 +0x3f8
    int32_t _ADVISOR_NEW_RESEARCH_GROUP_SIDESHOW();        // 0x100179b0 +0x3fc
    int32_t _ADVISOR_NEW_RESEARCH_GROUP_FEATURE();         // 0x10017a04 +0x400
    int32_t _ADVISOR_BUS_ARRIVED();                        // 0x10017a58 +0x404
    int32_t _ADVISOR_PLANE_ARRIVED();                      // 0x10017a9c +0x408
    int32_t _ADVISOR_ONLINE_NEW_ITEM();                    // 0x10017d64 +0x424
    int32_t _ADVISOR_ONLINE_MESSAGE();                     // 0x10017dac +0x428
    int32_t _ADVISOR_CONGRAT_BIG_COASTER();                // 0x100184a4 returns 0
    int32_t _ADVISOR_CONGRAT_FUN_COASTER();                // 0x100184f0 returns 0
    int32_t _ADVISOR_CONGRAT_FUN_GOKART();                 // 0x1001853c returns 0
    int32_t _ADVISOR_CONGRAT_FUN_WATERRIDE();              // 0x10018588 returns 0
    int32_t _ADVISOR_CONGRAT_FAST_GOKART();                // 0x100185d8 returns 0
    int32_t _ADVISOR_CONGRAT_GIFTS();                      // 0x10018994 returns 0
    int32_t _ADVISOR_CONGRAT_GOLDEN_KEY_NONE();            // 0x100189dc returns 0
    int32_t _ADVISOR_CONGRAT_GOLDEN_KEY_JUNGLE();          // 0x10018a2c returns 0
    int32_t _ADVISOR_CONGRAT_GOLDEN_KEY_HALLOWEEN();       // 0x10018a80 returns 0
    int32_t _ADVISOR_CONGRAT_GOLDEN_KEY_SPACE();           // 0x10018ad8 returns 0
    int32_t _ADVISOR_CONGRAT_GOLDEN_KEY_FANTASY();         // 0x10018b2c returns 0
    int32_t _ADVISOR_CONGRAT_KEY_AT_START();               // 0x10018b80 returns 0
    int32_t _ADVISOR_CONGRAT_ALL_COMPLETE();               // 0x10018bd0 returns 0x2710
    int32_t _ADVISOR_CONGRAT_ALL_COMPLETE_LEAVING();       // 0x10018c20 returns 0x270f

    // Unit v51073 (src/game/advisor/AdvisorScoring.cpp).
    // Returns 0 unless an offset is given: then the balance word at that Mac offset.
    int32_t _ADVISOR_GENERAL_WON_FIRST_GOLDEN_TICKET();    // 0x1001adf8 +0x5b4
    int32_t _ADVISOR_WELCOME_NAMEENTRY();                  // 0x1001adac
    int32_t _ADVISOR_GENERAL_BLOWUP_ADVISOR_1();           // 0x1001ae50
    int32_t _ADVISOR_WEATHER_FUNNY_STORM();                // 0x1001ad60
    int32_t _ADVISOR_GENERAL_BLOWUP_ADVISOR_2();           // 0x1001aea4
    int32_t _ADVISOR_WEATHER_FUNNY_SNOW();                 // 0x1001ad14
    int32_t _ADVISOR_WEATHER_FUNNY_RAIN();                 // 0x1001acc8
    int32_t _ADVISOR_RIDE_EXIT_BLOCKED();                  // 0x1001ac7c
    int32_t _ADVISOR_GOLD_TICKET_CLUES_FOR_REMAINDER();    // 0x1001affc +0x5c0
    int32_t _ADVISOR_OBJECTIVE_MET_OFFERDOUBLE();          // 0x1001b444 +0x62c
    int32_t _ADVISOR_OBJECTIVE_MET_OFFERDOUBLE_2();        // 0x1001b498 +0x62c
    int32_t _ADVISOR_CONGRAT_GOLDEN_KEY_ANYPARK();         // 0x1001b4ec
    int32_t _ADVISOR_RIDE_CONDEMNED();                     // 0x1001a6e0 +0x578
    int32_t _ADVISOR_CASH_LOW();                           // 0x1001b774
    int32_t _ADVISOR_CASH_LOW_TAKE_LOAN();                 // 0x1001b7b8
    int32_t _ADVISOR_PEEPS_STUCK_BREAKDOWN();              // 0x1001b804
    int32_t _ADVISOR_PEEPS_STUCK_CONDEMNED();              // 0x1001b854
    int32_t _ADVISOR_PREBUILT_PARK();                      // 0x1001b8a4 +0x5f8
    int32_t _ADVISOR_CHALLENGE();                          // 0x1001bf28 +0x62c
    int32_t _ADVISOR_WELCOME_LOST_WORLD();                 // 0x10019258
    int32_t _ADVISOR_WELCOME_SPACE();                      // 0x10019210
    int32_t _ADVISOR_WELCOME_HALLOWEEN();                  // 0x100191c4
    int32_t _ADVISOR_WELCOME_FANTASY();                    // 0x1001917c
    int32_t _ADVISOR_WELCOME_VETERAN();                    // 0x10019134
    int32_t _ADVISOR_WELCOME_ENTER_NAME();                 // 0x100190e8
    int32_t _ADVISOR_WELCOME_NO_NAME();                    // 0x100190a0
    int32_t _ADVISOR_WELCOME_NEWBIE();                     // 0x10019058
    int32_t _ADVISOR_BUY_MORE_LAND();                      // 0x10019010
    int32_t _ADVISOR_BANKRUPTED();                         // 0x10016734 +0x350
    int32_t _ADVISOR_TUTORIAL_MESSAGE();                   // 0x10016114 +0x328

    // Unit v5L04 (2026-10-20): the scorers the source table at 0x1027df30 names that no unit has translated (the launcher's generated
    // stand-ins answer 0, which is what the Mac's 8 byte bodies answer; _ADVISOR_MAX_MECHANICS is 120 bytes and still to translate).
    int32_t _ADVISOR_STAFF_GUARD_COVERAGE();               // 0x10012d2c (8 bytes, untranslated)
    int32_t _ADVISOR_STAFF_GUARD_COVERAGE_LOST_WORLD();    // 0x10012d7c (8 bytes, untranslated)
    int32_t _ADVISOR_RIDES_BREAKDOWN_MECHANIC_ON_IT();     // 0x10014500 (8 bytes, untranslated)
    int32_t _ADVISOR_FEATURES_FAR_TO_TOILETS();            // 0x10015514 (8 bytes, untranslated)
    int32_t _ADVISOR_FEATURES_BUILD_REST_AREA();           // 0x10015bc0 (8 bytes, untranslated)
    int32_t _ADVISOR_RESEARCH_RIDE_RESEARCHED();           // 0x10015dc0 (8 bytes, untranslated)
    int32_t _ADVISOR_RESEARCH_ADDON_RESEARCHED();          // 0x10015e14 (8 bytes, untranslated)
    int32_t _ADVISOR_RESEARCH_SHOP_RESEARCHED();           // 0x10015e68 (8 bytes, untranslated)
    int32_t _ADVISOR_RESEARCH_SIDESHOW_RESEARCHED();       // 0x10015ebc (8 bytes, untranslated)
    int32_t _ADVISOR_RESEARCH_FEATURE_RESEARCHED();        // 0x10015f14 (8 bytes, untranslated)
    int32_t _ADVISOR_GENERAL_WON_GOLDEN_TICKET();          // 0x10016038 (8 bytes, untranslated)
    int32_t _ADVISOR_AWARD_GIVEN();                        // 0x1001608c (8 bytes, untranslated)
    int32_t _ADVISOR_AWARD_MOVED();                        // 0x100160d0 (8 bytes, untranslated)
    int32_t _ADVISOR_MAX_MECHANICS();                      // 0x100170b0 (120 bytes, unit v5L21)
    int32_t _ADVISOR_BOAT_ARRIVED();                       // 0x10017ae4 (8 bytes, unit v5L21)

    int32_t mCursor;           // 0x00 the place in the table GetHighestScoringFromNext goes on from (the constructor zeroes it)
    uint16_t mWord4;           // 0x04  written by LastMinuteCheck and the scorers (not set by the constructor)
    uint32_t mWord8;           // 0x08  likewise
    CAdvisorBalance mBalance;  // 0x0c  (its vptr is at 0x10 on the Mac)
};

// Unit v5081: the toilet ids the DIRTY_TOILETS scorers compare with (zero in the original's data).
extern uint16_t gAdvisorNoToilet1;   // 0x1027defc
extern uint16_t gAdvisorNoToilet2;   // 0x1027defe

// Unit v5122: the "no thing" ids the CONGRAT_PRICING scorers compare with (zero in the original's data).
extern uint16_t gAdvisorNoneFoodValue;         // 0x1027df1a
extern uint16_t gAdvisorNoneShopsValue;        // 0x1027df1c
extern uint16_t gAdvisorNoneRestaurantValue;   // 0x1027df1e
extern uint16_t gAdvisorNoneDrinksValue;       // 0x1027df20
extern uint16_t gAdvisorNoneSideshowsValue;    // 0x1027df22
extern uint16_t gAdvisorNoneFoodSat;           // 0x1027df24
extern uint16_t gAdvisorNoneShopsSat;          // 0x1027df26
extern uint16_t gAdvisorNoneRestaurantSat;     // 0x1027df28
extern uint16_t gAdvisorNoToilet3;   // 0x1027df00

// Unit v5896: the "no ride" id _ADVISOR_QUEUE_UNCONNECTED compares the unconnected queue's ride with (zero in the original's data).
extern uint16_t gAdvisorNoRideQueue;   // 0x1027df04
extern uint16_t gAdvisorNoToilet4;   // 0x1027df02

// Unit v5256: more "no thing" ids (zero in the original's data).
extern uint16_t gAdvisorNoneSideshowsPoorValue;   // 0x1027df0e
extern uint16_t gAdvisorNoneFoodPoorSat;          // 0x1027df10
extern uint16_t gAdvisorNoneShopsPoorSat;         // 0x1027df12
extern uint16_t gAdvisorNoneRestaurantPoorSat;    // 0x1027df14
extern uint16_t gAdvisorNoneDrinksPoorSat;        // 0x1027df16
extern uint16_t gAdvisorNoneSideshowsPoorSat;     // 0x1027df18
extern uint16_t gAdvisorNoneDrinksGreatSat;       // 0x1027df2a

// Unit v5282: more "no thing" ids (zero in the original's data).
extern uint16_t gAdvisorNoneFoodPoorValue;         // 0x1027df06
extern uint16_t gAdvisorNoneShopsPoorValue;        // 0x1027df08
extern uint16_t gAdvisorNoneRestaurantPoorValue;   // 0x1027df0a

// Unit v5767: the "no thing" id of the drinks poor value scorer (zero in the original's data).
extern uint16_t gAdvisorNoneDrinksPoorValue;   // 0x1027df0c

// Unit v5018 (src/game/advisor/AdvisorScoring.cpp): the static initialiser 0x1001c34c.
#include "TbDynamicStringTemplate.h"
extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
extern unsigned char gAdvisorStringStorage[];   // 0x102b4110 the file's global string (TbDynamicStringTemplate<char16_t>)
extern unsigned char gAdvisorStringNode[8];     // 0x102b4124 its registration node
extern AdvisorScorer gAdvisorScorerSources[kAdvisorMessageCount];   // 0x1027df30, 12 bytes each on the Mac: the scorers as constants
void DestroyAdvisorString(void* object, int16_t deleteFlag);
void __sinit_AdvisorScoring_cpp();   // 0x1001c34c

// Unit v5035 (src/game/ui/ui_cityLobbyController.cpp): the static initialiser 0x101e0cec.
extern const char gCityLobbyStringPool[];                 // 0x1024edc2 (literals of ui_cityLobbyController.cpp)
extern "C" void CityLobbyStringDestructor(void* object, int16_t deleteFlag);   // the string destructor import
extern "C" void* CityLobbyStringConstruct(void* self, const char16_t* text);   // the string constructor glue

// Unit v51116 (src/AdvisorScoring_..._AWARD_MOVED.cpp).
namespace v51116_callee {
void OperatorDelete(void* block);   // 0x10238218
}
// 0x1001010c ~CAdvisorBalance with the hidden delete flag (the Mac's vptr store at +4 has no host equivalent); answers this.
CAdvisorBalance* CAdvisorBalanceDestruct(CAdvisorBalance* self, int16_t deleteFlag);
// CMsgTag: a message with a vptr word (the destructor 0x1000e2d0 stores 0x1027de84 there; mVtable stands for it).
struct CMsgTag {
    uint32_t mVtable;        // 0x00
    int32_t GetType() const; // 0x1000e90c
};
CMsgTag* CMsgTagDestruct(CMsgTag* self, int16_t deleteFlag);   // 0x1000e2d0
struct tagBUMPRIDE;
struct tabCOLOBJECT;
struct tagBUMP;
struct tagPEEPOFF;
// The sound library's inline byte swaps (the Mac's SamsUtilities::USwap32<T*> etc.; one function per address, names invented).
namespace v51116_swap {
tagBUMPRIDE* USwap32_tagBUMPRIDE(tagBUMPRIDE* p);       // 0x10021bf8
tabCOLOBJECT* USwap32_tabCOLOBJECT(tabCOLOBJECT* p);    // 0x10021c6c
tagBUMP* USwap32_tagBUMP(tagBUMP* p);                   // 0x10021ce0
tagPEEPOFF* USwap32_tagPEEPOFF(tagPEEPOFF* p);          // 0x10021d48
int32_t* USwap32_long_ptr(int32_t* p);                  // 0x10009ad0
void USwapInplace32_long_10021db8(int32_t& v);
void USwapInplace32_ulong_10021dfc(uint32_t& v);
void USwapInplace32_long_10009c74(int32_t& v);
void USwapInplace32_int_10009c30(int32_t& v);
void USwapInplace32_ulong_10009b2c(uint32_t& v);
void USwapBlock32_ulong(uint32_t* words, int32_t count);   // 0x10009bd0
void USwapBlock32_long(int32_t* words, int32_t count);     // 0x10009b74
}
