// BankAccount.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of BankAccount code, named after it.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CMsg.h"
#include "CMsgDeath.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "ppc_math.h"
#include "Serialise.h"
#include "tb_rain.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

// CBankAccount constructor 0x100f29f4 (664 bytes) and SetAdmissionFee 0x100f3a40 (80 bytes), main app, unit w122b.

namespace {
// Lazy centre block at 0x100f2b70, repeated before each subscription, including after allocation failure.
CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}
}

CBankAccount::CBankAccount()
    : mBase(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{1}, static_cast<CThing::ThingModel>(0x10)) {
    mBalance = 0;
    mBatchBalance = 0;
    mWithdrawalsEnabled = 0;
    mAdmissionFee = 0;
    mLastBalance = 0;
    mTurnEnteredRed = 0;
    mBalance = gMainBalance.mValues[kMainBalanceInitialCash].mInteger;
    mBatchBalance = 0;
    mProfitThisYear = 0;
    mWithdrawalsEnabled = 1;
    for (uint32_t i = 0; i < 8; ++i) {
        CLoan& loan = mLoans[i];
        const BalanceValue* entry = &gMainBalance.mValues[kMainBalanceLoanInfo + 4 * i];
        loan.mLenderNameIndex = entry[3].mInteger;
        loan.mAmountAvailable = entry[0].mInteger;
        loan.mAPRInPercent = entry[1].mInteger;
        loan.mRepaymentPeriodInMonths = entry[2].mInteger;
        loan.mLoanBought = 0;
        loan.mMonthsRepaid = 0;
        loan.mLoanAvailable = loan.mAmountAvailable <= 10000u;
        const double months = static_cast<double>(loan.mRepaymentPeriodInMonths);
        const double apr = static_cast<double>(loan.mAPRInPercent);
        const uint32_t amount = loan.mAmountAvailable;
        double exponent = months / 12.0;
        const double rate = apr / 100.0;
        exponent = exponent * 0.5;
        const double base = 1.0 + rate;
        const double power = std::pow(base, exponent);
        const double principal = static_cast<double>(amount);
        const double total = principal * power;
        loan.mMonthlyRepayment = cw_cvt_fp2unsigned(total / months);
    }
    SetAdmissionFee(gMainBalance.mValues[kMainBalanceInitialAdmissionFee].mInteger);
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0xc));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x13));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0xd));
}

// The bank account's remaining members (main app, BlockSave.cpp range of the inventory; unit w177): FinalDestroy 0x100f2cb8, MakeDeposit 0x100f32ec,
// BeenInTheRedFor 0x100f3490, ReceiveMessage 0x100f3564, PayAdmission 0x100f3ac8, GetNumberOfLoansBought 0x100f3bd0, ApplyForLoan 0x100f3db8,
// OutstandingOnLoan 0x100f4278, GetCheapestLoanTaken 0x100f42e4, GetProfitThisYear 0x100f439c and TotalOutstandingOnLoans 0x100f43dc. The Mac inlines
// OutstandingOnLoan into the two loops and MakeDeposit into PayAdmission and ApplyForLoan; the host calls them (the same events in the same order).

namespace {
// The string pool at 0x102406ca (TOC word -0x4ed4) plus the offset in the comment (w173's pool, src/BankAccount.cpp, defines its own copy).
const char kEnormousDepositText[] = "Bank account - making enormous deposit - you sure that's right?\n";   // +0x154
const char kEnteredRedText[] = "Entered red after the current game turn!!\n";                             // +0x195
const char kUnknownMessageText[] = "Unknown message\n";                                                   // +0x1c0
const char kAlreadyBoughtText[] = "Applying for loan already bought\n";                                   // +0x29b
const char kVettingText[] = "(Might want to put some sort of vetting process in here...)\n";             // +0x2bd
const char kAcceptedText[] = "Accepted for this loan.  Thankyou for your custom\n";                       // +0x2fa

constexpr int32_t kLoanCount = 8;
constexpr uint32_t kEnormousDeposit = 1000000;
constexpr int32_t kMsgEndOfMonth = 0xc;
constexpr int32_t kMsgEndOfYear = 0xd;   // name mine: the message that clears the year's profit
constexpr int32_t kMsgEvent = 0x13;
constexpr int32_t kEventBankrupt = 2;
constexpr int32_t kEventRestart = 10;   // name mine: the event that puts the opening cash back
constexpr int32_t kWorldStateSilent = 4;
}  // namespace

// 0x100f2cb8 (36 bytes): CThing::FinalDestroy of the base.
void CBankAccount::FinalDestroy() { mBase.FinalDestroy(); }

// The bank account's save, month and loans (main app, BlockSave.cpp range of the inventory; unit w173): CBankAccount::Serialise 0x100f2d10 (1,392 bytes),
// MonthlyUpdate 0x100f36a8 (752), GetLoanInfo 0x100f3c44 (320), AnyLoanAvailable 0x100f3ee0 (432) and PayOffLoan 0x100f40c8 (384). The withdrawal that
// MonthlyUpdate and PayOffLoan inline is CBankAccount::MakeWithdrawal (src/Things2_small.cpp), called here.

namespace {
// The string pool at 0x102406ca (TOC word -0x4ed4) plus the offset in the comment.
const char kAdmissionFee[] = "mAdmissionFee";                        // +0x00
const char kBalance[] = "mBalance";                                  // +0x0e
const char kBatchBalance[] = "mBatchBalance";                        // +0x17
const char kWithdrawalsEnabled[] = "mWithdrawalsEnabled";            // +0x25
const char kLastBalance[] = "mLastBalance";                          // +0x39
const char kTurnEnteredRed[] = "mTurnEnteredRed";                    // +0x46
const char kProfitThisYear[] = "mProfitThisYear";                    // +0x56
const char kLoanAvailable[] = "mLoans[loan].loan_available";         // +0x66
const char kAmountAvailable[] = "mLoans[loan].amount_available";     // +0x82
const char kAprInPercent[] = "mLoans[loan].APR_in_percent";          // +0xa0
const char kRepaymentPeriod[] = "mLoans[loan].repayment_period_in_months";   // +0xbc
const char kMonthlyRepayment[] = "mLoans[loan].monthly_repayment";   // +0xe4
const char kLoanBought[] = "mLoans[loan].loan_bought";               // +0x103
const char kMonthsRepaid[] = "mLoans[loan].months_repaid";           // +0x11c
const char kLenderNameIndex[] = "mLoans[loan].lenderNameIndex";      // +0x137
const char kPayingText[] = "Paying the %d'th monthly instalment of loan %d, costing %d\n";                // +0x1d1
const char kZeroMonthsText[] = "We have a repayment time of 0 months, something is not right\n";         // +0x20d
const char kFinishedText[] = "Congratulations!  You've finished paying off loan %d!\n";                  // +0x24b
const char kNotBoughtText[] = "Cannot pay off a loan you haven't bought\n";                              // +0x32d
const char kCannotAffordText[] = "Cannot afford to pay off loan\n";                                      // +0x357

constexpr int32_t kSavedVersions = 2;
constexpr uint32_t kParkValueSeconds = 0x14;
constexpr int32_t kMonthsInRed = 6;
constexpr int64_t kMonthTicks = 25920000000000LL;   // 0x1792f8648000: thirty days of 100 ns ticks
constexpr int32_t kEventMonthsInRed = 2;

// Loads or saves one value, by the save flag.
template <class V>
int32_t Var(int32_t save, void* file, V* value, const char* name) {
    return save != 0 ? SAVEVAR<V>(file, value, name, 0) : LOADVAR<V>(file, value, name, 0);
}

CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// 0x10237e1c: the signed 64 bit division of the span by thirty days; the Mac keeps the low word.
int32_t MonthsOf(const TbTimeDiff& span) { return static_cast<int32_t>(span.mTicks / kMonthTicks); }
}  // namespace

int32_t CBankAccount::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (version < 0 || version >= kSavedVersions) return 0;
    if (Var<uint32_t>(save, file, &mAdmissionFee, kAdmissionFee) != 0) return 1;
    if (Var<int32_t>(save, file, &mBalance, kBalance) != 0) return 1;
    if (Var<int32_t>(save, file, &mBatchBalance, kBatchBalance) != 0) return 1;
    if (Var<int32_t>(save, file, &mWithdrawalsEnabled, kWithdrawalsEnabled) != 0) return 1;
    if (Var<int32_t>(save, file, &mLastBalance, kLastBalance) != 0) return 1;
    if (Var<MacULong>(save, file, reinterpret_cast<MacULong*>(&mTurnEnteredRed), kTurnEnteredRed) != 0) return 1;
    if (Var<int32_t>(save, file, &mProfitThisYear, kProfitThisYear) != 0) return 1;
    for (int32_t loan = 0; loan < kLoanCount; ++loan) {
        CLoan& l = mLoans[loan];
        if (Var<int32_t>(save, file, &l.mLoanAvailable, kLoanAvailable) != 0) return 1;
        if (Var<uint32_t>(save, file, &l.mAmountAvailable, kAmountAvailable) != 0) return 1;
        if (Var<uint32_t>(save, file, &l.mAPRInPercent, kAprInPercent) != 0) return 1;
        if (Var<uint32_t>(save, file, &l.mRepaymentPeriodInMonths, kRepaymentPeriod) != 0) return 1;
        if (Var<uint32_t>(save, file, &l.mMonthlyRepayment, kMonthlyRepayment) != 0) return 1;
        if (Var<int32_t>(save, file, &l.mLoanBought, kLoanBought) != 0) return 1;
        if (Var<uint32_t>(save, file, &l.mMonthsRepaid, kMonthsRepaid) != 0) return 1;
        if (Var<uint32_t>(save, file, &l.mLenderNameIndex, kLenderNameIndex) != 0) return 1;
    }
    return 0;
}

// The park analyser's three messages (main app, unit w161): the constructors 0x1015210c (CMsgAddToChallenge), 0x10151cfc (CMsgResearchCompleted) and
// 0x10151f14 (CMsgStaffInfo), the GetType functions 0x100ef1f4, 0x101244d0 and 0x1012c244, the destructors 0x100e4584, 0x1012412c and 0x1012b4f4, and
// CBankAccount::GetBalance 0x100f32b4. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

// 0x100f32b4 (8 bytes): the word at +0xc.
int32_t CBankAccount::GetBalance() const { return mBalance; }

// 0x100f32ec (144 bytes): an amount of a million or more asserts; the cash, the analyser's deposits history (word 5, +0x1fc90) and the year's profit take it.
void CBankAccount::MakeDeposit(uint32_t amount) {
    GAME_ASSERTMSG(amount < kEnormousDeposit, kEnormousDepositText);
    mBalance = static_cast<int32_t>(static_cast<uint32_t>(mBalance) + amount);
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    analyser->mHistory1F100[5].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[5].mTemp) + amount);
    mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) + amount);
}

// The things' second batch, the small functions (unit w130): CParkAnalyser::BuiltSomething 0x100e46a4 (120 bytes) and NumberOfStaffType 0x100e8144
// (404 bytes), CBankAccount::MakeWithdrawal 0x100f33b0 (172 bytes), CAdvisor::IsPlaying 0x1000c6e0 (84 bytes) and FindHighestScoringSlotInQueue
// 0x1000b900 (216 bytes), CWeather::SetPrecipitationType 0x10139388 (256 bytes).

// The balance goes down; the turn it first goes below 0 is kept; the park's total of withdrawals (the analyser at the game world's, not the
// world global) and the year's profit take the amount off.
void CBankAccount::MakeWithdrawal(uint32_t amount) {
    if (mWithdrawalsEnabled == 0) return;
    mBalance = static_cast<int32_t>(static_cast<uint32_t>(mBalance) - amount);
    if (mBalance < 0 && mLastBalance >= 0) mTurnEnteredRed = gWorld->mTurn;
    mLastBalance = mBalance;
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    analyser->mHistory1F100[2].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[2].mTemp) + amount);
    mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) - amount);
}

// 0x100f3490 (160 bytes): answers the span by value. Not in the red (the last balance is not negative): zero ticks. Else the world's turn must not
// be before the turn the account went red (an assertion) and the park time says how long ago that was. gWorld, not gGameWorld.
TbTimeDiff CBankAccount::BeenInTheRedFor() {
    TbTimeDiff span;
    if (mLastBalance >= 0) {
        span.mTicks = 0;
    } else {
        GAME_ASSERTMSG(gWorld->mTurn >= mTurnEnteredRed, kEnteredRedText);
        span = gWorld->mParkTime.GetFunnyTimeSinceGameTurn(mTurnEnteredRed);
    }
    return span;
}

// 0x100f3564 (264 bytes). The message type: 0xc the month end, 0xd the year end (the profit is cleared), 0x13 an event: 2 starts the bankruptcy sequence
// unless the world's state word is 4 or the winning animation is playing (and then the message is done), else 10 puts the opening cash back and clears
// the batch; every other type (below 0xc too) asserts.
void CBankAccount::ReceiveMessage(const CMsg& message) {
    switch (message.GetType()) {
        case kMsgEndOfMonth:
            MonthlyUpdate();
            return;
        case kMsgEndOfYear:
            mProfitThisYear = 0;
            return;
        case kMsgEvent: {
            const CMsgEvent& event = static_cast<const CMsgEvent&>(message);
            if (event.mEvent == kEventBankrupt && gGameWorld->mF1DA738 != kWorldStateSilent && !gGameWorld->IsPlayingWinningAnim()) {
                gGameWorld->StartBankruptcySequence();
                return;
            }
            if (event.mEvent == kEventRestart) {
                mBalance = gMainBalance.mValues[kMainBalanceInitialCash].mInteger;
                mBatchBalance = 0;
            }
            return;
        }
        default:
            GAME_ASSERTMSG(0, kUnknownMessageText);
            return;
    }
}

void CBankAccount::MonthlyUpdate() {
    const uint32_t batch = static_cast<uint32_t>(mBatchBalance);
    GAME_ASSERTMSG(batch < kEnormousDeposit, kEnormousDepositText);
    mBalance = static_cast<int32_t>(static_cast<uint32_t>(mBalance) + batch);
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    analyser->mHistory1F100[5].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[5].mTemp) + batch);
    mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) + batch);
    mBatchBalance = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(kLoanCount); ++i) {
        CLoan& loan = mLoans[i];
        if (loan.mLoanBought == 0) continue;
        CTRACE(2, kPayingText, static_cast<int32_t>(loan.mMonthsRepaid + 1), static_cast<int32_t>(i), static_cast<int32_t>(loan.mMonthlyRepayment));
        MakeWithdrawal(loan.mMonthlyRepayment);
        loan.mMonthsRepaid = loan.mMonthsRepaid + 1;
        GAME_ASSERTMSG(loan.mRepaymentPeriodInMonths != 0, kZeroMonthsText);
        const uint32_t period = loan.mRepaymentPeriodInMonths;
        if (period != 0) {
            const uint32_t repayment = loan.mMonthlyRepayment;
            const uint32_t remaining = (repayment * period - loan.mAmountAvailable) / period;
            mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) + (repayment - remaining));
        }
        if (loan.mMonthsRepaid == loan.mRepaymentPeriodInMonths) {
            CTRACE(2, kFinishedText, static_cast<int32_t>(i));
            loan.mLoanBought = 0;
            loan.mMonthsRepaid = 0;
        }
    }
    if (mBalance >= 0) return;
    TbTimeDiff span;
    if (mLastBalance >= 0) {
        span.mTicks = 0;
    } else {
        GAME_ASSERTMSG(gWorld->mTurn >= mTurnEnteredRed, kEnteredRedText);
        span = gWorld->mParkTime.GetFunnyTimeSinceGameTurn(mTurnEnteredRed);
    }
    if (MonthsOf(span) >= kMonthsInRed) {
        CMsgEvent message(static_cast<EventType>(kEventMonthsInRed));
        MessageCentreForPost()->PostMessage(message);
    }
}

// The message centre's post and the message and subscription objects it uses (main app, unit w107): CMessageCentre::PostMessage
// 0x1015256c, the CMsgEvent constructor 0x10151df8 and destructor 0x100f39cc (and GetType 0x100f4470, which its vtable needs), the
// CSubscriptionHeader constructor 0x1000a3f8 and destructor 0x101522e0. The inventory files them under RideDB.cpp and BlockSave.cpp.

// The Mac only resets the vtable and, for a positive flag, deletes the object (the C++ delete does that).
CMsgEvent::~CMsgEvent() {}

void CBankAccount::SetAdmissionFee(uint32_t fee) {
    CTRACE(0x18, "Admission fee set to %d\n", fee);
    mAdmissionFee = fee;
}

// 0x100f3ac8 (212 bytes): the admission fee is deposited (MakeDeposit's body inlined), added to the admissions history (word 6, +0x1fee0) and handed to
// the analyser as a visitor's payment. The fee and the analyser are read again before each step, as on the Mac.
void CBankAccount::PayAdmission() {
    MakeDeposit(mAdmissionFee);
    const uint32_t fee = mAdmissionFee;
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    analyser->mHistory1F100[6].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[6].mTemp) + fee);
    const uint32_t paid = mAdmissionFee;
    gGameWorld->GetParkAnalyser()->PersonPaidAdmission(static_cast<int32_t>(paid));
}

// 0x100f3bd0 (56 bytes).
int32_t CBankAccount::GetNumberOfLoansBought() {
    int32_t count = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(kLoanCount); ++i) {
        if (mLoans[i].mLoanBought != 0) ++count;
    }
    return count;
}

CLoan CBankAccount::GetLoanInfo(uint32_t loan) {
    uint32_t debt = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(kLoanCount); ++i) {
        if (mLoans[i].mLoanBought == 0) continue;
        debt += mLoans[i].mMonthlyRepayment * (mLoans[i].mRepaymentPeriodInMonths - mLoans[i].mMonthsRepaid);
    }
    const int32_t value = static_cast<int32_t>(gWorld->GetParkAnalyser()->GetParkValue(kParkValueSeconds));
    // The int is converted exactly to a double, rounded to single precision by fsubs, then truncated by fctiwz.
    const int32_t afford = ppc_fctiwz(static_cast<double>(static_cast<float>(value)));
    const int32_t room = static_cast<int32_t>(static_cast<uint32_t>(afford) - debt);
    mLoans[loan].mLoanAvailable = static_cast<int32_t>(mLoans[loan].mAmountAvailable) > room ? 0 : 1;
    return mLoans[loan];
}

// 0x100f3db8 (244 bytes): no bound test on `loan`. A bought loan only traces. Otherwise two traces, then the amount is deposited (which adds it to the
// year's profit) and taken off the profit again by a second read of the amount, and the loan is marked bought.
bool CBankAccount::ApplyForLoan(uint32_t loan) {
    CLoan& offer = mLoans[loan];
    if (offer.mLoanBought != 0) {
        CTRACE(2, kAlreadyBoughtText);
        return false;
    }
    CTRACE(2, kVettingText);
    CTRACE(2, kAcceptedText);
    MakeDeposit(offer.mAmountAvailable);
    mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) - offer.mAmountAvailable);
    offer.mLoanBought = 1;
    return true;
}

bool CBankAccount::AnyLoanAvailable() {
    for (uint32_t i = 0; i < static_cast<uint32_t>(kLoanCount); ++i) {
        const CLoan info = GetLoanInfo(i);
        if (info.mLoanBought == 0 && info.mLoanAvailable != 0) return true;
    }
    return false;
}

bool CBankAccount::PayOffLoan(uint32_t loan) {
    CLoan& l = mLoans[loan];
    if (l.mLoanBought == 0) {
        CTRACE(0x1a, kNotBoughtText);
        return false;
    }
    const uint32_t cost = l.mMonthlyRepayment * (l.mRepaymentPeriodInMonths - l.mMonthsRepaid);
    if (static_cast<uint32_t>(mBalance) < cost) {
        CTRACE(0x1a, kCannotAffordText);
        return false;
    }
    MakeWithdrawal(cost);
    l.mLoanBought = 0;
    l.mMonthsRepaid = 0;
    GAME_ASSERTMSG(l.mRepaymentPeriodInMonths != 0, kZeroMonthsText);
    const uint32_t period = l.mRepaymentPeriodInMonths;
    if (period != 0) {
        const uint32_t remaining = (l.mMonthlyRepayment * period - l.mAmountAvailable) / period;
        // mMonthsRepaid is read again after the zeroing above: the factor is the whole period.
        mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) + (cost - remaining * (period - l.mMonthsRepaid)));
    }
    return true;
}

// 0x100f4278 (52 bytes): no bound test on `loan`; the product wraps in 32 bits.
uint32_t CBankAccount::OutstandingOnLoan(uint32_t loan) const {
    const CLoan& offer = mLoans[loan];
    if (offer.mLoanBought == 0) return 0;
    return offer.mMonthlyRepayment * (offer.mRepaymentPeriodInMonths - offer.mMonthsRepaid);
}

// 0x100f42e4 (124 bytes): the first loan of the least (unsigned) nonzero outstanding; 0xffffffff when none.
uint32_t CBankAccount::GetCheapestLoanTaken() const {
    uint32_t best = 0xffffffffu;
    uint32_t cheapest = 0;
    bool found = false;
    for (uint32_t i = 0; i < static_cast<uint32_t>(kLoanCount); ++i) {
        const uint32_t outstanding = OutstandingOnLoan(i);
        if (outstanding != 0 && (outstanding < cheapest || !found)) {
            found = true;
            best = i;
            cheapest = outstanding;
        }
    }
    return best;
}

// 0x100f439c (8 bytes).
int32_t CBankAccount::GetProfitThisYear() const { return mProfitThisYear; }

// 0x100f43dc (84 bytes).
uint32_t CBankAccount::TotalOutstandingOnLoans() const {
    uint32_t total = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(kLoanCount); ++i) total += OutstandingOnLoan(i);
    return total;
}

// The serialisation instances for the basic types: LOADVAR and SAVEVAR of int, bool,
// unsigned int, unsigned short, char and wchar_t (char16_t here, the Mac's wchar_t is two
// bytes); unit w146 adds unsigned char, short and the thing id struct (see the end). Each is one explicit specialisation over the shared bodies in Serialise.h, so it
// is a separate symbol as the original's. The sizes are 4, 1, 4, 2, 1 and 2 bytes; the 4 and
// 2-byte values are swapped to the file's little-endian order, the 1-byte ones are not.
//
// Quirks kept: LOADVAR<bool> and LOADVAR<char> store the file's byte as it is (a bool is
// not normalised: a byte of 2 stays 2); a failed read or write leaves the value alone and
// answers 1; the two asserts of the success path come before the swap in the loaders and
// before the write in the savers.

template<> int32_t LOADVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f44a4, 236 bytes
}
template<> int32_t SAVEVAR<uint32_t>(void* file, uint32_t* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f45bc, 260 bytes
}
