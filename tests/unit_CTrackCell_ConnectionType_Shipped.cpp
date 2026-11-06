// Proof library (libtpw_unit_CTrackCell_ConnectionType_Shipped.dylib, tests/test_CTrackCell_ConnectionType_Shipped.py). The Mac's
// CTrackCell::ConnectionType writes the words 0x102de0b4 and 0x102de0b8, the words the mouse code calls gLLCCDE0B4 and gLLCCDE0B8.
// gLLCCE0B4 and gLLCCE0B8 are the separate pylon words 0x1029e0b4 and 0x1029e0b8, initially -1 (LLCellRide.cpp), which the Mac's
// ConnectionType never touches; here they have their shipped initial values, so a ConnectionType using them fails the test.
#include "unit_CTrackCell_ConnectionType.cpp"
int32_t gLLCCE0B4 = -1;   // src/game/park/LLCellRide.cpp
int32_t gLLCCE0B8 = -1;   // src/game/park/LLCellRide.cpp

extern "C" {
void tpw_ct_set_pylon(int32_t x, int32_t y) {
    gLLCCE0B4 = x;
    gLLCCE0B8 = y;
}
int32_t tpw_ct_get_pylon(int32_t which) { return which == 0 ? gLLCCE0B4 : gLLCCE0B8; }
}
