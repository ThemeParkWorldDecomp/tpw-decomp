// ltms TbTextFormatter (the range 0x10005cc0..0x10008d00 of "after TbIResourceIndex.cpp"): lays a string out in a rectangle
// (lines, justification, wrapping, Hebrew runs) and draws it character by character through TbFontRender. Unit w35a.
//
// Host design. The Mac object is 0xb4 bytes with three Metrowerks std::vector members; here the vectors are MslVector (the
// same three words, the data pointer native) and the line records hold native pointers into the character vector. The
// constructor leaves the same fields alone as the Mac's (+0x94, +0x98, +0x9c and +0xb0 are never set by it).
#pragma once
#include <cstdint>

#include "TbError.h"
#include "TbFontRender.h"
#include "TbGraphicFile.h"

// The character types GetCharType hands out, as flag bits of CCharacterInfo::mType (the names are inferred from the users).
enum : uint8_t {
    kCharNormal = 0x01,       // an ordinary character
    kCharDelimiter = 0x02,    // space and tab: trailing ones are dropped at a wrap and are not drawn there
    kCharLineBreak = 0x04,    // line feed and carriage return: ends a line, takes no room
    kCharCarriageReturn = 0x08,   // carriage return only: a break in front of one is absorbed into it
    kCharWordBreak = 0x10,    // space and tab: a wrapped line may end here
    kCharHidden = 0x40,       // set by LayoutLines on the delimiters dropped at a wrap: DrawText skips them
};

// One character of the string. Mac 0x10 bytes (the halfword at +4 holds the flags in its high byte; the other padding bytes
// are never written, so the Mac's vector holds whatever was on the stack there).
struct CCharacterInfo {
    uint16_t mChar;        // +0x00  the UTF-16 character (0 when the font's table has no such character)
    uint16_t mIndex;       // +0x02  the font's character index
    uint8_t mType;         // +0x04  the kChar flags
    uint8_t mPad5;         // +0x05
    uint16_t mPad6;        // +0x06
    int32_t mWidth;        // +0x08  the advance, from the font
    uint16_t mPosition;    // +0x0c  the character's number in the string (a halfword: wraps after 65,535)
    uint16_t mPadE;        // +0x0e
};
static_assert(sizeof(CCharacterInfo) == 0x10, "CCharacterInfo is 0x10 bytes");

// One line: the characters [mBegin, mEnd) of the character vector and its width. Mac 0xc bytes with 32-bit pointers.
struct CLineInfo {
    CCharacterInfo* mBegin;   // +0x00
    CCharacterInfo* mEnd;     // +0x04
    uint32_t mWidth;          // +0x08
};

// The Metrowerks std::vector: capacity, size and the data, in that order (Mac 0xc bytes). tear_down lets go of the data
// block (the loop over the elements in the original counts down and does nothing: they have no destructors). The other
// members are not translated yet.
template <class T>
struct MslVector {
    uint32_t mCapacity;   // +0x00
    uint32_t mSize;       // +0x04
    T* mData;             // +0x08
    void tear_down();
    void clear();
    void push_back(const T& element);
    void resize(uint32_t size);
    // Unit w44: the general erase and insert resize is built on (src/ltms_textformatter_rest.cpp); only MslVector<TbPoint> has them.
    T* erase(T* first, T* last);
    void insert(T* position, uint32_t count, const T& value);
};
template <> void MslVector<CCharacterInfo>::tear_down();   // 0x10007cb4
template <> void MslVector<CLineInfo>::tear_down();        // 0x1000839c
template <> void MslVector<TbPoint>::tear_down();          // 0x1000879c
// Unit w44, src/ltms_textformatter_rest.cpp (the size drops to 0, the block stays; push_back doubles the capacity from 1; resize pads
// with zero points through insert and cuts through erase):
template <> void MslVector<CCharacterInfo>::clear();                              // 0x10007d74
template <> void MslVector<CCharacterInfo>::push_back(const CCharacterInfo&);     // 0x10007e0c
template <> void MslVector<CLineInfo>::clear();                                   // 0x10008454
template <> void MslVector<CLineInfo>::push_back(const CLineInfo&);               // 0x100084e4
template <> void MslVector<TbPoint>::clear();                                     // 0x1000884c
template <> void MslVector<TbPoint>::resize(uint32_t size);                       // 0x100088d4
template <> TbPoint* MslVector<TbPoint>::erase(TbPoint* first, TbPoint* last);             // 0x10008be0
template <> void MslVector<TbPoint>::insert(TbPoint* position, uint32_t count, const TbPoint& value);   // 0x10008cf4

// The file-static line processor (0x10006cfc): puts the characters of [first, last) in display order for right-to-left runs.
// Unit w44 (src/ltms_textformatter_rest.cpp). The original's helpers (IsParenthesis, IsDelimiter, GetNextDelimiter, GetNextNonDelimiter,
// IsHebInLine, InvertLine) are inlined into it; FixParenthesis is the one real call.
void ProcessLine(CCharacterInfo* first, CCharacterInfo* last);
// 0x1000691c: swaps the bracket at `first` with the bracket that closes it (or opens it) in [first, last).
void FixParenthesis(CCharacterInfo* first, CCharacterInfo* last);
// Unit v5757 (src/ltms_textformatter_rest_InvertLine.cpp). Each takes [first, last). (InvertLine 0x10006c3c is file-local in
// src/ltms_textformatter_rest.cpp; the unit's file has its own copy for the test, declared in tests/unit_InvertLine.cpp.)
bool IsHebInLine(CCharacterInfo* first, CCharacterInfo* last);                      // 0x10006bb4
CCharacterInfo* GetNextNonDelimiter(CCharacterInfo* first, CCharacterInfo* last);   // 0x10006b04
CCharacterInfo* GetNextDelimiter(CCharacterInfo* first, CCharacterInfo* last);      // 0x10006a68

// Mac 0xb4 bytes, vtable 0x10011478 (only the destructor, slot 8).
class TbTextFormatter {
public:
    // The values the two setters accept: horizontal 0, 1 and 2; vertical 2, 4 and 5 (3 is accepted by neither). The
    // original's enumerator names are not known.
    enum eJustification : int32_t { kLeft = 0, kRight = 1, kCentre = 2, kTop = 4, kBottom = 5 };

    TbTextFormatter();                                                        // 0x10005dc4
    virtual ~TbTextFormatter();                                               // 0x100061a8
    TbError SetVerticalJustification(eJustification value);                   // 0x10006288: -1 unless 2, 4 or 5
    TbError SetHorizontalJustification(eJustification value);                 // 0x1000632c: stores 0 to 2, and ALWAYS answers -1
    const TbPoint* GetCharPositionArray() const;                              // 0x100063b4
    uint32_t GetCharPositionArraySize() const;                                // 0x100063fc
    // 0x10006448: the answer for a string with no characters: one position, at the corner of `rect` the justification names.
    TbError EmptyStringProcess(const TbRect& rect);
    // 0x100065b8 and 0x100066f4: CreateInfoList, then (unless the string is empty) the wrap width, Layout and DrawText. Draw
    // always answers 0 once the layout is done, whatever DrawText says. Measure draws nothing unless +0x88 is set (it then
    // measures: DrawText without drawing), answers -1 without an output rectangle, and leaves it alone for an empty string.
    TbError Draw(const TbRect& rect, const uint8_t* text, TbFontRenderParams& params, TbRect* drawn);
    TbError Measure(const TbRect& rect, const uint8_t* text, TbFontRenderParams& params, TbRect* measured);
    // 0x1000700c: walks the lines (ProcessLine first), fills the position array and, when `draw` is set, draws each character
    // through mRender and answers the union of what was drawn in `bounds` (zeros when nothing was).
    TbError DrawText(TbFontRenderParams& params, TbRect* bounds, bool draw);
    // 0x10007514: the character list of `text` (bytes through the font's converter); -1 without a font or a converter. Takes a
    // reference on the font through GetFont and never lets it go.
    TbError CreateInfoList(const uint8_t* text, TbFontRenderParams& params);
    uint8_t GetCharType(char16_t character);                                  // 0x100076ec
    TbError Layout(TbRect rect);                                              // 0x1000776c: always 0
    int32_t LayoutLines(CCharacterInfo* first, CCharacterInfo* last);         // 0x10007994: the lines made
    int32_t GetCharIdxFromPosition(TbPoint position);                         // 0x10007c64: always 0 (not implemented)

    TbFontRender mRender;                  // +0x04 (0x58 bytes)
    MslVector<CCharacterInfo> mChars;      // +0x5c  the string's characters
    uint16_t mCharCounter;                 // +0x68  the next character number (a halfword)
    MslVector<CLineInfo> mLines;           // +0x6c
    uint32_t mMaxLineWidth;                // +0x78  the widest line
    MslVector<TbPoint> mPositions;         // +0x7c  where each character starts, and one more for the end
    uint8_t mMeasurePositions;             // +0x88  Measure draws without drawing (to fill the positions) when set
    TbPoint mCaret;                        // +0x8c  the position after the last character
    int32_t mVerticalJustification;        // +0x94
    int32_t mHorizontalJustification;      // +0x98
    uint32_t mWrapWidth;                   // +0x9c  the width a line may have: Draw and Measure store right - left (compared unsigned)
    TbRect mBounds;                        // +0xa0  the text block Layout placed (left, top, right, bottom)
    int32_t mLineHeight;                   // +0xb0  the font's vertical spacing

    // Kept from unit w35b's partial header (it names the justifications with these enumerators; same values, same type).
    static constexpr eJustification kJustifyLeft = kLeft;
    static constexpr eJustification kJustifyRight = kRight;
    static constexpr eJustification kJustifyCentre = kCentre;
    static constexpr eJustification kJustifyTop = kTop;
    static constexpr eJustification kJustifyBottom = kBottom;
};

// Unit v5297 (src/TbResource_CLineList_AddLine.cpp): the list of lines. Mac 0x10 bytes: the vector of lines, then the widest width.
// (TbTextFormatter::mLines and mMaxLineWidth at +0x6c and +0x78 are this object.) On the host the vector holds a native
// pointer, so mMaxWidth sits after it; the test reads the host offset from the layout.
class CLineList {
public:
    MslVector<CLineInfo> mLines;   // +0x00
    uint32_t mMaxWidth;            // +0x0c
    // 0x10005cc0: pushes the line {begin, end, width}, raises mMaxWidth to width when it is smaller (unsigned), and sets
    // kCharHidden on every character of [hideFrom, end).
    void AddLine(CCharacterInfo* begin, CCharacterInfo* hideFrom, CCharacterInfo* end, uint32_t width);
    // 0x10005f28: the destructor with the CodeWarrior hidden delete flag (a signed 16 bit value): nothing for NULL; else
    // tear_down of the vector and, for a flag above 0, operator delete. Answers this.
    CLineList* Destruct(int16_t deleteFlag);
};

// Unit v5749 (src/ltms_textformatter_std_length_error_Dtor_length_error.cpp): std::length_error, Mac 0x0c bytes.
struct MslRefCountedCharsBlock {   // std::_RefCountedPtr<char, std::_Array<char>>
    char* mPtr;                    // +0x00 of the member
    int32_t* mCount;               // +0x04 of the member
};
struct LengthError {
    const void* mVtable;                 // +0x00
    MslRefCountedCharsBlock mMessage;    // +0x04
};
extern char gLengthErrorVtbl[16];
extern char gLogicErrorVtbl[16];
extern char gExceptionVtbl[16];
// 0x10008a10: std::_RefCountedPtr<char, std::_Array<char>>::~_RefCountedPtr (flag is the hidden delete flag).
void MslRefCountedCharsDtor(MslRefCountedCharsBlock* self, int16_t deleteFlag);
// 0x10008108: std::length_error::~length_error: nothing for NULL; else the vtable steps, the message's destructor with -1,
// and operator delete for a flag above 0 (signed). Answers this.
LengthError* LengthErrorDtor(LengthError* self, int16_t deleteFlag);
void LengthErrorOperatorDelete(void* block);   // weak: the runtime's operator delete (0x1000f694)
// Unit v51035 (src/TbUniStringVTag_TbIntegerVTag_Format_std__RefCountedPtr__RefCountedPtr.cpp):
void* LengthErrorOperatorNew(uint32_t size);   // the runtime's operator new (0x1000f76c); never answers NULL
// 0x10008adc: std::_RefCountedPtr<char, std::_Array<char>>::_RefCountedPtr(char*): stores the pointer; for a non-NULL one a
// counter of 4 bytes holding 1 is allocated. Answers this. (The exception cleanup path in the listing is unreachable.)
MslRefCountedCharsBlock* MslRefCountedCharsBlockCtor(MslRefCountedCharsBlock* self, char* p);
// 0x100081c4: std::logic_error::~logic_error (hidden signed 16 bit delete flag): nothing for NULL; else the logic_error vtable,
// the message's destructor with -1, the exception vtable, operator delete for a flag above 0. Answers this.
LengthError* LogicErrorDtor(LengthError* self, int16_t deleteFlag);

// Unit v51120 (src/ltms_textformatter_rest_IsDelimiter.cpp). The destructors with the CodeWarrior hidden delete flag (a signed 16
// bit value, operator delete for a flag above 0) answer `this`; the three Metrowerks vectors' and CCharInfoList's take the object.
#include "CStaticBuffer.h"
#include "TbResource.h"
#include "TbVTag.h"
// 0x100060ec, 0x10005fb0, 0x10005e7c: std::vector<T,std::allocator<T>>::~vector (tear_down, then the delete).
MslVector<CCharacterInfo>* MslVectorCCharacterInfoDtor(MslVector<CCharacterInfo>* self, int16_t deleteFlag);
MslVector<CLineInfo>* MslVectorCLineInfoDtor(MslVector<CLineInfo>* self, int16_t deleteFlag);
MslVector<TbPoint>* MslVectorTbPointDtor(MslVector<TbPoint>* self, int16_t deleteFlag);
// 0x10006060: CCharInfoList::~CCharInfoList: the character vector at +0.
struct CCharInfoList {
    MslVector<CCharacterInfo> mChars;   // +0x00
};
CCharInfoList* CCharInfoListDtor(CCharInfoList* self, int16_t deleteFlag);
// 0x10008274: std::_MSLstring::~_MSLstring: the counted character block at +0 (its destructor with -1), then the delete.
struct MslString {
    MslRefCountedCharsBlock mText;   // +0x00
};
MslString* MslStringDtor(MslString* self, int16_t deleteFlag);
// 0x100089a0: std::logic_error::what: the message pointer at +4. 0x100089d8: std::exception::what: the text "exception"
// (the data at TOC 0x188 + 0x1f).
const char* LogicErrorWhat(const LengthError* self);
const char* MslExceptionWhat(const void* self);
// 0x1000439c and 0x100043e4: SamsUtilities::USwapInplace32<unsigned long> and USwapInplace16<unsigned short> in ltms.
void LtmsUSwapInplace32ULong(uint32_t& word);
void LtmsUSwapInplace16UShort(uint16_t& half);
// 0x1000514c, 0x10004f64: TbRefReleaseSmartPtr<T>::~TbRefReleaseSmartPtr: lets go of the held resource (Release, vtable +8).
template <class T>
struct TbRefReleaseSmartPtr {
    T* mPtr;   // Mac +0
    ~TbRefReleaseSmartPtr();
};
// 0x1000937c: TbMBStringVTag::Format: appends the multibyte string at +0xc to the buffer (length -1); answers 0.
class TbMBStringVTag : public TbVTag {
public:
    int32_t Format(CStaticBuffer& out, TbVarFormat& format) override;
    const unsigned char* mString;   // Mac +0xc
};
// 0x10004068: __sinit_TbIResourceIndex_cpp: the five resource factories' vtable pointers (0x10011754 .. 0x10011764). The vtables
// themselves are stand-ins (the factories are not translated).
extern char gRLE4bitFontFactoryVtbl[16];
extern char gTableFactoryVtbl[16];
extern char gMBInfoFactoryVtbl[16];
extern char gUnicodeToMBFactoryVtbl[16];
extern char gMBToUnicodeFactoryVtbl[16];
extern const void* gRLE4bitFontFactoryVptr;    // 0x10011754
extern const void* gTableFactoryVptr;          // 0x10011758
extern const void* gMBInfoFactoryVptr;         // 0x1001175c
extern const void* gUnicodeToMBFactoryVptr;    // 0x10011760
extern const void* gMBToUnicodeFactoryVptr;    // 0x10011764
void __sinit_TbIResourceIndex_cpp();
