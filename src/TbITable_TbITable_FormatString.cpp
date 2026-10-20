// TbITable::FormatString, the multibyte overload (ltms 0x10004e30).
#include <cstdarg>
#include <cstdint>

#include "CStaticBuffer.h"
#include "TbITable.h"

// CStaticMBBuffer's Close and appends are unit v5312's: src/CStaticUnicodeBuffer_CStaticMBBuffer_AppendMB.cpp.
// Lead 2026-10-20 (v51128): its destructor (0x10000b24, the vtable with it) is translated in src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp;
// the Mac inlines it here, and the buffer's destructor at the return does that Close and Release.

int32_t TbITable::FormatString(unsigned char* out, uint32_t size, uint32_t id, ...) const {
    CStaticMBBuffer buffer;
    buffer.mCursor = out;
    buffer.mEnd = out + size;
    buffer.mConverter = mUnicodeToMB;
    if (buffer.mConverter) buffer.mConverter->AddRef();
    buffer.mOverflow = false;
    va_list args;
    va_start(args, id);
    const int32_t result = DoFormat(buffer, id, reinterpret_cast<char*>(args));
    va_end(args);
    return result;
}
