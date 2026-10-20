// TPAdvisor.cpp (main app of SimTheme Park 2.04b3). PARTIAL: only the text file reader
// (0x10030c08..0x10031014) so far; the quick-load indexes are read with it. The advisor's model is in
// src/TPAdvisor_model.cpp.
//
// The reader knows four classes of character: letters (A-Z, a-z), digits, punctuation
// (every other printable ASCII character but the space) and the two comment characters
// '#' and ';' (which are punctuation too). Everything else is white space. It looks at the
// character at the cursor without asking first whether the cursor is still inside the
// text, so the character just past the end can end up in a string.
#include "TPAdvisor.h"

namespace {
bool Letter(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool Digit(char c) { return c >= '0' && c <= '9'; }
bool Punctuation(char c) {
    return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~');
}
bool Comment(char c) { return c == '#' || c == ';'; }
}  // namespace

int32_t IsComment(sFileParse* parse) { return Comment(*parse->mCursor) ? 1 : 0; }
int32_t IsLetter(sFileParse* parse) { return Letter(*parse->mCursor) ? 1 : 0; }
int32_t IsNumber(sFileParse* parse) { return Digit(*parse->mCursor) ? 1 : 0; }
int32_t IsMiscChar(sFileParse* parse) { return Punctuation(*parse->mCursor) ? 1 : 0; }

int32_t IsWhiteSpace(sFileParse* parse) {
    const char c = *parse->mCursor;
    return Letter(c) || Digit(c) || Punctuation(c) || Comment(c) ? 0 : 1;
}

int32_t SkipWhitespace(sFileParse* parse) {
    for (;;) {
        if (parse->mSize <= parse->mPos) return 1;
        if (IsComment(parse) != 0 || IsLetter(parse) != 0 || IsNumber(parse) != 0 || IsMiscChar(parse) != 0) return 0;
        if (parse->mSize <= parse->mPos) return 1;
        ++parse->mCursor;
        ++parse->mPos;
    }
}

int32_t GetString(sFileParse* parse, char* text, int32_t size, int32_t letters, int32_t digits, int32_t punctuation,
                  int32_t comments) {
    int32_t count = 0;
    *text = '\0';
    if (parse->mSize <= parse->mPos) return 1;
    for (;;) {
        if (size - 1 <= count) return 0;
        const char c = *parse->mCursor;
        const bool wanted = (letters != 0 && Letter(c)) || (digits != 0 && Digit(c)) || (punctuation != 0 && Punctuation(c)) ||
                            (comments != 0 && Comment(c));
        if (!wanted) return 0;
        ++count;
        *text++ = c;
        *text = '\0';
        if (parse->mSize <= parse->mPos) return 1;
        ++parse->mCursor;
        ++parse->mPos;
    }
}
