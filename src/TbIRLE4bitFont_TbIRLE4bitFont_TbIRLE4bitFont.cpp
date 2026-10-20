#include "TbIRLE4bitFont.h"
#include "TbIResourceIndex.h"
#include "platform/ltms.h"
#include "platform/samsutils.h"

// 0x100040d8: the base constructor, the vtable, then the three file pointers cleared.
// mMBToUnicode (Mac +0x14) is not touched.
TbIRLE4bitFont::TbIRLE4bitFont() {
    mData = nullptr;
    mHeader = nullptr;
    mOffsets = nullptr;
}
