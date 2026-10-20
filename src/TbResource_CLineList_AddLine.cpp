// CLineList (ltms, 0x10005cc0 and 0x10005f28): the lines of the text formatter.
#include <new>

#include "TbTextFormatter.h"

// 0x10005cc0
void CLineList::AddLine(CCharacterInfo* begin, CCharacterInfo* hideFrom, CCharacterInfo* end, uint32_t width) {
    CLineInfo line;
    line.mBegin = begin;
    line.mEnd = end;
    line.mWidth = width;
    mLines.push_back(line);
    mMaxWidth = (mMaxWidth < width) ? width : mMaxWidth;
    for (CCharacterInfo* c = hideFrom; c != end; ++c) c->mType |= kCharHidden;
}

// 0x10005f28
CLineList* CLineList::Destruct(int16_t deleteFlag) {
    if (this != nullptr) {
        mLines.tear_down();
        if (deleteFlag > 0) ::operator delete(static_cast<void*>(this));
    }
    return this;
}
