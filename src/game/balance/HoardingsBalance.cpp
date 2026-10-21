// HoardingsBalance.cpp: name proven by __sinit_HoardingsBalance_cpp in the main application.

#include "AdvisorScoring.h"
#include "BMP_InsideArea.h"
#include "CBalanceTable.h"
#include "Debug.h"
#include "HoardingsBalance.h"
#include "rsse.h"
#include "TPAdvisor.h"
#include <cstdint>
#include <cstring>
#include <new>

// Unit v51036: ride commands on the bump world (main app): BMP_SetRideDuration, BMP_RideWorn, BMP_GetCameraMesh, BMP_StartBump,
// BMP_StartBuoy, BMP_AddBuoyToRide, and the static initialiser of HoardingsBalance.cpp.

// 0x10021074: the vtable stores are the constructors' (the objects are built by the host's static initialisation).
// Stand-ins for the destructor vector 0x10265fa0 and the registration nodes 0x102b455c, 0x102b4340.
uint8_t gHoardingsDtorVector[16];
uint8_t gHoardingsNode434C[16];
uint8_t gHoardingsNode4130[16];

void __sinit_HoardingsBalance_cpp() {
    gHoardingsBalance434C.Init();
    __register_global_object(&gHoardingsBalance434C, gHoardingsDtorVector, gHoardingsNode434C);
    gHoardingsBalance4130.Init();
    __register_global_object(&gHoardingsBalance4130, gHoardingsDtorVector, gHoardingsNode4130);
}

// HoardingsBalance.cpp (SimTheme Park 2.04b3).
// PARTIAL: only 0x1002112c, 0x1002117c, 0x100211c4. No other owner code.

// Typed description rows at 0x102948a8, ending with type 12.
const BalanceDescription gHoardingsBalanceDescriptions[5] = {
    {2, "", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "X", 0, 0, 0.0f, 0.0f, 0, 0u},
    {4, "Y", 0, 0, 0.0f, 0.0f, 0, 0u},
    {3, "HoardingClicks", 0, 0, 0.0f, 0.0f, 64, 0u},
    {12, "", 0, 0, 0.0f, 0.0f, 0, 0u},
};

const BalanceDescription* CHoardingsBalance::GetBalanceDescription(int32_t row) {
    return &gHoardingsBalanceDescriptions[row];
}
BalanceValue* CHoardingsBalance::GetBalanceValuesStart() { return mValues; }
CHoardingsBalance::~CHoardingsBalance() { UnInit(); }

// The two hoardings tables (main app, SimTheme Park 2.04b3; unit w117). __sinit_HoardingsBalance_cpp (0x10021074) builds both, each with
// CBalanceTable::Init; CHoardingsBalance's constructor does not call Init itself (CMainBalance's does), so a static initialiser here does.

CHoardingsBalance gHoardingsBalance434C;   // 0x102b434c, loaded from the theme's "hoardings.sam"
CHoardingsBalance gHoardingsBalance4130;   // 0x102b4130, loaded from "endhoardings.sam"

namespace {
struct HoardingsInit {
    HoardingsInit() {
        gHoardingsBalance434C.Init();
        gHoardingsBalance4130.Init();
    }
} sHoardingsInit;
}  // namespace
