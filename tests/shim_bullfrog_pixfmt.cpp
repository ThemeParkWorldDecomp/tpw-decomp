// extern "C" wrappers for the tests of the bullfrog TbPixFmt.cpp range (test_LbExeKey,
// test_LbList, test_LbLog, test_TbRefRelease, test_bullfrog_CPU, test_TbTimeDiff,
// test_TbFPUPrecision, test_TbPixFmt).
#include <cstring>
#include <new>

#include "LbExeKey.h"
#include "LbList.h"
#include "LbLog.h"
#include "TbFPUPrecision.h"
#include "TbPixFmt.h"
#include "TbRefRelease.h"
#include "TbTimeDiff.h"
#include "bullfrog_CPU.h"

namespace {
// A TbRefRelease whose destructor reports itself (stands in for a derived class).
struct TestRef : TbRefRelease {
    ~TestRef() override;
};
}  // namespace

extern "C" {
// ---- LbExeKey
uint32_t tpw_LbExeKey_ReferenceNumber() { return LbExeKey_ReferenceNumber(); }
void tpw_LbExeKey_Initialise() { LbExeKey_Initialise(); }

// ---- LbList
void tpw_LbList_Init(TbList* l) { LbList_Init(l); }
void tpw_LbList_Insert(TbList* l, TbListNode* after, TbListNode* n) { LbList_Insert(l, after, n); }
int32_t tpw_LbList_NodeExists(const TbList* l, const TbListNode* n) { return LbList_NodeExists(l, n); }
TbListNode* tpw_LbList_GetFirst(const TbList* l) { return LbList_GetFirst(l); }
TbListNode* tpw_LbList_GetLast(const TbList* l) { return LbList_GetLast(l); }

// ---- LbLog (results as the TbError value; LbLog takes up to six word-sized arguments)
int32_t tpw_LbLog_Setup(TbLog* log, const char* name, uint32_t flags) { return LbLog_Setup(log, name, flags); }
int32_t tpw_LbLog(TbLog* log, const char* format, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
                  uint64_t a4, uint64_t a5) {
    return LbLog(log, format, a0, a1, a2, a3, a4, a5);
}
int32_t tpw_LbLog_Close(TbLog* log) { return LbLog_Close(log); }
int32_t tpw_LbLog_SetPrefix(TbLog* log, const char* prefix) { return LbLog_SetPrefix(log, prefix); }
int32_t tpw_LbLog_Delete(TbLog* log) { return LbLog_Delete(log); }
int32_t tpw_LbLog_Suspend(TbLog* log) { return LbLog_Suspend(log); }
int32_t tpw_LbLog_Restart(TbLog* log) { return LbLog_Restart(log); }
int32_t tpw_LbLog_IsInitialised(TbLog* log) { return LbLog_IsInitialised(log); }
int32_t tpw_LbLog_IsSuspended(TbLog* log) { return LbLog_IsSuspended(log); }

// ---- TbRefRelease (objects on the heap; *count reads the count before any delete)
int32_t tpw_test_ref_dtors = 0;
TbRefRelease* tpw_TbRefRelease_new(int32_t derived, int32_t count) {
    TbRefRelease* p = derived ? new TestRef : new TbRefRelease;
    p->mRefCount = count;
    return p;
}
// Placement construction over a garbage block; reports whether the vptr is TbRefRelease's.
int32_t tpw_TbRefRelease_ctor(void* block, int32_t* count) {
    TbRefRelease* p = new (block) TbRefRelease;
    *count = p->mRefCount;
    TbRefRelease probe;
    int32_t same = std::memcmp(block, static_cast<const void*>(&probe), sizeof(void*)) == 0;
    p->~TbRefRelease();
    return same;
}
int32_t tpw_TbRefRelease_AddRef(TbRefRelease* p) { return p->AddRef(); }
int32_t tpw_TbRefRelease_count(TbRefRelease* p) { return p->mRefCount; }
int32_t tpw_TbRefRelease_Release(TbRefRelease* p) { return p->Release(); }
// Destructor convention: storage is freed when flag > 0.
void tpw_TbRefRelease_dtor(TbRefRelease* p, int16_t flag) {
    if (flag > 0) delete p;
    else p->~TbRefRelease();
}
void tpw_TbRefRelease_free(void* p) { ::operator delete(p); }

// ---- bullfrog CPU
void tpw_bfCPU_detect(bullfrog::CPU* c) { c->detect(); }
int32_t tpw_bfCPU_clocks(bullfrog::CPU* c, int32_t unused) { return c->clocks_per_millisec(unused); }
int32_t* tpw_bfCPU_clock_mhz() { return &bullfrog::gCPUClockMHz; }

// ---- TbTimeDiff (values as int64; results through out)
void tpw_TbTimeDiff_add(const int64_t* a, const int64_t* b, int64_t* out) {
    TbTimeDiff x{*a}, y{*b};
    *out = (x + y).mTicks;
}
void tpw_TbTimeDiff_add_stamp(const int64_t* a, const int64_t* b, int64_t* out) {
    TbTimeDiff x{*a};
    TbTimeStamp y{*b};
    *out = (x + y).mTicks;
}
void tpw_TbTimeDiff_sub(const int64_t* a, const int64_t* b, int64_t* out) {
    TbTimeDiff x{*a}, y{*b};
    *out = (x - y).mTicks;
}
int32_t tpw_TbTimeDiff_add_assign(TbTimeDiff* a, const TbTimeDiff* b) { return &(*a += *b) == a; }
int32_t tpw_TbTimeDiff_sub_assign(TbTimeDiff* a, const TbTimeDiff* b) { return &(*a -= *b) == a; }
int32_t tpw_TbTimeDiff_mul_int(TbTimeDiff* a, int32_t f) { return &(*a *= f) == a; }
int32_t tpw_TbTimeDiff_mul_float(TbTimeDiff* a, float f) { return &(*a *= f) == a; }
int32_t tpw_TbTimeDiff_div_int(TbTimeDiff* a, int32_t d) { return &(*a /= d) == a; }
int32_t tpw_TbTimeDiff_div_float(TbTimeDiff* a, float d) { return &(*a /= d) == a; }
int32_t tpw_TbTimeDiff_lt(const TbTimeDiff* a, const TbTimeDiff* b) { return *a < *b; }
int32_t tpw_TbTimeDiff_le(const TbTimeDiff* a, const TbTimeDiff* b) { return *a <= *b; }
int32_t tpw_TbTimeDiff_gt(const TbTimeDiff* a, const TbTimeDiff* b) { return *a > *b; }
int32_t tpw_TbTimeDiff_ge(const TbTimeDiff* a, const TbTimeDiff* b) { return *a >= *b; }
int32_t tpw_TbTimeDiff_ne(const TbTimeDiff* a, const TbTimeDiff* b) { return *a != *b; }
int32_t tpw_TbTimeDiff_eq(const TbTimeDiff* a, const TbTimeDiff* b) { return *a == *b; }
int32_t tpw_TbTimeDiff_Set(TbTimeDiff* a, int32_t d, int32_t h, int32_t m, int32_t s, int32_t ms) {
    return a->Set(d, h, m, s, ms);
}
int32_t tpw_TbTimeDiff_Get(const TbTimeDiff* a, int32_t* d, int32_t* h, int32_t* m, int32_t* s, int32_t* ms) {
    return a->Get(d, h, m, s, ms);
}

// ---- TbFPUFlag (fields as three shorts after the vptr)
TbFPUFlag* tpw_TbFPUFlag_new(uint16_t saved, uint16_t set, uint16_t clear) {
    TbFPUFlag* f = new TbFPUFlag;
    f->mSaved = saved;
    f->mSet = set;
    f->mClear = clear;
    return f;
}
void tpw_TbFPUFlag_read(const TbFPUFlag* f, uint16_t* out) {
    out[0] = f->mSaved;
    out[1] = f->mSet;
    out[2] = f->mClear;
}
int32_t tpw_TbFPUFlag_CheckFlags(TbFPUFlag* f) { return f->CheckFlags(); }
void tpw_TbFPUFlag_Reset(TbFPUFlag* f) { f->Reset(); }
void tpw_TbFPUFlag_SetFlags(TbFPUFlag* f, uint16_t set, uint16_t clear) { f->SetFlags(set, clear); }
// Runs the destructor (the test frees the block afterwards, as the convention frees it
// when flag > 0) and reads the fields it left.
void tpw_TbFPUFlag_dtor(TbFPUFlag* f, uint16_t* out) {
    f->~TbFPUFlag();
    tpw_TbFPUFlag_read(f, out);
    ::operator delete(f);
}
void tpw_TbFPUFlag_free(TbFPUFlag* f) { delete f; }

// ---- TbPixFmt
void tpw_TbMaskInfo_Set(TbMaskInfo* m, uint32_t mask, uint32_t top) { m->Set(mask, top); }
void tpw_TbPixelCaps_Set(TbPixelCaps* c, const TbPixFormat* f) { c->Set(*f); }
}

namespace {
TestRef::~TestRef() { tpw_test_ref_dtors++; }
}  // namespace
