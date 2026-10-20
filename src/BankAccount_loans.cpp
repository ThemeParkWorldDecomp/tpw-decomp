// The bank account's remaining members (main app, BlockSave.cpp range of the inventory; unit w177): FinalDestroy 0x100f2cb8, MakeDeposit 0x100f32ec,
// BeenInTheRedFor 0x100f3490, ReceiveMessage 0x100f3564, PayAdmission 0x100f3ac8, GetNumberOfLoansBought 0x100f3bd0, ApplyForLoan 0x100f3db8,
// OutstandingOnLoan 0x100f4278, GetCheapestLoanTaken 0x100f42e4, GetProfitThisYear 0x100f439c and TotalOutstandingOnLoans 0x100f43dc. The Mac inlines
// OutstandingOnLoan into the two loops and MakeDeposit into PayAdmission and ApplyForLoan; the host calls them (the same events in the same order).
#include <cstdint>

#include "CMsg.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ParkTime.h"
#include "TbTimeDiff.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

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

// 0x100f32ec (144 bytes): an amount of a million or more asserts; the cash, the analyser's deposits history (word 5, +0x1fc90) and the year's profit take it.
void CBankAccount::MakeDeposit(uint32_t amount) {
    GAME_ASSERTMSG(amount < kEnormousDeposit, kEnormousDepositText);
    mBalance = static_cast<int32_t>(static_cast<uint32_t>(mBalance) + amount);
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    analyser->mHistory1F100[5].mTemp = static_cast<int32_t>(static_cast<uint32_t>(analyser->mHistory1F100[5].mTemp) + amount);
    mProfitThisYear = static_cast<int32_t>(static_cast<uint32_t>(mProfitThisYear) + amount);
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
