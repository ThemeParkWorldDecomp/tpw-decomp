// 0x10028cb0, 352 bytes: one tick of the bumper cars (AI and move, then collision and areas).
#include <cstdint>

#include "BMP_InsideArea.h"
#include "TPAdvisor.h"

namespace {
uint32_t& W(void* p, uint32_t off) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(p) + off); }
}  // namespace

void BMP_Tick() {
    tagBUMPWORLD* world = reinterpret_cast<tagBUMPWORLD*>(&gColList);
    if (world->magic != 0x4a454647u) return;
    world->pad04 = world->pad04 + 1;
    tagBUMPSLOT* slot = world->bumps.get();
    int32_t left = world->bumpCount;
    while (left != 0) {
        if (slot->flags & 1) {
            slot->flags &= 0xffdfffffu;
            left--;
            BMP_AI(reinterpret_cast<tagBUMP*>(slot));
            if (slot->flags & 1) BMP_Move(reinterpret_cast<tagBUMP*>(slot));
        }
        slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
    }
    slot = world->bumps.get();
    left = world->bumpCount;
    while (left != 0) {
        if (slot->flags & 1) {
            tagBUMP* b = reinterpret_cast<tagBUMP*>(slot);
            left--;
            BMP_Collision(b);
            if (static_cast<int32_t>(W(slot, 0xa4)) > 0) {
                int32_t n = static_cast<int32_t>(W(slot, 0xa8));
                W(slot, 0xa8) = n + 1;
                if (n >= 0x20) {
                    if (static_cast<int32_t>(W(slot, 0xa4)) > 0x10) {
                        Ptr32<uint8_t> col{W(slot, 0x98)};
                        Ptr32<tabBUOY> buoy{W(col.get(), 0x1c)};
                        tabBUOY* target = buoy.get();
                        if (target == nullptr) {
                            Ptr32<tagBUMPRIDE> ride{W(slot, 0x9c)};
                            target = GetFinishLine(ride.get());
                        }
                        SetNextBuoyTarget(target, b);
                        slot->flags |= 0x10000u;
                    }
                    W(slot, 0xa4) = 0;
                    W(slot, 0xa8) = 0;
                }
            }
            BMP_InsideArea(b);
            BMP_OutsideArea(b);
            BMP_ControlSound(b);
        }
        slot = reinterpret_cast<tagBUMPSLOT*>(reinterpret_cast<uint8_t*>(slot) + 0xac);
    }
}
