#include <cstring>
#include <new>
#include "FS_Online.h"
#include "FS_System.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "Text.h"
#include "platform/samsutils.h"

// 0x10178308. The static initialiser of FS_Online.cpp: two global wide strings are built from literals (at 0x1024d390 + 6 and + 0x14)
// and each is registered with its destructor for the exit-time teardown.
static const char16_t kLiteralA[] = u"upload";   // 0x1024d396
static const char16_t kLiteralB[] = u"gms.dat";   // 0x1024d3a4

void sinit_FS_Online_cpp() {
    new (&gUploadNameTail.mString) TbDynamicStringTemplate<char16_t>(kLiteralA);
    __register_global_object(&gUploadNameTail, reinterpret_cast<void*>(&UIMain_DestroyWString), gOnlineSinitNodeA);
    new (&gOnlineSinitString2.mString) TbDynamicStringTemplate<char16_t>(kLiteralB);
    __register_global_object(&gOnlineSinitString2, reinterpret_cast<void*>(&UIMain_DestroyWString), gOnlineSinitNodeB);
}
