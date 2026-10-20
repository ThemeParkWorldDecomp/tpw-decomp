// Unit w216: real stamp and zlib functions; recording calendar, allocation and buffer callbacks.
#include <cstdint>
#include <cstring>
#include <new>
#include "TbTimeDiff.h"
#include "LbFile.h"
#include "bfl_ZLib.h"
#include "platform/macdoze.h"
#include "calllog.h"
// Keep test-owned storage after a recorded delete so its guards remain observable.
static bool KeepRelease(void*, bool) { return calllog::on(); }
#define TPW_RELEASE_OBSERVER(p, array) KeepRelease(p, array)
#include "calllog.cpp"

using namespace calllog;
using namespace bfl::compress;
namespace {
alignas(16) uint8_t allocated[0x62a39 + 32];
uint8_t fullBytes[1 << 20];
uint32_t fullSizes[4096];
uint32_t fullCount, fullLength;
uint8_t emptyBytes[1 << 20];
uint32_t emptySizes[4096];
uint32_t emptyCount, emptyLength;
const uint8_t* input;
uint32_t inputLength, inputOffset;
}

// Both calendar fakes fill their outputs even on a queued failure.
int32_t NS_MacDoze::FileTimeToSystemTime(const tagFILETIME* ft, _SYSTEMTIME* st) {
    ev("FileTimeToSystemTime", {ft->dwLowDateTime, ft->dwHighDateTime});
    uint16_t* fields = reinterpret_cast<uint16_t*>(st);
    for (uint32_t i = 0; i < 8; ++i) fields[i] = static_cast<uint16_t>(ret("ftw", 0));
    return static_cast<int32_t>(ret("ftres", 0));
}
int32_t NS_MacDoze::SystemTimeToFileTime(const _SYSTEMTIME* st, tagFILETIME* ft) {
    ev("SystemTimeToFileTime", {st->wYear, st->wMonth, st->wDay, st->wHour, st->wMinute, st->wSecond, st->wMilliseconds});
    ft->dwLowDateTime = static_cast<uint32_t>(ret("low", 0));
    ft->dwHighDateTime = static_cast<uint32_t>(ret("high", 0));
    return static_cast<int32_t>(ret("stres", 0));
}

// The nothrow allocation is the only owned workspace; it is named on both sides.
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = on() && ret("newnull", 0) != 0;
    void* p = fail ? nullptr : allocated + 16;
    ev("new[]", {static_cast<int64_t>(size), p ? newlab(p) : 0});
    return p;
}

// Callback bytes are exported separately so full buffers are not truncated by calllog's text limit.
extern "C" int32_t tpw_szl_full(void* buffer, uint32_t size, void* context) {
    ev("full", {lab(buffer), size, lab(context)});
    fullSizes[fullCount++] = size;
    std::memcpy(fullBytes + fullLength, buffer, size);
    fullLength += size;
    return static_cast<int32_t>(ret("cbret", 0));
}
extern "C" int32_t tpw_szl_empty(void* buffer, uint32_t size, void* context) {
    ev("empty", {lab(buffer), size, lab(context)});
    emptySizes[emptyCount++] = size;
    std::memcpy(emptyBytes + emptyLength, buffer, size);
    emptyLength += size;
    const uint32_t remaining = inputLength - inputOffset;
    const uint32_t n = remaining < size ? remaining : size;
    std::memset(buffer, 0, size);
    if (n) std::memcpy(buffer, input + inputOffset, n);
    inputOffset += n;
    return static_cast<int32_t>(ret("cbret", 0));
}

extern "C" {
// Field sizes relied on by the test; the native callback uses native pointers.
int32_t tpw_szl_layout(int64_t* out) {
    out[0] = sizeof(TbTimeStamp);
    out[1] = sizeof(TbTimeDiff);
    out[2] = sizeof(ZLibCompHeader);
    out[3] = sizeof(Callbacks);
    return 4;
}
void tpw_szl_config(const uint8_t* data, uint32_t length) {
    std::memset(allocated, 0xd3, sizeof allocated);
    input = data;
    inputLength = length;
    inputOffset = 0;
    fullCount = 0;
    fullLength = 0;
    emptyCount = 0;
    emptyLength = 0;
}
const uint8_t* tpw_szl_full_bytes() { return fullBytes; }
const uint32_t* tpw_szl_full_sizes() { return fullSizes; }
uint32_t tpw_szl_full_count() { return fullCount; }
const uint8_t* tpw_szl_empty_bytes() { return emptyBytes; }
const uint32_t* tpw_szl_empty_sizes() { return emptySizes; }
uint32_t tpw_szl_empty_count() { return emptyCount; }
uint32_t tpw_szl_input_offset() { return inputOffset; }
int32_t tpw_szl_owned_guard() {
    for (uint32_t i = 0; i < 16; ++i) {
        if (allocated[i] != 0xd3 || allocated[0x62a39 + 16 + i] != 0xd3) return 0;
    }
    return 1;
}

// All stamp outputs remain in caller-owned guarded cells.
int32_t tpw_szl_stamp(int32_t op, TbTimeStamp* a, const uint32_t* b, int64_t* out, const int32_t* args) {
    const auto* stamp = reinterpret_cast<const TbTimeStamp*>(b);
    const auto* diff = reinterpret_cast<const TbTimeDiff*>(b);
    switch (op) {
    case 0: return a->AddYears(args[0]);
    case 1: return a->AddMonths(args[0]);
    case 2: return a->SetTime(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    case 3: return &(a->operator=(*const_cast<NS_MacDoze::tagFILETIME*>(reinterpret_cast<const NS_MacDoze::tagFILETIME*>(b)))) == a;
    case 4: return &(a->operator=(*reinterpret_cast<const TbFileStamp*>(b))) == a;
    case 5: return &(*a += *diff) == a;
    case 6: return &(*a -= *diff) == a;
    case 7: return *a != *stamp;
    case 8: return *a >= *stamp;
    case 9: return *a <= *stamp;
    case 10: return *a == *stamp;
    case 11: return *a < *stamp;
    default: *out = (*a - *diff).mTicks; return 0;
    }
}

// k=0/1 memory member/wrapper, 2/3 buffered pack, 4/5 buffered unpack, 6/7 callback members.
uint32_t tpw_szl_zlib(uint32_t k, uint8_t* source, uint8_t* dest, uint32_t length, uint8_t* buffer,
                      uint32_t size, uint8_t* workspace, void* context) {
    CZLib lib;
    Callbacks cb(tpw_szl_full, tpw_szl_empty, context);
    switch (k) {
    case 0: return lib.MemCompress(dest, source, length, workspace);
    case 1: return LbZLib_MemCompress(source, dest, length, workspace);
    case 2: return lib.BufferedCompress(source, length, buffer, size, workspace, cb);
    case 3: return LbZLib_BufferedCompress(source, buffer, length, size, tpw_szl_full, context, workspace);
    case 4: return lib.BufferedDecompress(dest, buffer, size, workspace, cb);
    case 5: return LbZLib_BufferedDecompress(dest, buffer, size, tpw_szl_empty, context, workspace);
    case 6: return static_cast<uint32_t>(cb.BufferFull(buffer, size));
    default: return static_cast<uint32_t>(cb.BufferEmpty(buffer, size));
    }
}
uint32_t tpw_szl_size(uint32_t k) {
    CZLib lib;
    if (k == 0) return static_cast<uint32_t>(lib.DecompressWorkspaceSize());
    if (k == 1) return LbZLib_DeCompressWorkspaceSize();
    return LbZLib_CompressWorkspaceSize();
}

// Construction precedes the observed destruction. The abstract base is constructed as a Callbacks subobject.
void tpw_szl_construct(uint32_t k, void* obj) {
    if (k == 0) new (obj) CZLib;
    else if (k == 1) new (obj) CBufferedCompressionBase;
    else new (obj) Callbacks(tpw_szl_full, tpw_szl_empty, obj);
}
void* tpw_szl_dtor(uint32_t k, void* obj, int16_t flag) {
    if (obj) {
        if (k == 0) static_cast<CZLib*>(obj)->~CZLib();
        else if (k == 1) static_cast<CBufferedCompressionBase*>(obj)->~CBufferedCompressionBase();
        else if (k == 2) static_cast<Callbacks*>(obj)->~Callbacks();
        else static_cast<CBufferCallback*>(obj)->~CBufferCallback();
        if (flag > 0) ::operator delete(obj);
    }
    return obj;
}
}
