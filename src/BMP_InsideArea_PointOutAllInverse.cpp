// Unit v5112: point tests over the collision list (PointOutAllInverse, PointInAnyColObject, NavPointInArea).
#include "BMP_InsideArea.h"

int32_t PointOutAllInverse(int32_t x, int32_t y) {
    tabCOLOBJECT* p = gColList.first.get();
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                n--;
                if ((f & 0x40) != 0 && PointInColObject(x, y, p) != 0) return 0;
            }
            p++;
        }
    }
    return 1;
}

tabCOLOBJECT* PointInAnyColObject(int32_t x, int32_t y) {
    tabCOLOBJECT* p = gColList.first.get();
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                n--;
                if ((f & 0x40) == 0 && PointInColObject(x, y, p) != 0) return p;
            }
            p++;
        }
    }
    return nullptr;
}

NS_MacDoze::tagPOINT* NavPointInArea(tabCOLOBJECT* object, NS_MacDoze::tagPOINT* out) {
    int32_t* pt = reinterpret_cast<int32_t*>(out);   // x at +0, y at +4
    pt[0] = object->x;
    pt[1] = object->y;
    uint32_t f = object->flags;
    if ((f & 8) == 0) pt[1] = pt[1] - 0x87c;
    else if ((f & 0x10) == 0) pt[1] = pt[1] + 0x87c;
    if ((f & 2) == 0) pt[0] = pt[0] + 0x87c;
    else if ((f & 4) == 0) pt[0] = pt[0] - 0x87c;
    return out;
}
