// Unit v5L50: where a ride record's word sits on the host, for difftests whose world holds a ride record (RideInfo). The PPC side
// writes the word at the Mac's offset; the host's record is laid out wider (a vtable in the balance table and 8-byte value slots), so
// the host side writes it at the field of the same name. tests/ridelayout.py is the Python side.
// A unit file includes this once: the two functions are then exported from its test library.
#pragma once

#include <cstddef>
#include <cstdint>

#include "RideDB.h"

extern "C" {

// The host offset of the RideInfo word the Mac has at mac: a value slot (the Mac's 0x0c + 4 * slot, the slot's low word), the ride type
// number (0x78c), the UI type (0x7a8) or a sub ride type (0x7b8 + 4 * i). Any other offset answers -1.
int64_t tpw_ride_hostoffset(int32_t mac) {
    const int64_t values = static_cast<int64_t>(offsetof(RideInfo, mBalance) + offsetof(CRideBalance, mValues));
    if (mac >= 0x0c && mac < 0x0c + 4 * kRideSlot_SlotCount && (mac & 3) == 0)
        return values + static_cast<int64_t>((mac - 0x0c) / 4) * static_cast<int64_t>(sizeof(BalanceValue));
    if (mac == 0x78c) return static_cast<int64_t>(offsetof(RideInfo, mRideTypeNumber));
    if (mac == 0x7a8) return static_cast<int64_t>(offsetof(RideInfo, mUIType));
    if (mac >= 0x7b8 && mac < 0x7d8 && (mac & 3) == 0)
        return static_cast<int64_t>(offsetof(RideInfo, mSubRideTypes)) + (mac - 0x7b8);
    return -1;
}

int64_t tpw_ride_size() { return static_cast<int64_t>(sizeof(RideInfo)); }

}  // extern "C"
