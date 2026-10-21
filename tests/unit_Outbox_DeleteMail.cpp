// Test library for Outbox::DeleteMail (unit v5690; libtpw_unit_Outbox_DeleteMail.dylib, tests/test_Outbox_DeleteMail.py), main app:
//   src/game/online/email.cpp
//
// unlink is a fake that records the path as big-endian words (the same packing as the PowerPC stub).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

namespace outboxdel_callee {
void Unlink(const char* path) {
    size_t n = std::strlen(path);
    std::vector<int64_t> words;
    words.push_back(static_cast<int64_t>(n));
    if (n > 28) n = 28;
    for (size_t i = 0; i < n; i += 4) {
        uint32_t w = 0;
        for (size_t k = 0; k < 4; k++) w = (w << 8) | (i + k < n ? static_cast<uint8_t>(path[i + k]) : 0);
        words.push_back(w);
    }
    words.resize(8, 0);
    ev("unlink", {words[0], words[1], words[2], words[3], words[4], words[5], words[6], words[7]});
}
}  // namespace outboxdel_callee

extern "C" {

// Runs DeleteMail on an Outbox whose mF00 is `path`; writes mF00 afterwards to out (0x80 bytes).
void tpw_od_run(const char* path, int32_t id, char* out) {
    alignas(Outbox) static char storage[sizeof(Outbox)];
    std::memset(storage, 0, sizeof(storage));
    Outbox* o = reinterpret_cast<Outbox*>(storage);
    new (&o->mF00) std::string(path);
    o->DeleteMail(id);
    std::strncpy(out, o->mF00.c_str(), 0x7f);
    out[0x7f] = 0;
    o->mF00.~basic_string();
}

}  // extern "C"
