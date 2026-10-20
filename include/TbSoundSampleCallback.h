// TbSoundSampleCallback.h: the sample callback pipes of the sound library (the Mac's "sound symbols shared" library, Code at
// 0x10000000, TOC base r2 = 0x1003abf0), unit w79i. When a placeholder plays a sample, the codec factory (include/CCodecFactory.h)
// wraps the codec in a _TbSoundSampleCallback (the sample info carries the sound callback, flag 0x800) and a
// _TbSoundSampleCompletedCallback (flag 0x2000, set for every placeholder); both are pipes of the chain (include/TbSoundPipe.h)
// that carry the caller's callback and user data in the sample info.
//
// The Mac layouts (offsets are the Mac's; the host's pointers are native, so the host objects are larger):
//   TbSoundUserCallback : TbSoundSamplePipe, 0x1c bytes: +0x14 the pool link of THIS class while the object is idle, +0x18 mUserData.
//   _TbSoundSampleCallback and _TbSoundSampleCompletedCallback : TbSoundUserCallback, 0x24 bytes each: +0x1c the pool link of the
//       class itself while idle, +0x20 mCallback (the TbSndCallback of the info, +0x44 for the sample callback; the completed
//       callback is stored but nothing in the library reads it: see the note at the Completed class).
// Both derive from TbSoundSamplePipe through TbSoundUserCallback (their constructors call the pipe's), not from TbSoundSPCodec.
//
// The vtables (the vptr points at the RTTI pair, the slots start at +8; the slot order is include/TbSoundPipe.h's up to +0x38):
//   TbSoundUserCallback 0x1003c7d8: +0x08 destructor 0x100001b0, +0x1c GetInfo 0x10013e4c, +0x2c SetInfo 0x10013d78, the rest the
//       pipe's (Attatch 0x1000f440, Release 0x1000f588, Detatch 0x1000f834, FindPipe 0x1000f7b0, GetSampleRate 0x1000118c,
//       GetDataChunk 0x10001148, Restart 0x10001204, SetHandle 0x10000bb8, Completed 0x1000f628, SetDataStart 0x1000123c).
//   _TbSoundSampleCallback 0x1003c1a8: the pipe's with +0x08 destructor 0x1000d50c, +0x1c GetInfo 0x1000d078, +0x24 GetDataChunk
//       0x1000d11c and +0x2c SetInfo 0x1000cfa0.
//   _TbSoundSampleCompletedCallback 0x10035bbc: the pipe's with +0x08 destructor 0x10001018, +0x1c GetInfo 0x10013e4c (the user
//       callback's), +0x24 GetDataChunk 0x100010bc and +0x2c SetInfo 0x10013d78 (the user callback's).
// The Mac destructors take a hidden short flag in r4 (deleted through the class's operator delete with the Mac size when positive);
// the host mirrors that with the virtual destructor and the class's operator delete, as include/TbSoundPipe.h describes.
// Every function that returns a TbError does so through the hidden r3 buffer on the Mac (this moves to r4, the info to r5).
//
// Each class is pooled like the pipe (TbSoundSamplePipe's pool): operator new answers a block of the class's size from the list of
// idle objects when there is one, and the cleanup objects _CS_... (static, destructors at exit) clear the pools.
#pragma once
#include <cstddef>
#include <cstdint>

#include "TbError.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"

class TbSndCallback;

// ---- TbSoundUserCallback ------------------------------------------------------------------------------------------------------
// A pipe that keeps the user data of the sample's info (SetInfo with flag 0x1000, GetInfo answers it for a request with 0x1000).
class TbSoundUserCallback : public TbSoundSamplePipe {
public:
    // The pool: statics 0x1003f324 (total), 0x1003f328 (current), 0x1003f32c (low), 0x1003f330 (high), 0x1003f334 (first), level
    // 0x1003c7b8 (initially 10).
    static TbSoundUserCallback* mFirstTbSoundUserCallback;
    static int32_t mCurrentTbSoundUserCallback;
    static int32_t mLowTbSoundUserCallback;
    static int32_t mHighTbSoundUserCallback;
    static int32_t mTotalTbSoundUserCallback;
    static int32_t mLevelTbSoundUserCallback;

    TbSoundUserCallback();                                         // 0x10000d20 (64): the pipe's constructor, the vptr, mUserData = 0
    explicit TbSoundUserCallback(void* userData);                  // 0x100002c8 (72): the same with the user data
    ~TbSoundUserCallback() override;                               // +0x08, 0x100001b0 (104): deleting form frees 0x1c bytes
    // The pipe's SetInfo (down the chain), then mUserData = the info's user data when its flags have 0x1000. A negative answer from
    // the chain is -1, otherwise 0.
    TbError SetInfo(const TbSoundSampleInfo& info) override;       // +0x2c, 0x10013d78 (140)
    // When the info's REQUEST word (+0x18, mField18 in include/TbSoundInfo.h) has 0x1000: info.SetUserData(mUserData). The chain is
    // NOT asked (no call of the pipe's GetInfo).
    void GetInfo(TbSoundSampleInfo& info) override;                // +0x1c, 0x10013e4c (60)

    static void* operator new(std::size_t size);                   // 0x10013ae0 (156)
    static void operator delete(void* object, std::size_t size);   // 0x10013bb0 (112)
    static void TopUp();                                           // 0x10013c54 (176)
    static void SetLevel(int32_t level);                           // 0x10013d38 (12)
    static void ClearCache();                                      // 0x10013a4c (92)

    TbSoundUserCallback* mUserCallbackPoolLink;   // +0x14 the pool's link while the object is idle
    void* mUserData;                              // +0x18
};

// ---- _TbSoundSampleCallback ---------------------------------------------------------------------------------------------------
// Plays the codec behind it to its end and then asks the callback for the next sample (the placeholder chooses the next sound and
// attaches a new codec behind this pipe); when the callback attaches none, a timer codec of the same format keeps the channel
// busy.
class _TbSoundSampleCallback : public TbSoundUserCallback {
public:
    // The pool: statics 0x1003ec28 (total), 0x1003ec2c (current), 0x1003ec30 (low), 0x1003ec34 (high), 0x1003ec38 (first), level
    // 0x1003c16c (initially 10).
    static _TbSoundSampleCallback* mFirst_TbSoundSampleCallback;
    static int32_t mCurrent_TbSoundSampleCallback;
    static int32_t mLow_TbSoundSampleCallback;
    static int32_t mHigh_TbSoundSampleCallback;
    static int32_t mTotal_TbSoundSampleCallback;
    static int32_t mLevel_TbSoundSampleCallback;

    explicit _TbSoundSampleCallback(TbSndCallback* callback);      // 0x10000c94 (72): the user callback's no-argument constructor
    ~_TbSoundSampleCallback() override;                            // +0x08, 0x1000d50c (100): deleting form frees 0x24 bytes
    // The user callback's SetInfo (NOT the pipe's), then mCallback = the info's callback when its flags have 0x800.
    TbError SetInfo(const TbSoundSampleInfo& info) override;       // +0x2c, 0x1000cfa0 (140)
    // The user callback's GetInfo, then info.SetCallback(mCallback) when the request word has 0x800.
    void GetInfo(TbSoundSampleInfo& info) override;                // +0x1c, 0x1000d078 (88)
    // Fills the buffer from the pipe behind it. With nothing behind: 1 and `done` untouched. Else done = 0 and, until done reaches
    // the size (signed), the next pipe is asked for the rest (virtual +0x24, the flag always true); when it answers exactly 1 (its
    // sample is over) the next sample is chosen at the position (bytes delivered so far) and the guard detaches THIS pipe from
    // the chain at the end (CAutoDetach: it is deleted, the codec behind it stays attached to the previous pipe). Answers 0.
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) override;   // +0x24, 0x1000d11c (272)
    // Asks the callback for the next sample (SoundCallback(mHandle, mUserData), slot +8) with mNext cleared, releases the old
    // codec, and when the callback attached none starts a timer codec (the old codec's format byte, `position` milliseconds).
    void GetNextSample(uint32_t position);                         // 0x1000d380 (204)

    static void* operator new(std::size_t size);                   // 0x1000cd00 (156)
    static void operator delete(void* object, std::size_t size);   // 0x1000cdd0 (112)
    static void TopUp();                                           // 0x1000ce78 (176)
    static void SetLevel(int32_t level);                           // 0x1000cf5c (12)
    static void ClearCache();                                      // 0x1000cc68 (92)

    _TbSoundSampleCallback* mSampleCallbackPoolLink;   // +0x1c the pool's link while the object is idle
    TbSndCallback* mCallback;                          // +0x20
};

// ---- _TbSoundSampleCompletedCallback ------------------------------------------------------------------------------------------
// Forwards the data down the chain and keeps the completion callback of the info (the sample info's mCompletedCallback) and the
// user data. NOTHING in the library reads mCallback of this class (no function of the unit or of the pipe base calls it): the
// completion callback the placeholder receives is not called through this pipe.
class _TbSoundSampleCompletedCallback : public TbSoundUserCallback {
public:
    // The pool: statics 0x1003f270 (total), 0x1003f274 (current), 0x1003f278 (low), 0x1003f27c (high), 0x1003f280 (first), level
    // 0x1003c350 (initially 10).
    static _TbSoundSampleCompletedCallback* mFirst_TbSoundSampleCompletedCallback;
    static int32_t mCurrent_TbSoundSampleCompletedCallback;
    static int32_t mLow_TbSoundSampleCompletedCallback;
    static int32_t mHigh_TbSoundSampleCompletedCallback;
    static int32_t mTotal_TbSoundSampleCompletedCallback;
    static int32_t mLevel_TbSoundSampleCompletedCallback;

    _TbSoundSampleCompletedCallback(TbSndCallback* callback, void* userData);   // 0x10000114 (76): the user callback's (userData) constructor
    ~_TbSoundSampleCompletedCallback() override;                   // +0x08, 0x10001018 (104): deleting form frees 0x24 bytes
    // With nothing behind: 0 (`done` untouched). Else the answer of the pipe behind it for the same four arguments.
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool flag) override;   // +0x24, 0x100010bc (64)

    static void* operator new(std::size_t size);                   // 0x1000f178 (156)
    static void operator delete(void* object, std::size_t size);   // 0x1000f254 (112)
    static void TopUp();                                           // 0x1000f304 (176)
    static void SetLevel(int32_t level);                           // 0x1000f3f4 (12)
    static void ClearCache();                                      // 0x1000f0d8 (92)

    _TbSoundSampleCompletedCallback* mCompletedCallbackPoolLink;   // +0x1c the pool's link while the object is idle
    TbSndCallback* mCallback;                                      // +0x20
};

// ---- CAutoDetach --------------------------------------------------------------------------------------------------------------
// The guard of _TbSoundSampleCallback::GetDataChunk (0xc bytes on the Mac: +0 the pipe, +4 the flag byte).
class CAutoDetach {
public:
    explicit CAutoDetach(TbSoundSamplePipe* pipe);   // 0x1000d334 (16): the pipe, the flag cleared
    CAutoDetach& operator=(bool detach);             // 0x1000d26c (8): the flag
    ~CAutoDetach();                                  // 0x1000d29c (112): with the flag set, pipe->Detatch() (virtual +0x14)

    TbSoundSamplePipe* mPipe;   // +0
    bool mDetach;               // +4
};

// ---- the cleanup objects --------------------------------------------------------------------------------------------------------
// Static objects whose destructors clear the pools at program exit (src/TbSoundSampleCallback.cpp keeps one of each).
class _CS_TbSoundUserCallback {
public:
    ~_CS_TbSoundUserCallback();                      // 0x10013f2c (84)
};

class _CS__TbSoundSampleCallback {
public:
    ~_CS__TbSoundSampleCallback();                   // 0x1000d608 (84)
};

class _CS__TbSoundSampleCompletedCallback {
public:
    ~_CS__TbSoundSampleCompletedCallback();          // 0x1000f948 (84)
};
