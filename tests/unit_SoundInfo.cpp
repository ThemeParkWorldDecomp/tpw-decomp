// Test entry points for unit w77, the sound library's value classes (libtpw_unit_SoundInfo.dylib, driven by
// tests/test_SoundInfo.py in the program "sound symbols shared"):
//   src/TbSoundInfo.cpp   TbSoundPosition, TbSoundDistance, TbSoundReverbInfo, TbSoundVolumeControl, TbSoundSampleInfo,
//                         TbSoundSystemInfo (TbSoundSampleInfo::SetBank is w72's and is tested in test_SoundBank.py)
// The unit has no callees outside itself on the host, so there are no fakes: the test builds the records on both sides over
// a recognisable fill, runs one function and compares every record byte for byte.
//
// The host's records hold their pointers as native pointers, so the sample info, the system info and the volume control are
// loaded from and dumped to the Mac's layout (an array of 32-bit words in the Mac's order). A pointer member is mapped to
// the "label" of the token object it points at (LABEL_BASE + index in the token array the test passes); any other pointer
// member is the low 32 bits of the address, so an arbitrary word survives a round trip. The records whose host layout is the
// Mac's (position, distance, reverb info) are read and written by the test as plain bytes.
#include <cstdint>
#include <cstring>
#include <new>

#include "TbSoundInfo.h"

namespace {

constexpr uint32_t kLabelBase = 0xCAFE0000u;

// The record kinds the test loads and dumps.
enum Kind { kVolume = 0, kSample = 1, kSystem = 2 };

void* WordToPointer(uint32_t word, void* const* tokens, int count) {
    if (word >= kLabelBase && word < kLabelBase + static_cast<uint32_t>(count)) return tokens[word - kLabelBase];
    return reinterpret_cast<void*>(static_cast<uintptr_t>(word));
}

uint32_t PointerToWord(const void* pointer, void* const* tokens, int count) {
    for (int i = 0; i < count; ++i) {
        if (pointer == tokens[i]) return kLabelBase + static_cast<uint32_t>(i);
    }
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

float Bits(uint32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

uint32_t WordOf(float value) {
    uint32_t word;
    std::memcpy(&word, &value, sizeof word);
    return word;
}

}  // namespace

// The operations, in the order of OPS in tests/test_SoundInfo.py (the test checks the names).
enum Op {
    kPosCtor, kPosCtorInt, kPosSetF, kPosSetI, kPosSetX, kPosSetY, kPosSetZ, kPosGet, kPosLength, kPosDot, kPosValid, kPosNe,
    kPosMul, kPosDiv, kPosSub, kPosAdd, kPosCopy, kPosAssign,
    kDistCtor, kDistCtor3, kRevCtor, kVolCtor, kVolGet, kVolSetVolumes, kVolInit,
    kSiCtor, kSiSetNumber, kSiSetUserId, kSiSetPan, kSiSetVolume, kSiSetPitch, kSiSetFrequency, kSiSetPriority, kSiSetPosition,
    kSiSetVelocity, kSiSetDistance, kSiSetLoopCount, kSiSetCallback, kSiSetCompleted, kSiSetUserData, kSiSetReverb,
    kSiSetVolumeControl, kSiSetStreaming,
    kSysCtor, kSysCtorConfig, kSysCopy, kSysSetHWnd, kSysSetGuid, kSysSetClassId, kSysSetPosition, kSysSetDirection,
    kSysSetVelocity, kSysSetSpeaker, kSysSetBuffers, kSysSetMax, kSysSetHalfRate, kSysSetDevice, kSysGetBuffers,
    kSysGetSpeaker, kSysGetVelocity, kSysGetTop, kSysGetFront, kSysGetPosition, kSysGetMask, kSysGetMax, kSysGetPlaying,
    kOpCount
};

extern "C" {

int32_t tpw_si_op_count() { return kOpCount; }

// The sizes of the host records (the test allocates generously; this lets it check).
int32_t tpw_si_size(int32_t kind) {
    switch (kind) {
        case kVolume: return static_cast<int32_t>(sizeof(TbSoundVolumeControl));
        case kSample: return static_cast<int32_t>(sizeof(TbSoundSampleInfo));
        default: return static_cast<int32_t>(sizeof(TbSoundSystemInfo));
    }
}

// Loads a record from the Mac's words (36 for the sample info, 19 for the system info, 2 for the volume control).
void tpw_si_load(int32_t kind, void* object, const uint32_t* w, void* const* tokens, int32_t count) {
    if (kind == kVolume) {
        TbSoundVolumeControl* v = static_cast<TbSoundVolumeControl*>(object);
        v->mVolumes = static_cast<float*>(WordToPointer(w[0], tokens, count));
        v->mCount = w[1];
    } else if (kind == kSample) {
        TbSoundSampleInfo* s = static_cast<TbSoundSampleInfo*>(object);
        s->mNumber = w[0];
        s->mBank = static_cast<Bank::CBank*>(WordToPointer(w[1], tokens, count));
        s->mUserId = w[2];
        s->mField0C = w[3];
        s->mField10 = w[4];
        s->mFlags = w[5];
        s->mField18 = w[6];
        s->mVolume = w[7];
        s->mFrequency = Bits(w[8]);
        s->mPitch = static_cast<int32_t>(w[9]);
        s->mPriority = w[10];
        s->mPosition = static_cast<const TbSoundPosition*>(WordToPointer(w[11], tokens, count));
        s->mVelocity = static_cast<const TbSoundPosition*>(WordToPointer(w[12], tokens, count));
        s->mDistance.mA = Bits(w[13]);
        s->mDistance.mB = Bits(w[14]);
        s->mDistance.mC = Bits(w[15]);
        s->mLoopCount = w[16];
        s->mCallback = static_cast<TbSndCallback*>(WordToPointer(w[17], tokens, count));
        s->mCompletedCallback = static_cast<TbSndCallback*>(WordToPointer(w[18], tokens, count));
        s->mUserData = WordToPointer(w[19], tokens, count);
        s->mReverb.mField0 = w[20];
        s->mReverb.mField4 = w[21];
        s->mReverb.mField8 = w[22];
        s->mReverb.mFieldC = w[23];
        s->mReverb.mField10 = Bits(w[24]);
        s->mReverb.mField14 = w[25];
        s->mReverb.mField18 = Bits(w[26]);
        s->mReverb.mField1C = w[27];
        s->mReverb.mField20 = Bits(w[28]);
        s->mReverb.mField24 = Bits(w[29]);
        s->mReverb.mField28 = w[30];
        s->mReverb.mField2C = Bits(w[31]);
        s->mVolumeControl = static_cast<const TbSoundVolumeControl*>(WordToPointer(w[32], tokens, count));
        s->mVolumeControlIndex = w[33];
        s->mStreamingBufferSize = w[34];
        s->mPan = w[35];
    } else {
        TbSoundSystemInfo* s = static_cast<TbSoundSystemInfo*>(object);
        s->mField0 = w[0];
        s->mField4 = w[1];
        s->mField8 = w[2];
        s->mFieldC = w[3];
        s->mField10 = w[4];
        s->mMaxBuffers = w[5];
        s->mMask = w[6];
        s->mBuffersToUse = w[7];
        s->mHWnd = WordToPointer(w[8], tokens, count);
        s->mGuid = static_cast<NS_MacDoze::_GUID*>(WordToPointer(w[9], tokens, count));
        s->mClassId = static_cast<NS_MacDoze::_GUID*>(WordToPointer(w[10], tokens, count));
        s->mListenerPosition = static_cast<const TbSoundPosition*>(WordToPointer(w[11], tokens, count));
        s->mFrontDir = static_cast<const TbSoundPosition*>(WordToPointer(w[12], tokens, count));
        s->mTopDir = static_cast<const TbSoundPosition*>(WordToPointer(w[13], tokens, count));
        s->mListenerVelocity = static_cast<const TbSoundPosition*>(WordToPointer(w[14], tokens, count));
        s->mSpeakerConfig = static_cast<const TbSpeaker::Config*>(WordToPointer(w[15], tokens, count));
        s->mField40 = w[16];
        s->mSamplesPlaying = w[17];
        s->mHardwareDevice = WordToPointer(w[18], tokens, count);
    }
}

// Dumps a record to the Mac's words.
void tpw_si_dump(int32_t kind, const void* object, uint32_t* w, void* const* tokens, int32_t count) {
    if (kind == kVolume) {
        const TbSoundVolumeControl* v = static_cast<const TbSoundVolumeControl*>(object);
        w[0] = PointerToWord(v->mVolumes, tokens, count);
        w[1] = v->mCount;
    } else if (kind == kSample) {
        const TbSoundSampleInfo* s = static_cast<const TbSoundSampleInfo*>(object);
        w[0] = s->mNumber;
        w[1] = PointerToWord(s->mBank, tokens, count);
        w[2] = s->mUserId;
        w[3] = s->mField0C;
        w[4] = s->mField10;
        w[5] = s->mFlags;
        w[6] = s->mField18;
        w[7] = s->mVolume;
        w[8] = WordOf(s->mFrequency);
        w[9] = static_cast<uint32_t>(s->mPitch);
        w[10] = s->mPriority;
        w[11] = PointerToWord(s->mPosition, tokens, count);
        w[12] = PointerToWord(s->mVelocity, tokens, count);
        w[13] = WordOf(s->mDistance.mA);
        w[14] = WordOf(s->mDistance.mB);
        w[15] = WordOf(s->mDistance.mC);
        w[16] = s->mLoopCount;
        w[17] = PointerToWord(s->mCallback, tokens, count);
        w[18] = PointerToWord(s->mCompletedCallback, tokens, count);
        w[19] = PointerToWord(s->mUserData, tokens, count);
        w[20] = s->mReverb.mField0;
        w[21] = s->mReverb.mField4;
        w[22] = s->mReverb.mField8;
        w[23] = s->mReverb.mFieldC;
        w[24] = WordOf(s->mReverb.mField10);
        w[25] = s->mReverb.mField14;
        w[26] = WordOf(s->mReverb.mField18);
        w[27] = s->mReverb.mField1C;
        w[28] = WordOf(s->mReverb.mField20);
        w[29] = WordOf(s->mReverb.mField24);
        w[30] = s->mReverb.mField28;
        w[31] = WordOf(s->mReverb.mField2C);
        w[32] = PointerToWord(s->mVolumeControl, tokens, count);
        w[33] = s->mVolumeControlIndex;
        w[34] = s->mStreamingBufferSize;
        w[35] = s->mPan;
    } else {
        const TbSoundSystemInfo* s = static_cast<const TbSoundSystemInfo*>(object);
        w[0] = s->mField0;
        w[1] = s->mField4;
        w[2] = s->mField8;
        w[3] = s->mFieldC;
        w[4] = s->mField10;
        w[5] = s->mMaxBuffers;
        w[6] = s->mMask;
        w[7] = s->mBuffersToUse;
        w[8] = PointerToWord(s->mHWnd, tokens, count);
        w[9] = PointerToWord(s->mGuid, tokens, count);
        w[10] = PointerToWord(s->mClassId, tokens, count);
        w[11] = PointerToWord(s->mListenerPosition, tokens, count);
        w[12] = PointerToWord(s->mFrontDir, tokens, count);
        w[13] = PointerToWord(s->mTopDir, tokens, count);
        w[14] = PointerToWord(s->mListenerVelocity, tokens, count);
        w[15] = PointerToWord(s->mSpeakerConfig, tokens, count);
        w[16] = s->mField40;
        w[17] = s->mSamplesPlaying;
        w[18] = PointerToWord(s->mHardwareDevice, tokens, count);
    }
}

// Runs one operation. self: the record the function works on (constructors build in place, over whatever the test loaded);
// a0, a1: the pointer arguments (other records, the result record of operator+ and operator-, the Get outputs); w0..w2: the
// word arguments (floats as bit patterns, ints, opaque pointers as words). The answer: an integer (booleans, pointers as
// 0 or 1 where the test compares an identity) and, for the floating results, dret.
int64_t tpw_si_call(int32_t op, void* self, void* a0, void* a1, uint32_t w0, uint32_t w1, uint32_t w2, double* dret) {
    TbSoundPosition* pos = static_cast<TbSoundPosition*>(self);
    TbSoundSampleInfo* si = static_cast<TbSoundSampleInfo*>(self);
    TbSoundSystemInfo* sys = static_cast<TbSoundSystemInfo*>(self);
    TbSoundVolumeControl* vol = static_cast<TbSoundVolumeControl*>(self);
    const TbSoundPosition* otherPos = static_cast<const TbSoundPosition*>(a0);
    void* const opaque0 = reinterpret_cast<void*>(static_cast<uintptr_t>(w0));
    *dret = 0.0;
    switch (op) {
        case kPosCtor: new (self) TbSoundPosition(); return 0;
        case kPosCtorInt: new (self) TbSoundPosition(static_cast<int32_t>(w0), static_cast<int32_t>(w1), static_cast<int32_t>(w2)); return 0;
        case kPosSetF: pos->Set(Bits(w0), Bits(w1), Bits(w2)); return 0;
        case kPosSetI: pos->Set(static_cast<int32_t>(w0), static_cast<int32_t>(w1), static_cast<int32_t>(w2)); return 0;
        case kPosSetX: pos->SetX(Bits(w0)); return 0;
        case kPosSetY: pos->SetY(Bits(w0)); return 0;
        case kPosSetZ: pos->SetZ(Bits(w0)); return 0;
        case kPosGet: {
            float* out = static_cast<float*>(a0);
            pos->Get(out[0], out[1], out[2]);
            return 0;
        }
        case kPosLength: *dret = static_cast<double>(pos->length()); return 0;
        case kPosDot: *dret = pos->DotProduct(*otherPos); return 0;
        case kPosValid: return pos->Valid() ? 1 : 0;
        case kPosNe: return (*pos != *otherPos) ? 1 : 0;
        case kPosMul: return &(*pos *= Bits(w0)) == pos ? 1 : 0;
        case kPosDiv: return &(*pos /= Bits(w0)) == pos ? 1 : 0;
        case kPosSub: {
            // a0: the other operand, a1: the result record
            const TbSoundPosition result = pos->operator-(*otherPos);
            std::memcpy(a1, &result, sizeof result);
            return 0;
        }
        case kPosAdd: {
            const TbSoundPosition result = pos->operator+(*otherPos);
            std::memcpy(a1, &result, sizeof result);
            return 0;
        }
        case kPosCopy: new (self) TbSoundPosition(*otherPos); return 0;
        case kPosAssign: return &(*pos = *otherPos) == pos ? 1 : 0;
        case kDistCtor: new (self) TbSoundDistance(); return 0;
        case kDistCtor3: new (self) TbSoundDistance(Bits(w0), Bits(w1), Bits(w2)); return 0;
        case kRevCtor: new (self) TbSoundReverbInfo(); return 0;
        case kVolCtor: new (self) TbSoundVolumeControl(); return 0;
        case kVolGet: *dret = vol->GetVolume(w0); return 0;
        case kVolSetVolumes: vol->SetVolumes(static_cast<float*>(a0)); return 0;
        case kVolInit: vol->Init(w0); return 0;
        case kSiCtor: new (self) TbSoundSampleInfo(); return 0;
        case kSiSetNumber: si->SetNumber(w0); return 0;
        case kSiSetUserId: si->SetUserId(w0); return 0;
        case kSiSetPan: si->SetPan(w0); return 0;
        case kSiSetVolume: si->SetVolume(w0); return 0;
        case kSiSetPitch: si->SetPitch(static_cast<int32_t>(w0)); return 0;
        case kSiSetFrequency: si->SetFrequency(Bits(w0)); return 0;
        case kSiSetPriority: si->SetPriority(w0); return 0;
        case kSiSetPosition: si->SetPosition(*otherPos); return 0;
        case kSiSetVelocity: si->SetVelocity(*otherPos); return 0;
        case kSiSetDistance: si->SetDistance(*static_cast<const TbSoundDistance*>(a0)); return 0;
        case kSiSetLoopCount: si->SetLoopCount(w0); return 0;
        case kSiSetCallback: si->SetCallback(static_cast<TbSndCallback*>(opaque0)); return 0;
        case kSiSetCompleted: si->SetCompletedCallback(static_cast<TbSndCallback*>(opaque0)); return 0;
        case kSiSetUserData: si->SetUserData(opaque0); return 0;
        case kSiSetReverb: si->SetReverb(*static_cast<const TbSoundReverbInfo*>(a0)); return 0;
        case kSiSetVolumeControl: si->SetVolumeControl(*static_cast<const TbSoundVolumeControl*>(a0), w0); return 0;
        case kSiSetStreaming: si->SetStreamingBufferSize(w0); return 0;
        case kSysCtor: new (self) TbSoundSystemInfo(); return 0;
        case kSysCtorConfig: new (self) TbSoundSystemInfo(*static_cast<const TbSpeaker::Config*>(a0)); return 0;
        case kSysCopy: new (self) TbSoundSystemInfo(*static_cast<const TbSoundSystemInfo*>(a0)); return 0;
        case kSysSetHWnd: sys->SetHWnd(opaque0); return 0;
        case kSysSetGuid: sys->SetGuid(static_cast<NS_MacDoze::_GUID*>(opaque0)); return 0;
        case kSysSetClassId: sys->SetClassId(static_cast<NS_MacDoze::_GUID*>(opaque0)); return 0;
        case kSysSetPosition: sys->SetListenerPosition(*otherPos); return 0;
        case kSysSetDirection: sys->SetListenerDirection(*otherPos, *static_cast<const TbSoundPosition*>(a1)); return 0;
        case kSysSetVelocity: sys->SetListenerVelocity(*otherPos); return 0;
        case kSysSetSpeaker: sys->SetSpeakerConfig(*static_cast<const TbSpeaker::Config*>(a0)); return 0;
        case kSysSetBuffers: sys->SetBuffersToUse(w0); return 0;
        case kSysSetMax: sys->SetMaxBuffers(w0); return 0;
        case kSysSetHalfRate: sys->SetHalfRate(w0); return 0;
        case kSysSetDevice: sys->SetHardwareDevice(opaque0); return 0;
        case kSysGetBuffers: return sys->GetBuffersToUse();
        case kSysGetSpeaker: return static_cast<int64_t>(reinterpret_cast<intptr_t>(sys->GetSpeakerConfig()));
        case kSysGetVelocity: return static_cast<int64_t>(reinterpret_cast<intptr_t>(sys->GetListenerVelocity()));
        case kSysGetTop: return static_cast<int64_t>(reinterpret_cast<intptr_t>(sys->GetTopDir()));
        case kSysGetFront: return static_cast<int64_t>(reinterpret_cast<intptr_t>(sys->GetFrontDir()));
        case kSysGetPosition: return static_cast<int64_t>(reinterpret_cast<intptr_t>(sys->GetListenerPosition()));
        case kSysGetMask: return sys->GetMask() == &sys->mMask ? 1 : 0;
        case kSysGetMax: return sys->GetMaxBuffers();
        case kSysGetPlaying: return sys->GetSamplesPlaying();
        default: return -1;
    }
}

}  // extern "C"
