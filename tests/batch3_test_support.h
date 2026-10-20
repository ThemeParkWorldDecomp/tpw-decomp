// Controlled boundary fakes for this batch, never game implementations.
#pragma once
#include <cstdint>
#include "LbFile.h"
struct Batch3Event {
    int32_t kind, allocBefore;
    const void* object;
    const void* argument;
    uint32_t args[4];
    uint32_t words[20];
    uint8_t bytes[256];
    char text[64];
};
extern "C" {
extern int32_t tpw_batch3_on, tpw_batch3_n;
extern Batch3Event tpw_batch3_log[128];
extern uint32_t tpw_batch3_mask, tpw_batch3_answer;
extern uint32_t tpw_batch3_clock, tpw_batch3_count, tpw_batch3_result;
extern uint8_t tpw_batch3_data[4];
extern int32_t tpw_batch3_read_size;
extern const void* tpw_batch3_colour_context;
void tpw_batch3_debug(const char* text);
}
// The batch-3 LbFile fakes (C++ linkage: they return TbError).
TbError tpw_batch3_read(void*, void*, uint32_t, uint32_t*);
TbError tpw_batch3_write(void*, const void*, uint32_t, uint32_t*);
