// Call-log support for the per-unit test libraries (tests/unit_<Name>.cpp, built into
// libtpw_unit_<Name>.dylib with only the unit's own sources; see CMakeLists.txt and
// tests/calllog.py).
//
// Every function the unit calls that is not part of the unit is a fake in the unit file
// that records an event: its name, integer arguments (object labels, values, float bits)
// and texts. The PowerPC side records the same events from emulator stubs, and a test
// compares the two logs in order.
//
// Objects compare by label: 0 is NULL, the test gives the objects it sets up fixed labels,
// a constructor fake gives its object the next free label (newlab), and anything else is
// -1. Return values come from queues the test fills per key (ret); the last value of a
// queue repeats.
#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>

extern "C" {
// One recorded event. ints hold labels, integer arguments and float bit patterns.
struct TpwCallEvent {
    char name[48];
    int32_t nints;
    int32_t ntexts;
    int64_t ints[12];
    char texts[3][160];
};
#ifndef TPW_CALLLOG_EVENTS
#define TPW_CALLLOG_EVENTS 4096
#endif
extern TpwCallEvent tpw_cl_log[TPW_CALLLOG_EVENTS];
extern int32_t tpw_cl_capacity;
extern int32_t tpw_cl_n;     // events recorded (counts past the capacity, so overflow shows)
extern int32_t tpw_cl_on;    // recording switch
extern int32_t tpw_cl_heap[4];   // blocks handled by operator new, new[], delete, delete[]

void tpw_cl_reset(int32_t firstNewLabel);
void tpw_cl_label(int32_t label, const void* object);
void tpw_cl_queue(const char* key, int64_t value);
int32_t tpw_cl_lab(const void* object);
}

namespace calllog {

bool on();
int32_t lab(const void* object);
int32_t newlab(const void* object);
void forget(const void* object);
int64_t ret(const char* key, int64_t dflt = 0);
// The queue for "key:arg" (for returns that depend on an argument).
int64_t ret(const char* key, int64_t arg, int64_t dflt);
uint32_t fbits(float value);
// A UTF-16 text as recorded: ASCII kept, anything else '?'.
std::string narrow(const char16_t* text);
void ev(const char* name, std::initializer_list<int64_t> ints = {}, std::initializer_list<const char*> texts = {});

}  // namespace calllog
