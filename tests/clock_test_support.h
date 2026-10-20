// Test controls and ordered observations for the Clock/Cheat placeholders.
#pragma once
#include <cstdint>

enum {
    kClockRead = 1, kClockReadFixed, kClockPause, kClockPauseFixed,
    kClockUnPause, kClockUnPauseFixed, kClockToggle, kClockToggleFixed,
    kClockPauseFlag, kClockSave, kClockSaveFixed, kClockLoad, kClockLoadFixed,
    kClockAdjust, kClockAdjustFixed, kClockVariableCtor, kClockFixedCtor,
    kCheatMsgCtor, kCheatSubscriptionCtor, kCheatPost, kCheatDebug,
    kCheatRead, kCheatWrite
};

extern "C" {
struct ClockTestEvent {
    int32_t kind;
    int32_t allocBefore;
    const void* object;
    const void* argument;
    uint32_t value;
    uint32_t count;
    uint32_t words[17];
    uint32_t context[25];
    char text[64];
};
extern int32_t tpw_clock_log_on;
extern int32_t tpw_clock_log_n;
extern ClockTestEvent tpw_clock_log[256];
extern int32_t tpw_clock_returns[32];
extern uint32_t tpw_clock_masks[32];
extern const void* tpw_clock_context;
extern uint32_t tpw_cheat_msg_flags;
extern uint32_t tpw_cheat_subscription_words[4];
extern uint32_t tpw_cheat_post_mask;
extern int32_t tpw_cheat_io_i;
extern uint32_t tpw_cheat_io_count[2];
extern uint32_t tpw_cheat_io_result[2];
extern uint8_t tpw_cheat_io_data[2];
extern uint8_t tpw_cheat_io_write[2];
}
