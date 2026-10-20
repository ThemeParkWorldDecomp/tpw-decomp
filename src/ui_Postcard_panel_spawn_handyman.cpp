// keyboard.cpp: the spawn and loan actions of the key tables (unit v5028).
#include <new>
#include <string>

#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "online.h"

namespace {
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellTag;

CellTag CurrentCell() {
    CellTag cell;
    cell.mIndex = gIntrface.mF24;
    return cell;
}

template <class T>
void MakeStaff(uint32_t size) {
    CellTag cell = CurrentCell();
    void* memory = online_callee::OperatorNew(size);
    if (memory) new (memory) T(cell);
}
}  // namespace

// 0x1014dbf4 (136 bytes)
int32_t spawn_person() {
    if (gGameWorld->ParkGates_GetStatus() == 1) {
        void* memory = online_callee::OperatorNew(0x22c);
        if (memory) {
            CellTag cell = CurrentCell();
            new (memory) CPerson(cell);
        }
    } else {
        gGameWorld->SetParkOpenOrClosed(1, 0);
    }
    return 1;
}

// 0x1014dca0 (176 bytes)
int32_t spawn_five_persons() {
    if (gGameWorld->ParkGates_GetStatus() == 1) {
        for (int32_t i = 0; i < 5; i++) {
            void* memory = online_callee::OperatorNew(0x22c);
            if (memory) {
                CellTag cell = CurrentCell();
                new (memory) CPerson(cell);
            }
        }
    } else {
        gGameWorld->SetParkOpenOrClosed(1, 0);
    }
    return 1;
}

// 0x1014ddd0 (92 bytes)
int32_t start_winning_sequence() {
    if (gGameWorld->IsPlayingWinningAnim())
        gGameWorld->EndWinningSequence();
    else
        gGameWorld->StartWinningSequence();
    return 1;
}

// 0x1014debc (88 bytes)
int32_t spawn_entertainer() {
    MakeStaff<CEntertainer>(0x214);
    return 1;
}

// 0x1014df3c (88 bytes)
int32_t spawn_handyman() {
    MakeStaff<CHandyman>(0x218);
    return 1;
}

// 0x1014e138 (232 bytes): the cell is read once, before the first allocation
int32_t spawn_bag_o_staff() {
    CellTag cell = CurrentCell();
    void* memory = online_callee::OperatorNew(0x214);
    if (memory) new (memory) CGuard(cell);
    memory = online_callee::OperatorNew(0x214);
    if (memory) new (memory) CResearcher(cell);
    memory = online_callee::OperatorNew(0x218);
    if (memory) new (memory) CMechanic(cell);
    memory = online_callee::OperatorNew(0x218);
    if (memory) new (memory) CHandyman(cell);
    memory = online_callee::OperatorNew(0x214);
    if (memory) new (memory) CEntertainer(cell);
    return 1;
}

// 0x1014e248 (116 bytes)
int32_t apply_for_loan_1() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(0))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}

// 0x1014e2e4 (116 bytes)
int32_t apply_for_loan_2() {
    if (gGameWorld->GetBankAccount()->ApplyForLoan(1))
        CTRACE(2, "Successfully applied for loan\n");
    else
        CTRACE(2, "Declined for loan\n");
    return 1;
}
