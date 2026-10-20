// The parts of the park analyser, the bank account and the challenge HQ that their constructors build (unit w122b). Mac layouts, no pointers.
#pragma once
#include <cstdint>

// 0x0c bytes. <int> 0x100ef230 and <float> 0x100ef460 (24 bytes each): the time stamp word, mVal and mWorkedOut are stored in that order;
// -999 is the "no value" mark (the float's constant at 0x102650b0 is -999.0f). The first word is a CTimeSig image; uint32_t avoids the World.h include cycle.
template <class T>
struct CParkAnalyserVal {
    CParkAnalyserVal() : mTimeSig(0), mVal(static_cast<T>(-999)), mWorkedOut(false) {}
    uint32_t mTimeSig;   // 0x00
    T mVal;              // 0x04  "mVal"
    bool mWorkedOut;     // 0x08  "mWorkedOut"  (0x09 to 0x0b: padding, never written)
    // Unit w125 (<int> only; translated in src/ParkAnalyser_history.cpp, unit w157; src/ParkAnalyser_readers.cpp calls them): Get 0x100ef3c8 answers the
    // cached value, Set 0x100ef400 stores a value (and stamps the refresh clock). The first word is a CTimeSig image: SecondsSinceLastUpdate is asked of the Val.
    T Get() const;
    void Set(T value);
    // Unit w158 (src/ParkAnalyser_save.cpp): <int> 0x100ef278, <float> 0x100ef4a8
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);
};
static_assert(sizeof(CParkAnalyserVal<int>) == 0xc, "CParkAnalyserVal<int>");
static_assert(sizeof(CParkAnalyserVal<float>) == 0xc, "CParkAnalyserVal<float>");

// 0x7c bytes. 0x100e2f3c (152 bytes): the nine values are constructed in ascending order, nothing else is written.
struct CParkAnalyserCell {
    CParkAnalyserCell() {}
    int32_t mStartX;                  // 0x00  "mStartX" (CParkAnalyserCell::Init 0x100e6054 sets the first four)
    int32_t mStartY;                  // 0x04  "mStartY"
    int32_t mSizeX;                   // 0x08  "mSizeX"
    int32_t mSizeY;                   // 0x0c  "mSizeY"
    CParkAnalyserVal<float> mVal10;   // 0x10  (names of the nine values are mine)
    CParkAnalyserVal<float> mVal1C;   // 0x1c
    CParkAnalyserVal<int> mVal28;     // 0x28
    CParkAnalyserVal<int> mVal34;     // 0x34
    CParkAnalyserVal<int> mVal40;     // 0x40
    CParkAnalyserVal<int> mVal4C;     // 0x4c
    CParkAnalyserVal<int> mVal58;     // 0x58
    CParkAnalyserVal<int> mVal64;     // 0x64
    CParkAnalyserVal<int> mVal70;     // 0x70
    // Unit w156 (src/ParkAnalyser_cells.cpp): the counts of the map cells of the rectangle (mStartX, mStartY, mSizeX, mSizeY), each kept in its Val
    // (mVal4C walkable, mVal58 littered, mVal64 developed cells) and recounted when the Val has no value or is older than `maxAge` seconds; the
    // answer is the Val's. 0x100e659c (304 bytes), 0x100e670c (288 bytes) and 0x100e686c (448 bytes). `this` is also read as a CWorldSavvy.
    int32_t GetNumWalkableCells(uint32_t maxAge) const;
    int32_t GetNumLitteredCells(uint32_t maxAge) const;
    int32_t GetNumDevelopedCells(uint32_t maxAge) const;
    // Unit w123b: 0x100e6054 (24 bytes, untranslated: a launcher stand-in). Stores its four arguments in the first four words and answers 1;
    // CParkAnalyser::Init calls it once per cell and ignores the answer.
    int32_t Init(int32_t startX, int32_t startY, int32_t sizeX, int32_t sizeY);
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100e60a0 (1,220 bytes, src/ParkAnalyser_month.cpp, unit w161)
};
static_assert(sizeof(CParkAnalyserCell) == 0x7c, "CParkAnalyserCell is 0x7c bytes");

// 0x20 bytes: one loan offer of CBankAccount (CBankAccount::Serialise names, "mLoans[loan].xxx").
struct CLoan {
    int32_t mLoanAvailable;               // 0x00  "loan_available": 1 when mAmountAvailable <= 10000 at construction (unsigned)
    uint32_t mAmountAvailable;            // 0x04  "amount_available": the balance table's LoanAmount
    uint32_t mAPRInPercent;               // 0x08  "APR_in_percent": APRInPercent
    uint32_t mRepaymentPeriodInMonths;    // 0x0c  "repayment_period_in_months": RepaymentPeriodInMonths
    uint32_t mMonthlyRepayment;           // 0x10  "monthly_repayment": computed
    int32_t mLoanBought;                  // 0x14  "loan_bought": 0
    uint32_t mMonthsRepaid;               // 0x18  "months_repaid": 0
    uint32_t mLenderNameIndex;            // 0x1c  "lenderNameIndex": Lendername
};
static_assert(sizeof(CLoan) == 0x20, "CLoan is 0x20 bytes");

// Unit w123b: the global 32-bit enums of the challenge constructor's parameters (the original's types ChallengeType and StaffType, values unknown here),
// and the two object-id arguments, which are the high half of a register (tag_IDX_TYPE is completed in analyser.h).
enum ChallengeType : int32_t {};
enum StaffType : int32_t {};
struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;

// 0x30 bytes: one challenge (CChallenge::Serialise names). 0x100f5a34 (28 bytes) stores mType, mChallengeOffered, mChallengeLost,
// mChallengeDeclined (words) and mHaveWon (byte), in that order, and nothing else.
struct CChallenge {
    CChallenge() : mType(0), mChallengeOffered(0), mChallengeLost(0), mChallengeDeclined(0), mHaveWon(false) {}
    // Unit w123b: 0x100f5a78 (88 bytes, untranslated: a launcher stand-in, so no challenge is offered yet). On the Mac the first object id is the high
    // half of r10, the second the high half of the stack word +0x38, the staff type the word +0x3c and the last flag the word +0x40. The original
    // stores every field of the challenge (it clears the offered, lost, declined, won and followup thing id words as the default constructor does).
    CChallenge(ChallengeType type, int32_t targetTime, int32_t prize, ChallengeType followup, bool checkAtEndOnly, int32_t targetVal,
               tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> targetObj2,
               StaffType staffType, bool independent);
    int32_t mType;                   // 0x00  "mType" (ChallengeType)
    int32_t mTargetTime;             // 0x04  "mTargetTime"
    int32_t mTargetVal;              // 0x08  "mTargetVal"
    uint16_t mTargetObj;             // 0x0c  "mTargetObj" (tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>)
    uint16_t mTargetObj2;            // 0x0e  "mTargetObj2"
    int32_t mTargetStaffType;        // 0x10  "mTargetStaffType" (StaffType)
    int32_t mPrize;                  // 0x14  "mPrize"
    int32_t mDoubleOrNothingFollowup;   // 0x18  "mDoubleOrNothingFollowup" (ChallengeType)
    bool mCheckAtEndOnly;            // 0x1c  "mCheckAtEndOnly"
    bool mIndependent;               // 0x1d  "mIndependent"
    int32_t mChallengeOffered;       // 0x20  "mChallengeOffered"
    int32_t mChallengeLost;          // 0x24  "mChallengeLost"
    int32_t mChallengeDeclined;      // 0x28  "mChallengeDeclined"
    bool mHaveWon;                   // 0x2c  "mHaveWon"
    uint16_t mThingIdForFollowup;    // 0x2e  "mThingIdForFollowup" (tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>)
    // Unit w124: untranslated callees of CChallengeHQ::UpdateTurn.
    bool DidThisWinIt(int32_t& value, bool atEnd);   // 0x100f60d8
    int32_t GetActiveId() const;                     // 0x100f8ee4
    // Unit w174 (src/Challenge.cpp): the constructor above, DidThisWinIt and GetActiveId are translated there too.
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100f5b80
    int32_t GetOfferId() const;                      // 0x100f8d7c
    int32_t GetWinId() const;                        // 0x100f904c
    int32_t GetLoseId() const;                       // 0x100f91b0
};
static_assert(sizeof(CChallenge) == 0x30, "CChallenge is 0x30 bytes");
