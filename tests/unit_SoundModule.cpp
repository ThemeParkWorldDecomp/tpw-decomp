// w79a: Mac-word records, virtual calls, and recording external allocation/error/math fakes.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include "TbError.h"
#include "TbSoundInfo.h"
#include "calllog.h"

static bool gRecordArray;
static bool gHoldFreed;
static void* gFreed[8];
static uint32_t gFreedCount;
static bool ObserveRelease(void* block, bool) {
    if (!gHoldFreed || block == nullptr) return false;
    gFreed[gFreedCount++] = block;
    return true;
}
static void ObserveAllocation(size_t size, bool array, void* block) {
    if (array && gRecordArray && calllog::on()) {
        std::memset(block, 0xa5, size);
        calllog::ev("new[]", {static_cast<int64_t>(size), calllog::newlab(block)});
    }
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#define TPW_RELEASE_OBSERVER ObserveRelease
#include "calllog.cpp"

// The external error constructor records both the hidden buffer and its value.
class LoggedError {
public:
    explicit LoggedError(int32_t value) : mValue(value) { calllog::ev("TbError", {3, value}); }
    int32_t mValue;
};
static float RealPow(float a, float b) { return ::powf(a, b); }
float SoundModulePowf(float a, float b) {
    calllog::ev("powf", {calllog::fbits(a), calllog::fbits(b)});
    return RealPow(a, b);
}
#define TbError LoggedError
#define powf SoundModulePowf
#include "../src/TbSoundSystemModule.cpp"
#undef powf
#include "../src/snd_handles.cpp"
#undef TbError

// The four device operations are pure in the base and are never called by these tests.
class TestModule : public TbSoundSystemModule {
public:
    void Close() override {}
    LoggedError StreamData(TbSoundPipeChannel*) override { return LoggedError(77); }
    LoggedError StopChannel(uint32_t) override { return LoggedError(77); }
    LoggedError SetChannelVolume(uint32_t, TbSoundSampleInfo&) override { return LoggedError(77); }
};

static float Bits(uint32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

extern "C" float tpw_sm_pow(float a, float b) { return RealPow(a, b); }

// A volatile function pointer prevents the compiler from resolving virtual calls to TestModule.
static TbSoundSystemModule* PassModule(TbSoundSystemModule* object) { return object; }
static TbSoundSystemModule* (*volatile gPassModule)(TbSoundSystemModule*) = PassModule;

// Calls through a base pointer so every available virtual slot is exercised.
extern "C" void tpw_sm_module(int32_t op, const uint32_t* state, const uint32_t* args,
                             const uint32_t* token, uint32_t* out, double* frequency) {
    alignas(TestModule) unsigned char storage[sizeof(TestModule)];
    std::memset(storage, 0xa5, sizeof storage);
    TestModule* derived = ::new (storage) TestModule;
    TbSoundSystemModule* module = gPassModule(derived);
    module->mBuffers = state[1];
    module->mF008 = state[2];
    module->mRate = state[3];
    std::memcpy(static_cast<void*>(&module->mAutoDelete), &state[4], 1);
    std::memcpy(module->mPadding, reinterpret_cast<const unsigned char*>(&state[4]) + 1, 3);
    // Tokens have native object layouts. Compare every host byte against its saved image.
    alignas(TbSoundSystemInfo) unsigned char system[sizeof(TbSoundSystemInfo)];
    alignas(TbSoundSampleInfo) unsigned char sample[sizeof(TbSoundSampleInfo)];
    const auto* tokenBytes = reinterpret_cast<const unsigned char*>(token);
    for (size_t i = 0; i < sizeof system; ++i) system[i] = tokenBytes[i % 144];
    for (size_t i = 0; i < sizeof sample; ++i) sample[i] = tokenBytes[i % 144];
    auto* info = reinterpret_cast<TbSoundSystemInfo*>(system);
    info->mMaxBuffers = token[5];
    info->mMask = token[6];
    info->mBuffersToUse = token[7];
    unsigned char systemBefore[sizeof system];
    unsigned char sampleBefore[sizeof sample];
    std::memcpy(systemBefore, system, sizeof system);
    std::memcpy(sampleBefore, sample, sizeof sample);
    TbSoundPosition position;
    std::memcpy(&position, token, 12);
    TbSoundPosition top;
    std::memcpy(&top, token + 3, 12);
    TbSoundDistance distance;
    std::memcpy(&distance, token, 12);
    alignas(TbSpeaker::Config*) unsigned char config[16];
    std::memcpy(config, token, 16);
    alignas(TbSoundPipeChannel) unsigned char channel[sizeof(TbSoundPipeChannel)];
    std::memset(channel, 0xa5, sizeof channel);
    auto* pipe = reinterpret_cast<TbSoundPipeChannel*>(channel);
    pipe->mBufferSize = args[0];
    IDirectSound* direct = reinterpret_cast<IDirectSound*>(static_cast<uintptr_t>(token[0]));
    IDirectSoundBuffer* buffer = reinterpret_cast<IDirectSoundBuffer*>(static_cast<uintptr_t>(token[0]));
    uint32_t result = 0;
    switch (op) {
        case -1:
            module = gPassModule(::new (storage) TestModule);
            break;
        case -2:
            result = module->GetBufferSize(pipe);
            break;
        case -3:
            *frequency = TbSoundSystemModule::ConvertPitchIndexToFrequency(static_cast<int32_t>(args[0]));
            break;
        case 0: module->Process(); break;
        case 1: result = static_cast<uint32_t>(module->GetDirectSoundObject(direct)); break;
        case 2: module->GetDirectSoundBuffer(args[0], buffer); break;
        case 3: result = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(module->GetSystemObject(args[0]))); break;
        case 4: result = module->SetOptions(args[0]).mValue; break;
        case 5: result = module->Init(*info).mValue; break;
        case 6: result = module->StopAll().mValue; break;
        case 7: result = module->PauseChannel(args[0]).mValue; break;
        case 8: result = module->RestartChannel(args[0]).mValue; break;
        case 9: result = module->PauseAll().mValue; break;
        case 10: result = module->RestartAll().mValue; break;
        case 11: result = static_cast<uint32_t>(module->IsChannelPlaying(args[0])); break;
        case 12: result = static_cast<uint32_t>(module->SupportsEnvironmentalEffects()); break;
        case 13: result = module->SetLoPass(args[0], Bits(args[1])).mValue; break;
        case 14: result = module->SetDistanceFactor(args[0], distance).mValue; break;
        case 15: result = module->SetSpeakerConfig(*reinterpret_cast<TbSpeaker::Config*>(config)).mValue; break;
        case 16: result = module->Set3DDSPFunctionality(args[0], args[1]).mValue; break;
        case 17: result = module->SetSpeedOfSound(Bits(args[0])).mValue; break;
        case 18: result = module->SetPanRate(args[0]).mValue; break;
        case 19: result = module->SetPosition(args[0], position, static_cast<int32_t>(args[2])).mValue; break;
        case 20: result = module->GetPosition(args[0], position).mValue; break;
        case 21: result = module->SetVelocity(args[0], position).mValue; break;
        case 22: result = module->GetVelocity(args[0], position).mValue; break;
        case 23: result = module->SetListenerVelocity(position).mValue; break;
        case 24: result = module->SetListenerPosition(position).mValue; break;
        case 25: result = module->SetListenerDirection(position, top).mValue; break;
        case 26: result = module->SetPitch(args[0], args[1], static_cast<int32_t>(args[2])).mValue; break;
        case 27: result = module->SetFrequency(args[0], args[1], Bits(args[2])).mValue; break;
        case 28: result = module->SetChannelInfo(args[0], pipe, *reinterpret_cast<TbSoundSampleInfo*>(sample)).mValue; break;
        case 29: result = module->SetReverbEnvironment(args[0], Bits(args[1]), Bits(args[2]), Bits(args[3])).mValue; break;
        case 30: result = module->SetReverbPosition(position).mValue; break;
    }
    out[0] = 0x1003c5c4;
    out[1] = module->mBuffers;
    out[2] = module->mF008;
    out[3] = module->mRate;
    std::memcpy(out + 4, &module->mAutoDelete, 1);
    std::memcpy(reinterpret_cast<unsigned char*>(out + 4) + 1, module->mPadding, 3);
    out[5] = result;
    out[6] = std::memcmp(system, systemBefore, sizeof system) == 0;
    out[6] &= std::memcmp(&position, token, 12) == 0;
    out[6] &= std::memcmp(&top, token + 3, 12) == 0;
    out[6] &= std::memcmp(&distance, token, 12) == 0;
    out[6] &= std::memcmp(config, token, 16) == 0;
    out[6] &= std::memcmp(sample, sampleBefore, sizeof sample) == 0;
    out[7] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(direct));
    out[8] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(buffer));
    derived->~TestModule();
}

// Value operations use the same four words on both sides. Objects are allocated before recording.
extern "C" void tpw_sm_value(int32_t op, const uint32_t* words, const uint32_t* args, uint32_t* out) {
    CUniqueHandle handle;
    handle.mValue = words[0];
    CAtomicValue atomic;
    atomic.mValue = static_cast<int32_t>(words[0]);
    CChannelState state;
    state.mCount.mValue = static_cast<int32_t>(words[0]);
    CChannelState::mChannelState.mCount.mValue = static_cast<int32_t>(words[1]);
    CLowest lowest;
    lowest.mChannel = reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(words[0]));
    lowest.mPriority = words[1];
    CAutoCriticalSection lock;
    lock.mValue = words[0];
    CUniqueHandle::_mBitsPerCount = args[1];
    gHandleMaskInitialized = static_cast<uint8_t>(args[2]);
    gHandleIndexMask = args[3];
    uint32_t result = 0;
    switch (op) {
        case 0: ::new (&handle) CUniqueHandle; break;
        case 1: ::new (&handle) CUniqueHandle(reinterpret_cast<void*>(static_cast<uintptr_t>(args[0]))); break;
        case 2: result = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(static_cast<void*>(handle))); break;
        case 3: result = handle.GetIndex(); break;
        case 4: handle.SetIndex(static_cast<uint16_t>(args[0])); break;
        case 5: result = ++handle; break;
        case 6: ::new (&atomic) CAtomicValue; break;
        case 7: result = ++atomic; break;
        case 8: result = --atomic; break;
        case 9: result = static_cast<int32_t>(atomic); break;
        case 10: ::new (&state) CChannelState; break;
        case 11: result = CChannelState::Instance() == &CChannelState::mChannelState; break;
        case 12: state.AddChannel(); break;
        case 13: state.RemoveChannel(); break;
        case 14: result = state.GetNumberOfChannelsInUse(); break;
        case 15: ::new (&lowest) CLowest; break;
        case 16: lowest(args[0], reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(args[1]))); break;
        case 17: ::new (&lock) CAutoCriticalSection; break;
        case 18: result = static_cast<void**>(lock) == reinterpret_cast<void**>(&lock.mValue); break;
        case 19: case 20: result = &LB_MIN(words[0], words[1]) == &words[0] ? 0 : 1; break;
    }
    out[0] = handle.mValue;
    out[1] = static_cast<uint32_t>(atomic.mValue);
    out[2] = static_cast<uint32_t>(state.mCount.mValue);
    out[3] = static_cast<uint32_t>(CChannelState::mChannelState.mCount.mValue);
    out[4] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lowest.mChannel));
    out[5] = lowest.mPriority;
    out[6] = lock.mValue;
    out[7] = result;
    out[8] = gHandleMaskInitialized;
    out[9] = gHandleIndexMask;
}

// Destructor allocation labels: object 1, counted word 2, shared speaker 4, array allocation 100.
extern "C" void tpw_sm_lifetime(int32_t kind, uint32_t initial, uint32_t flag, int32_t nullObject,
                               int32_t hasPointer, uint32_t count, uint32_t* out) {
    uint32_t word = initial;
    uint32_t signedFlag = static_cast<int16_t>(flag) > 0;
    void* raw = ::operator new(256);
    std::memset(raw, 0xa5, 256);
    void* speaker = hasPointer ? ::operator new(16) : nullptr;
    TbSpeaker::AutoDelete::_mpSpeaker = speaker;
    tpw_cl_label(1, raw);
    tpw_cl_label(2, &word);
    tpw_cl_label(4, speaker);
    gHoldFreed = true;
    gFreedCount = 0;
    if (kind == 0) {
        auto* object = static_cast<CAutoCriticalSection*>(raw);
        object->mValue = initial;
        if (!nullObject) {
            if (signedFlag) delete object;
            else object->~CAutoCriticalSection();
        }
        std::memcpy(out, raw, 4);
    } else if (kind == 1 || kind == 2) {
        auto* object = static_cast<TbSoundCriticalSection*>(raw);
        // Destructor tests start with the supplied word, constructor tests increment it.
        if (kind == 1) {
            ::new (raw) TbSoundCriticalSection(hasPointer ? &word : nullptr);
        } else {
            object->mWord = hasPointer ? &word : nullptr;
            if (!nullObject) {
                if (signedFlag) delete object;
                else object->~TbSoundCriticalSection();
            }
        }
        uint32_t* stored;
        std::memcpy(&stored, raw, sizeof stored);
        out[1] = stored == &word ? 2 : stored == nullptr ? 0 : 0xffffffffu;
        out[0] = word;
    } else if (kind == 3 || kind == 4) {
        if (kind == 3) {
            auto* object = static_cast<TbSpeaker::AutoDelete*>(raw);
            if (!nullObject) {
                if (signedFlag) delete object;
                else object->~AutoDelete();
            }
        } else {
            auto* object = ::new (raw) TestModule;
            if (!nullObject) {
                if (signedFlag) delete object;
                else object->~TestModule();
            }
        }
        out[0] = TbSpeaker::AutoDelete::_mpSpeaker ? 4 : 0;
        if (kind == 3) {
            out[1] = *static_cast<unsigned char*>(raw);
        } else {
            auto* object = static_cast<TestModule*>(raw);
            size_t fields = reinterpret_cast<unsigned char*>(&object->mBuffers) - static_cast<unsigned char*>(raw);
            std::memcpy(out + 1, static_cast<unsigned char*>(raw) + fields, 12);
            std::memcpy(out + 4, &object->mAutoDelete, 1);
            std::memcpy(reinterpret_cast<unsigned char*>(out + 4) + 1, object->mPadding, 3);
        }
    } else {
        auto* object = static_cast<CAutoArrayPtr<unsigned char>*>(raw);
        if (kind == 5) {
            gRecordArray = true;
            ::new (raw) CAutoArrayPtr<unsigned char>(count);
            gRecordArray = false;
            out[0] = calllog::lab(object->mData);
        } else {
            // The data block is supplied by the test, so it has a fixed label.
            tpw_cl_on = 0;
            object->mData = hasPointer ? new unsigned char[count] : nullptr;
            if (object->mData != nullptr) std::memset(object->mData, 0xa5, count);
            tpw_cl_on = 1;
            tpw_cl_label(4, object->mData);
            if (kind == 6) out[0] = calllog::lab(static_cast<unsigned char*>(*object));
            else if (!nullObject) {
                if (signedFlag) delete object;
                else object->~CAutoArrayPtr<unsigned char>();
            }
        }
        unsigned char* stored;
        std::memcpy(&stored, raw, sizeof stored);
        out[1] = stored == nullptr ? 0 : 4;
        out[2] = 1;
        if (stored != nullptr) {
            for (uint32_t i = 0; i < count; ++i) out[2] &= stored[i] == 0xa5;
        }
        if (kind == 5) out[1] = calllog::lab(stored);
    }
    // Cleanup runs outside the compared call log.
    tpw_cl_on = 0;
    if (kind == 5 || kind == 6 || (kind == 7 && nullObject)) {
        delete[] static_cast<CAutoArrayPtr<unsigned char>*>(raw)->mData;
    }
    if (kind == 1 || kind == 5 || kind == 6 || nullObject || !signedFlag) ::operator delete(raw);
    if (kind != 3 && kind != 4) {
        ::operator delete(speaker);
        TbSpeaker::AutoDelete::_mpSpeaker = nullptr;
    } else if (nullObject) {
        ::operator delete(speaker);
        TbSpeaker::AutoDelete::_mpSpeaker = nullptr;
    }
    gHoldFreed = false;
    for (uint32_t i = 0; i < gFreedCount; ++i) std::free(gFreed[i]);
}

extern "C" void tpw_sm_buffer(int32_t set, const uint32_t* words, uint32_t value, uint32_t* out) {
    alignas(TbSoundPipeChannel) unsigned char storage[sizeof(TbSoundPipeChannel)];
    const auto* bytes = reinterpret_cast<const unsigned char*>(words);
    for (size_t i = 0; i < sizeof storage; ++i) storage[i] = bytes[i % 96];
    auto* channel = reinterpret_cast<TbSoundPipeChannel*>(storage);
    channel->mBufferSize = words[9];
    unsigned char expected[sizeof storage];
    std::memcpy(expected, storage, sizeof storage);
    size_t offset = reinterpret_cast<unsigned char*>(&channel->mBufferSize) - storage;
    uint32_t expectedValue = set ? value : words[9];
    std::memcpy(expected + offset, &expectedValue, 4);
    if (set) channel->SetBufferSize(value);
    out[0] = channel->GetBufferSize();
    out[1] = std::memcmp(storage, expected, sizeof storage) == 0;
}

// Sequences retain the same singleton, minimum pair or nested lock objects throughout a case.
extern "C" void tpw_sm_sequence(int32_t kind, uint32_t initial, uint32_t other, int32_t hasPointer,
                                const uint32_t* steps, uint32_t count, uint32_t* out) {
    CChannelState::mChannelState.mCount.mValue = static_cast<int32_t>(initial);
    CLowest lowest;
    lowest.mChannel = reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(initial));
    lowest.mPriority = other;
    uint32_t word = initial;
    alignas(TbSoundCriticalSection) unsigned char storage[32][sizeof(TbSoundCriticalSection)];
    uint32_t depth = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (kind == 0) {
            CChannelState* state = CChannelState::Instance();
            if (steps[i] == 0) state->AddChannel();
            if (steps[i] == 1) state->RemoveChannel();
            out[i * 2] = state->GetNumberOfChannelsInUse();
            out[i * 2 + 1] = state == &CChannelState::mChannelState;
        } else if (kind == 1) {
            lowest(steps[i * 2], reinterpret_cast<TbSoundPipeChannel*>(static_cast<uintptr_t>(steps[i * 2 + 1])));
            out[i * 2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lowest.mChannel));
            out[i * 2 + 1] = lowest.mPriority;
        } else {
            if (steps[i] != 0) {
                ::new (storage[depth++]) TbSoundCriticalSection(hasPointer ? &word : nullptr);
            } else {
                reinterpret_cast<TbSoundCriticalSection*>(storage[--depth])->~TbSoundCriticalSection();
            }
            out[i * 2] = word;
            out[i * 2 + 1] = depth;
        }
    }
}
