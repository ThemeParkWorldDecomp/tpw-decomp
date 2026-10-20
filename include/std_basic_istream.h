// std::basic_istream<char> read and getline (unit v5014). Layouts keep the Mac's 32-bit fields.
#pragma once
#include <cstdint>

#include "Ptr32.h"

struct MacStreamBuf;

// The stream's ios part: the stream buffer pointer at 0x24, the state bits at 0x34, the exception mask at 0x38.
struct MacIosBase {
    uint8_t pad0[0x24];
    Ptr32<MacStreamBuf> mStreamBuf;    // 0x24
    uint8_t pad28[0xc];
    uint32_t mState;        // 0x34
    uint32_t mExceptions;   // 0x38
};

// The stream buffer: vtable pointer at 0, get pointer at 8, end of get area at 0xc.
// vtable entries: 0x28 underflow/peek, 0x2c uflow/bump (TVectors on the Mac).
struct MacStreamBuf {
    Ptr32<uint8_t> mVtable;   // 0x00
    uint32_t pad4;            // 0x04
    Ptr32<uint8_t> mGptr;     // 0x08
    Ptr32<uint8_t> mEgptr;    // 0x0c
};

// The thrown ios_base::failure object the code builds on its stack.
struct MslThrowObject {
    const void* mVtbl;    // address of gIosFailureVtbl
    char* mMessage;       // new char[len + 1]
    int32_t* mCount;      // new int32_t(1)
};

extern char gIosFailureVtbl[];        // 0x1029d084 (initial word 0x1029d07c)
extern char gIosFailureDtorVec[];     // 0x102661d0 TVector of std::ios_base::failure::~failure

// Callees outside the unit (fakes in the test library).
bool MslIosGood(MacIosBase* ios);                       // std::ios_base::good 0x102079b4
void* MslIosTie(MacIosBase* ios);                       // basic_ios::tie 0x10207964
void MslOstreamFlush(void* stream);                     // basic_ostream::flush 0x1020a604
void MslIosSetstate(MacIosBase* ios, int32_t bits);     // ios_base::setstate 0x102077c0
int32_t MslStreamBufVirtual(MacStreamBuf* sb, uint32_t entry);   // __ptr_glue 0x1023baa8 with r12 = entry
void MslThrow(const char* type, MslThrowObject* object, const void* destructors);   // __throw

struct BasicIstreamChar {
    Ptr32<MacIosBase> mIos;   // 0x00
    int32_t mGcount;          // 0x04

    BasicIstreamChar* read(char* buffer, int32_t count);                      // 0x1020d6b4
    BasicIstreamChar* getline(char* buffer, int32_t count, char delimiter);   // 0x102079f8
};

// Unit v5068: std::__putnumber<char, ostreambuf_iterator<char>> 0x1020a140.
// The output stream buffer: pptr at 0x14, epptr at 0x18, vtable at 0 (entry 0x38 is overflow, through __ptr_glue).
struct MacPutStreamBuf {
    Ptr32<uint8_t> mVtable;     // 0x00
    uint8_t pad4[0x10];
    Ptr32<uint8_t> mPptr;       // 0x14
    Ptr32<uint8_t> mEpptr;      // 0x18
};

// The ios_base seen by the number writer: field width at 0x2c, format flags at 0x30.
struct MacPutIos {
    uint8_t pad0[0x2c];
    int32_t mWidth;             // 0x2c
    uint32_t mFlags;            // 0x30
};

// Callee outside the unit (fake in the test library): streambuf overflow(c) via __ptr_glue 0x1023baa8, vtable entry 0x38.
int32_t MslPutOverflow(MacPutStreamBuf* sb, uint32_t c);

// The iterator is its streambuf pointer; the returned iterator is NULL once a put failed.
MacPutStreamBuf* PutNumberChar(MacPutStreamBuf* iter, MacPutIos* ios, char fill, const char* prefix, int32_t prefixLen,
                               const char* suffix, int32_t suffixLen);   // 0x1020a140

// Unit v5251: std::basic_ostream<char>::sentry::~sentry 0x1020758c (132).
// The ostream starts with its ios pointer; the ios has the format flags at 0x30 and the state bits at 0x34.
struct MacSentryIos {
    uint8_t pad0[0x30];
    uint32_t mFlags;    // 0x30 (0x2000: unitbuf)
    uint32_t mState;    // 0x34
};
struct MacSentryStream {
    Ptr32<MacSentryIos> mIos;   // 0x00
};
// Packed: the stream pointer sits at the odd offset 2.
struct __attribute__((packed)) MacSentryObject {
    uint8_t mPad0;                    // 0x00
    uint8_t mDone;                    // 0x01
    Ptr32<MacSentryStream> mStream;   // 0x02
};
void MslOperatorDelete(void* p);                                  // operator delete 0x10238218 (fake in the test library)
// Flushes a unitbuf stream that is good and not yet done; frees the object for a positive (signed 16 bit) flag. Answers this.
MacSentryObject* OstreamSentryDtor(MacSentryObject* self, int16_t deleteFlag);   // 0x1020758c

// Unit v5663: std::basic_ostream<char>::flush 0x1020a604 (848). The stream starts with its ios pointer. Answers self.
BasicIstreamChar* OstreamFlush(BasicIstreamChar* self);

// Unit v5708: std::basic_istream<wchar_t> ~basic_istream 0x1021d4a0, getline 0x102202b0, basic_istream 0x102266e4
// (src/..._SaveIgnore_std_basic_istream_Dtor_basic_istream_2.cpp). ~basic_istream and getline are declared in online.h
// (ignore_callee::IstreamDtor, IstreamGetline). The host stream: Mac offsets in comments, native pointers.
namespace ignore_callee {
struct IgnoreIos;
struct IgnoreStream;
// Constructor: withVirtualBase (signed 16 bit, nonzero) points the stream at its own ios part first. Answers the stream.
IgnoreStream* IstreamCtor(IgnoreStream* stream, int16_t withVirtualBase, uint32_t streambuf);   // 0x102266e4
}  // namespace ignore_callee
struct WideIstream {
    ignore_callee::IgnoreIos* mIos;   // Mac 0x00
    int32_t mGcount;                  // Mac 0x04 (host 0x08)
    const void* mVtable;              // Mac 0x08 (host 0x10)
    // the ios part follows: Mac 0x0c, host 0x18
};
constexpr uint32_t kWideIstreamIosPart = 0x18;   // host offset of the ios part (Mac 0x0c)
namespace wistream_callee {
void IosInit(void* ios, uint32_t streambuf);   // std::ios_base::init 0x10238968
char16_t Widen(void* ios, char c);             // basic_ios<wchar_t>::widen 0x10226828
}  // namespace wistream_callee

// Unit v5898: std::basic_ostream<wchar_t>::flush 0x102268f8 and basic_ostream<wchar_t>(streambuf*) 0x10226c94
// (src/std_basic_ostream_flush_2.cpp). The stream starts with its ios pointer (Mac 0x00); the constructor writes Mac offsets.
extern uint32_t gWostreamVtbl[4];     // 0x102b1f08 (TOC -0x5eb4; initial word 0x102b1f00)
struct WostreamW {
    Ptr32<MacIosBase> mIos;   // 0x00
};
namespace wostream_u {
bool Good(void* ios);                                     // 0x102079b4
void* Tie(void* ios);                                     // 0x1021f888
void Setstate(void* ios, int32_t bits);                   // 0x102077c0
int32_t Virt(MacStreamBuf* sb, uint32_t entry);           // __ptr_glue 0x1023baa8 (r12 = entry)
void Throw(const char* type, MslThrowObject* object, const void* destructors);   // __throw
char* NewArray(uint32_t size);                            // operator new[]
int32_t* NewInt();                                        // operator new(4)
void IosInit(void* ios, uint32_t streambuf);              // 0x10238968
char16_t Widen(void* ios, char c);                        // 0x10226828
}  // namespace wostream_u
WostreamW* WostreamFlush(WostreamW* self);                                              // 0x102268f8
WostreamW* WostreamConstruct(WostreamW* self, int16_t flag, uint32_t streambuf);        // 0x10226c94
