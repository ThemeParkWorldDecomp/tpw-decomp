// The picture loaders of the Bullfrog library (bullfrog symbols shared): TbRenderArea (TbRaConvert.cpp range
// 0x10016560..0x10016ad0), TbGraphicFileLoader (0x10022870..0x100230cc), its TGA subclass TbTGALoader
// (0x10023a80..0x10023f30) and the readers under them: _TgaGLL (0x10021228..0x10021d28) and _AndyGLL, the line
// reader they share (0x1001de44, 0x1001e2f4). InterfaceBFCursor::LoadData (src/ui_cursors.cpp) reads a cursor's
// TGA through them. Mac offsets are in the comments; the host classes have their own layout.
//
// Host design. The Mac loaders have virtual members and TbGraphicFileLoader is abstract (ReadHeaderInfo, ReadData
// and ValidImage, slots 0xc, 0x10 and 0x14); the game makes only the TGA subclass, so the host classes have no
// vtable and the base's members call TbTGALoader's directly (static_cast), as the fakes in tests/unit_UIInit.cpp
// do. The reader classes (_AndyGLL, _TgaGLL) are only ever made on the stack by TbTGALoader and keep their
// virtual members, which DoRead calls.
#pragma once
#include <cstdint>

#include "TbPixFmt.h"

namespace bfl {
namespace FileSystem {
class CFile;
}  // namespace FileSystem
}  // namespace bfl

struct TbPalette;

// What the loader calls give back by value: 0 or more for success, negative for failure (the library's TbError).
struct TbResult {
    int32_t mCode;   // 0x00
};

struct TbPoint {
    int32_t x;   // 0x00
    int32_t y;   // 0x04
};
struct TbSize {
    int32_t width;    // 0x00
    int32_t height;   // 0x04
};
// Left and top inside, right and bottom outside.
struct TbRect {
    int32_t left;     // 0x00
    int32_t top;      // 0x04
    int32_t right;    // 0x08
    int32_t bottom;   // 0x0c
    // bullfrog 0x1002a110 (LbDraw.cpp, an import of ltms; 148 bytes, src/bfl_tbrect.cpp, unit w58): the smallest rectangle holding
    // both (the lower of the lefts and tops, the higher of the rights and bottoms, no test for an empty or an inverted one).
    // Unit w35a's DrawText calls it.
    TbRect Bounding(const TbRect& other) const;
    // bullfrog 0x10029f8c (LbDraw.cpp, 172 bytes, an import of ltms; unit w45a, src/bfl_tbrect.cpp): the larger of the lefts and tops
    // and the smaller of the rights and bottoms, where a right or bottom that comes out below its left or top is raised to it (so
    // the answer is never inverted: an empty one has right == left or bottom == top).
    TbRect Intersection(const TbRect& other) const;
    // Unit v5750: bullfrog 0x1002a06c (112 bytes): true when the rectangles overlap or touch (inclusive compares on all four sides).
    bool Intersects(const TbRect& other) const;
    // Unit v5L16: bullfrog 0x1002a1d4 (116 bytes, src/bfl_tbrect_TbRect_Intersects.cpp): left and right swapped when left is the greater
    // (signed), then top and bottom the same way; a member twin of LbRect_Normalise.
    void Normalise();
};

// Callees outside the loader (bullfrog library), faked by the unit tests and stood in for in the launcher.
uint8_t LbRect_Intersection(TbRect* rect, const TbRect* other);   // 0x10029ad0: clips `rect` to `other`, 1 when anything is left
void LbRect_Empty(TbRect* rect);                                  // 0x10029cd4: right = left, bottom = top
// Unit v51075: sound 0x10018158 (24), TbRect::TbRect(): the four sides zero. A free function so that TbRect stays an aggregate.
void TbRectConstruct(TbRect* rect);

// 0x1001a2f8 (Lbgfsave.cpp): `count` palette entries of four bytes from `count` triples (the fourth byte is 0).
void LbPalette_FromCompact(TbPalette* palette, const uint8_t* compact, uint32_t count);

class TbSurface;

// A picture in memory. Mac 0x28 bytes: the pixel format (a TbPixFormat, 0x18 bytes: four channel masks, the depth
// and the palettised flag), the pixels, the width, the height and the bytes in a row.
struct TbRenderArea {
    enum Cleared { kCleared };

    TbRenderArea();                                                               // 0x10016630: zeroes 0x10..0x27
    // Host only: what the default constructor does, inline. The loaders' constructors use it (tests/unit_UIInit.cpp
    // and tests/unit_ParticlesInit.cpp fake the out-of-line constructor and log it).
    explicit TbRenderArea(Cleared) {
        mDepth = 0;
        mHeight = 0;
        mData = nullptr;
        mWidth = 0;
        mF20 = 0;
        mStride = 0;
    }
    explicit TbRenderArea(const TbSurface* surface);                              // 0x10016560: a copy of the surface's area
    // 0x1001667c: `stride` 0 means a row of `size.width` pixels of the format's size.
    TbRenderArea(const TbSize& size, const TbPixFormat& format, uint8_t* data, uint32_t stride);
    TbResult AllocMemory();                  // 0x10016730: -1 if there are pixels already; mStride is set first, then GetMemSize bytes
    void FreeMemory();                       // 0x100167f8
    // 0x1001686c: the part of this area at `origin` of `size` (clipped to the area), made a client area in `client`:
    // the pixels start at the part, the rows keep this area's length. -1 when nothing is left.
    TbResult SetupClientRA(TbRenderArea& client, const TbPoint& origin, const TbSize& size) const;
    TbResult SetupClientRA(TbRenderArea& client, const TbRect& rect) const;       // 0x10016928: the same with the rectangle
    uint32_t GetMemSize() const;             // 0x10016ad0: mStride x the height

    uint8_t mF00[0x10];               // 0x00  the format's four channel masks
    int32_t mDepth;                   // 0x10  bits per pixel
    // 0x14 is the format's palettised flag. unit w21 read this word as the height (the interim loader of the launcher
    // wrote it that way); the picture's height is the word at 0x20. Both names are kept so that the fakes of other
    // units still compile.
    union {
        int32_t mHeight;              // 0x14  (w21's name)
        int32_t mPalettised;          // 0x14
    };
    uint8_t* mData;                   // 0x18
    int32_t mWidth;                   // 0x1c
    union {
        int32_t mF20;                 // 0x20  (w21's name)
        int32_t mPictureHeight;       // 0x20
    };
    int32_t mStride;                  // 0x24  bytes in a row
};

struct DDSURFACEDESC;
class IDirectDrawSurface;
// A drawing surface (WinScrn.cpp), 0x50 bytes on the Mac.
class TbSurface {
public:
    // Unit w231 (src/LbSurface.cpp)
    const TbRenderArea& GetRenderArea() const;   // bullfrog 0x10066e84, 204 B
    void CalculateSurfaceArea(DDSURFACEDESC&) const; // bullfrog 0x10066dcc, 112 B

    uint32_t mDrawFlags;             // 0x00
    uint8_t* mGhostTable;            // 0x04
    uint8_t* mFadeTable;             // 0x08
    uint8_t* mFadeRow;               // 0x0c
    uint8_t mColour[5];             // 0x10: alpha, red, green, blue, index
    uint8_t mPadding15[3];          // 0x15
    TbPalette* mPalette;            // 0x18
    uint32_t mF1C;                  // 0x1c
    uint32_t mF20;                  // 0x20
    IDirectDrawSurface* mDDSurface; // 0x24
    mutable TbRenderArea mArea{TbRenderArea::kCleared}; // 0x28
};

class TbTGALoader;

// The base of the loaders. Mac: the vtable pointer, a TbRenderArea (+4), the file's palette as 256 four-byte entries
// (+0x2c) and a flag (+0x42c) saying whether it has one; the subclass' members follow at 0x430.
class TbGraphicFileLoader {
public:
    TbGraphicFileLoader() : mArea(TbRenderArea::kCleared) {}                        // 0x10022870
    ~TbGraphicFileLoader() {}                                                       // 0x100228e0

    // 0x1002295c: opens the file (a CWadFile, `second` names the storage or is NULL) and reads its header into `area`
    // (NULL: only into the loader's own area).
    TbResult ReadHeader(const char* path, const char* second, TbRenderArea* area);
    // 0x10022b34: the same from an open file.
    TbResult ReadHeader(bfl::FileSystem::CFile& file, TbRenderArea* area);
    // 0x10022cd4: opens the file and reads its pixels into `area`'s memory; the loader's own area is copied to
    // `other` when it is not NULL. The second overload (0x10022e1c) does it from an open file, through a buffer.
    TbResult Load(const TbRenderArea& area, const char* path, const char* second, TbRenderArea* other);
    TbResult Load(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbRenderArea* other);
    bool IsValid(bfl::FileSystem::CFile& file);                                     // 0x100230cc: from the start of the file

    TbRenderArea mArea;        // 0x04  the picture the header describes
    uint8_t mPalette[0x400];   // 0x2c  red, green, blue and a zero byte for each of 256 entries
    int32_t mHasPalette;       // 0x42c
};

// A TGA file's 18-byte header, as the file has it (little endian; the Mac swaps the halfwords in place).
#pragma pack(push, 1)
struct TgaHeader {
    uint8_t mIdLength;         // 0x00  bytes of identification after the header
    uint8_t mMapType;          // 0x01  1: the file has a colour map
    uint8_t mImageType;        // 0x02  0 to 3 and 9 to 11 are accepted: 1 and 9 colour-mapped, 2 and 10 true colour, 3 and 11 grey, 9 to 11 run-length encoded
    uint16_t mMapStart;        // 0x03
    uint16_t mMapLength;       // 0x05  entries
    uint8_t mMapEntryBits;     // 0x07  24 is the only size accepted
    uint16_t mOriginX;         // 0x08
    uint16_t mOriginY;         // 0x0a
    uint16_t mWidth;           // 0x0c
    uint16_t mHeight;          // 0x0e
    uint8_t mDepth;            // 0x10  bits per pixel: 8, 15, 16, 24 or 32
    uint8_t mDescriptor;       // 0x11  bit 4: right to left, bit 5: top row first
};
#pragma pack(pop)
static_assert(sizeof(TgaHeader) == 0x12, "a TGA header is 18 bytes");

// The loader for TGA files. Mac 0x442 bytes: the base's 0x430 and the header.
class TbTGALoader : public TbGraphicFileLoader {
public:
    TbTGALoader();    // 0x10023a80
    ~TbTGALoader();   // 0x10023af0
    // 0x10023b70: the header of the file into the loader's area and palette (`area` is ignored: the reader works on
    // the loader's own).
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbRenderArea& area);
    // 0x10023c14: the pixels into `area` (`loaderArea` is ignored).
    TbResult ReadData(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbRenderArea& loaderArea);
    // 0x10023cc0: 1 when the file ends in a TGA 2.0 footer or begins with an acceptable header; the file's position is left wherever it was read to.
    bool ValidImage(bfl::FileSystem::CFile& file);

    TgaHeader mHeader;   // 0x430  as read by the reader
};

// The line reader the picture formats share (Lbgfsave.cpp). A subclass says how a line is read; DoRead reads a
// whole picture a line at a time into the line buffer and copies each line to the area. Mac: the vtable pointer is
// at 0x1c; the members before it are these.
class _AndyGLL {
public:
    virtual TbResult ReadFirstLine(bfl::FileSystem::CFile& file) = 0;   // slot 0x8: the first line into mLine
    virtual TbResult ReadNextLine(bfl::FileSystem::CFile& file) = 0;    // slot 0xc
    virtual uint32_t GetSourceLineLength() = 0;                         // slot 0x10: bytes of a line in the file
    virtual void WriteLine();                                           // slot 0x14, 0x1001e2f4: mDestBytes of mLine to mDest, which moves on by mDestStep
    // 0x1001de44: reads the picture of `loader` (the file `file`) into `area`: a line buffer of the larger of the
    // file's line and the loader's width in `area`'s pixels, the lines flipped when the file's first line is the
    // bottom one, converted by a CConverter when the formats differ. A line that fails to read ends the picture
    // with the answer 0 all the same.
    TbResult DoRead(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbGraphicFileLoader* loader);

    uint8_t* mLine;           // 0x00  the line buffer
    uint32_t mLineSize;       // 0x04
    uint32_t mDestBytes;      // 0x08  the bytes WriteLine copies: the narrower of the loader's and the area's rows, in the area's pixels
    uint32_t mSourceBytes;    // 0x0c  the loader's row in its own pixels
    int32_t mBottomUp;        // 0x10  1: the file's first line is the bottom one (the lines are written upwards from the last)
    uint8_t* mDest;           // 0x14  where WriteLine writes next
    int32_t mDestStep;        // 0x18  bytes mDest moves by, negative for a bottom-up file
};

// Unit w220: the BMP and PCX loaders of Lbgfsave.cpp. The Mac reads their header fields big-endian without
// swaps, so these headers stay raw bytes. The game makes only the TGA loader and does not reach these classes.
class TbBMPLoader;
class TbPCXLoader;

class _AndyBMPGLL : public _AndyGLL {  // Mac 0x64 bytes
public:
    TbResult ReadData(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbBMPLoader* loader);  // 0x1001e36c
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbBMPLoader* loader);  // 0x1001e478
    TbResult ReadFirstLine(bfl::FileSystem::CFile& file) override;              // 0x1001e9fc
    TbResult ReadNextLine(bfl::FileSystem::CFile& file) override;               // 0x1001ec08
    uint32_t GetSourceLineLength() override;                                  // 0x1001f004
    TbResult BMPLoadRLE(bfl::FileSystem::CFile& file);                          // 0x1001ede8

    uint32_t mLinesLeft;       // 0x20
    uint32_t mLineBytes;       // 0x24
    uint8_t* mRleData;         // 0x28
    uint8_t* mRleCursor;       // 0x2c
    int32_t mSkipLines;        // 0x30
    uint32_t mNextX;           // 0x34
    int32_t mRleEnd;           // 0x38
    uint8_t mInfo[0x28];       // 0x3c, BITMAPINFOHEADER as the file has it
    static uint8_t sFileHeader[0xe];  // Mac global 0x1007479c, static in Lbgfsave.cpp
};

class _AndyPCXGLL : public _AndyGLL {  // Mac 0xb2 bytes
public:
    TbResult ReadData(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbPCXLoader* loader);  // 0x1001fa70
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbPCXLoader* loader);  // 0x1001fb7c
    TbResult ReadFirstLine(bfl::FileSystem::CFile& file) override;              // 0x1001ff38
    TbResult ReadNextLine(bfl::FileSystem::CFile& file) override;               // 0x1001fff0
    uint32_t GetSourceLineLength() override;                                  // 0x10020294

    uint32_t mDecodeBytes;     // 0x20
    uint32_t mLinesLeft;       // 0x24, Mac 0x28 and 0x2c unused
    uint8_t mRunCount;         // 0x30
    uint8_t mRunValue;         // 0x31
    uint8_t mHeader[0x80];     // 0x32, PCX header as the file has it
};

class TbBMPLoader : public TbGraphicFileLoader {  // Mac 0x458 bytes
public:
    TbBMPLoader();   // Host only
    ~TbBMPLoader();  // Host only
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbRenderArea& area);  // 0x10023190
    TbResult ReadData(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbRenderArea& loaderArea);  // 0x10023234
    bool ValidImage(bfl::FileSystem::CFile& file);                             // 0x100232e0
    uint8_t mInfo[0x28];       // 0x430
};

class TbPCXLoader : public TbGraphicFileLoader {  // Mac 0x4b0 bytes
public:
    TbPCXLoader();   // Host only
    ~TbPCXLoader();  // Host only
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbRenderArea& area);  // 0x10023630
    TbResult ReadData(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbRenderArea& loaderArea);  // 0x100236d4
    bool ValidImage(bfl::FileSystem::CFile& file);                             // 0x10023780
    uint8_t mHeader[0x80];     // 0x430
};

static_assert(sizeof(_AndyBMPGLL::sFileHeader) == 0xe, "BMP file header size");
static_assert(sizeof(TbBMPLoader::mInfo) == 0x28, "BMP loader header size");
static_assert(sizeof(_AndyBMPGLL::mInfo) == 0x28, "BMP reader header size");
static_assert(sizeof(TbPCXLoader::mHeader) == 0x80, "PCX loader header size");
static_assert(sizeof(_AndyPCXGLL::mHeader) == 0x80, "PCX reader header size");

// The TGA line reader: a stack object of TbTGALoader's ReadHeaderInfo and ReadData. Mac: 0x20 onwards.
class _TgaGLL : public _AndyGLL {
public:
    // 0x10021228: the header (ReadHeaderInfo) and, when it is fine, DoRead.
    TbResult ReadData(const TbRenderArea& area, bfl::FileSystem::CFile& file, TbTGALoader* loader);
    // 0x10021330: reads and checks the header into the loader (the loader's area gets the format of the file's depth,
    // the width and the height; the colour map goes to the palette), and keeps a copy of it and the flags below here.
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbTGALoader* loader);
    TbResult ReadFirstLine(bfl::FileSystem::CFile& file) override;      // 0x100217dc: counts the lines, then ReadNextLine
    TbResult ReadNextLine(bfl::FileSystem::CFile& file) override;       // 0x10021874
    uint32_t GetSourceLineLength() override;                            // 0x10021d28: the width times the bytes of a pixel
    TbResult ReverseScanLine(uint8_t* line);                            // 0x10021a28: the line's pixels in reverse order
    // 0x10021b00: one line of run-length encoded pixels into mLine. A run that reaches past the end of the line writes
    // past it (the original does not look).
    TbResult TGALoadRLE(bfl::FileSystem::CFile& file);

    int32_t mRle;             // 0x20  1 for the run-length encoded image types
    int32_t mRightToLeft;     // 0x24  the descriptor's bit 4: each line is reversed after it is read
    uint32_t mLinesLeft;      // 0x28
    uint32_t mLineBytes;      // 0x2c  the bytes of a line in the file
    TgaHeader mHeader;        // 0x30  the file's header
};

// Unit w223 (src/Lbgfsave_iffgiftim.cpp)
class TbIFFLoader;
class TbGIFLoader;
class TbTIMLoader;

class _AndyIFFGLL : public _AndyGLL {  // Mac 0x60
public:
    TbResult ReadData(const TbRenderArea&, bfl::FileSystem::CFile&, TbIFFLoader*); // 0x1001f078
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile&, TbIFFLoader*); // 0x1001f184
    TbResult ReadFirstLine(bfl::FileSystem::CFile&) override; // 0x1001f578
    TbResult ReadNextLine(bfl::FileSystem::CFile&) override; // 0x1001f7b8
    uint32_t GetSourceLineLength() override; // 0x1001fa14
    uint8_t* mRead;          // 0x20
    uint32_t mPlanes;        // 0x24
    uint32_t mPlaneBytes;    // 0x28
    uint32_t mLinesRead;     // 0x2c
    uint32_t mLineBytes;     // 0x30
    uint32_t mHeight;        // 0x34
    uint8_t mHeader[0x28];   // 0x38
    static uint8_t sChunk[8]; // 0x10074794, invented name
};

class _AndyGIFGLL : public _AndyGLL {  // Mac approximately 0x188
public:
    TbResult ReadData(const TbRenderArea&, bfl::FileSystem::CFile&, TbGIFLoader*); // 0x100202e8
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile&, TbGIFLoader*); // 0x100204b8
    TbResult ReadFirstLine(bfl::FileSystem::CFile&) override; // 0x100206b4
    TbResult ReadNextLine(bfl::FileSystem::CFile&) override; // 0x10020a68
    int32_t _GifGetNextCode(bfl::FileSystem::CFile&); // 0x10020d10, scalar return
    TbResult _GifReadPalette(bfl::FileSystem::CFile&, TbPalette*); // 0x10020f3c
    void WriteLine() override; // 0x100210e8
    uint32_t GetSourceLineLength() override; // 0x100211e8
    uint32_t mInterlaced;     // 0x20
    uint32_t mNewCodes;       // 0x24
    uint32_t mSize;           // 0x28
    uint32_t mFc;             // 0x2c
    uint32_t mOc;             // 0x30
    TbPalette* mPalette;     // 0x34
    uint32_t mPass;           // 0x38
    uint32_t mRow;            // 0x3c
    uint8_t* mFirstDest;     // 0x40
    uint32_t mAvailBytes;     // 0x44
    uint32_t mBitsLeft;       // 0x48
    uint32_t mCurrSize;       // 0x4c
    uint32_t mClear;          // 0x50
    uint32_t mTopSlot;        // 0x54
    uint32_t mEnding;         // 0x58
    uint32_t mSlot;           // 0x5c
    uint8_t* mBytes;         // 0x60
    uint8_t mByteBuff[257];  // 0x64
    uint8_t* mStack;         // 0x168
    uint8_t* mSp;            // 0x16c
    uint8_t* mSuffix;        // 0x170
    uint16_t* mPrefix;       // 0x174, native internal halfwords
    uint8_t mHeader[0xd];    // 0x178
    static uint8_t sPalette[0x300]; // 0x10074490, invented name
    static uint8_t sB1;       // 0x10074790, invented name
};

class _TimGLL : public _AndyGLL { // Mac 0x3c
public:
    TbResult ReadData(const TbRenderArea&, bfl::FileSystem::CFile&, TbTIMLoader*); // 0x10021d70
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile&, TbTIMLoader*); // 0x10021e78
    TbResult ReadFirstLine(bfl::FileSystem::CFile&) override; // 0x10022448
    TbResult ReadNextLine(bfl::FileSystem::CFile&) override; // 0x100224d8
    uint32_t GetSourceLineLength() override; // 0x10022818
    uint32_t mLinesLeft;      // 0x20
    uint32_t mLineBytes;      // 0x24
    uint32_t mHeight;         // 0x28
    uint32_t mWidth;          // 0x2c
    uint32_t mWordWidth;      // 0x30
    uint8_t mHeader[8];      // 0x34
};

// Loader sizes inferred from member offsets; the binary has no constructors for these classes.
class TbIFFLoader : public TbGraphicFileLoader { // Mac 0x458
public:
    TbIFFLoader(); // host only
    ~TbIFFLoader(); // host only
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&); // 0x100233b0
    TbResult ReadData(const TbRenderArea&, bfl::FileSystem::CFile&, TbRenderArea&); // 0x10023454
    bool ValidImage(bfl::FileSystem::CFile&); // 0x10023500
    uint8_t mHeader[0x28]; // 0x430
};
class TbGIFLoader : public TbGraphicFileLoader { // Mac 0x440, padded
public:
    TbGIFLoader(); // host only
    ~TbGIFLoader(); // host only
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&); // 0x10023850
    TbResult ReadData(const TbRenderArea&, bfl::FileSystem::CFile&, TbRenderArea&); // 0x100238f4
    bool ValidImage(bfl::FileSystem::CFile&); // 0x100239a0
    uint8_t mHeader[0xd]; // 0x430
};
class TbTIMLoader : public TbGraphicFileLoader { // Mac 0x438
public:
    TbTIMLoader(); // host only
    ~TbTIMLoader(); // host only
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&); // 0x10023f7c
    TbResult ReadData(const TbRenderArea&, bfl::FileSystem::CFile&, TbRenderArea&); // 0x10024020
    bool ValidImage(bfl::FileSystem::CFile&); // 0x100240cc
    uint8_t mHeader[8]; // 0x430
};
extern const TbPixFormat LbgfsaveTim16; // 0x100747c4, invented name, __sinit 0x1002419c
extern const TbPixFormat LbgfsaveTim24; // 0x100747ac, invented name
static_assert(sizeof(TbIFFLoader::mHeader) == 0x28 && sizeof(_AndyIFFGLL::mHeader) == 0x28, "IFF headers");
static_assert(sizeof(TbGIFLoader::mHeader) == 0xd && sizeof(_AndyGIFGLL::mHeader) == 0xd, "GIF headers");
static_assert(sizeof(TbTIMLoader::mHeader) == 8 && sizeof(_TimGLL::mHeader) == 8, "TIM headers");
