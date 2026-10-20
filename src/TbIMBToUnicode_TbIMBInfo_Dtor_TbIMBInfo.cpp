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

// 0x10000b24: Close runs through the virtual slot (at 0xc), then the converter is released (slot 8) if still held.
CStaticMBBuffer::~CStaticMBBuffer() {
    Close();
    if (mConverter != nullptr) mConverter->Release();
}

// 0x10000024
CCharacterHandler::CCharacterHandler() {}

// 0x10000060
CCharacterHandler::~CCharacterHandler() {}
