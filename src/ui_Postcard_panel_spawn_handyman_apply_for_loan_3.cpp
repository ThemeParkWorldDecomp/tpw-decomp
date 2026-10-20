#include "keytable_actions.h"
#include "World.h"
#include "ThingReceivers.h"
#include "Debug.h"

// 0x1014e380 (116 bytes)
int32_t apply_for_loan_3() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(2))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}

// 0x1014e41c (116 bytes)
int32_t apply_for_loan_4() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(3))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}
