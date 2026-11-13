// host_sound_callback_release.cpp: a sentence whose next sample cannot start, as the TPI worlds' rides play theirs (their
// sentences' samples come from the donor's bank, and some of them fail to start). The sentence's callback pipe asks its
// callback for the next sample from inside the pull (_TbSoundSampleCallback::GetNextSample), and a sample that fails to start
// makes CCodecFactory::CreateCodec release the channel: in the original that deleted the callback pipe still running, which
// then wrote to the freed block and was deleted again by its guard (CAutoDetach). Its pool held the block twice (linked to
// itself), the next two sounds got the same pipe, a chain looped on itself and the game hung in TbSoundSamplePipe::FindPipe
// (2026-11-12). In the mod's worlds (TPW_MOD_HOOK tpi_worlds in CCodecFactory::CreateCodec) the channel is left to the
// running pipe, which goes on as when the callback starts nothing (a timer codec of silence, then the channel ends).
//
// The pipes, codecs and pools are the real ones, cut from their src files (the same as tests/host_sound_wavfile_keep.cmake,
// with _TbSoundSampleCallback::CallsBackIn); the channel (the pull and the release, as TbSoundPipeChannel::GetDataChunk and
// ReleasePipe) and the sentence (the failed start: the last lines of CCodecFactory::CreateCodec, then the attach of
// TbSoundSampleStore::CreateNewSample) are the test's. The test plays in one of the mod's worlds (the mod on, the theme the
// mod's). Run by ctest ("sound_callback_release") in a build with the port's fixes; a build without them makes the program,
// which takes the original's path and fails. Exits nonzero on a failure.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

#include "AudioPlaceHolder.h"
#include "LbTime.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleCallback.h"
#ifdef TPW_TEST_PORT_FIX
#include "TpiGateFlow.h"
#endif

using bfl::FileSystem::CFile;

// The pools' words, defined with much else in src/game/advisor/AdvisorScoring.cpp (TbSoundSamplePipe's come with its pool,
// src/sound/TbSoundSamplePipe.cpp).
_TbSoundSampleWAV* _TbSoundSampleWAV::mFirst_TbSoundSampleWAV = nullptr;
int32_t _TbSoundSampleWAV::mCurrent_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLow_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mHigh_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mTotal_TbSoundSampleWAV = 0;
int32_t _TbSoundSampleWAV::mLevel_TbSoundSampleWAV = 10;
_TbSoundSampleWAVFile* _TbSoundSampleWAVFile::mFirst_TbSoundSampleWAVFile = nullptr;
int32_t _TbSoundSampleWAVFile::mCurrent_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLow_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mHigh_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mTotal_TbSoundSampleWAVFile = 0;
int32_t _TbSoundSampleWAVFile::mLevel_TbSoundSampleWAVFile = 10;
TbTimerCodec* TbTimerCodec::mFirstTbTimerCodec = nullptr;
int32_t TbTimerCodec::mCurrentTbTimerCodec = 0;
int32_t TbTimerCodec::mLowTbTimerCodec = 0;
int32_t TbTimerCodec::mHighTbTimerCodec = 0;
int32_t TbTimerCodec::mTotalTbTimerCodec = 0;
int32_t TbTimerCodec::mLevelTbTimerCodec = 10;
TbSoundUserCallback* TbSoundUserCallback::mFirstTbSoundUserCallback = nullptr;
int32_t TbSoundUserCallback::mCurrentTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLowTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mHighTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mTotalTbSoundUserCallback = 0;
int32_t TbSoundUserCallback::mLevelTbSoundUserCallback = 10;
_TbSoundSampleCallback* _TbSoundSampleCallback::mFirst_TbSoundSampleCallback = nullptr;
int32_t _TbSoundSampleCallback::mCurrent_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLow_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mHigh_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mTotal_TbSoundSampleCallback = 0;
int32_t _TbSoundSampleCallback::mLevel_TbSoundSampleCallback = 10;

// The library's clock, which the timer codecs ask: a counter that moves 1000 seconds a call (a timer codec of the pull's
// bytes lasts some 4600 seconds at the rate 0 of a codec with no header), so nothing depends on time.
uint32_t LbTime_GetClock() {
    static uint32_t clock = 0;
    clock += 1000000;
    return clock;
}

// The open file's out-of-line members (bullfrog imports), which the PCM file codec of the cut files reaches; never called.
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t CFile::GetSize() { return -1; }
int32_t CFile::GetPos() { return -1; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return -1; }

TbSndCallback::TbSndCallback() {}
void TbSndCallback::SoundCallback(void*, void*) {}
void TbSndCallback::CompletedCallback(void*, void*, int32_t) {}

namespace {

constexpr int32_t kPull = 0x1200;   // the pull buffer of platform/sound_device.cpp (Stream::PullOnce): 4608 bytes
constexpr int kPulls = 200;         // pulls before the test gives up on the channel ending

int gFailures = 0;

void Fail(const char* scenario, const char* what) {
    std::printf("  FAIL %s: %s\n", scenario, what);
    ++gFailures;
}

// The head of a sample's chain, as TbSoundPipeChannel: the pipe behind it fills the buffer, and when it answers that its
// sample is over the channel releases the chain (ReleasePipe: the pipe behind it is released and forgotten).
class Channel : public TbSoundSamplePipe {
public:
    int32_t Pull(uint8_t* buffer, int32_t size) {
        int32_t done = 0;
        if (mNext != nullptr && mNext->GetDataChunk(buffer, size, done, false) == 1) {
            ReleasePipe();
            return 1;
        }
        return 0;
    }
    void ReleasePipe() {
        if (mNext != nullptr) {
            mNext->Release();
            mNext = nullptr;
            ++mReleases;
        }
    }
    int32_t mReleases = 0;
};

uint32_t gNextHandle = 1;

// A sentence whose next sample cannot start. Its first sample is a short silence (a timer codec, a TbSoundSampleCodec as
// every sample is); when it ends, SoundCallback is asked for the next one, which fails to start: the factory's failure path
// (CCodecFactory::CreateCodec's last lines, with the fix) and then the store's attach of the codec it did not make.
class Sentence : public TbSndCallback {
public:
    Sentence(Channel* channel, Channel* other) : mChannel(channel), mOther(other) {}

    void Start() {
        _TbSoundSampleCallback* callback = new _TbSoundSampleCallback(this);
        callback->Attatch(new TbTimerCodec(0, 0));
        callback->SetHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(gNextHandle++)));
        mChannel->Attatch(callback);
    }

    void SoundCallback(void* handle, void*) override {
        ++mCalls;
        TbSoundSamplePipe* caller = mChannel->FindPipe(handle);
        if (caller == nullptr) mLost = true;
#ifdef TPW_TEST_PORT_FIX
        mSeenInOwn = _TbSoundSampleCallback::CallsBackIn(mChannel);
        mSeenInOther = _TbSoundSampleCallback::CallsBackIn(mOther);
        // CCodecFactory::CreateCodec: in the mod's worlds the channel is left to the running pipe.
        if (!(tpw::TpiWorldInPlay() && _TbSoundSampleCallback::CallsBackIn(mChannel))) mChannel->ReleasePipe();
#else
        mChannel->ReleasePipe();   // the original's CCodecFactory::CreateCodec
#endif
        // TbSoundSampleStore::CreateNewSample: the codec the factory answered (none) is attached behind the caller.
        if (caller != nullptr && !mLost) caller->Attatch(nullptr);
    }

    Channel* mChannel;
    Channel* mOther;
    int32_t mCalls = 0;
    bool mSeenInOwn = false;
    bool mSeenInOther = true;
    bool mLost = false;
};

// The callback pipes' free list: true when it holds a block twice (the double delete links a block to itself).
bool CallbackPoolHoldsABlockTwice() {
    std::set<const void*> seen;
    int steps = 0;
    for (_TbSoundSampleCallback* idle = _TbSoundSampleCallback::mFirst_TbSoundSampleCallback; idle != nullptr && steps < 10000;
         idle = idle->mSampleCallbackPoolLink, ++steps) {
        if (!seen.insert(idle).second) return true;
    }
    return false;
}

// The crash: the sentence's next sample fails to start inside the pull. The pool keeps each block once, the next two callback
// pipes are two, the sentence goes on in silence and its channel ends once.
void NextSampleFails() {
    const char* name = "next sample fails to start";
    Channel channel;
    Channel other;
    Sentence sentence(&channel, &other);
#ifdef TPW_TEST_PORT_FIX
    if (_TbSoundSampleCallback::CallsBackIn(&channel)) Fail(name, "a channel is calling back before any pull");
#endif
    sentence.Start();
    std::vector<uint8_t> buffer(kPull, 0xA5);
    int pull = 0;
    while (sentence.mCalls == 0 && pull < kPulls) {
        channel.Pull(buffer.data(), kPull);
        ++pull;
    }
    if (sentence.mCalls != 1) Fail(name, "the sentence was not asked for its next sample once");
    if (sentence.mLost) Fail(name, "the callback pipe was not found by its handle");
#ifdef TPW_TEST_PORT_FIX
    if (!sentence.mSeenInOwn) Fail(name, "the channel was not seen calling back from its callback");
    if (sentence.mSeenInOther) Fail(name, "another channel was seen calling back");
    if (_TbSoundSampleCallback::CallsBackIn(&channel)) Fail(name, "the channel is still calling back after the pull");
#endif
    const bool twice = CallbackPoolHoldsABlockTwice();
    std::printf("  %s: asked %d, pool holds a block twice %s, releases after the pull %d\n", name, sentence.mCalls,
                twice ? "YES" : "no", channel.mReleases);
    if (twice) {
        // The original's path: nothing more is safe to run (the next two pipes are one block).
        Fail(name, "the callback pipe was deleted twice (its pool holds its block twice)");
        return;
    }
    if (channel.mReleases != 0) Fail(name, "the channel was released while its callback ran");
    if (channel.mNext == nullptr || channel.mNext->mNext != nullptr) Fail(name, "the channel does not hold one pipe (the silence)");
    while (channel.mNext != nullptr && pull < kPulls) {
        channel.Pull(buffer.data(), kPull);
        ++pull;
    }
    if (channel.mNext != nullptr) Fail(name, "the channel did not end");
    if (channel.mReleases != 1) Fail(name, "the channel was not released exactly once");
    for (uint8_t byte : buffer) {
        if (byte != 0) {
            Fail(name, "the pull is not silence");
            break;
        }
    }
    _TbSoundSampleCallback* a = new _TbSoundSampleCallback(&sentence);
    _TbSoundSampleCallback* b = new _TbSoundSampleCallback(&sentence);
    if (a == b) Fail(name, "two callback pipes are the same block");
    delete a;
    delete b;
    if (CallbackPoolHoldsABlockTwice()) Fail(name, "the pool holds a block twice at the end");
}

// A pull with no callback: nothing is calling back, on any channel.
void NothingCallingBack() {
#ifdef TPW_TEST_PORT_FIX
    const char* name = "nothing calling back";
    Channel channel;
    if (_TbSoundSampleCallback::CallsBackIn(&channel)) Fail(name, "an empty channel is calling back");
    if (_TbSoundSampleCallback::CallsBackIn(nullptr)) Fail(name, "no chain is calling back");
#endif
}

}  // namespace

int main() {
    std::printf("sound callback release: a sentence's next sample that fails to start inside the pull\n");
#ifdef TPW_TEST_PORT_FIX
    gModTpiWorlds = true;   // the mod on, in one of its worlds
    tpw::gTpiThemeIsMod = true;
#else
    std::printf("  (a build without the port's fixes: the original's path)\n");
#endif
    NothingCallingBack();
    NextSampleFails();
    if (gFailures != 0) {
        std::fprintf(stderr, "FAILED: %d checks\n", gFailures);
        return 1;
    }
    std::printf("sound callback release: OK\n");
    return 0;
}
