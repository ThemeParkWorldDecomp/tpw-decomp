// 0x10233378 TPGlobals::~TPGlobals (2624 bytes).
// The original calls the string destructors of the 159 members one by one from the last (0x4c7c) to the first
// (0x4024), then, when the hidden delete flag (a short, r4) is > 0, operator delete(this). The member order of the
// class is the Mac's offset order, so the implicit member destruction runs the same destructors in the same order.
// The null check and the delete flag belong to the deleting variant, which the C++ compiler generates.
#include "TPGlobals.h"

TPGlobals::~TPGlobals() {}
