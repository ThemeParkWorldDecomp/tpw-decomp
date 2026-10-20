// ltms shared, TbUniStringVTag::Format 0x100092f0 (64 bytes), unit w43.
#include "TbVTag.h"

int32_t TbUniStringVTag::Format(CStaticBuffer& out, TbVarFormat&) {
    out.AppendUnicode(mText, 0xffffffffu);
    return 0;
}
