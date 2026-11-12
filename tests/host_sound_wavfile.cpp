// host_sound_wavfile.cpp: the PCM samples of a streaming bank played by an endless sentence, as the TPI worlds' rides play
// theirs (Land of Invention's bumper karts and squirt rides, cat_ridesSFX.map events 134 and 300). Every bank streams
// (CBankFactory::ConstructBank makes a TbMMFileBank), so a PCM sample is read by _TbSoundSampleWAVFile through the bank's one
// open file, and TbMMFileBank::RegisterBank leaves that file near its end. The original codec reads from wherever the file
// is: the bytes it counts as left are negative, the file's Read takes them as a huge count (on to the end of the file, past
// the 4608-byte pull buffer of platform/sound_device.cpp), and the chunk it answers is negative, so the sentence's callback
// pipe starts its next sample, which does the same, one call deeper each time, until the game crashes (2026-11-11). The
// port's fixes: wavfileseek (src/sound/TbSoundSampleWAVFile.cpp) reads each sample from its own place, and chaindepth
// (src/sound/TbSoundSampleCallback.cpp) bounds the chain of samples ending in one pull (a sample of no length).
//
// The pipes, codecs and pools are the real ones, cut from their src files (tests/host_sound_wavfile_keep.cmake); the bank
// file, the sentence (the TbSndCallback that attaches the next sample, as TbSoundCore::NewSample and
// TbSoundSampleStore::CreateNewSample do: the pipe found by its handle from the channel, a new callback pipe with a new handle
// and the codec behind it) and the channel (the pull, as TbSoundPipeChannel::GetDataChunk) are the test's. A runaway chain is
// cut at kRunaway samples in one pull (a pipe that claims the rest of the buffer and writes nothing), so a build without the
// fixes ends and reports instead of crashing. Run by ctest ("sound_wavfile") in a build with the port's fixes; a build without
// them makes the program, which fails. Exits nonzero on a failure.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "AudioPlaceHolder.h"
#include "LbTime.h"
#include "TbSoundBank.h"
#include "TbSoundCodecs.h"
#include "TbSoundPipe.h"
#include "TbSoundSampleCallback.h"

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

// The library's clock (the timer codec of a sentence that stops asks it): a counter, so nothing depends on time.
uint32_t LbTime_GetClock() {
    static uint32_t clock = 0;
    return ++clock;
}

// The open file's out-of-line members (bullfrog imports); the bank file below overrides what the codec calls.
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

constexpr int32_t kPull = 0x1200;      // the pull buffer of platform/sound_device.cpp (Stream::PullOnce): 4608 bytes
constexpr int32_t kRunaway = 1000;     // samples started in one pull before the test cuts the chain
constexpr uint32_t kBankBytes = 0x10000;
constexpr uint32_t kRegistered = kBankBytes - 0x40;   // where the bank's registration leaves the file

// A sample of the bank: its place in the file and its size (the header's mSize). Its bytes are 1 + id * 64 + (i % 61), so
// every sample's bytes are its own and never zero (the silence of the timer codec and of the bound).
struct Sample {
    uint32_t start;
    uint32_t size;
    uint8_t id;
};

uint8_t SampleByte(const Sample& sample, uint32_t i) { return static_cast<uint8_t>(1 + sample.id * 64 + i % 61); }

// The bank's one open file, shared by every codec. A read is checked against the pull buffer it lands in: a count that runs
// past it is recorded (the heap overflow) and only what fits is copied.
class BankFile : public CFile {
public:
    BankFile() : mBytes(kBankBytes, 0xEE) {}
    void Put(const Sample& sample) {
        for (uint32_t i = 0; i < sample.size; ++i) mBytes[sample.start + i] = SampleByte(sample, i);
    }
    int32_t Read(void* dest, uint32_t size) override {
        uint8_t* out = static_cast<uint8_t*>(dest);
        int64_t space = mPullEnd - out;
        if (out < mPullStart || space < 0) space = 0;
        if (static_cast<int64_t>(size) > space) {
            if (!mOverflow) mOverflowCount = size;
            mOverflow = true;
        }
        int64_t count = size;
        if (count > space) count = space;
        const int64_t left = static_cast<int64_t>(mBytes.size()) - mPosition;
        if (count > left) count = left > 0 ? left : 0;
        if (count > 0) std::memcpy(out, mBytes.data() + mPosition, static_cast<std::size_t>(count));
        // The real file moves on by what it read, which for the huge count is to its end.
        const int64_t moved = static_cast<int64_t>(size) < left ? static_cast<int64_t>(size) : (left > 0 ? left : 0);
        mPosition += moved;
        return static_cast<int32_t>(moved);
    }
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        if (whence == 0) mPosition = offset;
        else if (whence == 1) mPosition += static_cast<int32_t>(offset);
        else mPosition = static_cast<int64_t>(mBytes.size()) + static_cast<int32_t>(offset);
        return 0;
    }
    int32_t GetSize() override { return static_cast<int32_t>(mBytes.size()); }
    int32_t GetPos() override { return static_cast<int32_t>(mPosition); }

    std::vector<uint8_t> mBytes;
    int64_t mPosition = kRegistered;
    uint8_t* mPullStart = nullptr;
    uint8_t* mPullEnd = nullptr;
    bool mOverflow = false;
    uint32_t mOverflowCount = 0;
};

// What the test puts behind a runaway chain: it claims the rest of the buffer, writes nothing, and never ends.
class Stopper : public TbSoundSamplePipe {
public:
    int32_t GetDataChunk(void*, int32_t size, int32_t& done, bool) override {
        done = size;
        return 0;
    }
};

// The head of a sample's chain, as TbSoundPipeChannel::GetDataChunk: the pipe behind it fills the buffer and the rest is
// zeroed.
class Channel : public TbSoundSamplePipe {
public:
    int32_t Pull(uint8_t* buffer, int32_t size) {
        int32_t done = 0;
        int32_t result = 0;
        if (mNext != nullptr) result = mNext->GetDataChunk(buffer, size, done, false);
        if (done < 0) done = 0;
        if (done < size) std::memset(buffer + done, 0, static_cast<std::size_t>(size - done));
        return result;
    }
};

uint32_t gNextHandle = 1;

// An endless sentence over its samples: each one that ends starts the next (cycling). SoundCallback finds the callback pipe
// that called it by its handle from the channel and attaches the new sample there.
class Sentence : public TbSndCallback {
public:
    Sentence(Channel* channel, BankFile* file, std::vector<Sample> samples) : mChannel(channel), mFile(file), mSamples(samples) {}

    void Start() { mChannel->Attatch(Make()); }

    void SoundCallback(void* handle, void*) override {
        ++mStartedThisPull;
        int32_t length = 0;
        for (TbSoundSamplePipe* pipe = mChannel->mNext; pipe != nullptr; pipe = pipe->mNext) ++length;
        if (length > mLongestChain) mLongestChain = length;
        TbSoundSamplePipe* caller = mChannel->FindPipe(handle);
        if (caller == nullptr) {
            mLost = true;
            return;
        }
        if (mStartedThisPull >= kRunaway) {
            mRunaway = true;
            caller->Attatch(new Stopper);
            return;
        }
        caller->Attatch(Make());
    }

    // The bytes the sentence should have played so far, `count` of them from the first sample on.
    std::vector<uint8_t> Expected(std::size_t count) const {
        std::vector<uint8_t> out;
        std::size_t index = 0;
        while (out.size() < count) {
            const Sample& sample = mSamples[index % mSamples.size()];
            for (uint32_t i = 0; i < sample.size && out.size() < count; ++i) out.push_back(SampleByte(sample, i));
            ++index;
        }
        return out;
    }

    Channel* mChannel;
    BankFile* mFile;
    std::vector<Sample> mSamples;
    std::vector<TbSampleHeader> mHeaders = std::vector<TbSampleHeader>(16);
    std::size_t mIndex = 0;
    int32_t mStartedThisPull = 0;
    int32_t mLongestChain = 0;
    bool mRunaway = false;
    bool mLost = false;

private:
    // A callback pipe with a new handle and the next sample's codec behind it.
    TbSoundSamplePipe* Make() {
        const Sample& sample = mSamples[mIndex % mSamples.size()];
        TbSampleHeader& header = mHeaders[mIndex % mHeaders.size()];
        ++mIndex;
        std::memset(&header, 0, sizeof header);
        header.mSize = static_cast<int32_t>(sample.size);
        header.mRate = 0x5622;
        header.mBits = 8;
        _TbSoundSampleWAVFile* codec = new _TbSoundSampleWAVFile(mFile, &header, sample.start, nullptr);
        _TbSoundSampleCallback* callback = new _TbSoundSampleCallback(this);
        callback->Attatch(codec);
        callback->SetHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(gNextHandle++)));
        return callback;
    }
};

int gFailures = 0;

void Fail(const char* scenario, const char* what) {
    std::printf("  FAIL %s: %s\n", scenario, what);
    ++gFailures;
}

// One pull of one channel, its buffer checked against the file's reads.
void PullOnce(Sentence& sentence, BankFile& file, std::vector<uint8_t>& played) {
    std::vector<uint8_t> buffer(kPull, 0xA5);
    file.mPullStart = buffer.data();
    file.mPullEnd = buffer.data() + kPull;
    sentence.mStartedThisPull = 0;
    sentence.mChannel->Pull(buffer.data(), kPull);
    played.insert(played.end(), buffer.begin(), buffer.end());
}

void Report(const char* scenario, const Sentence& sentence, const BankFile& file) {
    std::printf("  %s: longest chain %d pipes, overflow %s", scenario, sentence.mLongestChain, file.mOverflow ? "YES" : "no");
    if (file.mOverflow) std::printf(" (a read of %u bytes into the %d-byte pull buffer)", file.mOverflowCount, kPull);
    std::printf(", runaway %s\n", sentence.mRunaway ? "YES" : "no");
    if (file.mOverflow) Fail(scenario, "a read ran past the pull buffer");
    if (sentence.mRunaway) Fail(scenario, "the samples chained without end in one pull");
    if (sentence.mLost) Fail(scenario, "a callback pipe was not found by its handle");
}

// The crash: short PCM samples of an endless sentence, the file left at the bank's end. Each sample's bytes in order.
void EndlessSentence() {
    const char* name = "endless sentence of PCM samples";
    BankFile file;
    const std::vector<Sample> samples = {{0x200, 1000, 0}, {0x800, 1500, 1}, {0x1000, 700, 2}};
    for (const Sample& sample : samples) file.Put(sample);
    Channel channel;
    Sentence sentence(&channel, &file, samples);
    sentence.Start();
    std::vector<uint8_t> played;
    for (int pull = 0; pull < 8; ++pull) PullOnce(sentence, file, played);
    Report(name, sentence, file);
    if (played != sentence.Expected(played.size())) Fail(name, "the bytes played are not the samples' own, in order");
    if (sentence.mLongestChain > 16) Fail(name, "the chain grew past what a pull of short samples needs");
    channel.mNext->Release();
    channel.mNext = nullptr;
}

// Two sentences on the same bank file, pulled in turn: each plays its own samples (each codec reads from its own place).
void TwoSentencesOneFile() {
    const char* name = "two sentences on one bank file";
    BankFile file;
    const std::vector<Sample> first = {{0x200, 3000, 0}, {0x2000, 2500, 1}};
    const std::vector<Sample> second = {{0x4000, 5000, 2}, {0x6000, 1200, 3}};
    for (const Sample& sample : first) file.Put(sample);
    for (const Sample& sample : second) file.Put(sample);
    Channel channelA;
    Channel channelB;
    Sentence a(&channelA, &file, first);
    Sentence b(&channelB, &file, second);
    a.Start();
    b.Start();
    std::vector<uint8_t> playedA;
    std::vector<uint8_t> playedB;
    for (int pull = 0; pull < 6; ++pull) {
        PullOnce(a, file, playedA);
        PullOnce(b, file, playedB);
    }
    Report(name, a, file);
    if (b.mRunaway || b.mLost) Fail(name, "the second sentence ran away or lost its pipe");
    if (playedA != a.Expected(playedA.size())) Fail(name, "the first sentence did not play its own bytes");
    if (playedB != b.Expected(playedB.size())) Fail(name, "the second sentence did not play its own bytes");
    channelA.mNext->Release();
    channelA.mNext = nullptr;
    channelB.mNext->Release();
    channelB.mNext = nullptr;
}

// Samples of no length (each ends without a byte): the chain of one pull is bounded, the pull is silence, and the sentence
// goes on in the next pull.
void SamplesOfNoLength() {
    const char* name = "endless sentence of empty samples";
    BankFile file;
    const std::vector<Sample> samples = {{0x200, 0, 0}, {0x400, 0, 1}};
    Channel channel;
    Sentence sentence(&channel, &file, samples);
    sentence.Start();
    std::vector<uint8_t> played;
    for (int pull = 0; pull < 3; ++pull) PullOnce(sentence, file, played);
    Report(name, sentence, file);
    for (uint8_t byte : played) {
        if (byte != 0) {
            Fail(name, "the pull is not silence");
            break;
        }
    }
    if (sentence.mLongestChain > 80) Fail(name, "the chain of one pull is not bounded");
    if (channel.mNext == nullptr) Fail(name, "the sentence stopped");
    if (channel.mNext != nullptr) channel.mNext->Release();
    channel.mNext = nullptr;
}

}  // namespace

int main() {
    std::printf("sound wavfile: the PCM samples of a streaming bank in an endless sentence\n");
    EndlessSentence();
    TwoSentencesOneFile();
    SamplesOfNoLength();
    if (gFailures != 0) {
        std::fprintf(stderr, "FAILED: %d checks\n", gFailures);
        return 1;
    }
    std::printf("sound wavfile: OK\n");
    return 0;
}
