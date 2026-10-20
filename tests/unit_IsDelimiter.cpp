// Test library for the small ltms functions (unit v51120; libtpw_unit_IsDelimiter.dylib, tests/test_IsDelimiter.py), main app:
//   src/ltms_textformatter_rest_IsDelimiter.cpp   the destructors, smart pointers, swaps, what()s, Format and the static initialiser
//   src/ltms_textformatter_rest.cpp               IsDelimiter (tested where it is)
//   src/ltms_textformatter.cpp                    SetHorizontalJustification and the vectors' tear_down (the real ones run on both sides)
//   src/TbITableResource.cpp                      TbITable::AddRef
//
// The callees outside the unit are recording fakes: operator delete and delete[] (the global ones, so the real tear_down is seen),
// TbResource::Release / AddRef, TbIFont::~TbIFont and the fake CStaticBuffer. The destructors that the compiler writes with the
// hidden delete flag are called here as the explicit destructor plus ::operator delete for a flag above 0.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbIRLE4bitFont.h"
#include "TbIResourceIndex.h"
#include "TbITable.h"
#include "TbResource.h"
#include "TbTextFormatter.h"

// IsDelimiter has internal linkage in its file, so the file is compiled into this translation unit (it is not in the cmake list).
#include "../src/ltms_textformatter_rest.cpp"

using namespace calllog;

// ---- the global operator delete, recording (the memory comes from calloc and is never freed)

// ---- TbResource (src/TbResource.cpp is not compiled here)
int32_t TbResource::Release() {
    ev("Release", {lab(this)});
    return 0;
}
void TbResource::AddRef() { ev("TbResource_AddRef", {lab(this)}); }

namespace SamsUtilities {
void _USwapBlock32(unsigned int*, int) {}   // a callee of the other functions of the compiled files; never called here
}

// TbIResourceIndex's members the compiled files call (never called here)
TbError TbIResourceIndex::GetResource(void*, void**) { return TbError(-1); }
TbError TbIResourceIndex::GetPreOpenedResource(void*, void**) { return TbError(-1); }
uint16_t TbIResourceIndex::GetResourceType(void*) { return 0; }
void* TbIResourceIndex::GetResourceItemHandle(uint32_t) { return nullptr; }

namespace NS_MacDoze {
int32_t InterlockedDecrement(int*) { return 0; }   // TbITable::Release's (in the same file as AddRef); never called here
}

// TbFontRender (the formatter's members in src/ltms_textformatter.cpp use it; never called here)

TbError TbFontRender::Initialise(const TbFontRenderParams&) { return TbError(-1); }
TbError TbFontRender::DrawChar(const TbPoint&, uint16_t, TbRect&) { return TbError(-1); }
TbRect TbRect::Bounding(const TbRect& other) const { return other; }
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams(const TbFontRenderParams&) {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams& TbFontRenderParams::operator=(const TbFontRenderParams&) { return *this; }
TbIFont* TbFontRenderParams::GetFont() const { return nullptr; }
TbFontRender::TbFontRender() {}
TbFontRender::~TbFontRender() {}

void TbResource::ManagerAddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }

// TbIFont::~TbIFont and TbIMBInfo::~TbIMBInfo are now defined in src/TbIRLE4bitFont.cpp and src/TbIMBToUnicode.cpp.

// The font's other virtuals (the vtable of the destructor's class needs them).
// (now defined by src/TbIRLE4bitFont.cpp, which the unit compiles)

namespace {

// The buffer the Format test appends to: every virtual logs its slot (0 the destructor is never called, 1 Close ...).
struct FakeBuffer : CStaticBuffer {
    void Close() override { ev("virt", {lab(this), 1, 0, 0}); }
    void AppendMB(const unsigned char* text, uint32_t size) override { ev("virt", {lab(this), 2, lab(text), static_cast<int32_t>(size)}); }
    void AppendMB(uint16_t) override { ev("virt", {lab(this), 3, 0, 0}); }
    void AppendUnicode(const char16_t*, uint32_t) override { ev("virt", {lab(this), 4, 0, 0}); }
    void AppendUnicode(char16_t) override { ev("virt", {lab(this), 5, 0, 0}); }
};

template <class T>
T* P(uint64_t address) { return reinterpret_cast<T*>(static_cast<uintptr_t>(address)); }

uint64_t U(const void* pointer) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)); }

}  // namespace

extern "C" {

uint64_t tpw_u_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vptr0", &gRLE4bitFontFactoryVptr},       {"vptr1", &gTableFactoryVptr},         {"vptr2", &gMBInfoFactoryVptr},
        {"vptr3", &gUnicodeToMBFactoryVptr},       {"vptr4", &gMBToUnicodeFactoryVptr},   {"vtbl0", gRLE4bitFontFactoryVtbl},
        {"vtbl1", gTableFactoryVtbl},              {"vtbl2", gMBInfoFactoryVtbl},         {"vtbl3", gUnicodeToMBFactoryVtbl},
        {"vtbl4", gMBToUnicodeFactoryVtbl},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return U(entry.address);
    return 0;
}

uint64_t tpw_u_alloc(uint32_t size) { return U(std::calloc(size, 1)); }

// kind 1: a TbIMBToUnicode, 2: a TbIUnicodeToMB (resources with a vtable whose Release is the fake), 3: the fake buffer.
void tpw_u_make(int32_t kind, uint64_t address) {
    void* where = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    if (kind == 1) new (where) TbIMBToUnicode();
    else if (kind == 2) new (where) TbIUnicodeToMB();
    else new (where) FakeBuffer();
}

// The host offsets the module relies on: 0 the horizontal justification in TbTextFormatter, 1 the counter in a counted block,
// 2 the message in a logic_error, 3 the string in TbMBStringVTag, 4 the pointer in a smart pointer.
int32_t tpw_u_layout(int64_t* out) {
    alignas(16) static char formatter[sizeof(TbTextFormatter)];
    alignas(16) static char tag[sizeof(TbMBStringVTag)];
    int32_t n = 0;
    out[n++] = reinterpret_cast<char*>(&reinterpret_cast<TbTextFormatter*>(formatter)->mHorizontalJustification) - formatter;
    out[n++] = static_cast<int64_t>(offsetof(MslRefCountedCharsBlock, mCount));
    out[n++] = static_cast<int64_t>(offsetof(LengthError, mMessage));
    out[n++] = reinterpret_cast<const char*>(&reinterpret_cast<TbMBStringVTag*>(tag)->mString) - tag;
    out[n++] = static_cast<int64_t>(offsetof(TbRefReleaseSmartPtr<TbIMBToUnicode>, mPtr));
    return n;
}

void tpw_u_sinit() { __sinit_TbIResourceIndex_cpp(); }

uint32_t tpw_u_isdelim(uint64_t p) { return static_cast<uint32_t>(IsDelimiter(P<CCharacterInfo>(p))); }

int32_t tpw_u_sethj(uint64_t object, int32_t value) {
    return P<TbTextFormatter>(object)->SetHorizontalJustification(static_cast<TbTextFormatter::eJustification>(value));
}

// kind 0: the character vector, 1: the line vector, 2: the point vector, 3: CCharInfoList.
uint64_t tpw_u_vdtor(int32_t kind, uint64_t p, int32_t flag) {
    const int16_t f = static_cast<int16_t>(flag);
    if (kind == 0) return U(MslVectorCCharacterInfoDtor(P<MslVector<CCharacterInfo>>(p), f));
    if (kind == 1) return U(MslVectorCLineInfoDtor(P<MslVector<CLineInfo>>(p), f));
    if (kind == 2) return U(MslVectorTbPointDtor(P<MslVector<TbPoint>>(p), f));
    return U(CCharInfoListDtor(P<CCharInfoList>(p), f));
}

// kind 0: TbIUnicodeToMB, 1: TbIMBToUnicode, 2: TbIRLE4bitFont: the explicit destructor, then the delete for a flag above 0.
uint64_t tpw_u_cdtor(int32_t kind, uint64_t p, int32_t flag) {
    const int16_t f = static_cast<int16_t>(flag);
    void* block = reinterpret_cast<void*>(static_cast<uintptr_t>(p));
    if (block != nullptr) {
        if (kind == 0) P<TbIUnicodeToMB>(p)->TbIUnicodeToMB::~TbIUnicodeToMB();
        else if (kind == 1) P<TbIMBToUnicode>(p)->TbIMBToUnicode::~TbIMBToUnicode();
        else P<TbIRLE4bitFont>(p)->TbIRLE4bitFont::~TbIRLE4bitFont();
        if (f > 0) ::operator delete(block);
    }
    return p;
}

// kind 0: TbRefReleaseSmartPtr<TbIMBToUnicode>, 1: <TbIUnicodeToMB>.
uint64_t tpw_u_smart(int32_t kind, uint64_t p, int32_t flag) {
    const int16_t f = static_cast<int16_t>(flag);
    void* block = reinterpret_cast<void*>(static_cast<uintptr_t>(p));
    if (block != nullptr) {
        if (kind == 0) P<TbRefReleaseSmartPtr<TbIMBToUnicode>>(p)->~TbRefReleaseSmartPtr();
        else P<TbRefReleaseSmartPtr<TbIUnicodeToMB>>(p)->~TbRefReleaseSmartPtr();
        if (f > 0) ::operator delete(block);
    }
    return p;
}

uint64_t tpw_u_mslstring(uint64_t p, int32_t flag) { return U(MslStringDtor(P<MslString>(p), static_cast<int16_t>(flag))); }

void tpw_u_refdtor(uint64_t p, int32_t flag) { MslRefCountedCharsDtor(P<MslRefCountedCharsBlock>(p), static_cast<int16_t>(flag)); }

uint64_t tpw_u_logicwhat(uint64_t p) { return U(LogicErrorWhat(P<LengthError>(p))); }

uint64_t tpw_u_excwhat(uint64_t p) { return U(MslExceptionWhat(P<void>(p))); }

void tpw_u_swap16(uint64_t p) { LtmsUSwapInplace16UShort(*P<uint16_t>(p)); }

void tpw_u_swap32(uint64_t p) { LtmsUSwapInplace32ULong(*P<uint32_t>(p)); }

void tpw_u_addref(uint64_t p) { P<TbITable>(p)->TbITable::AddRef(); }

int32_t tpw_u_format(uint64_t tag, uint64_t buffer, uint64_t format) {
    return P<TbMBStringVTag>(tag)->TbMBStringVTag::Format(*P<FakeBuffer>(buffer), *P<TbVarFormat>(format));
}

}  // extern "C"
