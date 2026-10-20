// AdvisorBalance.cpp: CAdvisorBalance, the balance table of data:advisor:advisor.sam (main app, Advisor.cpp 0x1000e93c and 0x1000e98c).
// The class is only its two virtuals: the description rows (src/AdvisorBalance_table.cpp, generated) and the value slots.
#include "AdvisorScoring.h"

// 0x1000e93c: the row's description (0x3c bytes a row from 0x1028318c); no bounds test.
const BalanceDescription* CAdvisorBalance::GetBalanceDescription(int32_t row) {
    return &gAdvisorBalanceDescriptions[row];
}

// 0x1000e98c: this + 8, the table's first slot (the reserved slot 0).
BalanceValue* CAdvisorBalance::GetBalanceValuesStart() {
    return mValues;
}
