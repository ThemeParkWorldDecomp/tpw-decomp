#include "TbVTag.h"
#include "platform/msl.h"

// ltms 0x1000923c: the value as text (at most 0x11 characters) appended to `out`.
int32_t TbIntegerVTag::Format(CStaticBuffer& out, TbVarFormat&) {
    char16_t text[0x11];
    int n = swprintf(text, 0x11, u"%d", mValue);   // format string at 0x1000fc8e
    out.AppendUnicode(text, n);
    return 0;
}
