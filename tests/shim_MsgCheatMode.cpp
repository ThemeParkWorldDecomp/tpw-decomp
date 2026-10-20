// extern "C" wrappers for CMsgCheatMode, main app 0x10147050 and 0x101472bc.
// Native C++ has different vtables and pointer widths. Compare the flags/payload and
// actual storage deletion; the PowerPC test checks its exact derived vtable store.
#include <new>
#include <cstring>
#include "MsgCheatMode.h"

extern "C" {
CMsgCheatMode* tpw_cheat_message_create(uint32_t flags, uint8_t payload, const uint8_t* tail) {
    auto* message = new (::operator new(sizeof(CMsgCheatMode) + 4)) CMsgCheatMode(false);
    message->mFlags = flags;
    message->mCheating = payload;
    std::memcpy(&message->mCheating + 1, tail, 7);
    return message;
}
int32_t tpw_CMsgCheatMode_GetType(const CMsgCheatMode* message) { return message->GetType(); }
void tpw_cheat_message_fields(const CMsgCheatMode* message, uint32_t* flags, uint8_t* payload, uint8_t* tail) {
    *flags = message->mFlags;
    *payload = message->mCheating;
    std::memcpy(tail, &message->mCheating + 1, 7);
}
void* tpw_CMsgCheatMode_dtor(CMsgCheatMode* message, int16_t flag, uint32_t* flags, uint8_t* payload, uint8_t* tail) {
    if (message != nullptr) {
        // Save byte offsets before ending the lifetime; afterwards inspect the storage
        // with memcpy, before the optional free, instead of reading a destroyed object.
        auto* flagsAt = reinterpret_cast<unsigned char*>(&message->mFlags);
        auto* payloadAt = &message->mCheating;
        message->~CMsgCheatMode();
        std::memcpy(flags, flagsAt, sizeof *flags);
        std::memcpy(payload, payloadAt, sizeof *payload);
        std::memcpy(tail, payloadAt + 1, 7);
        if (flag > 0) ::operator delete(message);
    }
    return message;
}
void tpw_cheat_message_free_storage(void* message) { ::operator delete(message); }
}
