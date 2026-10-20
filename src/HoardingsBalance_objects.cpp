// The two hoardings tables (main app, SimTheme Park 2.04b3; unit w117). __sinit_HoardingsBalance_cpp (0x10021074) builds both, each with
// CBalanceTable::Init; CHoardingsBalance's constructor does not call Init itself (CMainBalance's does), so a static initialiser here does.
#include "HoardingsBalance.h"

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
