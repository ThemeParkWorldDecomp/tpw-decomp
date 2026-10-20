// The bank account's save, month and loans (main app, BlockSave.cpp range of the inventory; unit w173): CBankAccount::Serialise 0x100f2d10 (1,392 bytes),
// MonthlyUpdate 0x100f36a8 (752), GetLoanInfo 0x100f3c44 (320), AnyLoanAvailable 0x100f3ee0 (432) and PayOffLoan 0x100f40c8 (384). The withdrawal that
// MonthlyUpdate and PayOffLoan inline is CBankAccount::MakeWithdrawal (src/Things2_small.cpp), called here.
#include <cstdint>
#include <new>

#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

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
const char kEnormousDepositText[] = "Bank account - making enormous deposit - you sure that's right?\n";   // +0x154
const char kEnteredRedText[] = "Entered red after the current game turn!!\n";                             // +0x195
const char kPayingText[] = "Paying the %d'th monthly instalment of loan %d, costing %d\n";                // +0x1d1
const char kZeroMonthsText[] = "We have a repayment time of 0 months, something is not right\n";         // +0x20d
const char kFinishedText[] = "Congratulations!  You've finished paying off loan %d!\n";                  // +0x24b
const char kNotBoughtText[] = "Cannot pay off a loan you haven't bought\n";                              // +0x32d
const char kCannotAffordText[] = "Cannot afford to pay off loan\n";                                      // +0x357

constexpr int32_t kLoanCount = 8;
constexpr int32_t kSavedVersions = 2;
constexpr uint32_t kEnormousDeposit = 1000000;
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
