// See calllog.h. Also replaces the global operator new and delete for the unit library:
// deleting an object that has a label records a "delete" event (the PowerPC side records
// the same from operator delete and from destructor stubs called with a positive flag).
#include "calllog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

extern "C" {
TpwCallEvent tpw_cl_log[TPW_CALLLOG_EVENTS];
int32_t tpw_cl_capacity = TPW_CALLLOG_EVENTS;
int32_t tpw_cl_n = 0;
int32_t tpw_cl_on = 0;
// How many blocks operator new, new[], delete and delete[] have handled (a test clears and
// reads them around a call to compare the heap traffic with the original's).
int32_t tpw_cl_heap[4] = {};
}

namespace {

// A unit file that includes this one may raise the two capacities first (a test with
// hundreds of live objects, or with a long queue of results).
#ifndef TPW_CALLLOG_LABELS
#define TPW_CALLLOG_LABELS 512
#endif
#ifndef TPW_CALLLOG_QUEUE_VALUES
#define TPW_CALLLOG_QUEUE_VALUES 64
#endif
constexpr int32_t kLabels = TPW_CALLLOG_LABELS;
const void* gObjects[kLabels];
int32_t gNextLabel = 100;

struct Queue {
    char key[64];
    int64_t values[TPW_CALLLOG_QUEUE_VALUES];
    int32_t count;
    int32_t next;
};
Queue gQueues[128];
int32_t gQueueCount = 0;

Queue* find_queue(const char* key) {
    for (int32_t i = 0; i < gQueueCount; ++i)
        if (std::strcmp(gQueues[i].key, key) == 0) return &gQueues[i];
    return nullptr;
}

void copy_text(char* out, std::size_t size, const char* text) {
    if (text == nullptr) text = "(null)";
    std::strncpy(out, text, size - 1);
    out[size - 1] = '\0';
}

}  // namespace

extern "C" {

void tpw_cl_reset(int32_t firstNewLabel) {
    tpw_cl_n = 0;
    for (auto& object : gObjects) object = nullptr;
    gNextLabel = firstNewLabel;
    gQueueCount = 0;
}

void tpw_cl_label(int32_t label, const void* object) {
    if (label > 0 && label < kLabels) gObjects[label] = object;
}

int32_t tpw_cl_lab(const void* object) { return calllog::lab(object); }

void tpw_cl_queue(const char* key, int64_t value) {
    Queue* queue = find_queue(key);
    if (queue == nullptr && gQueueCount < 128) {
        queue = &gQueues[gQueueCount++];
        copy_text(queue->key, sizeof(queue->key), key);
        queue->count = 0;
        queue->next = 0;
    }
    if (queue != nullptr && queue->count < TPW_CALLLOG_QUEUE_VALUES) queue->values[queue->count++] = value;
}

}  // extern "C"

namespace calllog {

bool on() { return tpw_cl_on != 0; }

int32_t lab(const void* object) {
    if (object == nullptr) return 0;
    for (int32_t i = 1; i < kLabels; ++i)
        if (gObjects[i] == object) return i;
    return -1;
}

int32_t newlab(const void* object) {
    forget(object);
    if (gNextLabel >= kLabels) return -1;
    gObjects[gNextLabel] = object;
    return gNextLabel++;
}

void forget(const void* object) {
    if (object == nullptr) return;
    for (int32_t i = 1; i < kLabels; ++i)
        if (gObjects[i] == object) gObjects[i] = nullptr;
}

int64_t ret(const char* key, int64_t dflt) {
    Queue* queue = find_queue(key);
    if (queue == nullptr || queue->count == 0) return dflt;
    const int32_t i = queue->next < queue->count ? queue->next++ : queue->count - 1;
    return queue->values[i];
}

int64_t ret(const char* key, int64_t arg, int64_t dflt) {
    char full[64];
    std::snprintf(full, sizeof(full), "%s:%lld", key, static_cast<long long>(arg));
    return ret(full, dflt);
}

uint32_t fbits(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

std::string narrow(const char16_t* text) {
    std::string out;
    if (text == nullptr) return "(null)";
    for (; *text != 0 && out.size() < 159; ++text) out += *text < 0x80 ? static_cast<char>(*text) : '?';
    return out;
}

void ev(const char* name, std::initializer_list<int64_t> ints, std::initializer_list<const char*> texts) {
    if (!on()) return;
    if (tpw_cl_n >= 1000000) {
        // a translation that never leaves a loop of fakes: end the run with a reason
        std::fprintf(stderr, "calllog: over a million events (last: %s), probably an endless loop\n", name);
        std::abort();
    }
    const int32_t n = tpw_cl_n++;
    if (n >= TPW_CALLLOG_EVENTS) return;
    TpwCallEvent& event = tpw_cl_log[n];
    copy_text(event.name, sizeof(event.name), name);
    event.nints = 0;
    for (int64_t value : ints)
        if (event.nints < 12) event.ints[event.nints++] = value;
    event.ntexts = 0;
    for (const char* text : texts)
        if (event.ntexts < 3) copy_text(event.texts[event.ntexts++], sizeof(event.texts[0]), text);
}

}  // namespace calllog

namespace {

void* allocate(std::size_t size, bool array) {
    void* p = std::malloc(size != 0 ? size : 1);
    if (p == nullptr) throw std::bad_alloc();
    ++tpw_cl_heap[array ? 1 : 0];
#ifdef TPW_ALLOCATION_OBSERVER
    TPW_ALLOCATION_OBSERVER(size, array, p);
#endif
    return p;
}

void release(void* p, bool array) {
    if (p != nullptr) ++tpw_cl_heap[array ? 3 : 2];
    if (p != nullptr && calllog::on() && calllog::lab(p) > 0) {
        calllog::ev("delete", {calllog::lab(p)});
        calllog::forget(p);
    }
#ifdef TPW_RELEASE_OBSERVER
    // A unit file that includes this one may keep the freed blocks until the end of its case (so that no address is reused
    // while a stale pointer to it is compared): the observer answers true for a block it keeps.
    if (TPW_RELEASE_OBSERVER(p, array)) return;
#endif
    std::free(p);
}

}  // namespace

void* operator new(std::size_t size) { return allocate(size, false); }
void* operator new[](std::size_t size) { return allocate(size, true); }
void operator delete(void* p) noexcept { release(p, false); }
void operator delete[](void* p) noexcept { release(p, true); }
void operator delete(void* p, std::size_t) noexcept { release(p, false); }
void operator delete[](void* p, std::size_t) noexcept { release(p, true); }
