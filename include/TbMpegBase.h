// The sound library's MPEG audio decoder object, CMpegBase (the Mac sound library, program "sound symbols shared";
// source file names in its inventory: LbMpeg.cpp, StreamableMpeg2.cpp), unit w71a: the object, its pool, the bit reader,
// the header parser, the frame reader and the layer I decoder. The class is mpglib (the mpg123 decoder as used in LAME's
// mpglib) wrapped in an object; the game's samples are MPEG-2 LSF (22050 Hz) layer I and II streams.
//
// The Mac object is 0x24f0 bytes:
//   +0x0000 the synthesis buffers, float [2 channels][2 banks][0x120] (a channel is 0x900 bytes, a bank 0x480), 0x1200 bytes
//           that OpenSynth clears (0x900 when using_MMX is set) and PolySynth (unit w71b) fills; then 0x40 bytes nobody
//           reads or writes
//   +0x1240 the decoded frame, 0x1240 bytes (Read decodes into it when the caller's request is not frame aligned)
//   +0x2480 the vptr, +0x2484 the pool link, +0x2488 .. +0x24ec the fields below
// The host object keeps the Mac's ORDER of members but not its offsets: the vptr is the compiler's, and the two pointers
// (the pool link and the bit stream) are native. The offsets before the first native pointer are asserted (below) and the
// Mac offset of every member is in its comment. The virtual slots are the Mac's: +0x08 the destructor, +0x0c SeekBits,
// +0x10 DecodeHeader, +0x14 GetHeader (CMpegMacFilePlayerUnpooled overrides the last three).
//
// The pool (the POOLED pattern, like _TbSoundSampleMPEG2File's): operator new takes an object from a list of idle ones
// (linked through mNextPooled) when the size asked is the object's, delete puts it back; TopUp fills the list up to the
// level SetLevel set, ClearCache frees the list. The six counters are the class's statics.
#pragma once
#include <cstddef>
#include <cstdint>

struct al_table;   // the layer II allocation table (unit w71b)

// The global the class tests before it clears the synthesis buffers; CMpegBase_DecodeMethod's constructor sets it to 0,
// and nothing on the Mac sets it otherwise (0x1003e380).
extern int32_t using_MMX;

// Decode_Layer1's scratch (0x1003e588, 0x1003e488, 0x1003e388): file-level on the Mac too; the tests read them.
extern uint32_t gMpegLayer1Alloc[64];
extern int32_t gMpegLayer1Scale[64];
extern int32_t gMpegLayer1Sample[64];

class CMpegBase {
public:
    float mSynthBuf[2][2][0x120];   // +0x0000  channel, bank, sample (0x1200 bytes)
    uint8_t mSynthSpare[0x40];      // +0x1200  not used by the code that was translated
    int16_t mFrame[0x920];          // +0x1240  the buffered frame (the shorts Decode wrote)
    // +0x2480 the vptr
    CMpegBase* mNextPooled;         // +0x2484  the pool's link (a 32-bit address on the Mac)
    int32_t mSampleRate;            // +0x2488  Hz, from the rate table
    int32_t mSampleRate2;           // +0x248c  the same word again (OpenMpegFile reads this one)
    int32_t mFrameBytes;            // +0x2490  decoded bytes of a frame: channels x 0x300 (layer I) or x 0x900
    uint8_t mLayer;                 // +0x2494  4 - the two header bits (1 layer I, 2 layer II, 3 layer III, 4 reserved)
    int32_t mBitRate;               // +0x2498  kbit/s from the table
    int32_t mFrameSize;             // +0x249c  bytes of the frame after the four header bytes
    int32_t mFrameBytes2;           // +0x24a0  the readable size of a frame (OpenLayer copies mFrameBytes)
    int32_t mFrames;                // +0x24a4  frames in the stream (Open: muldiv of the size and the timing words)
    const uint8_t* mData;           // +0x24a8  the stream (a 32-bit address on the Mac; read as big-endian words)
    int32_t mBitPos;                // +0x24ac  bit position in mData
    int32_t mFrameStartBit;         // +0x24b0  bit position of the next frame's header
    int32_t mTotalBytes;            // +0x24b4  mFrames x mFrameBytes
    int32_t mPosition;              // +0x24b8  bytes decoded so far (Read's last line)
    int32_t mSbLimit;               // +0x24bc  layer II (unit w71b)
    uint8_t mOpen;                  // +0x24c0
    uint8_t mBo;                    // +0x24c1  the synthesis buffer offset: 1 after OpenLayer, PolySynth steps it down mod 16
    uint8_t mMpeg25;                // +0x24c2  1 when header bit 20 is 0
    uint8_t mLsf;                   // +0x24c3  1 for MPEG-2 (low sampling frequency)
    uint8_t mSampleRateIndex2;      // +0x24c4  (not written for MPEG 2.5)
    uint8_t mUnused24c5;            // +0x24c5  not used by the code translated
    uint8_t mId;                    // +0x24c6  header bit 19
    uint8_t mNoCrc;                 // +0x24c7  bit 16
    uint8_t mBitRateIndex;          // +0x24c8  bits 12 to 15
    uint8_t mSampleRateIndex;       // +0x24c9  bits 10 and 11, plus 3 for an lsf stream, plus 6 for MPEG 2.5
    uint8_t mPadding;               // +0x24ca  bit 9
    uint8_t mMode;                  // +0x24cb  bits 6 and 7 (3 is mono)
    uint8_t mModeExt;               // +0x24cc  bits 4 and 5
    uint8_t mCopyright;             // +0x24cd  bit 3
    uint8_t mOriginal;              // +0x24ce  bit 2
    uint8_t mUnused24cf;            // +0x24cf  not used by the code translated
    int32_t mChannels;              // +0x24d0  1 for mode 3, else 2
    int32_t mChannels32;            // +0x24d4  mChannels << 5: the shorts a layer I granule writes
    int32_t mT1;                    // +0x24d8  OpenLayer's timing words (1, the quotient and the remainder / 100 of
    int32_t mT2;                    // +0x24dc  bit rate x 144000 over the rate, the rate / 100); Open turns them into
    int32_t mT3;                    // +0x24e0  mFrames with muldiv
    int32_t mT4;                    // +0x24e4
    int32_t mFramesDecoded;         // +0x24e8
    int32_t mFrameConsumed;         // +0x24ec  bytes of mFrame already handed out; Reset sets it to mFrameBytes2

    // The pool's statics: the Mac keeps them in the data section next to the class (0x1003e35c .. 0x10035d8c).
    static CMpegBase* mFirstCMpegBase;    // 0x1003e35c  the list of idle objects
    static int32_t mCurrentCMpegBase;     // 0x1003e350  how many are in it
    static int32_t mLowCMpegBase;         // 0x1003e354  the fewest it ever held after a take
    static int32_t mHighCMpegBase;        // 0x1003e358  the most it ever held
    static int32_t mTotalCMpegBase;       // 0x1003e34c  objects taken from the list
    static int32_t mLevelCMpegBase;       // 0x10035d8c  the level TopUp fills up to (initially 10)

    CMpegBase();                                          // 0x10002604
    virtual ~CMpegBase();                                 // 0x10002640 (the deleting form passes the size 0x24f0)
    static void* operator new(std::size_t size);          // 0x10002398
    static void operator delete(void* object, std::size_t size);   // 0x1000245c
    static void TopUp();                                  // 0x100024f4
    static void SetLevel(int32_t level);                  // 0x100025cc
    static void ClearCache();                             // 0x10002310

    virtual void SeekBits(int32_t byte);                  // 0x10002754  slot +0x0c
    virtual int32_t DecodeHeader();                       // 0x10002a44  slot +0x10
    virtual int32_t GetHeader();                          // 0x10002aec  slot +0x14: 1 when the first word is no frame header
    int32_t GetHeader(uint32_t header);                   // 0x10002b54
    uint32_t GetBits(int32_t count);                      // 0x1000278c
    int32_t ProcessHeader(uint32_t header);               // 0x10002820

    void OpenSynth();                                     // 0x100026d8
    void CloseSynth();                                    // 0x100027f0
    int32_t OpenLayer();                                  // 0x10002bb4
    int32_t CloseLayer();                                 // 0x10002c90
    int32_t Open(void* data, int32_t size);               // 0x10002d9c
    int32_t Open(uint32_t header, int32_t size);          // 0x10002ea4
    int32_t Close();                                      // 0x1000301c
    void Reset();                                         // 0x10002fa8
    int32_t Read(void* out, int32_t bytes);               // 0x1000309c
    int32_t Decode(int16_t* out);                         // 0x10002cc4

    int32_t Decode_Layer1(int16_t* out);                  // 0x100043b0
    void get_sb_stereo(int32_t band, char (*alloc)[2], uint32_t (*scale)[32], int16_t* out, int32_t* mulstab);   // 0x10004270
    static void Init_Tables();                            // 0x10005a58 (the Mac passes a `this` the function ignores)
    static void Close_Tables();                           // 0x10005aa8

    // Unit w71b (declared here, defined there; the tests of this unit fake the first two):
    void PolySynth(int16_t* out, float* fraction);        // 0x10006c38  800 bytes
    int32_t Decode_Layer2(int16_t* out);                  // 0x10005698  912 bytes
    void II_step_two(char* balloc, float (*fraction)[2][32], char* scale, int32_t x1, const al_table* alloc);   // 0x1000483c
    void II_step_twoMMX(int32_t* x1, char* balloc, int16_t (*fraction)[2][32], char* scale, const al_table* alloc);   // 0x10004ee0
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(CMpegBase, mFrame) - offsetof(CMpegBase, mSynthBuf) == 0x1240, "the frame follows the synthesis buffers");
static_assert(offsetof(CMpegBase, mNextPooled) - offsetof(CMpegBase, mFrame) == 0x1240, "the pool link follows the frame");
static_assert(sizeof(CMpegBase::mSynthBuf) == 0x1200, "two channels of two 0x480 byte banks");
static_assert(sizeof(void*) != 4 || sizeof(CMpegBase) == 0x24f0, "the Mac object is 0x24f0 bytes");
static_assert(offsetof(CMpegBase, mFrameConsumed) - offsetof(CMpegBase, mChannels) == 0x24ec - 0x24d0, "the timing words");
static_assert(offsetof(CMpegBase, mOriginal) - offsetof(CMpegBase, mOpen) == 0x24ce - 0x24c0, "the header bytes");
#pragma clang diagnostic pop

// mpglib's integer allocation table of layer II (0, -1, -3, -7, ...: 1 - 2^n), the words at 0x10039440 that get_sb_stereo
// reads; and the multiplier table muls[27][64] of layers I and II (0x10035df0; row 0 is zero, the game's code addresses it
// from 0x10035ef0 = row 1) that Decode_Layer1 indexes with the allocation + 1 and the scale factor index. The table is
// computed (mpglib's formula, 2^(j/3) times the row's multiplier) and the unit test compares every word with the dump.
int32_t MpegIntegerAllocation(int32_t index);
float MpegMultiplier(int32_t row, int32_t scale);

// The static cleanup object the Mac registers for the class (0x10004110): its destructor clears the pool at exit.
class _CS_CMpegBase {
public:
    ~_CS_CMpegBase();   // 0x10004110
};

// The decoder-method switch object (0x1000422c, 0x10004190): its constructor clears using_MMX; the destructor closes the
// MMX tables when it was set.
class CMpegBase_DecodeMethod {
public:
    CMpegBase_DecodeMethod();    // 0x1000422c
    ~CMpegBase_DecodeMethod();   // 0x10004190
};

// mullw then divw (muldiv__Fiii, 0x100022e4): a * b / c with the product wrapped to 32 bits; the dividend where the
// processor leaves the quotient undefined (a zero divisor), as the emulator answers.
int32_t muldiv(int32_t a, int32_t b, int32_t c);

// ---------------------------------------------------------------------------------------------------------------------
// Unit w71b: the second half of the decoder (src/sound/MpegDecode.cpp): PolySynth, dct64, WRITE_SAMPLE, Decode_Layer2 and the two
// forms of II_step_two, with the static tables they use.

// mpglib's allocation table entry (4 bytes on the Mac, two big-endian shorts): the first entry of a subband is {the bits of
// its allocation code, 0}, the entries after it (1 << bits of them in all) are {the bits of a sample code, the step d}; d < 0
// is an ungrouped sample (the code plus d), d > 0 (3, 5 or 9) a group of three samples read through MpegGroupTable(d).
struct al_table {
    int16_t bits;
    int16_t d;
};

// dct64 (0x10005adc, 4,408 bytes): mpglib's 32 point DCT. samples are 32 floats; the results go to out0[0x00, 0x10, ... 0x100]
// and out1[0x00, ... 0xf0] (16 floats apart). Its scratch is a file-level array of 64 floats (0x1003eac8), gMpegDctBuf.
void dct64(float* out0, float* out1, float* samples);

// WRITE_SAMPLE (0x10006f88, 72 bytes): the sample truncated to an int (fctiwz) and clipped to the int16 range.
void WRITE_SAMPLE(int16_t* out, float sample);

// The file-level scratch of Decode_Layer2 on the Mac (the tests read them): fraction [3 sub-granules][2 channels][32 bands]
// (0x1003e688), the scale factors 3 a coded band (0x1003e988), the bit allocations (0x1003ea48), the scale factor selectors
// (0x1003ea88), and dct64's work array (0x1003eac8).
extern float gMpegL2Fraction[3][2][32];
extern uint8_t gMpegL2Scale[0xc0];
extern int8_t gMpegL2Alloc[0x40];
extern int8_t gMpegL2Select[0x40];
extern float gMpegDctBuf[64];

// The static tables (the test compares every word with the Mac's data):
const float* MpegDecwin();                       // decwin[544], the synthesis window (0x1003b488)
float MpegDct64Cosine(int32_t index);            // the 31 constants 0x10033204 down to 0x1003318c: pnts[0][0..15], pnts[1][0..7],
                                                 // pnts[2][0..3], pnts[3][0..1], pnts[4][0] (1 / (2 cos((2k + 1) pi / n)))
const al_table* MpegAllocTable(int32_t index);   // alloc_0 .. alloc_4 (0x1003a2c8 ...), as PTR_DAT_1003b208 lists them
int32_t MpegAllocTableEntries(int32_t index);    // how many al_table entries each has
const uint8_t* MpegGroupTable(int32_t d);        // grp_3tab, grp_5tab, grp_9tab (0x100394c0 ...): three multiplier rows a code
const uint8_t* MpegLayer2Translate();            // [sample rate index 3][2 - channels][bit rate index 16] -> table index (0x1003b21c)
int32_t MpegLayer2SbLimit(int32_t index);        // sblims[5] = 27, 30, 8, 12, 30 (0x1003b27c)

// Unit v5274: CFormat (LbMpeg.cpp), the wave format record (WAVEFORMATEX-like) the player fills. The members up to +0x18 have
// the Mac's offsets on the host too; the stream info pointer at +0x18 is native here (4 bytes on the Mac).
struct CFormatInfo {
    uint8_t mPad[4];     // +0x00
    uint16_t mRate;      // +0x04  sample rate
    uint8_t mBits;       // +0x06  bits per sample
    uint8_t mFlags;      // +0x07  bit 0: stereo
};

class CFormat {
public:
    uint8_t mChannelsByte;       // +0x00  used when there is no stream info
    uint8_t mBitsByte;           // +0x01
    uint16_t mRateWord;          // +0x02
    uint16_t mFilled;            // +0x04  1 once FillWaveFormatEx has run (wFormatTag)
    uint16_t mChannels;          // +0x06
    uint32_t mSamplesPerSec;     // +0x08
    uint32_t mAvgBytesPerSec;    // +0x0c
    uint16_t mBlockAlign;        // +0x10
    uint16_t mBitsPerSample;     // +0x12
    uint16_t mExtraSize;         // +0x14
    const CFormatInfo* mInfo;    // +0x18  (native pointer)

    void FillWaveFormatEx();     // 0x100021b0
};

// Unit v51124 (sound, LbMpeg.cpp / StreamableMpeg2.cpp): small functions, free-function spellings.
struct TbError;
template<class T> class CAutoArrayPtr;
struct FileAccessRead {                       // the access object's vtable slot 0xc
    virtual int32_t Read(void* dest, uint32_t size) = 0;
};
void* VoidArrayNew(size_t n);
void VoidArrayDelete(void* p);
void VoidObjectDelete(void* p);
uint8_t* CFormat_operator_tWAVEFORMATEX(CFormat* self);                          // 0x10002280
int32_t CFile_Read(FileAccessRead* impl, void* dest, uint32_t size);            // 0x10001f38 (impl = this+4)
int32_t* TbError_ctor(int32_t* self, int32_t value);                              // 0x100015b8
int32_t TbError_operator_long(const int32_t* self);                              // 0x10000b88
void*** CAutoArrayPtrVoid_dtor(void*** self, int16_t freeFlag);                  // 0x10007340
void** CAutoArrayPtrVoid_operator_ptr(void*** self);                             // 0x100073d4
void*** CAutoArrayPtrVoid_ctor(void*** self, uint32_t count);                    // 0x100074a8
