// TbSoundCodecs.h: the sound library's four small codecs (the Mac's "sound symbols shared" library, Code at 0x10000000, TOC base
// r2 = 0x1003abf0), unit w80. CCodecFactory (a later unit) puts them behind a TbSoundPipeChannel: _TbSoundSampleWAV plays a
// sample held in memory (a TbFileBank's mapped file), _TbSoundSampleWAVFile reads it through an open CFile (a TbMMFileBank's
// streamed samples), _TbSoundCodecLoop restarts the codec behind it every time that one completes, and TbTimerCodec plays
// silence for a while (timed events). The classes derive from unit w78's (include/TbSoundPipe.h) and keep its conventions: the
// hidden TbError buffer, the Mac destructor flag as `delete this`, and the POOLED allocation of every object.
//
// The Mac layouts (offsets are the Mac's; the host's pointers are native, so the host objects are larger):
//   _TbSoundSampleWAV : TbSoundSampleCodec, 0x30 bytes: +0x2c the pool link.
//   _TbSoundSampleWAVFile : _TbSoundSampleWAV, 0x3c bytes: +0x30 the pool link, +0x34 mFile, +0x38 mStart.
//   _TbSoundCodecLoop : TbSoundSPCodec, 0x18 bytes: +0x14 the pool link.
//   TbTimerCodec : TbSoundSampleCodec, 0x30 bytes: +0x2c the pool link.
// Every class has its own pool (the CMpegBase pattern, see TbSoundSamplePipe): operator new takes an idle object from the list
// when the size asked is the class's own, operator delete returns one, TopUp fills the list up to mLevel, ClearCache frees it.
// The Mac keeps the five counters and the level of each pool as statics (mTotal, mCurrent, mLow, mHigh, mFirst in that address
// order, then mLevel in the initialised data). The pool link is the LAST word of the object; a base class's pool link is not
// shared with the derived class's (the WAVFile has its own at +0x30).
//
// The vtables (the vptr points at the RTTI pair, the slots start at +8; the order is the declaration order of TbSoundPipe.h):
//   _TbSoundSampleWAV 0x1003c3f8: the sample codec's, with the destructor 0x10011338, +0x24 GetDataChunk 0x10011154, +0x28
//       Restart 0x100113cc and +0x38 SetDataStart 0x1001140c overridden.
//   _TbSoundSampleWAVFile 0x1003c4e4: the same with the destructor 0x10011b4c, GetDataChunk 0x10011934 and Restart 0x10011ad8
//       (SetDataStart is the WAV codec's).
//   _TbSoundCodecLoop 0x1003c04c: the SP codec's (+0x5c does not exist) with the destructor 0x1000b060 and GetDataChunk
//       0x1000aee0 (Restart +0x28 is the SP codec's: it forwards to the codec behind).
//   TbTimerCodec 0x1003c758: the sample codec's with the destructor 0x100138dc and GetDataChunk 0x10013858 (Restart +0x28 is
//       the SP codec's, so a timer has no Restart of its own: it would call through the NULL codec behind it).
// The static cleanup objects _CS__TbSoundSampleWAV 0x100114ac, _CS__TbSoundSampleWAVFile 0x10011c48, _CS__TbSoundCodecLoop
// 0x1000b590 and _CS_TbTimerCodec 0x100139c8 (destructors that call ClearCache at program exit) are NOT translated, as w78
// left _CS_TbSoundSamplePipe 0x1000f9dc.
#pragma once
#include <cstddef>
#include <cstdint>

#include "TbSoundPipe.h"
#include "bfl_FileSystem.h"

// CMemoryCopy::Copy 0x10011294 (52 bytes): a member function of an empty class (the caller passes a stack temporary as this).
// It does not use this: it calls slot +8 of the global copier object (the pointer at 0x1003f34c, a CStandardCopy made by
// CMemoryCopyInit 0x10014b78) and CStandardCopy::Copy 0x10014cac is memcpy(dest, source, count). Translated here as the memcpy.
class CMemoryCopy {
public:
    // Unit w79b: the copier classes CMemoryCopyInit (0x10014b78) makes. The abstract copier: vtable 0x1003c8e0, slot +0x08 is
    // zero (pure).
    class CCopy {
    public:
        CCopy(); // 0x10014c70 (12): stores the vtable
        virtual void Copy(void* dest, const void* source, uint32_t count) = 0; // +0x08
    };
    // The standard copier: vtable 0x1003c900, slot +0x08 is Copy 0x10014cac (48 bytes), memcpy(dest, source, count).
    class CStandardCopy : public CCopy {
    public:
        CStandardCopy(); // 0x10014bfc (56): CCopy(), then its own vtable
        void Copy(void* dest, const void* source, uint32_t count) override; // 0x10014cac
    };

    void Copy(void* dest, const void* source, uint32_t count);

    static CCopy* _mpCopy; // 0x1003f34c: the global copier CMemoryCopyInit makes (defined in src/sound/ChannelState.cpp, src/sound/TbSoundCore.cpp)
};

class _TbSoundSampleWAV : public TbSoundSampleCodec {
public:
    // The pool: statics 0x1003f2b8 (total), 0x1003f2bc (current), 0x1003f2c0 (low), 0x1003f2c4 (high), 0x1003f2c8 (first),
    // level 0x1003c398 (initially 10).
    static _TbSoundSampleWAV* mFirst_TbSoundSampleWAV;
    static int32_t mCurrent_TbSoundSampleWAV;
    static int32_t mLow_TbSoundSampleWAV;
    static int32_t mHigh_TbSoundSampleWAV;
    static int32_t mTotal_TbSoundSampleWAV;
    static int32_t mLevel_TbSoundSampleWAV;

    _TbSoundSampleWAV(TbSampleHeader* header, TbRefRelease* owner);   // 0x10000d90 (60): the sample codec's constructor, then the vptr
    ~_TbSoundSampleWAV() override;                                    // +0x08, 0x10011338 (100): deleting form frees 0x30 bytes
    // Copies from the data at mDataStart + mDataPosition; see the source for the end of the data and the timed path.
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) override;   // +0x24, 0x10011154 (260)
    void Restart() override;                                          // +0x28, 0x100113cc (12): position 0
    void SetDataStart(void* start) override;                          // +0x38, 0x1001140c (8)

    static void* operator new(std::size_t size);                      // 0x10010ec4 (156)
    static void operator delete(void* object, std::size_t size);      // 0x10010f90 (112)
    static void TopUp();                                              // 0x10011034 (176)
    static void SetLevel(int32_t level);                              // 0x10011114 (12)
    static void ClearCache();                                         // 0x10010e34 (92)

    _TbSoundSampleWAV* mWAVPoolLink;   // +0x2c the pool's link while the object is idle
};

class _TbSoundSampleWAVFile : public _TbSoundSampleWAV {
public:
    // The pool: statics 0x1003f2dc (total), 0x1003f2e0 (current), 0x1003f2e4 (low), 0x1003f2e8 (high), 0x1003f2ec (first),
    // level 0x1003c458 (initially 10).
    static _TbSoundSampleWAVFile* mFirst_TbSoundSampleWAVFile;
    static int32_t mCurrent_TbSoundSampleWAVFile;
    static int32_t mLow_TbSoundSampleWAVFile;
    static int32_t mHigh_TbSoundSampleWAVFile;
    static int32_t mTotal_TbSoundSampleWAVFile;
    static int32_t mLevel_TbSoundSampleWAVFile;

    // `start` is the file offset of the sample's data (TbMMFileBank passes the sample's offset + 0x28).
    _TbSoundSampleWAVFile(bfl::FileSystem::CFile* file, TbSampleHeader* header, uint32_t start, TbRefRelease* owner);   // 0x10011868 (96)
    ~_TbSoundSampleWAVFile() override;                                // +0x08, 0x10011b4c (100): deleting form frees 0x3c bytes
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) override;   // +0x24, 0x10011934 (292)
    void Restart() override;                                          // +0x28, 0x10011ad8 (60): mFile->Seek(mStart, 0)

    static void* operator new(std::size_t size);                      // 0x100115c8 (156)
    static void operator delete(void* object, std::size_t size);      // 0x10011698 (112)
    static void TopUp();                                              // 0x10011740 (176)
    static void SetLevel(int32_t level);                              // 0x10011824 (12)
    static void ClearCache();                                         // 0x10011534 (92)

    _TbSoundSampleWAVFile* mWAVFilePoolLink;   // +0x30 the pool's link while the object is idle
    bfl::FileSystem::CFile* mFile;             // +0x34 the open file (not owned)
    uint32_t mStart;                           // +0x38 the file offset of the sample data
};

class _TbSoundCodecLoop : public TbSoundSPCodec {
public:
    // The pool: statics 0x1003ec04 (total), 0x1003ec08 (current), 0x1003ec0c (low), 0x1003ec10 (high), 0x1003ec14 (first),
    // level 0x1003c010 (initially 10).
    static _TbSoundCodecLoop* mFirst_TbSoundCodecLoop;
    static int32_t mCurrent_TbSoundCodecLoop;
    static int32_t mLow_TbSoundCodecLoop;
    static int32_t mHigh_TbSoundCodecLoop;
    static int32_t mTotal_TbSoundCodecLoop;
    static int32_t mLevel_TbSoundCodecLoop;

    _TbSoundCodecLoop();                                              // 0x10000548 (56): the SP codec's constructor, then the vptr
    ~_TbSoundCodecLoop() override;                                    // +0x08, 0x1000b060 (100): deleting form frees 0x18 bytes
    // Fills the buffer from the codec behind it (mNext, not tested for NULL), restarting that codec whenever it answers 1.
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) override;   // +0x24, 0x1000aee0 (216)

    static void* operator new(std::size_t size);                      // 0x1000ac50 (156)
    static void operator delete(void* object, std::size_t size);      // 0x1000ad1c (112)
    static void TopUp();                                              // 0x1000adc0 (176)
    static void SetLevel(int32_t level);                              // 0x1000aea0 (12)
    static void ClearCache();                                         // 0x1000abc0 (92)

    _TbSoundCodecLoop* mLoopPoolLink;   // +0x14 the pool's link while the object is idle
};

class TbTimerCodec : public TbSoundSampleCodec {
public:
    // The pool: statics 0x1003f300 (total), 0x1003f304 (current), 0x1003f308 (low), 0x1003f30c (high), 0x1003f310 (first),
    // level 0x1003c6f8 (initially 10).
    static TbTimerCodec* mFirstTbTimerCodec;
    static int32_t mCurrentTbTimerCodec;
    static int32_t mLowTbTimerCodec;
    static int32_t mHighTbTimerCodec;
    static int32_t mTotalTbTimerCodec;
    static int32_t mLevelTbTimerCodec;

    // Plays silence for `milliseconds`: the format byte, then SetTime(false, milliseconds + 0x1200) through the virtual slot.
    TbTimerCodec(uint8_t format, uint32_t milliseconds);              // 0x10013700 (120)
    ~TbTimerCodec() override;                                         // +0x08, 0x100138dc (100): deleting form frees 0x30 bytes
    int32_t GetDataChunk(void* buffer, int32_t size, int32_t& done, bool stopped) override;   // +0x24, 0x10013858 (76)

    static void* operator new(std::size_t size);                      // 0x10013484 (156)
    static void operator delete(void* object, std::size_t size);      // 0x1001354c (112)
    static void TopUp();                                              // 0x100135e8 (176)
    static void SetLevel(int32_t level);                              // 0x100136c4 (12)
    static void ClearCache();                                         // 0x100133f8 (92)

    TbTimerCodec* mTimerPoolLink;   // +0x2c the pool's link while the object is idle
};

// ---- Unit v51075 ---------------------------------------------------------------------------------------------------------
// The static cleanup objects: their destructor (the Mac's hidden delete flag in r4, answers this; ClearCache runs when this is
// not NULL, a flag above 0 frees the object through 0x1002ea94) is Destruct here, and __sinit_*_cpp hands it to the runtime's
// __register_global_object (0x1002eefc) with the object and its node (two words, the Mac addresses in the comments).
class _CS_TbTimerCodec {
public:
    static _CS_TbTimerCodec* Destruct(_CS_TbTimerCodec* self, int16_t deleteFlag);   // 0x100139c8 (84)
};
class _CS__TbSoundSampleWAV {
public:
    static _CS__TbSoundSampleWAV* Destruct(_CS__TbSoundSampleWAV* self, int16_t deleteFlag);   // 0x100114ac (84)
};
class _CS__TbSoundSampleWAVFile {
public:
    static _CS__TbSoundSampleWAVFile* Destruct(_CS__TbSoundSampleWAVFile* self, int16_t deleteFlag);   // 0x10011c48 (84)
};
// 0x10013f2c is NOT translated: a stand-in that only answers self (its address is what __sinit_UserCallback_cpp registers).
class _CS_TbSoundUserCallback;   // include/TbSoundSampleCallback.h
_CS_TbSoundUserCallback* DestructUserCallbackCleanup(_CS_TbSoundUserCallback* self, int16_t deleteFlag);
extern uint32_t gCS_TbSoundUserCallbackObject;   // 0x1003f338
extern uint32_t gCS_TbSoundUserCallbackNode;     // 0x1003f33c
extern uint32_t gCS_TbTimerCodecObject;          // 0x1003f314
extern uint32_t gCS_TbTimerCodecNode;            // 0x1003f318
extern uint32_t gCS_TbSoundSampleWAVFileObject;  // 0x1003f2f0
extern uint32_t gCS_TbSoundSampleWAVFileNode;    // 0x1003f2f4
void __sinit_UserCallback_cpp();           // 0x10013ed0 (48)
void __sinit_TimerCodec_cpp();             // 0x1001396c (48)
void __sinit_TbSoundSampleWAVFile_cpp();   // 0x10011be4 (48)

// The runtime's pow (glue 0x1002ee54, an import: two doubles in f1 and f2); the unit tests fake it.
double MslPow(double a, double b);
// std::powf 0x10012580 (40), std::fabs(float) 0x10017bf8 (32), std::fabsf 0x10017c38 (36), std::fabs(double) 0x10017c7c (8): in tbstd
// here, as <cmath> declares the real ones.
namespace tbstd {
float powf(float a, float b);
float fabs(float x);
float fabsf(float x);
double fabs(double x);
}  // namespace tbstd

// ---- Unit v51113: two more cleanup objects (Destruct as above: the hidden delete flag in r4, answers this; ClearCache when this is not
// NULL, a flag above 0 frees through 0x1002ea94) and the float square roots.
class _CS_TbSoundSamplePipe {
public:
    static _CS_TbSoundSamplePipe* Destruct(_CS_TbSoundSamplePipe* self, int16_t deleteFlag);   // 0x1000f9dc (84)
};
class _CS__TbSoundCodecLoop {
public:
    static _CS__TbSoundCodecLoop* Destruct(_CS__TbSoundCodecLoop* self, int16_t deleteFlag);   // 0x1000b590 (84)
};
namespace tbstd {
float sqrtf(float x);        // std::sqrtf 0x1000c70c (40): frsp of the runtime's sqrt (glue 0x1002ecec) of the float widened
float sqrtOfFloat(float x);  // std::sqrt(float) 0x1000c6cc (32): sqrtf
}  // namespace tbstd
