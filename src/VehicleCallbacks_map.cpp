// VehicleCallbacks.cpp (main app, SimTheme Park 2.04b3; unit w209): the two small callbacks of Vehicle::next_step that turn a vehicle's 16.16 position into a map
// square and the square into its number. The nav rules next_step asks, map_local_to_global and the candidate walks are unit w218's (VehicleCallbacks_nav.cpp).
#include "VehicleSteer.h"

#include "World.h"

// 0x101359e8 (20 bytes): the map square of a position: each coordinate's whole part (a LOGICAL shift of the 32-bit word; the word is not sign extended).
Vector2d map_global_to_local(uint32_t x, uint32_t y) {
    Vector2d local;
    local.x = static_cast<Fixed>(x >> 16);
    local.y = static_cast<Fixed>(y >> 16);
    return local;
}

// 0x10135fd4 (108 bytes): the number of a map square, x + (y * 128 + 1), as 16 bits; -1 for a square outside the level (CMap::IsXYValid). The third argument (the
// map's data) is never read.
int32_t map_fn(int32_t x, int32_t y, void* data) {
    if (CMap::IsXYValid(x, y) == 0) return -1;
    return static_cast<uint16_t>(static_cast<uint32_t>(x) + ((static_cast<uint32_t>(y) << 7) + 1u));
}
