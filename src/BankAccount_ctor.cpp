// CBankAccount constructor 0x100f29f4 (664 bytes) and SetAdmissionFee 0x100f3a40 (80 bytes), main app, unit w122b.
#include <cmath>
#include <new>

#include "CMessageCentre.h"
#include "Debug.h"
#include "MainBalance.h"
#include "ThingReceivers.h"
#include "ppc_math.h"

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

void CBankAccount::SetAdmissionFee(uint32_t fee) {
    CTRACE(0x18, "Admission fee set to %d\n", fee);
    mAdmissionFee = fee;
}
