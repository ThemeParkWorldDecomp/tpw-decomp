// MslRuntimeGlue.cpp: the real definitions of the Metrowerks runtime, locale and wide-ostream names the translated library code of
// MslSupport.cpp and its neighbours calls through (ui_ime.h, std_basic_istream.h, std_basic_filebuf.h, email.h, online.h,
// std_ctype_wchar.h, TPMain.h, TbSoundCodecs.h, TbTextFormatter.h, std_basic_streambuf_wchar.h). Only libtpw compiles this file:
// the unit test libraries list their own sources and fake these names themselves, so nothing here changes what a unit test compiles.
//
// Layout rules the shims follow (the translations mix two layouts, see the findings of the unit report):
//  - a block the translated code keeps in a 32-bit field (the std::locale of an ios_base) comes from the arena, the field holds
//    the arena offset;
//  - a block the translated code only reaches through a native pointer (facets, the classic locale's impl, exception messages)
//    comes from the host heap (IME::MslNew grants a zeroed block of twice the size), and operator delete frees either kind;
//  - __throw logs and returns, as IME::MslThrow does: the unit tests were checked on a __throw that returns.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "TPMain.h"
#include "TbSoundCodecs.h"
#include "TbTextFormatter.h"
#include "email.h"
#include "online.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"
#include "std_basic_streambuf_wchar.h"
#include "std_ctype_wchar.h"
#include "std_exception_what.h"
#include "ui_ime.h"

// ---- Variables.

// std::length_error's vtable block (0x10278b74, initialised data, original words 0x10278b6c, 0, 0x10265680, 0x10278b6c is the type
// descriptor and 0x10265678 the next block) and the TVector of its destructor (0x10265680, words 0x100024ac ~length_error and
// 0x10267cc0, the TOC of the main app). The words that name Mac data have no host referent and stay zero; the vtable's third word
// points at the destructor vector (the original does, too) and the vector's first word at the translated ~length_error, which
// fills the 8 bytes with a native pointer.
namespace IME {
char gMslLengthErrorDtorTV[8] = {};   // 0x10265680
char gMslLengthErrorVtbl[16] = {};    // 0x10278b74
char16_t gMslWcharNulA = 0;           // 0x102b1434: original 0 (initialised data, a zero halfword)
char16_t gMslWcharNulB = 0;           // 0x102b1436: original 0
}  // namespace IME

namespace {
struct LengthErrorTablesInit {
    LengthErrorTablesInit() {
        const void* destructor = reinterpret_cast<const void*>(&IME::MslLengthErrorDtor);
        std::memcpy(IME::gMslLengthErrorDtorTV, &destructor, sizeof(destructor));
        const uint32_t vector = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(IME::gMslLengthErrorDtorTV));
        std::memcpy(IME::gMslLengthErrorVtbl + 8, &vector, sizeof(vector));
    }
} gLengthErrorTablesInit;

// operator delete: arena blocks go back to the arena, anything else to the heap.
void AnyDelete(void* p) {
    if (p == nullptr) return;
    if (tpw::InArena(p)) {
        tpw::ArenaFree(p);
    } else {
        ::operator delete(p);
    }
}

uint32_t GetWord(const void* base, size_t offset) {
    uint32_t w;
    std::memcpy(&w, static_cast<const uint8_t*>(base) + offset, sizeof(w));
    return w;
}
void PutWord(void* base, size_t offset, uint32_t w) { std::memcpy(static_cast<uint8_t*>(base) + offset, &w, sizeof(w)); }
uint32_t LowWord(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- IME game functions: std::string and std::list<std::string> of the main app on the host's layout (native pointers).

// 0x101b2918 basic_string(const basic_string&, pos, n). MslString / MslStrRep have the layout of MslCharString / MslCharRep.
void IME::MslString_construct(MslString* self, const MslString* src, uint32_t pos, uint32_t n) {
    msl_char_string::construct(reinterpret_cast<MslCharString*>(self), reinterpret_cast<const MslCharString*>(src), pos, n);
}

// 0x10209f04 operator<(a, b): a.compare(0, a.size(), b.data(), b.size()) < 0.
bool IME::MslString_less(const MslString* a, const MslString* b) {
    const int32_t result = msl_char_string::compare(reinterpret_cast<const MslCharString*>(a), 0, a->mRep->mLength, b->mRep->mData,
                                                    b->mRep->mLength);
    return result < 0;
}

// 0x1020abec (432) std::list<std::string>::splice(pos, other): moves all of other before pos. A list that would grow past 0x7fffffff
// elements throws std::length_error (the throw returns here and the splice goes on, as in the translation v51070::StrListSplice,
// which keeps the Mac words and so cannot take this list).
void IME::MslStrList_splice(MslStrList* self, MslStrNode* pos, MslStrList* other) {
    const uint32_t otherSize = other->mSize;
    if (otherSize == 0) return;
    if (self->mSize > 0x7fffffffu - otherSize) {
        MslThrowObject object;
        static const char kMessage[] = "list::splice length error";   // the text the Mac builds on its stack
        object.mVtbl = gMslLengthErrorVtbl;
        object.mMessage = static_cast<char*>(MslNewArray(static_cast<uint32_t>(sizeof(kMessage))));
        object.mCount = nullptr;
        if (object.mMessage != nullptr) {
            object.mCount = static_cast<int32_t*>(MslNew(4));
            if (object.mCount != nullptr) *object.mCount = 1;
            MslStrcpy(object.mMessage, kMessage);
        }
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, gMslLengthErrorDtorTV);
    }
    MslStrNode* sentinel = reinterpret_cast<MslStrNode*>(&other->mHead);
    MslStrNode* first = other->mHead.mNext;
    MslStrNode* last = other->mHead.mPrev;
    other->mHead.mNext = sentinel;
    other->mHead.mPrev = sentinel;
    self->mSize = self->mSize + other->mSize;
    other->mSize = 0;
    pos->mPrev->mNext = first;
    first->mPrev = pos->mPrev;
    pos->mPrev = last;
    last->mNext = pos;
}

// ---- std::ctype<wchar_t>::classify (0x10010da0 in c/c++_shared, reached through the main app's glue 0x1023b140).
// The library keeps one halfword of class bits per character below 0x100 (the table at TOC r2-0x2668 = 0x1003fab8 of c/c++_shared,
// 256 halfwords, the values below are those bytes) and maps them to the MSL mask in a fixed order; a character from 0x100 up has
// no bits and so answers 0.
namespace {
const uint16_t kCtypeWcharBits[256] = {
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0004, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0030, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x00a0, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080,
    0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0008, 0x0008, 0x0008, 0x0008, 0x0001,
    0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0080, 0x0080,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0080, 0x0080, 0x0080, 0x0080, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040, 0x0080, 0x0008, 0x0008, 0x0008, 0x0008, 0x0040, 0x0040,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080, 0x0080,
    0x0008, 0x0080, 0x0080, 0x0080, 0x0080, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0000,
};
}  // namespace

uint32_t StdCtypeWchar::classify(char16_t c) const {
    const uint32_t bits = c < 0x100 ? kCtypeWcharBits[c] : 0;
    if ((bits & 0x08) != 0) return 8;
    const bool low = (bits & 0x03) != 0;
    const bool mid = (bits & 0x06) != 0;
    if (low && !mid) return 1;
    if (low && mid) return 2;
    if (!low && mid) return 4;
    if ((bits & 0x20) != 0) {
        if ((bits & 0x10) != 0) return 0x30;
        if ((bits & 0x40) != 0) return 0x60;
        return 0xa0;
    }
    if ((bits & 0x40) != 0) return 0x40;
    if ((bits & 0x80) != 0) return 0x80;
    return 0;
}

// ---- The locale runtime. The classic locale is built once on the host heap in the layout the translated locale code reads: a
// LocaleRefPtr (impl pointer, count pointer) whose impl is an empty facet vector and the name array "C" (the library's
// std::locale::classic 0x100106f0 builds the same through make_classic 0x100107dc: a __locale_imp, the name "C", a count of 1). The
// count stays at least 1 so the classic locale is never freed.
namespace {
const LocaleRefPtr* BuildClassicLocale() {
    static LocaleRefPtr classic;
    LocaleImp* imp = new LocaleImp();
    LocaleCharArray* name = new LocaleCharArray();
    char* text = new char[4]();
    text[0] = 'C';
    name->mWord0 = 1;                  // length
    name->mWord4 = (1 + 3) - (1 & 3);  // capacity
    name->mRefs = 0;                   // one owner
    name->mData = text;
    imp->mChars = name;
    classic.mImp = reinterpret_cast<LocaleImpRef*>(imp);
    classic.mCount = new int32_t(1);
    return &classic;
}
}  // namespace

// 0x10238998 std::locale::classic() (glue into c/c++_shared).
const LocaleRefPtr* v51033_callee::LocaleClassic() {
    static const LocaleRefPtr* classic = BuildClassicLocale();
    return classic;
}

// __ptr_glue vtable 0x1c: ctype<char>::widen(c) calls do_widen, which returns the character unchanged (0x1000aef0).
char v51033_callee::CtypeWiden(void* facet, char c) {
    (void)facet;
    return c;
}

// locale_imp_callee: the vector, character array and facet helpers of the locale impl (host layout: native pointers).
uint32_t locale_imp_callee::VecSize(void* vec) {   // 0x10072618
    return ::v51054::FacetVectorSize(static_cast<const MslPtrVector*>(vec));
}

// 0x100725a0 steps four bytes per element (the Mac's pointer size), and the host vector holds eight byte pointers: the host
// equivalent is the element's address in the pointer array.
LocaleFacetRef** locale_imp_callee::VecIndex(void* vec, uint32_t i) {
    MslPtrVector* v = static_cast<MslPtrVector*>(vec);
    return reinterpret_cast<LocaleFacetRef**>(const_cast<void**>(v->mData + i));
}

void locale_imp_callee::FacetDelete(void* facet, int32_t flag) {   // __ptr_glue vtable 0x08: the facet's deleting destructor
    ::v51054::FacetDestroy(static_cast<UseFacetObj*>(facet), static_cast<int16_t>(flag));
}

void* locale_imp_callee::CharArrayRemoveRef(void* arr) {   // 0x10072688
    return ::v51054::CharArrayRemoveRef(static_cast<LocaleCharArray*>(arr));
}

void locale_imp_callee::AllocDestroy(void* alloc, void* arr) {   // 0x1007272c
    ::v51054::CharArrayAllocDestroy(alloc, static_cast<LocaleCharArray*>(arr));
}

void locale_imp_callee::AllocDeallocate(void* alloc, void* arr, uint32_t n) {   // 0x10072364
    ::v51054::CharArrayAllocDeallocate(alloc, arr, n);
}

void locale_imp_callee::VecDtor(void* vec, int32_t flag) {   // 0x100724d0
    locale_imp_callee::VectorFacetDtor(vec, static_cast<int16_t>(flag));
}

void locale_imp_callee::Delete(void* p) { AnyDelete(p); }   // 0x10238218 operator delete

// use_facet_callee: facet allocation and construction. The facet objects are read through native pointers and freed through
// the facet's destructor, so they come from the host heap.
void* use_facet_callee::New(uint32_t size) { return IME::MslNew(size); }   // 0x102384b8 operator new

// 0x102388d8 std::codecvt<char,char,int>::codecvt(n): the base takes the reference count n at +4, then the vtable word (0x1003e67c).
void use_facet_callee::CodecvtCharCtor(void* p, uint32_t n) {
    PutWord(p, 4, n);
    PutWord(p, 0, 0x1003e67c);
}

// 0x1023b128 std::codecvt<wchar_t,char,int>::codecvt(n): the same, vtable word 0x1003e70c.
void use_facet_callee::CodecvtWcharCtor(void* p, uint32_t n) {
    PutWord(p, 4, n);
    PutWord(p, 0, 0x1003e70c);
}

// 0x10238980 std::ctype<char>::ctype(table, del, n) (library 0x100110e4): reference count n at +4, vtable word 0x1003eb5c, the
// table word at +8 and the delete flag byte at +0xc; no table means the library's default table (0x1003e088) and no deletion.
void use_facet_callee::CtypeCharCtor(void* p, const void* table, bool del, uint32_t n) {
    PutWord(p, 4, n);
    PutWord(p, 0, 0x1003eb5c);
    PutWord(p, 8, table != nullptr ? LowWord(table) : 0x1003e088);
    static_cast<uint8_t*>(p)[0xc] = table != nullptr ? (del ? 1 : 0) : 0;
}

void use_facet_callee::FacetDelete(void* facet, int32_t flag) {   // 0x1023baa8 vtable +8
    ::v51054::FacetDestroy(static_cast<UseFacetObj*>(facet), static_cast<int16_t>(flag));
}

// The locale impl's facet vector (UseFacetImp) starts like MslPtrVector: capacity, count, native data pointer.
void use_facet_callee::VecResize(UseFacetImp* vec, uint32_t n, const void* value) {   // 0x10073d58
    reinterpret_cast<MslPtrVector*>(vec)->resize(n, value);
}

void use_facet_callee::VecInsert(UseFacetImp* vec, void** pos, uint32_t count, void* const* value) {   // 0x10073ed4
    reinterpret_cast<MslPtrVector*>(vec)->insert(const_cast<const void**>(pos), count, reinterpret_cast<const void* const*>(value));
}

void use_facet_callee::VecErase(UseFacetImp* vec, void** first, void** last) {   // 0x10073e10
    reinterpret_cast<MslPtrVector*>(vec)->erase(const_cast<const void**>(first), const_cast<const void**>(last));
}

// ---- operator new and delete, the C string primitives, __throw and the process entry points.

void* LengthErrorOperatorNew(uint32_t size) { return IME::MslNew(size); }   // 0x1000f76c: the runtime's operator new, never NULL

void MslOperatorDelete(void* p) { AnyDelete(p); }   // 0x10238218 operator delete
void OstreamDelete(void* object) { AnyDelete(object); }

uint32_t wostream_callee::Strlen(const char* text) { return static_cast<uint32_t>(IME::MslStrlen(text)); }   // 0x10238650
char* wostream_callee::NewBytes(uint32_t size) { return static_cast<char*>(IME::MslNewArray(size)); }       // 0x10238668 new[]
int32_t* wostream_callee::NewInt() { return static_cast<int32_t*>(IME::MslNew(4)); }                         // 0x102384b8 new(4)
void wostream_callee::CopyString(char* to, const char* from) { IME::MslStrcpy(to, from); }                  // 0x10238410 strcpy

char* wostream_u::NewArray(uint32_t size) { return static_cast<char*>(IME::MslNewArray(size)); }   // operator new[]
int32_t* wostream_u::NewInt() { return static_cast<int32_t*>(IME::MslNew(4)); }                     // operator new(4)

// __throw (0x10238680): logs the exception like IME::MslThrow and returns, the behaviour the translations were checked on.
void MslThrow(const char* type, MslThrowObject* object, const void* destructors) { IME::MslThrow(type, object, destructors); }
void wostream_u::Throw(const char* type, MslThrowObject* object, const void* destructors) { IME::MslThrow(type, object, destructors); }
void wostream_callee::Throw(const char* type, void* object, const void* destructors) { IME::MslThrow(type, object, destructors); }

double MslPow(double a, double b) { return std::pow(a, b); }   // the runtime's pow

// wmemcpy (c/c++_shared glue 0x102389e0) on 16-bit characters.
char16_t* wmemcpy(char16_t* dst, const char16_t* src, size_t n) {
    if (n != 0) std::memcpy(dst, src, n * sizeof(char16_t));
    return dst;
}

void RuntimeInit() {}                       // 0x10238134: the glue is `li r3,0; stw r3,0(r1); blr`, nothing to start
void RuntimeExit(int32_t code) { std::exit(code); }   // 0x1023a0a8: exit

// ---- The ios_base and streambuf side. An ios_base keeps the Mac's 32-bit words: the stream buffer (0x24), the locale (0x20) and the
// tie (0x3c) are arena offsets.

namespace {
// std::ios_base::init (c/c++_shared 0x100080a8): the state, the format flags, the width and precision, a new std::locale (the copy
// of the global locale, the classic one when that has no impl), and the callback bookkeeping cleared.
void IosBaseInit(void* ios, uint32_t streambuf) {
    PutWord(ios, 0x24, streambuf);
    PutWord(ios, 0x38, 0);
    PutWord(ios, 0x34, streambuf == 0 ? 1 : 0);
    PutWord(ios, 0x30, 0x1002);
    PutWord(ios, 0x2c, 0);
    PutWord(ios, 0x28, 6);
    PutWord(ios, 0x20, 0);
    LocaleRefPtr* locale = static_cast<LocaleRefPtr*>(tpw::ArenaAllocZeroed(sizeof(LocaleRefPtr)));
    if (locale != nullptr) {
        ::LocaleConstruct(locale);
        PutWord(ios, 0x20, tpw::ArenaOffset(locale));
    }
    if (GetWord(ios, 0x20) == 0) PutWord(ios, 0x34, 1);
    for (size_t offset = 4; offset <= 0x1c; offset += 4) PutWord(ios, offset, 0);
}

LocaleRefPtr* IosLocale(const void* ios) {
    const uint32_t offset = GetWord(ios, 0x20);
    return offset == 0 ? nullptr : reinterpret_cast<LocaleRefPtr*>(tpw::ArenaBase() + offset);
}
}  // namespace

void OstreamIosInit(MacOstreamIos* ios, uint32_t streambuf) { IosBaseInit(ios, streambuf); }   // 0x10238968
void wostream_u::IosInit(void* ios, uint32_t streambuf) { IosBaseInit(ios, streambuf); }       // 0x10238968

// std::ios_base::~ios_base (c/c++_shared 0x10007dbc): the vtable word, the callbacks (none are ever registered by translated code,
// they would be Mac function descriptors), the locale object (its reference dropped, then the object freed), the three arrays,
// and the object itself for a positive (signed 16 bit) flag. The locale word is cleared so a second destruction frees nothing twice.
void OstreamIosBaseDtor(MacOstreamIos* ios, int32_t flags) {
    if (ios == nullptr) return;
    PutWord(ios, 0, LowWord(gIosBaseVt));
    if (LocaleRefPtr* locale = IosLocale(ios)) {
        ::LocaleRefPtrDestroy(locale, -1);
        AnyDelete(locale);
        PutWord(ios, 0x20, 0);
    }
    for (const size_t offset : {size_t(0x18), size_t(0x10), size_t(4)}) {
        const uint32_t word = GetWord(ios, offset);
        if (word != 0) AnyDelete(tpw::ArenaBase() + word);
        PutWord(ios, offset, 0);
    }
    if (static_cast<int16_t>(flags) > 0) AnyDelete(ios);
}

// basic_ios<char>::widen (0x10072960 widens through BasicIosWiden, which reads the locale as a native pointer at +0x20): the ios
// here keeps the locale as an arena offset, so the same steps run on that: the locale's ctype<char> facet, then its widen.
char OstreamWiden(MacOstreamIos* ios, char c) {
    LocaleRefPtr* locale = IosLocale(ios);
    if (locale == nullptr) return c;
    void* facet = ::UseFacetCtypeChar(reinterpret_cast<const UseFacetLocale*>(locale));
    return v51033_callee::CtypeWiden(facet, c);
}

// std::ios_base::good (0x102079b4): the state word at +0x34 is zero.
bool MslIosGood(MacIosBase* ios) { return ios->mState == 0; }
bool wostream_u::Good(void* ios) { return static_cast<MacIosBase*>(ios)->mState == 0; }
bool wostream_callee::Good(void* ios) { return static_cast<MacIosBase*>(ios)->mState == 0; }

// basic_ios::tie (0x10207964 and 0x1021f888): the word at +0x3c, an arena offset (zero when no stream is tied).
void* MslIosTie(MacIosBase* ios) {
    const uint32_t offset = GetWord(ios, 0x3c);
    return offset == 0 ? nullptr : tpw::ArenaBase() + offset;
}
void* wostream_u::Tie(void* ios) { return MslIosTie(static_cast<MacIosBase*>(ios)); }
void* wostream_callee::Tie(void* ios) { return MslIosTie(static_cast<MacIosBase*>(ios)); }

// std::ios_base::setstate (0x102077c0).
void wostream_u::Setstate(void* ios, int32_t bits) { MslIosSetstate(static_cast<MacIosBase*>(ios), bits); }
void wostream_callee::Setstate(void* ios, int32_t bits) { MslIosSetstate(static_cast<MacIosBase*>(ios), bits); }

void MslOstreamFlush(void* stream) { OstreamFlush(static_cast<BasicIstreamChar*>(stream)); }   // 0x1020a604 basic_ostream<char>::flush
void wostream_callee::Flush(void* stream) { WostreamFlush(static_cast<WostreamW*>(stream)); }  // 0x102268f8 basic_ostream<wchar_t>::flush

void OfstreamClose(void* filebuf) { FilebufCharClose(static_cast<BasicFilebufChar*>(filebuf)); }   // 0x10071b54 basic_filebuf<char>::close
void OfstreamLocaleDtor(void* locale, int32_t flag) {                                              // 0x10071f34 ~_RefCountedPtr
    ::LocaleRefPtrDestroy(static_cast<LocaleRefPtr*>(locale), static_cast<int16_t>(flag));
}

// __ptr_glue 0x1023baa8: a call through a slot of the stream buffer's vtable (r12 = the slot's entry). The Mac vtable words are
// descriptors of library code, so the call is answered by the class of the object (the first word, as the filebuf glue tells
// them apart): the narrow and the wide filebuf run their translations, any other buffer answers what the base class answers.
// The slot is the offset itself when the caller passed one, else the vtable word that equals the entry.
namespace {
constexpr uint32_t kSlotSync = 0x1c;
constexpr uint32_t kSlotUnderflow = 0x28;
constexpr uint32_t kSlotUflow = 0x2c;
constexpr uint32_t kSlotOverflow = 0x38;

bool IsWideFilebuf(const void* sb) {
    const uint32_t w = GetWord(sb, 0);
    return w == LowWord(gIfsWVtblD) || w == LowWord(gIfsWVtblC);
}
bool IsCharFilebuf(const void* sb) {
    const uint32_t w = GetWord(sb, 0);
    return w == LowWord(gFilebufCharVtblB) || w == LowWord(gFilebufCharVtblA);
}

uint32_t SlotOf(const MacStreamBuf* sb, uint32_t entry) {
    if (entry == kSlotSync || entry == kSlotUnderflow || entry == kSlotUflow || entry == kSlotOverflow) return entry;
    const uint8_t* vtable = sb->mVtable.get();
    if (vtable == nullptr || !tpw::InArena(vtable)) return 0;
    for (const uint32_t slot : {kSlotUflow, kSlotUnderflow, kSlotSync, kSlotOverflow}) {
        if (GetWord(vtable, slot) == entry) return slot;
    }
    return 0;
}

int32_t StreamBufCall(MacStreamBuf* sb, uint32_t entry, uint32_t argument) {
    const bool wide = IsWideFilebuf(sb);
    const bool narrow = IsCharFilebuf(sb);
    switch (SlotOf(sb, entry)) {
        case kSlotUflow:
            return filebuf_wclose_callee::VirtualUnderflow(sb);
        case kSlotUnderflow:
            if (narrow) return ::FilebufCharUnderflow(reinterpret_cast<BasicFilebufChar*>(sb));
            if (wide) return static_cast<int32_t>(reinterpret_cast<BasicFilebufWchar*>(sb)->underflow());
            return -1;
        case kSlotSync:
            if (narrow) return ::v51054::FilebufCharSync(reinterpret_cast<BasicFilebufChar*>(sb));
            if (wide) return reinterpret_cast<BasicFilebufWchar*>(sb)->sync();
            return 0;
        case kSlotOverflow:
            if (narrow) return reinterpret_cast<BasicFilebufChar*>(sb)->overflow(static_cast<int32_t>(argument));
            if (wide) return static_cast<int32_t>(reinterpret_cast<BasicFilebufWchar*>(sb)->overflow(static_cast<char16_t>(argument)));
            return -1;
        default:
            return -1;
    }
}
}  // namespace

int32_t MslStreamBufVirtual(MacStreamBuf* sb, uint32_t entry) { return StreamBufCall(sb, entry, 0); }
int32_t wostream_u::Virt(MacStreamBuf* sb, uint32_t entry) { return StreamBufCall(sb, entry, 0); }

// streambuf overflow(c) through vtable entry 0x38.
int32_t MslPutOverflow(MacPutStreamBuf* sb, uint32_t c) {
    return StreamBufCall(reinterpret_cast<MacStreamBuf*>(sb), kSlotOverflow, c);
}
