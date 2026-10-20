#include <cstdint>
#include <string>
#include "online.h"
#include "ui_ime.h"

uint8_t gCParserVtableBlock[16];   // 0x102b1b3c

// 0x10211064
CParser::CParser() {
    char unusedAllocator;
    char lessA, allocA, lessB, allocB;
    (void)unusedAllocator;
    mVtable = gCParserVtableBlock + 8;
    online_callee::WStringCtor(&mString);
    parser_callee::TreeCtor(mSeparators, &lessA, &allocA);
    parser_callee::TreeCtor(mWhitespace, &lessB, &allocB);
    SetupSeparatorSet();
    SetupWhitespaceSet();
}

// 0x10211344
void CParser::SetupWhitespaceSet() {
    AddWhitespace(u' ');
    AddWhitespace(u'\n');
    AddWhitespace(u'\t');
    AddWhitespace(u'\r');
}
