// MslFilebufGlue.cpp: the real definitions of the callee shims and variables the translated basic_filebuf / basic_ifstream /
// basic_streambuf code of MslSupport.cpp calls through (std_basic_filebuf.h, std_basic_ifstream.h, std_basic_istream.h,
// std_basic_streambuf_wchar.h, email.h). Only libtpw compiles this file: the unit test libraries list their own sources and
// define these names themselves (recording fakes, test variables), so nothing here changes what a unit test compiles.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <unordered_map>

#include "email.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"
#include "std_basic_streambuf_wchar.h"
#include "ui_ime.h"

// ---- Variables. The Mac holds the first group in initialised data and the second in BSS / import slots (zero).

// The vtables of std::basic_ifstream<wchar_t> and its bases (0x102b1e04 ... 0x102b1f20, initialised data). The original words are
// Mac addresses of other tables and of untranslated c++_shared glue (TOC vectors): only the identity of each table matters on the
// host (the translated destructor and constructor compare and store their addresses), so the words are kept as the Mac has them.
uint32_t gIfsWVtblA[4] = {0x102b1dfc, 0, 0x10268908, 0x102b1dfc};   // 0x102b1e04
uint32_t gIfsWVtblB[4] = {0x102b1e38, 0, 0x10268910, 0x102b1e38};   // 0x102b1e40
uint32_t gIfsWVtblC[4] = {0x102b1e58, 0, 0x102688e8, 0x102688e0};   // 0x102b1e60
uint32_t gIfsWVtblD[4] = {0x102b1ea8, 0, 0x10268918, 0x10268848};   // 0x102b1eb0
uint32_t gIfsWVtblE[4] = {0x102b1de0, 0, 0x10268928, 0x102b1dcc};   // 0x102b1f20

// std::ios_base::failure: the throw-site object (0x1029d084, initialised data; its first word is 0x1029d07c on the Mac, the
// type's descriptor, and the third is the destructor vector 0x102661d0) and the TVector of the ~failure destructors (0x102661d0,
// words 0x1007020c / 0x10267cc0 pairs). The words are Mac addresses with no host referent, so the host tables are zero apart from
// the object's link to the vector, which is the only word a host test of the throw path can follow.
alignas(8) char gIosFailureDtorVec[16] = {};
alignas(8) char gIosFailureVtbl[16] = {};
namespace {
struct IosFailureInit {
    IosFailureInit() {
        const uint32_t vec = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIosFailureDtorVec));
        std::memcpy(gIosFailureVtbl + 8, &vec, sizeof(vec));
    }
} gIosFailureInit;
}  // namespace

// BSS and import slots (0x103bcfb4 ... 0x103bd0d8): zero is the Mac's start value. gLocaleGlobal (std::locale::global_s, 0x103bcfd4)
// stays an empty locale until the locale runtime (the facet constructors and std::locale::classic) is wired.
uint32_t gLocaleIdCount = 0;      // 0x103bd06c id_count_s__Q33std6locale2id
uint32_t gIdCtypeChar = 0;        // 0x103bd074 std::ctype<char>::id
uint32_t gIdCodecvtCharChar = 0;  // 0x103bcfb4 std::codecvt<char,char,int>::id
uint32_t gIdCtypeWchar = 0;       // 0x103bd0d8 std::ctype<wchar_t>::id
uint32_t gIdCodecvtWcharChar = 0; // 0x103bd03c std::codecvt<wchar_t,char,int>::id
LocaleRefPtr gLocaleGlobal = {};  // 0x103bcfd4
uint8_t gIfsWFiles[0xfc] = {};    // 0x103bd0bc (import __files)
char gMslEmptyChar = 0;           // 0x102b1fb2: the NUL the string code copies in (one zero byte in initialised data)
char gMslChangeSizeNul = 0;       // 0x10278bd3: the terminator change_size writes (a zero byte in the code constants)

namespace {

// ---- The memory the shims allocate and the FILE* the Mac words stand for.

// operator new through the arena (the translated code keeps the result in a 32-bit field as an arena offset). The block is zeroed
// and has room to spare, as IME::MslNew grants.
void* ArenaNew(uint32_t size) {
    const size_t n = static_cast<size_t>(size) * 2 + 32;
    return tpw::ArenaAllocZeroed(n < 64 ? 64 : n);
}

// operator delete: arena blocks go back to the arena, anything else to the heap.
void AnyDelete(void* p) {
    if (p == nullptr) return;
    if (tpw::InArena(p)) {
        tpw::ArenaFree(p);
    } else {
        ::operator delete(p);
    }
}

// A FILE* is a 32-bit word on the Mac. The host hands out small tokens and keeps the FILE* here (the three std streams
// at gIfsWFiles are never opened through these shims).
std::unordered_map<uint32_t, FILE*>& Files() {
    static std::unordered_map<uint32_t, FILE*> files;
    return files;
}
uint32_t gNextToken = 0x1000;

FILE* FileOf(uint32_t token) {
    auto it = Files().find(token);
    return it == Files().end() ? nullptr : it->second;
}

// The token of a FILE* word that arrives either as the token itself or as the arena address of the token (the translated code
// forms the word both ways).
uint32_t TokenOfWord(const void* word) {
    const uintptr_t v = reinterpret_cast<uintptr_t>(word);
    if (v <= 0xffffffffu) return static_cast<uint32_t>(v);
    const uintptr_t base = reinterpret_cast<uintptr_t>(tpw::gArenaBase);
    if (tpw::gArenaBase != nullptr && v >= base && v - base <= 0xffffffffu) return static_cast<uint32_t>(v - base);
    return 0;
}

// ---- The Mac-layout reference-counted string (MslStrRep: len, cap, refs, data as an arena offset; refs 0 = one owner).

inline char* RepData(const MslStrRep* r) { return r->data != 0 ? reinterpret_cast<char*>(tpw::gArenaBase + r->data) : nullptr; }

MslStrRep* NewRep(uint32_t cap) {
    MslStrRep* rep = static_cast<MslStrRep*>(ArenaNew(0x10));
    rep->len = 0;
    rep->cap = static_cast<int32_t>(cap);
    rep->refs = 1;
    rep->data = tpw::ArenaOffset(ArenaNew(cap + 1));
    return rep;
}

void FreeRep(MslStrRep* rep) {
    AnyDelete(RepData(rep));
    AnyDelete(rep);
}

// Drops one reference; the last owner frees the body (the compiler's inlined release, 0x1000221c).
void ReleaseRep(MslStrRep*& s) {
    MslStrRep* rep = s;
    if (rep == nullptr) return;
    int32_t refs = rep->refs == 0 ? 1 : rep->refs;
    rep->refs = refs - 1;
    if (refs - 1 == 0) FreeRep(rep);
    s = nullptr;
}

// Copy on write: a shared body is copied before it changes (refs > 1).
void Unshare(MslStrRep*& s) {
    if (s->refs <= 1) return;
    MslStrRep* copy = NewRep(static_cast<uint32_t>(s->cap));
    std::memcpy(RepData(copy), RepData(s), static_cast<size_t>(s->len) + 1);
    copy->len = s->len;
    s->refs = s->refs - 1;
    s = copy;
}

// Room for n characters and the terminator (the body is replaced when it is too small).
void Reserve(MslStrRep*& s, uint32_t n) {
    Unshare(s);
    if (static_cast<uint32_t>(s->cap) >= n) return;
    uint32_t cap = static_cast<uint32_t>(s->cap) * 2;
    if (cap < n) cap = n;
    MslStrRep* bigger = NewRep(cap);
    std::memcpy(RepData(bigger), RepData(s), static_cast<size_t>(s->len) + 1);
    bigger->len = s->len;
    bigger->refs = s->refs == 0 ? 1 : s->refs;
    FreeRep(s);
    s = bigger;
}

void SetLength(MslStrRep*& s, uint32_t n, char fill) {
    Reserve(s, n);
    char* d = RepData(s);
    if (static_cast<uint32_t>(s->len) < n) std::memset(d + s->len, fill, n - static_cast<uint32_t>(s->len));
    s->len = static_cast<int32_t>(n);
    d[n] = 0;
}

void StringInitRep(MslStrRep** s, uint32_t n, char c) {
    *s = NewRep(n < 3 ? 3 : n);
    SetLength(*s, n, c);
}

void StringAssignRep(MslStrRep** s, uint32_t n, char c) {
    Reserve(*s, n);
    (*s)->len = 0;
    SetLength(*s, n, c);
}

void StringResizeRep(MslStrRep** s, uint32_t n, char c) { SetLength(*s, n, c); }

void StringAppendRep(MslStrRep** s, uint32_t count, char c) { SetLength(*s, static_cast<uint32_t>((*s)->len) + count, c); }

char* StringIndexRep(MslStrRep** s, uint32_t i) {
    Unshare(*s);
    (*s)->refs = 0;
    return RepData(*s) + i;
}

// ---- The classic locale's codecvt facets. The facets live in untranslated c++_shared code and the facet constructors are not
// wired, so the vtable calls answer what the C locale's facets answer: codecvt<char,char,int> converts nothing (noconv, 3);
// codecvt<wchar_t,char,int> maps one byte to one character (max_length 1, encoding 1, unshift noconv).
constexpr int32_t kCvtOk = 0;
constexpr int32_t kCvtPartial = 1;
constexpr int32_t kCvtError = 2;
constexpr int32_t kCvtNoconv = 3;

// ---- Objects the streambuf shims tell apart by their first word (the vtable's low half, as the constructors store it).

inline uint32_t FirstWord(const void* object) {
    uint32_t w;
    std::memcpy(&w, object, sizeof(w));
    return w;
}
inline uint32_t LowWord(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

bool IsWideFilebuf(const void* sb) {
    const uint32_t w = FirstWord(sb);
    return w == LowWord(gIfsWVtblD) || w == LowWord(gIfsWVtblC);
}
bool IsCharFilebuf(const void* sb) {
    const uint32_t w = FirstWord(sb);
    return w == LowWord(gFilebufCharVtblB) || w == LowWord(gFilebufCharVtblA);
}

}  // namespace

// ---- filebuf_callee: the wide filebuf's callees outside the unit.
namespace filebuf_callee {

// 0x10238938 fgetc: the word is the FILE* token (or the arena address of it).
int32_t Fgetc(void* file) {
    FILE* f = FileOf(TokenOfWord(file));
    return f != nullptr ? std::fgetc(f) : -1;
}

// 0x1023abe8 wmemmove: n is a count of 16-bit characters.
void Wmemmove(void* dst, const void* src, uint32_t n) { std::memmove(dst, src, static_cast<size_t>(n) * sizeof(char16_t)); }

// __ptr_glue vtable 0x18: codecvt::max_length() of the classic facet.
int32_t CodecvtMaxLength(void* cvt) {
    (void)cvt;
    return 1;
}

// __ptr_glue vtable 0x10: codecvt<wchar_t,char,int>::in widens each byte to one character.
int32_t CodecvtIn(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                  char16_t* to, char16_t* toEnd, char16_t** toNext) {
    (void)cvt;
    (void)state;
    while (from < fromEnd && to < toEnd) *to++ = static_cast<uint8_t>(*from++);
    *fromNext = from;
    *toNext = to;
    return from < fromEnd ? kCvtPartial : kCvtOk;
}

// 0x102384b8 operator new.
void* New(uint32_t size) { return ArenaNew(size); }

// 0x10238218 operator delete.
void Delete(void* p) { AnyDelete(p); }

// 0x100731b4 basic_string<char>::append(n, c) on the Mac string body.
void StringAppend(MslStrRep** s, uint32_t count, char c) { StringAppendRep(s, count, c); }

// 0x100733b0 basic_string<char>::operator[].
char* StringIndex(MslStrRep** s, uint32_t i) { return StringIndexRep(s, i); }

}  // namespace filebuf_callee

// ---- filebuf_char_callee: the narrow filebuf's callees.
namespace filebuf_char_callee {

// __ptr_glue vtable 0x1c: codecvt<char,char,int>::always_noconv() is true.
int32_t CodecvtAlwaysNoconv(void* cvt) {
    (void)cvt;
    return 1;
}

// __ptr_glue vtable 0x10: codecvt<char,char,int>::in copies nothing and answers noconv.
int32_t CodecvtInChar(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                      char* to, char* toEnd, char** toNext) {
    (void)cvt;
    (void)state;
    (void)fromEnd;
    (void)toEnd;
    *fromNext = from;
    *toNext = to;
    return kCvtNoconv;
}

int32_t CodecvtMaxLength(void* cvt) {
    (void)cvt;
    return 1;
}

// 0x10238638 operator delete[].
void DeleteArray(void* p) { AnyDelete(p); }

// 0x102388c0 fclose.
int32_t Fclose(uint32_t file) {
    FILE* f = FileOf(file);
    if (f == nullptr) return -1;
    Files().erase(file);
    return std::fclose(f);
}

// 0x102389b0 fopen: the game's relative path is mapped to the player's folder (read modes) or the writable folder.
uint32_t Fopen(const char* name, const char* mode) {
    const bool reading = mode != nullptr && mode[0] == 'r' && std::strchr(mode, '+') == nullptr;
    const std::string path = reading ? tpw::GamePathForReading(name) : tpw::GamePathForWriting(name);
    FILE* f = std::fopen(path.c_str(), mode);
    if (f == nullptr) return 0;
    const uint32_t token = gNextToken++;
    Files()[token] = f;
    return token;
}

// 0x102388f0 fseek.
int32_t Fseek(uint32_t file, int32_t offset, int32_t whence) {
    FILE* f = FileOf(file);
    return f != nullptr ? std::fseek(f, offset, whence) : -1;
}

// 0x10070e1c basic_streambuf<char>::getloc: the locale of a narrow filebuf sits at Mac 0x1c. The other caller (basic_ios<char>::widen)
// passes the ios_base, whose getloc is 0x10072a34.
void Getloc(void* out, void* streambuf) {
    if (IsCharFilebuf(streambuf)) {
        ::v51054::LocaleRefPtrCopy(static_cast<LocaleRefPtr*>(out),
                                   reinterpret_cast<const LocaleRefPtr*>(static_cast<const uint8_t*>(streambuf) + 0x1c));
    } else {
        ::v51054::IosBaseGetloc(static_cast<LocaleRefPtr*>(out), static_cast<const IosBaseLocalePtr*>(streambuf));
    }
}

// 0x10072d24 std::locale::locale().
void LocaleCtor(void* locale) { ::LocaleConstruct(static_cast<LocaleRefPtr*>(locale)); }

// 0x10071f34 ~_RefCountedPtr<__locale_imp>.
void LocaleDtor(void* locale, int32_t flag) { ::LocaleRefPtrDestroy(static_cast<LocaleRefPtr*>(locale), static_cast<int16_t>(flag)); }

// 0x102386c8 memmove.
void Memmove(void* dst, const void* src, uint32_t n) { std::memmove(dst, src, n); }

// 0x1007363c basic_string(const allocator&).
void StringCtor(MslStrRep** s, void* allocator) {
    (void)allocator;
    StringInitRep(s, 0, 0);
}

// 0x1000221c ~basic_string.
void StringDtor(MslStrRep** s, int32_t flag) {
    (void)flag;
    ReleaseRep(*s);
}

// 0x10072a94 basic_filebuf<char>::unshift (0x10226dcc is the wide one, the same shape).
int32_t Unshift(void* filebuf) { return ::v51054::FilebufCharUnshift(static_cast<BasicFilebufChar*>(filebuf)) ? 1 : 0; }

// 0x100739a0 use_facet<codecvt<char,char,int>>.
void* UseFacetCodecvt(void* locale) { return ::UseFacetCodecvtCharChar(static_cast<const UseFacetLocale*>(locale)); }

}  // namespace filebuf_char_callee

// __ptr_glue vtable 0xc: codecvt<char,char,int>::out copies nothing and answers noconv.
namespace filebuf_charout_callee {
int32_t CodecvtOutChar(void* cvt, void* state, const char* from, const char* fromEnd, const char** fromNext,
                       char* to, char* toEnd, char** toNext) {
    (void)cvt;
    (void)state;
    (void)fromEnd;
    (void)toEnd;
    *fromNext = from;
    *toNext = to;
    return kCvtNoconv;
}
}  // namespace filebuf_charout_callee

// ---- The narrow filebuf's host-layout string (MslCharRep): the translated basic_string<char> members.
namespace filebuf_charstr_callee {
void StringInit(MslCharRep** s, uint32_t n, char c, void* alloc) {   // 0x10072e6c
    msl_char_string::init(reinterpret_cast<MslCharString*>(s), n, c, alloc);
}
void StringAssign(MslCharRep** s, uint32_t n, char c) {   // 0x100730e8
    msl_char_string::assignFill(reinterpret_cast<MslCharString*>(s), n, c);
}
void StringResize(MslCharRep** s, uint32_t n, char c) {   // 0x10073558
    msl_char_string::resize(reinterpret_cast<MslCharString*>(s), n, c);
}
void StringDtor(MslCharRep** s, int32_t flag) {   // 0x1000221c
    msl_char_string::destroy(reinterpret_cast<MslCharString*>(s), static_cast<int16_t>(flag));
}
}  // namespace filebuf_charstr_callee

// ---- filebuf_unshift_callee: the wide filebuf's unshift callees.
namespace filebuf_unshift_callee {

// __ptr_glue vtable 0x14: codecvt::unshift writes nothing and answers noconv for the classic facets.
int32_t CodecvtUnshift(void* cvt, void* state, char* to, char* toEnd, char** toNext) {
    (void)cvt;
    (void)state;
    (void)toEnd;
    *toNext = to;
    return kCvtNoconv;
}

// 0x10238920 fflush.
int32_t Fflush(uint32_t file) {
    FILE* f = FileOf(file);
    return f != nullptr ? std::fflush(f) : -1;
}

void LocaleDtor(void* locale, int32_t flag) {   // 0x10071f34
    ::LocaleRefPtrDestroy(static_cast<LocaleRefPtr*>(locale), static_cast<int16_t>(flag));
}

}  // namespace filebuf_unshift_callee

// ---- filebuf_wclose_callee.
namespace filebuf_wclose_callee {

// 0x100737a0 std::__locale_imp::~__locale_imp with the delete flag.
void LocaleImpDtor(void* impl, int32_t flag) { ::LocaleImpDestroy(static_cast<LocaleImp*>(impl), static_cast<int16_t>(flag)); }

// 0x10225cb8 use_facet<codecvt<wchar_t,char,int>>.
void* UseFacetCodecvtW(void* locale) { return ::UseFacetCodecvtWcharChar(static_cast<const UseFacetLocale*>(locale)); }

// __ptr_glue vtable 0x2c: the filebuf's virtual uflow (the narrow and the wide filebuf are the two classes that reach here).
int32_t VirtualUnderflow(void* fb) {
    if (IsWideFilebuf(fb)) return static_cast<int32_t>(static_cast<BasicFilebufWchar*>(fb)->uflow());
    if (IsCharFilebuf(fb)) return static_cast<BasicFilebufChar*>(fb)->uflow();
    return -1;
}

}  // namespace filebuf_wclose_callee

// ---- filebuf_wseek_callee.
namespace filebuf_wseek_callee {

// __ptr_glue vtable 0x18: codecvt::encoding() is 1 (one byte per character, a constant width).
int32_t CodecvtEncoding(void* cvt) {
    (void)cvt;
    return 1;
}

// __ptr_glue vtable 0xc: codecvt<wchar_t,char,int>::out narrows each character; one above 0xff is an error.
int32_t CodecvtOut(void* cvt, void* state, const char16_t* from, const char16_t* fromEnd, const char16_t** fromNext,
                   char* to, char* toEnd, char** toNext) {
    (void)cvt;
    (void)state;
    int32_t result = kCvtOk;
    while (from < fromEnd) {
        if (to >= toEnd) {
            result = kCvtPartial;
            break;
        }
        if (*from > 0xff) {
            result = kCvtError;
            break;
        }
        *to++ = static_cast<char>(*from++);
    }
    *fromNext = from;
    *toNext = to;
    return result;
}

void Delete(void* p) { AnyDelete(p); }   // 0x10238218

// 0x102388a8 fputc.
int32_t Fputc(int32_t c, uint32_t file) {
    FILE* f = FileOf(file);
    return f != nullptr ? std::fputc(c, f) : -1;
}

int32_t Fseek(uint32_t file, int32_t offset, int32_t whence) { return filebuf_char_callee::Fseek(file, offset, whence); }   // 0x102388f0

// 0x10238908 ftell.
int32_t Ftell(uint32_t file) {
    FILE* f = FileOf(file);
    return f != nullptr ? static_cast<int32_t>(std::ftell(f)) : -1;
}

void StringAssign(MslStrRep** s, uint32_t n, char c) { StringAssignRep(s, n, c); }                 // 0x100730e8
void StringInit(MslStrRep** s, uint32_t n, char c, void* alloc) {                                    // 0x10072e6c
    (void)alloc;
    StringInitRep(s, n, c);
}
void StringResize(MslStrRep** s, uint32_t n, char c) { StringResizeRep(s, n, c); }                 // 0x10073558

// 0x10226dcc basic_filebuf<wchar_t>::unshift.
bool Unshift(BasicFilebufWchar* fb) { return fb->unshift(); }

}  // namespace filebuf_wseek_callee

// ---- ifstream_callee: the wide ifstream's callees.
namespace ifstream_callee {

// 0x102266e4 basic_istream<wchar_t>(streambuf*).
void IstreamCtor(void* self, int32_t flag, void* streambuf) {
    ignore_callee::IstreamCtor(static_cast<ignore_callee::IgnoreStream*>(self), static_cast<int16_t>(flag), LowWord(streambuf));
}

// 0x10226ef4 basic_filebuf<wchar_t>(FILE*).
void FilebufCtor(void* self, void* file) {
    ::FilebufWcharConstruct(static_cast<BasicFilebufWchar*>(self), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)));
}

// 0x10071f34 ~_RefCountedPtr<__locale_imp>.
void LocaleDtor(void* locale, int32_t flag) { ::LocaleRefPtrDestroy(static_cast<LocaleRefPtr*>(locale), static_cast<int16_t>(flag)); }

// 0x10238950 std::ios_base::~ios_base (the one function the ostream side calls as OstreamIosBaseDtor).
void IosBaseDtor(void* ios, int32_t flag) { ::OstreamIosBaseDtor(static_cast<MacOstreamIos*>(ios), flag); }

void Delete(void* p) { AnyDelete(p); }   // 0x10238218

}  // namespace ifstream_callee

// 0x10072bbc basic_filebuf<char>(FILE*) for the Mac-layout ifstream of email.cpp.
namespace ifstream_char_callee {
void FilebufCtor(void* filebuf, uint32_t file) {
    static_cast<BasicFilebufChar*>(filebuf)->construct(reinterpret_cast<void*>(static_cast<uintptr_t>(file)));
}
}  // namespace ifstream_char_callee

// ---- streambuf_callee / streambuf_io_callee: the base class's virtual calls.
namespace streambuf_callee {

void LocaleAssign(void* dst, const void* src) {   // 0x10072128 _RefCountedPtr::operator=
    *static_cast<LocaleRefPtr*>(dst) = *static_cast<const LocaleRefPtr*>(src);
}

void LocaleCopyCtor(void* out, const void* src) {   // 0x10072030 _RefCountedPtr(const _RefCountedPtr&)
    ::v51054::LocaleRefPtrCopy(static_cast<LocaleRefPtr*>(out), static_cast<const LocaleRefPtr*>(src));
}

// __ptr_glue vtable 0x28: the filebuf<char> underflow when the object is one, else the base class's underflow (end of file).
int32_t Underflow(void* sb) {
    if (IsCharFilebuf(sb)) return ::FilebufCharUnderflow(static_cast<BasicFilebufChar*>(sb));
    return -1;
}

}  // namespace streambuf_callee

namespace streambuf_io_callee {

// __ptr_glue vtable 0x2c: uflow.
int32_t Uflow(void* sb) {
    if (IsCharFilebuf(sb)) return static_cast<BasicFilebufChar*>(sb)->uflow();
    return static_cast<BasicStreambufChar*>(sb)->uflow();
}

// __ptr_glue vtable 0x38: overflow.
int32_t Overflow(void* sb, int32_t c) {
    if (IsCharFilebuf(sb)) return static_cast<BasicFilebufChar*>(sb)->overflow(c);
    return -1;
}

}  // namespace streambuf_io_callee

// ---- The wide istream and streambuf.
namespace wistream_callee {
// 0x10238968 std::ios_base::init (the ostream side's OstreamIosInit).
void IosInit(void* ios, uint32_t streambuf) { ::OstreamIosInit(static_cast<MacOstreamIos*>(ios), streambuf); }
}  // namespace wistream_callee

namespace wstreambuf_callee {
// __ptr_glue vtable 0x28: underflow of the base class answers end of file.
uint16_t Underflow(MslWStreambuf* self) {
    (void)self;
    return 0xffff;
}
}  // namespace wstreambuf_callee
