// TestAdvisor2/3/4 (step the advisor index and call the advisor) and delete_ride (keyboard actions).
#include "keytable_actions.h"
#include "Advisor.h"
#include "LLCellSystem.h"
#include "intrface.h"

// 0x1014c2b0
int32_t TestAdvisor2() {
    if (ADV_Stat() == 0) {
        gTestAdvisorIndex += 10;
        if (gTestAdvisorIndex >= 0x265) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    }
    return 1;
}

// 0x1014c33c
int32_t TestAdvisor3() {
    if (ADV_Stat() == 0) {
        gTestAdvisorIndex -= 10;
        if (gTestAdvisorIndex < 1) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    }
    return 1;
}

// 0x1014c3c8
int32_t TestAdvisor4() {
    if (ADV_Stat() == 0) {
        gTestAdvisorIndex += 0x32;
        if (gTestAdvisorIndex >= 0x265) gTestAdvisorIndex = 1;
        ADV_Call(AdvisorResponse{gTestAdvisorIndex});
    }
    return 1;
}

// 0x1014bf74
int32_t delete_ride() {
    LLCC_SystemSetMode(static_cast<CELL_MODES>(0x33), true);
    int32_t v = gIntrface.mF24;
    LLCM_LeftMouseButtonUp((v - 1) & 0x7f, static_cast<int32_t>(static_cast<uint32_t>(v - 1) >> 7), 0, 0);
    return 1;
}
