// std::basic_filebuf<wchar_t> (unit v5025). The layout keeps the Mac's 32-bit fields (Mac offsets in comments).
#pragma once
#include <cstddef>
#include <cstdint>

// The reference-counted string body the original builds for the multibyte path: 16 bytes.
struct MslStrRep {
    int32_t len;      // 0x00
    int32_t cap;      // 0x04
    int32_t refs;     // 0x08
    uint32_t data;    // 0x0c  char* (32-bit address on the host: arena memory)
};

// The NUL character the original copies into the new string (TOC slot -0x3978, address 0x102b1fb2).
extern char gMslEmptyChar;

// std::fpos<int> as the original returns it (hidden result pointer): 8 bytes.
struct FposInt {
    int32_t off;     // 0x00
    int32_t state;   // 0x04
};

struct BasicFilebufWchar {
    uint32_t mVtable;         // 0x00
    uint32_t mEback;          // 0x04 start of the get area
    uint32_t mGptr;           // 0x08
    uint32_t mEgptr;          // 0x0c
    uint8_t pad10[0x14];      // 0x10
    uint32_t mFile;           // 0x24 FILE*
    uint8_t mState[4];        // 0x28 mbstate
    uint32_t mCodecvt;        // 0x2c codecvt facet
    char16_t mBuf[14];        // 0x30 pushback buffer
    uint8_t mFlag4c;          // 0x4c
    uint8_t mUnbuffered;      // 0x4d
    uint8_t pad4e[2];         // 0x4e

    uint32_t uflow();         // 0x10221410 (1544)
    // Unit v5093 (no layout change)
    FposInt seekoff(int32_t off, int32_t dir, uint32_t mode);   // 0x10220cd4 (452)
    FposInt seekpos(int32_t off, int32_t state, uint32_t mode); // 0x10220b30 (304)
    char16_t overflow(char16_t c);                              // 0x10220f18 (1016)
    // Unit v5137 (no layout change)
    BasicFilebufWchar* close();                                 // 0x10221b40 (164)
    uint32_t underflow();                                       // 0x10221a64 (140)
    char16_t pbackfail(char16_t c);                             // 0x10221360 (96)
    void imbue(const void* loc);                                // 0x10220964 (244)
    // Unit v5743 (no layout change)
    bool unshift();                                             // 0x10226dcc (216)
    int32_t sync();                                             // 0x1021e22c (92)
    BasicFilebufWchar* destroy(int16_t flag);                   // 0x1021cd4c (220) ~basic_filebuf with the delete flag
};

// Unit v5743: callees outside the unit (fakes in the test library).
namespace filebuf_unshift_callee {
int32_t CodecvtUnshift(void* cvt, void* state, char* to, char* toEnd, char** toNext);   // __ptr_glue, vtable 0x14
int32_t Fflush(uint32_t file);                                                          // 0x10238920
void LocaleDtor(void* locale, int32_t flag);                                            // 0x10071f34
}  // namespace filebuf_unshift_callee

// Unit v5093: callees outside the unit (fakes in the test library).
namespace filebuf_wseek_callee {
int32_t CodecvtEncoding(void* cvt);                              // __ptr_glue, vtable 0x18
int32_t CodecvtOut(void* cvt, void* state, const char16_t* from, const char16_t* fromEnd, const char16_t** fromNext,
                   char* to, char* toEnd, char** toNext);        // __ptr_glue, vtable 0xc
bool Unshift(BasicFilebufWchar* fb);                             // 0x10226dcc
int32_t Fseek(uint32_t file, int32_t offset, int32_t whence);    // 0x102388f0
int32_t Ftell(uint32_t file);                                    // 0x10238908
int32_t Fputc(int32_t c, uint32_t file);                         // 0x102388a8
void StringInit(MslStrRep** s, uint32_t n, char c, void* alloc); // 0x10072e6c
void StringAssign(MslStrRep** s, uint32_t n, char c);            // 0x100730e8
void StringResize(MslStrRep** s, uint32_t n, char c);            // 0x10073558
void Delete(void* p);                                            // 0x10238218
}  // namespace filebuf_wseek_callee

// Callees outside the unit (fakes in the test library).
namespace filebuf_callee {
int32_t Fgetc(void* file);                                   // 0x10238938
void Wmemmove(void* dst, const void* src, uint32_t n);       // 0x1023abe8
int32_t CodecvtMaxLength(void* cvt);                         // __ptr_glue, vtable 0x18
int32_t CodecvtIn(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                  char16_t* to, char16_t* toEnd, char16_t** toNext);   // __ptr_glue, vtable 0x10
void* New(uint32_t size);                                    // 0x102384b8
void Delete(void* p);                                        // 0x10238218
void StringAppend(MslStrRep** s, uint32_t count, char c);    // 0x100731b4
char* StringIndex(MslStrRep** s, uint32_t i);                // 0x100733b0
}  // namespace filebuf_callee

// Unit v5046: std::basic_filebuf<char> (the narrow one). Mac offsets in comments; pointers are 32-bit words.
extern uint32_t gFilebufCharVtblA[4];   // TOC slot -0x7020, address 0x1029d0f8
extern uint32_t gFilebufCharVtblB[4];   // TOC slot -0x7018, address 0x1029d148

struct BasicFilebufChar {
    uint32_t mVtable;         // 0x00
    uint32_t mEback;          // 0x04
    uint32_t mGptr;           // 0x08
    uint32_t mEgptr;          // 0x0c
    uint32_t mZero10[3];      // 0x10 .. 0x1b
    uint32_t mLocale[2];      // 0x1c std::locale (8 bytes)
    uint32_t mFile;           // 0x24 FILE*
    uint32_t mState;          // 0x28 mbstate
    uint32_t mCodecvt;        // 0x2c codecvt facet
    uint8_t mBuf[14];         // 0x30 pushback buffer
    uint8_t mFlag3e;          // 0x3e
    uint8_t mUnbuffered;      // 0x3f

    BasicFilebufChar* construct(void* file);                    // 0x10072bbc (272)
    BasicFilebufChar* open(const char* name, int32_t mode);     // 0x10071c44 (396)
    int32_t uflow();                                            // 0x1007165c (984)
    // Unit v5233 (no layout change)
    FposInt seekpos(int32_t off, int32_t state, uint32_t mode); // 0x10070eb4 (304)
    FposInt seekoff(int32_t off, int32_t dir, uint32_t mode);   // 0x10071058 (444)
    int32_t overflow(int32_t c);                                // 0x10071294 (720)
    // Unit v51049 (src/game/HLSprite.cpp), no layout change
    void imbue(const void* loc);                                // 0x10070bf4 (244)
    int32_t pbackfail(int32_t c);                               // 0x100715b4 (88)
};

// Unit v5233: the codecvt<char> out call (__ptr_glue, vtable 0xc); the other callees are declared above
// (filebuf_wseek_callee::CodecvtEncoding/Ftell/Fputc/StringInit/StringAssign/StringResize,
// filebuf_char_callee::Fseek/Unshift/StringDtor).
// The string callees with the host's native-pointer string body (MslCharRep, declared further down in this header).
// The codecvt word at Mac 0x2c is an opaque handle on the host (arena memory is not 32-bit addressable).
struct MslCharRep;
namespace filebuf_charstr_callee {
void StringInit(MslCharRep** s, uint32_t n, char c, void* alloc);   // 0x10072e6c
void StringAssign(MslCharRep** s, uint32_t n, char c);              // 0x100730e8
void StringResize(MslCharRep** s, uint32_t n, char c);              // 0x10073558
void StringDtor(MslCharRep** s, int32_t flag);                      // 0x1000221c
}  // namespace filebuf_charstr_callee
namespace filebuf_charout_callee {
int32_t CodecvtOutChar(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                       char* to, char* toEnd, char** toNext);
}

namespace filebuf_char_callee {
void LocaleCtor(void* locale);                                  // 0x10072d24
void Getloc(void* out, void* streambuf);                        // 0x10070e1c
void* UseFacetCodecvt(void* locale);                            // 0x100739a0
void LocaleDtor(void* locale, int32_t flag);                    // 0x10071f34
int32_t CodecvtAlwaysNoconv(void* cvt);                         // __ptr_glue, vtable 0x1c
int32_t CodecvtMaxLength(void* cvt);                            // __ptr_glue, vtable 0x18
int32_t CodecvtInChar(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                      char* to, char* toEnd, char** toNext);    // __ptr_glue, vtable 0x10
void Memmove(void* dst, const void* src, uint32_t n);           // 0x102386c8
void StringCtor(MslStrRep** s, void* allocator);                // 0x1007363c
void StringDtor(MslStrRep** s, int32_t flag);                   // 0x1000221c
uint32_t Fopen(const char* name, const char* mode);             // 0x102389b0
int32_t Fseek(uint32_t file, int32_t offset, int32_t whence);   // 0x102388f0
int32_t Fclose(uint32_t file);                                  // 0x102388c0
void DeleteArray(void* p);                                      // 0x10238638
int32_t Unshift(void* filebuf);                                 // 0x10072a94
}  // namespace filebuf_char_callee

// Unit v5104: std::basic_streambuf<char> (base part, 0x24 bytes). Mac offsets in comments.
// The host object uses native pointers (arena memory is not 32-bit addressable), so its offsets differ from the Mac's.
struct BasicStreambufChar {
    const void* mVtable;      // Mac 0x00
    const void* mEback;       // Mac 0x04
    const uint8_t* mGptr;     // Mac 0x08
    const void* mEgptr;       // Mac 0x0c
    uint32_t mZero10[3];      // Mac 0x10 .. 0x1b
    uint32_t mLocale[2];      // Mac 0x1c std::locale (8 bytes)

    int32_t uflow();                          // 0x10070690 (116)
    void imbue(const void* loc);              // 0x1007048c (72)
    void getloc(void* out) const;             // 0x10070e1c (72)
    BasicStreambufChar* destroy(int16_t flag);  // 0x10070304 (144), ~basic_streambuf with the delete flag
};

namespace streambuf_callee {
int32_t Underflow(void* sb);                          // __ptr_glue, vtable 0x28
void LocaleCopyCtor(void* out, const void* src);      // 0x10072030
void LocaleAssign(void* dst, const void* src);        // 0x10072128
}  // namespace streambuf_callee

// Unit v5137: callees of close/underflow/imbue outside the unit (fakes in the test library).
namespace filebuf_wclose_callee {
int32_t VirtualUnderflow(void* fb);                    // __ptr_glue, vtable 0x2c
void Getloc(void* out, void* streambuf);               // 0x10220ab4 (locale: two 32-bit words, impl and refcount pointer)
void* UseFacetCodecvtW(void* locale);                  // 0x10225cb8
void LocaleImpDtor(void* impl, int32_t flag);          // 0x100737a0
}  // namespace filebuf_wclose_callee

// Unit v5147: std::basic_streambuf<char> xsgetn / xsputn. The full object with the put area (native pointers on the host).
// Mac offsets in comments; the host layout differs (8-byte pointers).
struct BasicStreambufCharIO {
    const void* mVtable;      // Mac 0x00
    const void* mEback;       // Mac 0x04
    uint8_t* mGptr;           // Mac 0x08
    uint8_t* mEgptr;          // Mac 0x0c
    uint8_t* mPbase;          // Mac 0x10
    uint8_t* mPptr;           // Mac 0x14
    uint8_t* mEpptr;          // Mac 0x18
    uint32_t mLocale[2];      // Mac 0x1c

    int32_t xsgetn(char* dst, int32_t count);        // 0x10070974 (276)
    int32_t xsputn(const char* src, int32_t count);  // 0x10070808 (280)
};

namespace streambuf_io_callee {
int32_t Uflow(void* sb);                  // __ptr_glue, vtable 0x2c
int32_t Overflow(void* sb, int32_t c);    // __ptr_glue, vtable 0x38
}  // namespace streambuf_io_callee

// Unit v5146: std::basic_string<char> compare 0x10209cac, substring constructor 0x101b2918, append(n, c) 0x100731b4
// (src/game/MslSupport.cpp). The string is one 32-bit word on the Mac: the address of the shared MslStrRep
// (len +0, cap +4, refs +8, data +0xc; refs 0 = not shared yet). Arena memory is 32-bit addressable, so the word is kept as is.
struct MslCharRep {
    uint32_t len;    // 0x00
    uint32_t cap;    // 0x04
    uint32_t refs;   // 0x08  0 = not shared yet
    char* data;      // 0x0c  (native pointer: +0x10 on the host)
};
struct MslCharString {
    MslCharRep* mRep;   // 0x00 (native pointer here; 4 bytes on the Mac)
};
extern char gMslCharNul;   // 0x102ac358 (TOC slot -0x4054): the terminator the substring constructor writes (zero)
namespace msl_char_string {
int32_t compare(const MslCharString* self, uint32_t pos, uint32_t n1, const char* s, uint32_t n2);          // 0x10209cac (492)
MslCharString* construct(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n);           // 0x101b2918 (792)
MslCharString* append(MslCharString* self, uint32_t n, char c);                                              // 0x100731b4 (408)
void change_size(MslCharString* self, uint32_t n, bool keep);                                                // 0x100027b0 (callee, fake in tests)
}  // namespace msl_char_string

// Unit v5196: std::_RefCountedPtr<std::__locale_imp<__int128>, std::_Single<...>>::operator= (0x10072128, 368 B).
// Native pointers on the host (the offsets differ from the Mac's; Mac offsets in comments). The impl is a std::vector<facet*>
// (Mac 0x00..0x0b) with a CharArray pointer at Mac 0x0c.
struct LocaleFacetRef {
    const void* mVtable;   // Mac 0x00
    int32_t mRefs;         // Mac 0x04
};
struct LocaleImpRef {
    uint8_t mVector[0x10];   // Mac 0x00 std::vector<facet*> (opaque here)
    void* mCharArray;        // Mac 0x0c
};
struct LocaleRefPtr {
    LocaleImpRef* mImp;   // Mac 0x00 __locale_imp*
    int32_t* mCount;      // Mac 0x04 reference count
    LocaleRefPtr& operator=(const LocaleRefPtr& other);   // 0x10072128 (368)
};
namespace locale_imp_callee {
uint32_t VecSize(void* vec);                                   // 0x10072618
LocaleFacetRef** VecIndex(void* vec, uint32_t i);              // 0x100725a0
void FacetDelete(void* facet, int32_t flag);                   // __ptr_glue, vtable 0x08
void* CharArrayRemoveRef(void* arr);                           // 0x10072688
void AllocDestroy(void* alloc, void* arr);                     // 0x1007272c
void AllocDeallocate(void* alloc, void* arr, uint32_t n);      // 0x10072364
void VecDtor(void* vec, int32_t flag);                         // 0x100724d0
void Delete(void* p);                                          // 0x10238218
void* VectorFacetDtor(void* vec, int16_t flag);                // 0x100724d0 (104, unit v5577): vector<facet*>::~vector
}  // namespace locale_imp_callee

// Unit v5665: std::__use_facet<F>(const std::locale&). The locale is one word (the impl address, 32-bit); the impl's
// std::vector<facet*> has the count at +4 and the data pointer at +8. Facets are 32-bit words (arena is 32-bit addressable).
extern uint32_t gLocaleIdCount;          // 0x103bd06c id_count_s__Q33std6locale2id
extern uint32_t gIdCtypeChar;            // 0x103bd074 std::ctype<char>::id
extern uint32_t gIdCodecvtCharChar;      // 0x103bcfb4 std::codecvt<char,char,int>::id
extern uint32_t gIdCtypeWchar;           // 0x103bd0d8 std::ctype<wchar_t>::id
extern uint32_t gIdCodecvtWcharChar;     // 0x103bd03c std::codecvt<wchar_t,char,int>::id
struct UseFacetObj {          // a facet: vtable word (a Mac constant) +0, reference count +4
    uint32_t mVtable;         // Mac 0x00
    int32_t mRefs;            // Mac 0x04
};
struct UseFacetImp {          // the locale impl; the pointers are native on the host (8 bytes, 4 on the Mac)
    uint32_t mPad;            // Mac 0x00
    uint32_t mCount;          // Mac 0x04 vector count
    void** mData;             // Mac 0x08 vector data
};
struct UseFacetLocale {
    UseFacetImp* mImp;        // Mac 0x00
};
void* UseFacetCtypeChar(const UseFacetLocale* locale);          // 0x10073b54 (424)
void* UseFacetCodecvtCharChar(const UseFacetLocale* locale);    // 0x100739a0 (340)
void* UseFacetCtypeWchar(const UseFacetLocale* locale);         // 0x102261a0 (348)
void* UseFacetCodecvtWcharChar(const UseFacetLocale* locale);   // 0x102298e4 (416)
namespace use_facet_callee {
void* New(uint32_t size);                                              // 0x102384b8
void CtypeCharCtor(void* p, const void* table, bool del, uint32_t n);  // 0x10238980
void CodecvtCharCtor(void* p, uint32_t n);                             // 0x102388d8
void CodecvtWcharCtor(void* p, uint32_t n);                            // 0x1023b128
void VecResize(UseFacetImp* vec, uint32_t n, const void* value);       // 0x10073d58
void VecInsert(UseFacetImp* vec, void** pos, uint32_t count, void* const* value);   // 0x10073ed4
void VecErase(UseFacetImp* vec, void** first, void** last);            // 0x10073e10
void FacetDelete(void* facet, int32_t flag);                           // 0x1023baa8, vtable +8
}  // namespace use_facet_callee

// Unit v5253: std::basic_string<char> constructors, destructor, init, assign, resize, append (src/game/MslSupport.cpp).
// MslCharString / MslCharRep as above (Mac rep: len +0, cap +4, refs +8, data +0xc). The callees are IME::MslNew / MslNewRep /
// MslNewArray / MslDelete / MslStrlen / MslStrcpy / MslThrow / MslRefCountedCharsCtor (ui_ime.h) and msl_char_string::change_size.
namespace msl_char_string {
MslCharString* constructEmpty(MslCharString* self);                                                    // 0x1007363c (240)
MslCharString* constructFromCStr(MslCharString* self, const char* text, const void* allocator);        // 0x10002338 (252)
MslCharString* destroy(MslCharString* self, int16_t flag);                                             // 0x1000221c (188)
void init(MslCharString* self, const char* first, const char* last);                                   // 0x10002b8c (248)
MslCharString* assignFill(MslCharString* self, uint32_t n, char c);                                    // 0x100730e8 (104)
void resize(MslCharString* self, uint32_t n, char c);                                                  // 0x10073558 (128)
MslCharString* assignChars(MslCharString* self, const char* text, uint32_t n);                         // 0x101b2440 (108)
MslCharString* appendChars(MslCharString* self, const char* text, uint32_t n);                         // 0x1020100c (136)
}  // namespace msl_char_string
namespace msl_string_callee {
void* Memcpy(void* to, const void* from, uint32_t n);   // memcpy glue 0x102386b0 (weak real definition in the src file, the test overrides)
void* Memset(void* to, int32_t c, uint32_t n);          // memset glue 0x10238560
}  // namespace msl_string_callee
extern char gMslInitNul;   // 0x10278bd2 (TOC slot -0x5d54): the terminator init writes (zero)
extern char gMslCtorNul;   // 0x1029d294 (TOC slot -0x53b0): the terminator the default constructor writes (zero)

// Unit v5376: assign(str, pos, n) 0x10001c80 (src/game/MslSupport.cpp); change_size (declared above) is defined there too.
// replace(pos, n1, n2, c) 0x10002d20 is a callee (fake in the test).
extern char gMslChangeSizeNul;   // 0x10278bd3 (TOC slot -0x5d50): the terminator change_size writes (zero)
namespace msl_char_string {
MslCharString* assign(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n);   // 0x10001c80 (732)
void replace(MslCharString* self, uint32_t pos, uint32_t n1, uint32_t n2, char c);                // 0x10002d20 (callee, fake in tests)
}  // namespace msl_char_string

// Unit v5941 (src/game/MslSupport.cpp): replace(pos, n1, n2, c) 0x10002d20 (1176) and init(n, c, allocator) 0x10072e6c (516).
extern char gMslReplaceNulCopy;    // 0x10278bd0 (TOC slot -0x5d58): the terminator replace writes on the copy path (zero)
extern char gMslReplaceNulPlace;   // 0x10278bd1 (TOC slot -0x5d5c): the terminator replace writes in place (zero)
extern char gMslInitFillNul;       // 0x1029d295 (TOC slot -0x53ac): the terminator init(n, c) writes (zero)
namespace msl_char_string {
void init(MslCharString* self, uint32_t n, char c, const void* allocator);   // 0x10072e6c (516)
}  // namespace msl_char_string

// Unit v5916 (src/game/MslSupport.cpp): std::vector<const void*> insert 0x10073ed4 (736),
// erase 0x10073e10 (116), resize 0x10073d58 (108) and std::__locale_imp::~__locale_imp 0x100737a0 (352).
// The elements are native pointers on the host (8 bytes; 4 on the Mac), the data pointer too.
struct MslPtrVector {
    uint32_t mCapacity;    // Mac 0x00
    uint32_t mSize;        // Mac 0x04
    const void** mData;    // Mac 0x08
    void insert(const void** pos, uint32_t n, const void* const* value);   // 0x10073ed4
    const void** erase(const void** first, const void** last);             // 0x10073e10
    void resize(uint32_t n, const void* value);                            // 0x10073d58
};
struct LocaleCharArray {   // the reference counted character array of a locale impl
    uint32_t mWord0;       // Mac 0x00
    uint32_t mWord4;       // Mac 0x04
    uint32_t mRefs;        // Mac 0x08
    void* mData;           // Mac 0x0c (native pointer here)
};
struct LocaleImp {         // std::__locale_imp<__int128>: the facet vector, then the character array
    MslPtrVector mFacets;          // Mac 0x00 (12 bytes there)
    LocaleCharArray* mChars;       // Mac 0x0c
};
LocaleImp* LocaleImpDestroy(LocaleImp* self, int16_t deleteFlag);   // 0x100737a0 (the destructor with the hidden flag)

// Unit v5944 (src/game/MslSupport.cpp): basic_filebuf<wchar_t>(FILE*) 0x10226ef4 (272), open 0x10221c30 (396),
// callees: filebuf_char_callee (LocaleCtor, LocaleDtor, CodecvtAlwaysNoconv, Fopen, Fseek, Fclose, DeleteArray),
// filebuf_wclose_callee (Getloc, UseFacetCodecvtW), filebuf_wseek_callee::Unshift.
BasicFilebufWchar* FilebufWcharConstruct(BasicFilebufWchar* self, uint32_t file);                      // 0x10226ef4
BasicFilebufWchar* FilebufWcharOpen(BasicFilebufWchar* self, const char* name, uint32_t mode);         // 0x10221c30

// std::__tree<std::pair<const nocase_string, BuddyInfo>, ...>: native pointers on the host (arena memory is not 32-bit addressable);
// Mac offsets in comments, host offsets after them.
struct BuddyTreeNode {
    BuddyTreeNode* mLeft;        // Mac 0x00 (host 0x00)
    BuddyTreeNode* mRight;       // Mac 0x04 (host 0x08)
    uintptr_t mParentColor;      // Mac 0x08 (host 0x10) parent | colour bit 0
    uint8_t mPair[8];            // Mac 0x0c (host 0x18) std::pair<const nocase_string, BuddyInfo>
};
struct BuddyTree {
    uint32_t mCount;             // Mac 0x00 element count (0xffffffff = full)
    BuddyTreeNode* mRoot;        // Mac 0x04 (host 0x08) header node passed to balance_insert
    void* mField8;               // Mac 0x08 (host 0x10)
    BuddyTreeNode* mLeftmost;    // Mac 0x0c (host 0x18)
};
BuddyTreeNode* BuddyTreeInsertNodeAt(BuddyTree* self, BuddyTreeNode* parent, bool left, bool setLeftmost, const void* pair);   // 0x102273bc (536)
namespace buddy_tree_callee {
uint32_t Strlen(const char* s);                                  // 0x10238650
void* NewArray(uint32_t size);                                   // 0x10238668
void* New(uint32_t size);                                        // 0x102384b8
char* Strcpy(char* to, const char* from);                        // 0x10238410
void Throw(const char* type, void* object, const void* dtor);    // 0x10238680
void* PairCtor(void* self, const void* src);                     // 0x10227840
void BalanceInsert(void* node, void* root);                      // 0x10153b24
}  // namespace buddy_tree_callee

// Unit v5950 (src/game/online/email.cpp): std::operator==(const string&, const string&) 0x1020702c (76).
// Answers compare(a, 0, a.size, b.data, b.size) == 0.
namespace msl_char_string {
bool equal(const MslCharString* a, const MslCharString* b);
}

// Unit v51025 (src/game/online/online.cpp): the buddy map's tree (BuddyTree above): find 0x1021f8d8 (196),
// insert_one 0x10222d9c (236), destroy 0x10222564 (604), erase 0x102278fc (324). The iterator is the node pointer; end() is the address of
// mRoot (Mac tree + 4). The pair at the node is a BuddyEntry (online.h): the key's rep pointer, then the two flag bytes.
struct BuddyTreeInsertResult {   // the pair<iterator, bool> insert_one answers through a hidden pointer
    BuddyTreeNode* mNode;        // Mac 0x00 (host 0x00)
    bool mInserted;              // Mac 0x04 (host 0x08)
};
BuddyTreeNode* BuddyTreeFind(BuddyTree* self, const void* key);                        // 0x1021f8d8 (answers the node, end() when absent)
void BuddyTreeInsertOne(BuddyTreeInsertResult* result, BuddyTree* self, const void* pair);   // 0x10222d9c
void BuddyTreeDestroy(BuddyTree* self, BuddyTreeNode* node);                           // 0x10222564 (frees the subtree, node included)
void BuddyTreeErase(BuddyTree* self, BuddyTreeNode* position);                         // 0x102278fc
namespace buddy_tree_callee {
void AllocDestroy(void* allocator, void* pair);                      // 0x10222c78 std::allocator<pair>::destroy
void Deallocate(void* allocator, void* node, uint32_t count);        // 0x102229f8 std::allocator<node>::deallocate
void Delete(void* p);                                                // 0x10238218 operator delete
}  // namespace buddy_tree_callee

// Unit v51033 (src/game/MslSupport.cpp). Free functions (the receiver may be NULL for the destructor).
char* MslCharStringIndex(MslCharString* self, uint32_t i);                  // 0x100733b0 (328) basic_string<char>::operator[]
extern LocaleRefPtr gLocaleGlobal;                                           // 0x103bcfd4 (TOC slot -0x7034) std::locale::global_s
LocaleRefPtr* LocaleConstruct(LocaleRefPtr* self);                           // 0x10072d24 (160) std::locale::locale()
LocaleRefPtr* LocaleRefPtrDestroy(LocaleRefPtr* self, int16_t flag);         // 0x10071f34 (136) ~_RefCountedPtr with delete flag
struct BasicIosChar;                                                         // opaque: basic_ios<char>
char BasicIosWiden(const BasicIosChar* self, char c);                        // 0x10072960 (136)
BasicFilebufChar* FilebufCharClose(BasicFilebufChar* self);                  // 0x10071b54 (164)
int32_t FilebufCharUnderflow(BasicFilebufChar* self);                        // 0x10071a80 (132)
namespace v51033_callee {
const LocaleRefPtr* LocaleClassic();                                         // std::locale::classic, glue 0x10238998
char CtypeWiden(void* facet, char c);                                        // __ptr_glue, vtable 0x1c
}  // namespace v51033_callee

// Unit v51049 (src/game/HLSprite.cpp): std::locale::~locale 0x10070d44 (172). The locale is two 32-bit words
// (impl and count pointers; arena memory is 32-bit addressable). Callees: filebuf_wclose_callee::LocaleImpDtor, locale_imp_callee::Delete.
struct StdLocaleWords {
    uint32_t mImp;     // Mac 0x00
    uint32_t mCount;   // Mac 0x04
};
StdLocaleWords* LocaleStdDestroy(StdLocaleWords* self, int16_t flag);   // 0x10070d44

// Unit v51054 (src/game/rides/FlyingRide.cpp): small locale / streambuf / filebuf / ios_base::failure helpers. No layout changes.
namespace IME { struct MslExceptionObject; }
struct IosBaseLocalePtr {      // std::ios_base prefix
    uint8_t pad0[0x20];
    const LocaleRefPtr* mLocale;   // Mac 0x20 address of a std::locale (two words: impl, count pointer); native pointer on the host
};
namespace v51054 {
void* UseFacet_CtypeChar(const UseFacetLocale* locale);              // 0x10073934 (40)
void* UseFacet_CodecvtCharChar(const UseFacetLocale* locale);        // 0x1007245c (40)
void* UseFacet_CtypeWchar(const UseFacetLocale* locale);             // 0x1021eb10 (40)
void* UseFacet_CodecvtWcharChar(const UseFacetLocale* locale);       // 0x10225cb8 (40)
UseFacetObj* FacetDestroy(UseFacetObj* self, int16_t flag);          // 0x10072df0 (76) std::locale::facet::~facet
bool FilebufCharUnshift(BasicFilebufChar* self);                     // 0x10072a94 (216)
int32_t FilebufCharSync(BasicFilebufChar* self);                     // 0x100703e4 (92)
void IosBaseGetloc(LocaleRefPtr* out, const IosBaseLocalePtr* ios);   // 0x10072a34 (48)
void CharArrayAllocDestroy(void* alloc, LocaleCharArray* arr);       // 0x1007272c (48)
LocaleCharArray* CharArrayRemoveRef(LocaleCharArray* arr);           // 0x10072688 (52)
uint32_t FacetVectorSize(const MslPtrVector* vec);                   // 0x10072618 (8)
void** FacetVectorIndex(MslPtrVector* vec, uint32_t i);              // 0x100725a0 (16)
void CharArrayAllocDeallocate(void* alloc, void* arr, uint32_t n);   // 0x10072364 (40)
LocaleRefPtr* LocaleRefPtrCopy(LocaleRefPtr* self, const LocaleRefPtr* other);   // 0x10072030 (44)
FposInt StreambufSeekoff(BasicStreambufChar* self, int32_t off, int32_t dir, uint32_t mode);     // 0x10070b60 (20)
FposInt StreambufSeekpos(BasicStreambufChar* self, int32_t off, int32_t state, uint32_t mode);   // 0x10070ad8 (20)
int32_t StreambufOverflow(BasicStreambufChar* self, int32_t c);      // 0x100707b0 (8)
int32_t StreambufPbackfail(BasicStreambufChar* self, int32_t c);     // 0x10070754 (8)
int32_t StreambufUnderflow(BasicStreambufChar* self);                // 0x10070634 (8)
int32_t StreambufShowmanyc(BasicStreambufChar* self);                // 0x100705d8 (8)
int32_t StreambufSync(BasicStreambufChar* self);                     // 0x10070584 (8)
BasicStreambufChar* StreambufSetbuf(BasicStreambufChar* self, char* buffer, int32_t n);   // 0x10070530 (4)
IME::MslExceptionObject* IosFailureDtor(IME::MslExceptionObject* self, int16_t flag);     // 0x1007020c (136)
const char* IosFailureWhat(const IME::MslExceptionObject* self);     // 0x100702c8 (8)
}  // namespace v51054

// Unit v51062 (src/COnlineControl_..._NoLongerAFK.cpp): std::basic_streambuf<wchar_t> (the base part, 0x24 bytes on the Mac), its trivial
// virtuals, std::ctype<wchar_t>::do_widen and the buddy map's erase_one. The vtable word is a native pointer on the host, so the fields after
// it sit 4 bytes higher (the locale is two words on the Mac, the host's LocaleRefPtr is two pointers); Mac offsets in comments.
struct BasicStreambufWchar {
    const void* mVtable;     // Mac 0x00 (host 0x00)
    uint8_t mBody[0x18];     // Mac 0x04..0x1b (host 0x08..0x1f): the get and put areas
    LocaleRefPtr mLocale;    // Mac 0x1c (host 0x20) std::locale

    BasicStreambufWchar* destroy(int16_t flag);              // 0x1021e14c (144), ~basic_streambuf with the delete flag
    void imbue(const void* loc);                             // 0x1021e2d4 (72): the locale is assigned
    BasicStreambufWchar* setbuf(char16_t* buffer, int32_t count);   // 0x1021e378 (4): a bare return, so r3 stays this
    int32_t sync();                                          // 0x1021e3cc (8): 0
    int32_t showmanyc();                                     // 0x1021e420 (8): 0
    int32_t underflow();                                     // 0x1021e47c (12): 0xffff
    int32_t pbackfail(int32_t c);                            // 0x1021e5a8 (12): 0xffff
    int32_t overflow(int32_t c);                             // 0x1021e608 (12): 0xffff
};
char16_t CtypeWcharDoWiden(const void* self, char c);        // 0x1021d584 (8): extsb
// 0x1021e8e8 erase_one on the buddy map: lower bound with value_compare (this + 8), then erase; answers 0 or 1.
namespace buddy_erase_callee {
bool ValueCompareNodeKey(void* compare, const void* pair, const void* key);   // 0x1021f000 value_compare::operator()(pair, key)
bool ValueCompareKeyNode(void* compare, const void* key, const void* pair);   // 0x1021eed0 value_compare::operator()(key, pair)
}  // namespace buddy_erase_callee
uint32_t BuddyTreeEraseOneCount(BuddyTree* self, const void* key);            // 0x1021e8e8 (220)
