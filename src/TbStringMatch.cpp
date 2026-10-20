// TbStringMatch (bullfrog library): a wildcard mask kept as a list of parts. A part is a run
// of plain text, or a '*' or '?' followed by the plain text after it; a run of wildcards
// after a '*' is one '*'. MatchString walks the parts: '?' takes exactly one character, '*'
// goes to the first place its text occurs (it never tries a later one), and a last part of
// plain text or '?' must end the string, while anything may follow a last '*' part.
#include "TbStringMatch.h"
#include <cstring>

TbCharStringList::~TbCharStringList() {}
TbStringMatch::~TbStringMatch() {}

TbError TbStringMatch::SetMask(const char* mask) {
    while (mCount != 0) {
        TbDynamicStringTemplate<char> removed = RemoveHead();
    }
    TbDynamicStringTemplate<char> source(mask);
    TbDynamicStringTemplate<char> part;
    const char* cursor = source.mData;
    while (*cursor != '*' && *cursor != '?' && *cursor != 0) ++cursor;
    if (cursor > source.mData) {
        part = source.Left(static_cast<uint32_t>(cursor - source.mData));
        AddTail(part);
    }
    if (*cursor != 0) {
        part = *cursor++;
        while (*cursor != 0) {
            if (*cursor == '*' || *cursor == '?') {
                AddTail(part);
                part = *cursor;
                if (*cursor == '*') {
                    while (cursor[1] == '?' || cursor[1] == '*') ++cursor;
                }
                ++cursor;
            } else part += *cursor++;
        }
        AddTail(part);
    }
    return TbError(0);
}

bool TbStringMatch::MatchString(const char* text) { return MatchSubString(text, mHead); }
bool TbStringMatch::MatchSubString(const char* text, void* position) {
    if (position == nullptr) return true;
    auto* node = static_cast<CNode*>(position);
    const char* pattern = node->mData.mData;
    if (*pattern == '?') {
        if (*text == 0) return false;
        if (std::strncmp(text + 1, pattern + 1, node->mData.Length() - 1) != 0) return false;
    } else if (*pattern == '*') {
        const char* found = std::strstr(text, pattern + 1);
        if (found == nullptr) return false;
        return MatchSubString(found + node->mData.Length() - 1, node->mNext);
    } else if (std::strncmp(text, pattern, node->mData.Length()) != 0) return false;
    const int32_t length = node->mData.Length();
    return node->mNext == nullptr ? text[length] == 0 : MatchSubString(text + length, node->mNext);
}
