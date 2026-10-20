// Wavelet.h: engine library, original Wavelet.cpp.
//
// PARTIAL: only the callbacks so far. The game installs C-library style file functions
// with SetFileCallbacks; every engine loader (Bitmap::load, Landscape, STGAHeader,
// CSpriteFileIO, CWaveLoad, ...) reads through them. SetEngineCallbacks installs the
// engine's allocator and four more hooks. The variables are not exported; their names
// here are invented from the parameter types and their users.
#pragma once
#include <cstdint>

using TbFileOpenFn = void* (*)(const char* name, const char* mode);
using TbFileCloseFn = uint32_t (*)(void* file);
using TbFileReadFn = uint32_t (*)(void* dest, uint32_t size, uint32_t count, void* file);
using TbFileSeekFn = uint32_t (*)(void* file, uint32_t offset, uint32_t whence);
using TbFileTellFn = int32_t (*)(void* file);
using TbFileNameFn = char* (*)(const char* name, int32_t flags);

extern TbFileOpenFn gFileOpen;     // engine data 0x101735e4
extern TbFileCloseFn gFileClose;   // 0x101735e0
extern TbFileReadFn gFileRead;     // 0x101735d8
extern TbFileSeekFn gFileSeek;     // 0x101735dc
extern TbFileTellFn gFileTell;     // 0x101735d4
extern TbFileNameFn gFileName;     // 0x101735d0

void SetFileCallbacks(TbFileOpenFn open, TbFileCloseFn close, TbFileReadFn read, TbFileSeekFn seek,
                      TbFileTellFn tell, TbFileNameFn name);

// SetEngineCallbacks' hooks; the comments name what the game installs (main app).
using TbIsFirstPersonFn = int32_t (*)();
using TbUseSmallTexturesFn = bool (*)();
using TbReadClockFn = uint32_t (*)();
using TbUpdateMouseFn = void (*)();
using TbMallocFn = void* (*)(uint32_t size);
using TbFreeFn = void (*)(void*& block);   // frees and clears the caller's pointer

extern TbIsFirstPersonFn gIsFirstPerson;        // engine data 0x101735cc  MacIsFirstPerson
extern TbUseSmallTexturesFn gUseSmallTextures;  // 0x101735c4  UseSmallTextures
extern TbReadClockFn gReadClock;                // 0x101735c8  MacReadClock (milliseconds)
extern TbUpdateMouseFn gUpdateMouse;            // 0x101735c0  MacUpdateMouse
extern TbMallocFn gMalloc;                      // 0x10051f70  MacMalloc; DefaultMalloc until set
extern TbFreeFn gFree;                          // 0x10051f74  MacFree; DefaultFree until set

// The allocator defaults: they only complain (DebugStr); DefaultMalloc returns NULL.
void* DefaultMalloc(uint32_t size);
void DefaultFree(void*& block);

void SetEngineCallbacks(TbIsFirstPersonFn isFirstPerson, TbUseSmallTexturesFn useSmallTextures,
                        TbReadClockFn readClock, TbUpdateMouseFn updateMouse, TbMallocFn malloc,
                        TbFreeFn free);

// Small helpers the inventory files in this range (0x10038a78..0x10038bf8).
// The number of set bits in a mask.
int32_t BitsSet(uint32_t mask);
// The position of a mask's lowest set bit (0 for an empty mask).
int32_t ShiftFactor(uint32_t mask);
// Fills gSineTable: sin of 256 steps round the circle.
void CreateSineTable();
extern float gSineTable[256];   // engine data 0x101731c0 (not exported; name invented)

// The wavelet decompressor: an interface (CWavelet) and its one implementation
// (CWaveletImp, 0x18 bytes on the Mac: +0 the pointer to the CWavelet at +0x10, +4 the
// vtable). The engine keeps one instance; Instance() makes it on first use and counts a
// reference each time. The version 0x13 decoder and the two dispatching overloads are in
// src/Wavelet_decode.cpp, the version 0x12 decoder in src/Wavelet_decode_v12.cpp, and the
// fast decoders and their transforms in src/Wavelet_fast.cpp (0x10039894..0x1003b0d4).
class CWavelet {
public:
    virtual int32_t Decompress(uint32_t width, uint32_t height, bool colour, const uint8_t* source, uint8_t* dest,
                               float a, float b, float c) = 0;
    virtual int32_t Decompress(int32_t a, int32_t b, int32_t c, int32_t d, void* p, void* q, void* r, float e,
                               float f, float g, float h) = 0;
    virtual int32_t Decompress(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, void* p,
                               void* q, void* r, float h, float i, float j, float k) = 0;
    virtual int32_t AddRef() = 0;
    virtual int32_t GetRefCount() = 0;
    virtual int32_t Release() = 0;
    virtual ~CWavelet();                 // 0x10039460

    // The engine's decompressor, with one more reference (0x1003ed0c).
    static CWavelet* Instance();
};

class CWaveletImp : public virtual CWavelet {
public:
    enum : uint32_t { kBufferSize = 0x62a39 };

    CWaveletImp();                       // inline in CWavelet::Instance
    int32_t Decompress(uint32_t width, uint32_t height, bool colour, const uint8_t* source, uint8_t* dest, float a,
                       float b, float c) override;                                          // 0x10039694
    int32_t Decompress(int32_t a, int32_t b, int32_t c, int32_t d, void* p, void* q, void* r, float e, float f,
                       float g, float h) override;                                          // 0x10039718
    int32_t Decompress(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f, int32_t g, void* p, void* q,
                       void* r, float h, float i, float j, float k) override;               // 0x100397dc
    int32_t AddRef() override;           // 0x10039598
    int32_t GetRefCount() override;      // 0x100395d8
    int32_t Release() override;          // 0x10039610: destroys the object with its last reference
    ~CWaveletImp() override;             // 0x100394d0
    virtual int32_t Decompress_v12(int32_t a, int32_t b, int32_t c, void* p, void* q, void* r, float e, float f,
                                   float g, float h);                                       // 0x1003b0d4
    virtual int32_t Decompress_v13(int32_t a, int32_t b, int32_t c, void* p, void* q, void* r, float e, float f,
                                   float g, float h);                                       // 0x1003d230

    int32_t mReferences;                 // 0x08  1 after construction
    uint8_t* mBuffer;                    // 0x0c  kBufferSize bytes (new[])

    // The vertical inverse D4 transform of one column (0x1003d054): `count` values `stride`
    // floats apart, the smooth half first, into `count` consecutive floats. A member that does
    // not use `this`.
    void ReverseTransformVert(const float* source, float* dest, int32_t count, int32_t stride);

private:
    // The fast path's decoders, called by Decompress 0x10039694.
    void DecompressColorFast(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest, float a, float b,
                             float c);                                                      // 0x10039894
    void DecompressMonoFast(uint32_t width, uint32_t height, const uint8_t* source, uint8_t* dest,
                            float a);                                                       // 0x1003a67c
    void ReverseChannelFast(float* source, float* dest, uint32_t stride, uint32_t count);      // 0x1003ab50
    void ConvertAndBlend(uint32_t width, uint32_t height, uint32_t stride, uint8_t* dest);     // 0x1003af4c
};

// ---- The version 0x13 decoder's statics (src/Wavelet_decode.cpp; none has a symbol, the
// names are invented) ---------------------------------------------------------------------
// Five planes of 256 x 256 floats, contiguous in engine data from 0x10173614 in this order;
// the fast decoders share them through the same TOC entries (0x1004fb24, 0x1004fb3c,
// 0x1004fb40, 0x1004fb44, 0x1004fb38). They are separate statics, not the instance's mBuffer.
// The LZSS output goes to gWaveletWorkspace (TOC 0x1004fb48 points at that pointer).
enum : uint32_t { kWaveletPlaneFloats = 0x10000 };
enum : int32_t {
    kWaveletAlpha = 0,   // 0x10173614  the alpha block's image
    kWaveletCr = 1,      // 0x101b3614  the red difference, half size
    kWaveletCb = 2,      // 0x101f3614  the blue difference, half size
    kWaveletLuma = 3,    // 0x10233614  the brightness
    kWaveletRows = 4,    // 0x10273614  each horizontal pass's output, read by the vertical pass
};
extern float gWaveletPlanes[5][kWaveletPlaneFloats];
// The Daubechies D4 synthesis coefficients, set by __sinit_Wavelet_cpp (0x1003f1f0) in double
// precision and rounded once: (1 + sqrt 3) / (4 sqrt 2), (3 + sqrt 3) / ..., (3 - sqrt 3) / ...,
// (1 - sqrt 3) / ...
extern float gWaveletH0;                 // engine data 0x10173610
extern float gWaveletH1;                 // 0x1017360c
extern float gWaveletH2;                 // 0x10173608
extern float gWaveletH3;                 // 0x10173604

// The LZSS unpacker (0x10039210): bit codes read least significant bit first from 16-bit
// little-endian words. A 0 flag is followed by an 8-bit literal; a 1 flag by a 12-bit offset
// back into the output and a 7-bit length minus one; offset 0 ends the stream. It trusts its
// input: no bound on either side.
void LzssDecompress(void* source, void* dest);
// The next `bits` bits of the stream (0x10039360); read_reset (0x10039424) empties the bit
// buffer.
uint32_t read_code(uint32_t bits);
void read_reset();
extern uint32_t gLzssBitCount;           // engine data 0x101735ec  bits left in gLzssBitBuffer
extern uint32_t gLzssBitBuffer;          // 0x101735f0
extern uint32_t gLzssUnused;             // 0x101735f4  LzssDecompress clears it; nothing reads it
extern uint32_t gLzssSourcePos;          // 0x101735f8  the next byte of the stream to read
extern uint8_t* gLzssDest;               // 0x101735fc  the output's start
extern const uint8_t* gLzssSource;       // 0x10173600

// The version 0x12 decoder's unpacker, from the bullfrog library ("bullfrog symbols shared"
// 0x100257d8, imported by the engine; untranslated): LbZLib_MemDecompress(source, dest,
// workspace) hands the work to bfl::compress::CZLib::MemDecompress (0x10024efc), which swaps
// the source's 0x1c-byte header in place on the Mac, checks its "BILZ" tag and inflates the
// zlib stream after it into dest (+4 of the header: the unpacked size; +0xc: zlib's window
// bits), with `workspace` as zlib's memory (a block of its own from new[] when NULL).
// Returns the unpacked size, or 0 (nothing written) for a block without the tag or a failed
// inflate.
uint32_t LbZLib_MemDecompress(void* source, void* dest, void* workspace);

extern CWavelet* gWaveletInstance;       // engine data 0x101735e8 (not exported; names invented)
// Where the decompressor works: the free space of the render memory's dynamic pool at the
// time the first instance was made (0x102b3614).
extern uint8_t* gWaveletWorkspace;

// tb_decode_wavelet.cpp (its own assertions name it; the inventory's tb_depthoffield.cpp
// range): the engine's reference to the decompressor. InitWaveLoad also points the
// procedural textures' scratch buffer (gProcTextureBuffer, tb_proctexture.h) 0x400 bytes
// into the dynamic pool's free space.
int32_t InitWaveLoad();     // 0x10005c84
int32_t CloseWaveLoad();    // 0x10005d68
extern CWavelet* gWaveLoadWavelet;       // engine data 0x10052b04 (not exported; name invented)
