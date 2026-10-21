// Test library for the region shapes' virtual members (libtpw_unit_UICoolShapes.dylib; see tests/test_UICoolShapes.py):
// src/game/ui/CoolRegion.cpp (PointIn, Shift, Adjust and the destructor of CoolCircle, CoolEllipse and CoolPoly), under it the
// constructors (src/game/ui/CoolRegion.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/InterfaceListBox.cpp, src/game/ui/InterfaceListControl2.cpp) and the base class (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). Nothing is faked but the allocator, which counts
// (tests/calllog.cpp).
//
// The Python module drives this library through extern "C" operations on three shapes in slots: make one, ask PointIn, Shift,
// Adjust, destroy it (with the Mac's flag: positive deletes the object as well).
#include <cstdint>
#include <new>

#include "calllog.h"
#include "UI_Base.h"

namespace {
constexpr int kSlots = 3;
CoolRegion* gShapes[kSlots];
int gKinds[kSlots];   // 1 circle, 2 ellipse, 3 polygon, 0 none
int64_t Heap[4];
}  // namespace

// The unit allocates with new (std::nothrow); the library's default for that form does not go through the counting operator new.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return operator new(size); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return operator new[](size); }

extern "C" {

// Forgets the shapes (the objects of an earlier case are leaked: the case that made them may have destroyed them already).
void tpw_ucs_reset() {
    for (int i = 0; i < kSlots; ++i) {
        gShapes[i] = nullptr;
        gKinds[i] = 0;
    }
    for (int i = 0; i < 4; ++i) Heap[i] = 0;
}

// Makes a shape in a slot: kind 1 circle (a[0..2]), 2 ellipse (a[0..3]), 3 polygon (a[0] points, `points` their numbers). The
// constructors are not what is under test here, so their heap traffic is not counted.
void tpw_ucs_make(int32_t kind, int32_t slot, const int64_t* a, const int16_t* points) {
    const int32_t before[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    if (kind == 1)
        gShapes[slot] = new CoolCircle(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]));
    else if (kind == 2)
        gShapes[slot] = new CoolEllipse(static_cast<int16_t>(a[0]), static_cast<int16_t>(a[1]), static_cast<int16_t>(a[2]),
                                        static_cast<int16_t>(a[3]));
    else
        gShapes[slot] = new CoolPoly(static_cast<int16_t>(a[0]), points);
    gKinds[slot] = kind;
    for (int i = 0; i < 4; ++i) tpw_cl_heap[i] = before[i];
}

// Writes into a polygon's fields without going through the constructor (so that the count and the array can disagree).
void tpw_ucs_poke(int32_t slot, int32_t count, int32_t nullPoints) {
    auto* poly = static_cast<CoolPoly*>(gShapes[slot]);
    poly->mCount = static_cast<int16_t>(count);
    if (nullPoints != 0) {
        delete[] poly->mPoints;
        poly->mPoints = nullptr;
    }
}

int64_t tpw_ucs_pointin(int32_t slot, int32_t x, int32_t y) {
    return gShapes[slot]->PointIn(static_cast<int16_t>(x), static_cast<int16_t>(y)) ? 1 : 0;
}

void tpw_ucs_shift(int32_t slot, int32_t dx, int32_t dy) { gShapes[slot]->Shift(static_cast<int16_t>(dx), static_cast<int16_t>(dy)); }

void tpw_ucs_adjust(int32_t slot, int32_t a, int32_t b, int32_t c, int32_t d) {
    gShapes[slot]->Adjust(static_cast<int16_t>(a), static_cast<int16_t>(b), static_cast<int16_t>(c), static_cast<int16_t>(d));
}

// The destructor with the Mac's flag: positive also deletes the object, anything else destroys it where it is.
void tpw_ucs_destroy(int32_t slot, int32_t flagWord) {
    const int16_t flag = static_cast<int16_t>(flagWord);   // (a short on the Mac)
    const int32_t before[4] = {tpw_cl_heap[0], tpw_cl_heap[1], tpw_cl_heap[2], tpw_cl_heap[3]};
    CoolRegion* shape = gShapes[slot];
    if (flag > 0) {
        delete shape;
    } else {
        shape->~CoolRegion();
    }
    for (int i = 0; i < 4; ++i) Heap[i] += tpw_cl_heap[i] - before[i];
    if (flag <= 0) ::operator delete(shape);   // (after the counting)
    gShapes[slot] = nullptr;
}

// A shape's words: circle x, y, radius; ellipse x, y, radii; polygon count, points present, the numbers. Answers how many.
int32_t tpw_ucs_dump(int32_t slot, int64_t* out) {
    int n = 0;
    if (gKinds[slot] == 1) {
        const auto* c = static_cast<const CoolCircle*>(gShapes[slot]);
        out[n++] = c->mX;
        out[n++] = c->mY;
        out[n++] = c->mRadius;
    } else if (gKinds[slot] == 2) {
        const auto* c = static_cast<const CoolEllipse*>(gShapes[slot]);
        out[n++] = c->mX;
        out[n++] = c->mY;
        out[n++] = c->mRadiusX;
        out[n++] = c->mRadiusY;
    } else if (gKinds[slot] == 3) {
        const auto* c = static_cast<const CoolPoly*>(gShapes[slot]);
        out[n++] = c->mCount;
        out[n++] = c->mPoints != nullptr;
        if (c->mPoints != nullptr)
            for (int i = 0; i < 2 * c->mCount; ++i) out[n++] = c->mPoints[i];
    }
    return n;
}

void tpw_ucs_heap(int64_t* out) {
    for (int i = 0; i < 4; ++i) out[i] = Heap[i];
}

}  // extern "C"
