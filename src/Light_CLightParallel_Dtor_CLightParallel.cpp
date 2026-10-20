#include "Light.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"

#include <cstdint>
#include <new>

#include "platform/arena.h"

unsigned char gCLightParallelVt1[0x40];   // 0x10278c60
unsigned char gCLightParallelVt2[0x40];   // 0x10278c9c
unsigned char gCLightParallelVt3[0x40];   // 0x10278cb4

namespace {
// A Mac address is an offset from the arena base (include/Ptr32.h).
inline uint32_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(p) - tpw::gArenaBase); }
inline uint32_t Rd(uint32_t a) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + a); }
inline void Wr(uint32_t a, uint32_t v) { *reinterpret_cast<uint32_t*>(tpw::gArenaBase + a) = v; }
}  // namespace

// The import operator delete (0x10238218); weak so the unit's test library can replace it with a recording fake.
__attribute__((weak)) void CLightParallel_Delete(void* block) { ::operator delete(block); }

// 0x10007750: restores the vtable words the Mac destructor writes, then deletes when the flag is positive.
CLightParallel* CLightParallel_DtorMac(CLightParallel* self, int16_t flag) {
    if (self == nullptr) return self;
    uint32_t t = Off(self);
    uint32_t t2 = Off(gCLightParallelVt2);   // r10
    uint32_t t3 = Off(gCLightParallelVt3);   // r9
    uint32_t t1 = Off(gCLightParallelVt1);   // r3
    Wr(t + 4, t1);
    Wr(Rd(t) + 0x5c, t1 + 0x24);
    Wr(Rd(Rd(t)), t1 + 0xc);
    uint32_t b = Rd(Rd(t));
    Wr(b + 4, (t + 8) - b);
    uint32_t a = Rd(t);
    Wr(a + 0x60, (t + 0x10) - a);
    if (flag != 0) {
        if (t + 0x10 != 0) {
            Wr(t + 0x6c, t3);
            uint32_t r = Rd(t + 0x10);
            Wr(r, t3 + 0x18);
            Wr(r + 4, (t + 0x70) - r);
        }
        if (t + 8 != 0) Wr(t + 8, t2);
    }
    if (flag > 0) CLightParallel_Delete(self);
    return self;
}

// 0x101cff88: stores the value, tells both draw objects (Change, flag 1, frame = the low 16 bits), then the window.
int32_t InterfaceImage::SetValue(int32_t value) {
    mValue = value;
    DOChange change;
    change.mFrame = static_cast<int16_t>(value);
    if (mDrawObject != nullptr) mDrawObject->Change(1, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(1, &change);
    InterfaceWindow::SetValue(value);
    return 1;
}
