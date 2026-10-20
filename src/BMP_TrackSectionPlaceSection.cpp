// BMP_TrackSectionPlaceSection 0x1002ef98 (TPAdvisor.cpp): registers a track section on a ride record: a section record, the buoys
// (0x20 bytes, from the pool at list+0x18) that fence it, and a collision object (0x1c bytes, from the pool at list+0x20).
// The list (main app data 0x102b56c0), the ride records (0xd0 bytes), the sections (0x28 bytes), the buoys and the collision
// objects are read by Mac offset; their pointer words are arena offsets (Ptr32).
#include <cmath>
#include <cstdint>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "Ptr32.h"
#include "platform/arena.h"
#include "ppc_math.h"

int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9);

namespace {

uint8_t* A(uint32_t off) { return off != 0 ? tpw::gArenaBase + off : nullptr; }
uint32_t O(const uint8_t* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
uint32_t& W(uint8_t* base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }

// The case each value of the type's low halfword (0..13) takes in the jump table at 0x1029cbd8 (0: nothing).
// 1 0x1002f138, 2 0x1002f2f0, 3 0x1002f4a0, 4 0x1002f658, 5 E (0x1002f810), 6 F (0x1002f8f8), 7 G; 8 and 9 are E and F entered
// after their "li r22, 0x100" (r22 stays 0, so the flag is 0x20 instead of 0x120).
const uint8_t kCaseOf[14] = {0, 1, 2, 3, 4, 1, 2, 3, 4, 8, 9, 7, 5, 6};

// One buoy from the pool at list+0x18, linked to the section; the pool full or missing: TRACE and the NULL the caller then reads.
uint8_t* NewBuoy(uint8_t* list, uint8_t* section, uint32_t a, uint32_t b, uint32_t c) {
    uint8_t* p = A(W(list, 0x18));
    if (p != nullptr) {
        int32_t i = 0;
        for (; i < 0x800; i++, p += 0x20)
            if ((W(p, 0) & 1) == 0) break;
        if (i >= 0x800) {
            TRACE("Out of Collision Objects \n");   // 0x1023cbba (the original's message for the buoy pool)
            p = nullptr;
        }
    } else {
        TRACE("Out of Collision Objects \n");
    }
    if (p != nullptr) {
        W(list, 0x1c) = W(list, 0x1c) + 1;
        W(p, 0) = 0x1f;
        W(p, 4) = a;
        W(p, 8) = b;
        W(p, 0x10) = c;
        W(p, 0xc) = c;
        W(p, 0x14) = 0;
        W(p, 0x18) = 0;
        W(p, 0x1c) = 0;
        W(p, 0x14) = W(section, 0x1c);
        W(section, 0x1c) = O(p);
        W(p, 0x18) = O(section);
    }
    return p;
}

}  // namespace

int32_t BMP_TrackSectionPlaceSection(int32_t id, int32_t x, int32_t y, int32_t type, int32_t f7, uint32_t f8, int32_t f9) {
    uint8_t* list = reinterpret_cast<uint8_t*>(&gColList);
    if (W(list, 0) != 0x4a454647u) return 0;
    uint8_t* ride = A(W(list, 8) + (static_cast<uint32_t>(id) & 0xffu) * 0xd0u);
    const int32_t diff = static_cast<int32_t>(O(ride) - W(list, 8));
    if (id != static_cast<int32_t>((W(ride, 0) << 8) | static_cast<uint32_t>(diff / 0xd0))) return 0;
    for (uint8_t* s = A(W(ride, 0xbc)); s != nullptr; s = A(W(s, 0x24))) {
        if (static_cast<int32_t>(W(s, 0x14)) == x && static_cast<int32_t>(W(s, 0x18)) == y) {
            TRACE("Duplicate track section - IGNORED!\n");
            return 0;
        }
    }
    uint8_t* sec = A(W(list, 0x28));
    if (sec != nullptr) {
        int32_t i = 0;
        for (; i < 0x400; i++, sec += 0x28)
            if ((W(sec, 0) & 1) == 0) break;
        if (i >= 0x400) sec = nullptr;
    }
    if (sec == nullptr) return 0;
    W(sec, 0) = 1;
    W(list, 0x2c) = W(list, 0x2c) + 1;
    W(sec, 4) = static_cast<uint32_t>(type);
    W(sec, 8) = static_cast<uint32_t>(f7);
    W(sec, 0xc) = f8;
    W(sec, 0x10) = static_cast<uint32_t>(f9);
    W(sec, 0x14) = static_cast<uint32_t>(x);
    W(sec, 0x18) = static_cast<uint32_t>(y);
    W(sec, 0x20) = O(ride);
    W(sec, 0x24) = 0;
    if (W(ride, 0xbc) == 0) {
        W(ride, 0xbc) = O(sec);
    } else {
        uint8_t* t = A(W(ride, 0xbc));
        while (W(t, 0x24) != 0) t = A(W(t, 0x24));
        W(t, 0x24) = O(sec);
    }

    const uint32_t ux = static_cast<uint32_t>(x);
    const uint32_t uy = static_cast<uint32_t>(y);
    const uint32_t a1 = W(ride, 0xc) + 0xc00u - W(ride, 8);      // 0xc00 + width
    auto a2 = [&]() { return 0xc00u - W(ride, 0xc) + W(ride, 8); };   // 0xc00 - width
    uint8_t* first = nullptr;
    uint32_t r25 = 0, r23 = 0;
    const uint32_t kind = static_cast<uint32_t>(type) & 0xffffu;
    uint8_t* p;
    const uint32_t c = kind < 14 ? kCaseOf[kind] : 0;
    switch (c) {
    case 1:
        p = NewBuoy(list, sec, ux + 0x1800, uy, a1);
        W(p, 0) &= 0xffffffebu;
        first = p;
        p = NewBuoy(list, sec, ux + 0x1800, uy, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xffffffebu;
        r25 = 0xf84;
        r23 = 0xf84;
        break;
    case 2:
        p = NewBuoy(list, sec, ux, uy, a1);
        W(p, 0) &= 0xffffffedu;
        first = p;
        p = NewBuoy(list, sec, ux, uy, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xffffffedu;
        r25 = 0x87c;
        r23 = 0xf84;
        break;
    case 3:
        p = NewBuoy(list, sec, ux, uy + 0x1800, a1);
        W(p, 0) &= 0xfffffff5u;
        first = p;
        p = NewBuoy(list, sec, ux, uy + 0x1800, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xfffffff5u;
        r25 = 0x87c;
        r23 = 0x87c;
        break;
    case 4:
        p = NewBuoy(list, sec, ux + 0x1800, uy + 0x1800, a1);
        W(p, 0) &= 0xfffffff3u;
        first = p;
        p = NewBuoy(list, sec, ux + 0x1800, uy + 0x1800, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xfffffff3u;
        r25 = 0xf84;
        r23 = 0x87c;
        break;
    case 5:
    case 8:
        p = NewBuoy(list, sec, ux + 0xc00, uy + 0xc00, 0xc20);
        W(p, 0) |= (c == 5 ? 0x120u : 0x20u);
        first = p;
        r25 = 0xc00;
        r23 = 0xc00;
        W(p, 0x10) = W(ride, 0xc) - W(ride, 8);
        break;
    case 6:
    case 9:
        p = NewBuoy(list, sec, ux + 0xc00, uy + 0xc00, W(ride, 0xc) - W(ride, 8));
        W(p, 0) |= (c == 6 ? 0x120u : 0x20u);
        first = p;
        r25 = 0xc00;
        r23 = 0xc00;
        W(p, 0x10) = 0xc20;
        break;
    case 7: {
        uint32_t d = 0xc00u - a2();
        d = d * d + 0x900000u;
        const uint32_t root = static_cast<uint32_t>(ppc_fctiwz(std::sqrt(static_cast<double>(static_cast<int32_t>(d)))));
        p = NewBuoy(list, sec, ux + 0xc00, uy + 0xc00, root);
        first = p;
        const int32_t k = static_cast<int32_t>(W(ride, 0));
        if (k == -5 || k == -8 || k == -12 || k == -13) W(p, 0) |= 0x80;
        p = NewBuoy(list, sec, ux + 0x1800, uy, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xffffffebu;
        p = NewBuoy(list, sec, ux, uy, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xffffffedu;
        p = NewBuoy(list, sec, ux, uy + 0x1800, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xfffffff5u;
        p = NewBuoy(list, sec, ux + 0x1800, uy + 0x1800, a2());
        W(p, 0) |= 0x40;
        W(p, 0) &= 0xfffffff3u;
        r25 = 0xc00;
        r23 = 0xc00;
        break;
    }
    default:
        break;
    }

    if ((static_cast<uint32_t>(type) & 0x10000u) == 0) {
        uint8_t* col = A(W(list, 0x20));
        if (col != nullptr) {
            int32_t i = 0;
            for (; i < 0x400; i++, col += 0x1c) {
                if ((W(col, 0) & 1) == 0) {
                    W(col, 4) = static_cast<uint32_t>(i);
                    break;
                }
            }
            if (i >= 0x400) col = nullptr;
        }
        if (col == nullptr) TRACE("Out of Buoys\n");   // 0x1023cbac (the original's message for the collision object pool)
        if (col != nullptr) {
            W(list, 0x24) = W(list, 0x24) + 1;
            W(col, 0) = 1;
            W(col, 8) = ux + r25;
            W(col, 0xc) = (uy + 0x1800u) - r23;
            W(col, 0x10) = 0;
            W(col, 0x14) = 0;
            W(col, 0x18) = 0;
            W(col, 0x10) = O(ride);
            W(ride, 0xac) = W(ride, 0xac) + 1;
            uint8_t* head = A(W(ride, 0xa8));
            if (head == nullptr) {
                W(ride, 0xa8) = O(col);
            } else {
                while (W(head, 0x14) != 0) head = A(W(head, 0x14));
                W(head, 0x14) = O(col);
            }
            W(col, 0x18) = O(first);
            W(first, 0x1c) = O(col);
        }
    }
    return 1;
}
