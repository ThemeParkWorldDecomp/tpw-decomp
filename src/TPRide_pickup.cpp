// TPRide.cpp (main app), unit w143: GetPickupWorldPosn (0x10067220, 8 bytes): `lwz r3, TOC -> 0x102c6cec; blr`, the address of the world
// point the pointer picks up (gPickupWorldPosn, src/GameSys.cpp).
#include "TPEngine.h"

s3DPoint* GetPickupWorldPosn() {
    return &gPickupWorldPosn;
}
