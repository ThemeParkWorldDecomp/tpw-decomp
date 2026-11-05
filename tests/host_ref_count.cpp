// host_ref_count.cpp: a sound bank's reference count from two threads. Each codec holds a reference on its bank (CRefManager,
// include/TbSoundBank.h): the game adds it as a sample starts (CCodecFactory::CreateCodec) and SDL's audio thread drops it as
// the sample ends (the codec's destructor). TbRefRelease counted with a plain load, add and store, so in parallel one thread's
// change was lost now and then: a lost add left the count one short, and the bank was deleted while still registered (the next
// sample read a freed bank); a lost drop left it one over (a bank never freed). The port's fix (refcount,
// src/bullfrog/TbRefRelease.cpp) adds and drops atomically. The functions are the real ones, from that file; a "game" thread
// adds references and an "audio" thread drops each one once it is there, as the game does, and at the end the count must be
// the bank's own one reference, with no delete before the last drop. Run by ctest ("ref_count") in a build with the port's
// fixes; a build without them makes the program, which fails. Exits nonzero on a failure.
#include <atomic>
#include <cstdio>
#include <thread>

#include "TbRefRelease.h"

namespace {

std::atomic<int> gDeletes{0};

// The bank. Its storage is static and never freed, so a delete that comes too early is counted instead of freeing memory the
// threads still use.
class Bank : public TbRefRelease {
public:
    ~Bank() override { ++gDeletes; }
    static void* operator new(std::size_t, void* at) { return at; }
    static void operator delete(void*) {}
};

alignas(Bank) unsigned char gBankBytes[sizeof(Bank)];

constexpr long kSamples = 20000000;   // samples started (and ended) in the run

}  // namespace

int main() {
    Bank* bank = new (gBankBytes) Bank;   // the bank's own reference: 1
    std::atomic<long> started{0};
    std::thread game([&] {
        for (long i = 0; i < kSamples; ++i) {
            bank->AddRef();
            started.store(i + 1, std::memory_order_release);
        }
    });
    std::thread audio([&] {
        for (long ended = 0; ended < kSamples;) {
            if (ended < started.load(std::memory_order_acquire)) {
                bank->Release();
                ++ended;
            }
        }
    });
    game.join();
    audio.join();

    int failures = 0;
    const int32_t count = bank->mRefCount;
    const int deletes = gDeletes.load();
    std::printf("%s: after %ld samples the count is the bank's one reference (%d)\n", count == 1 ? "ok" : "FAILED", kSamples,
                count);
    if (count != 1) ++failures;
    std::printf("%s: the bank was not deleted while its samples played (%d deletes)\n", deletes == 0 ? "ok" : "FAILED", deletes);
    if (deletes != 0) ++failures;
    if (failures == 0) {
        bank->Release();   // the bank's own reference: the one delete
        if (gDeletes.load() != 1) {
            std::printf("FAILED: the last release deleted the bank %d times\n", gDeletes.load());
            ++failures;
        }
    }
    if (failures != 0) {
        std::fprintf(stderr, "FAILED: %d checks\n", failures);
        return 1;
    }
    std::printf("ref count: OK\n");
    return 0;
}
