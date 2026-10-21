// Differential library for unit w125, the park clock's leaves (src/bullfrog/TbTimeStamp.cpp): TbTimeStamp::operator+, operator-, operator> and GetTime.
//
// The only callee is NS_MacDoze::FileTimeToSystemTime, a fake that records the FILETIME words it is handed (low word, then high word) and answers what
// the case queues: "ftres" is the result, "ftw" the eight SYSTEMTIME fields in the structure's order (year, month, day of week, day, hour, minute,
// second, millisecond). It writes the fields whatever it answers, so a function that reads them after a failure shows.
#include <cstdint>

#include "TbTimeDiff.h"
#include "calllog.h"
#include "platform/macdoze.h"

using namespace calllog;

int32_t NS_MacDoze::FileTimeToSystemTime(const tagFILETIME* fileTime, _SYSTEMTIME* time) {
    ev("FileTimeToSystemTime", {static_cast<int64_t>(fileTime->dwLowDateTime), static_cast<int64_t>(fileTime->dwHighDateTime)});
    time->wYear = static_cast<uint16_t>(ret("ftw", 0));
    time->wMonth = static_cast<uint16_t>(ret("ftw", 0));
    time->wDayOfWeek = static_cast<uint16_t>(ret("ftw", 0));
    time->wDay = static_cast<uint16_t>(ret("ftw", 0));
    time->wHour = static_cast<uint16_t>(ret("ftw", 0));
    time->wMinute = static_cast<uint16_t>(ret("ftw", 0));
    time->wSecond = static_cast<uint16_t>(ret("ftw", 0));
    time->wMilliseconds = static_cast<uint16_t>(ret("ftw", 0));
    return static_cast<int32_t>(ret("ftres", 0));
}

extern "C" {
// The result buffers are 8 bytes; the host's stamps and spans are plain int64_t.
void tpw_pc_add(const int64_t* self, const int64_t* diff, int64_t* out) {
    const TbTimeStamp s{*self};
    const TbTimeDiff d{*diff};
    *out = (s + d).mTicks;
}
void tpw_pc_sub(const int64_t* self, const int64_t* other, int64_t* out) {
    const TbTimeStamp s{*self};
    const TbTimeStamp o{*other};
    *out = (s - o).mTicks;
}
int32_t tpw_pc_gt(const int64_t* self, const int64_t* other) {
    const TbTimeStamp s{*self};
    const TbTimeStamp o{*other};
    return s > o ? 1 : 0;
}
// Eight output pointers (any may be NULL); the answer is the TbError's value.
int32_t tpw_pc_gettime(const int64_t* self, int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) {
    const TbTimeStamp s{*self};
    return static_cast<int32_t>(s.GetTime(a, b, c, d, e, f, g, h));
}
}
