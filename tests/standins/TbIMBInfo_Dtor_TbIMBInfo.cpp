// The weak stand-ins of the unit library tpw_unit_TbIMBInfo_Dtor_TbIMBInfo
// (cmake/units/TbIMBInfo_Dtor_TbIMBInfo.cmake), as src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp of before the
// regroup (v5L30b) defined them, unchanged, with the file's asm statement that makes the vtables and typeinfo they
// emit weak too (the regroup had left it in src/ltms/TbFontRenderParams.cpp, where it made the real ones weak). The
// regroup put each function into the src file of its original source file, where these defaults would sit next to
// the real definitions, so it dropped them; the unit library compiles them from here, so that it builds what it
// built before.

#include "TbResource.h"
#include "CStaticBuffer.h"
#include "TbITable.h"
#include "CCharacterHandler.h"

// Lead 2026-10-20 (v51128): src/TbIMBToUnicode.cpp and src/TbIRLE4bitFont.cpp define the same two empty destructors (untested,
// and seven unit test libraries link them), so these two are weak, and so are the vtables and typeinfo they emit as the key
// functions (as src/TbAudioBody_create_SamsUtilities_UCommand_SetImmediate.cpp does): libtpw links those, this unit's test
// library takes these.
asm(".weak_definition __ZTV9TbIMBInfo\n.weak_definition __ZTI9TbIMBInfo\n.weak_definition __ZTS9TbIMBInfo\n"
    ".weak_definition __ZTV7TbIFont\n.weak_definition __ZTI7TbIFont\n.weak_definition __ZTS7TbIFont");

// 0x10002f3c: stores the TbIMBInfo vtable, then TbResource's (the compiler's chain); deletes when asked.
__attribute__((weak)) TbIMBInfo::~TbIMBInfo() {}
// 0x10002cb8
__attribute__((weak)) TbIFont::~TbIFont() {}
