// EngineUtil.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Debug.h"
#include "FS_System.h"
#include "IgArray.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/indirectx.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_fpu.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_rain.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "Wavelet.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>
#include <type_traits>

// Wavelet.cpp (engine library of SimTheme Park 2.04b3).
// PARTIAL: the callbacks so far: SetFileCallbacks (0x10038964), translated with
// landscape.cpp, whose loaders read through them; DefaultMalloc (0x100388c0), DefaultFree
// (0x10038914) and SetEngineCallbacks (0x100389f8), translated with tb_capture.cpp;
// BitsSet, ShiftFactor and CreateSineTable (0x10038a78..0x10038bf8), translated with
// engine start-up. The rest of the file is still to do.

TbFileOpenFn gFileOpen;
TbFileCloseFn gFileClose;
TbFileReadFn gFileRead;
TbFileSeekFn gFileSeek;
TbFileTellFn gFileTell;
TbFileNameFn gFileName;

void SetFileCallbacks(TbFileOpenFn open, TbFileCloseFn close, TbFileReadFn read, TbFileSeekFn seek,
                      TbFileTellFn tell, TbFileNameFn name) {
    gFileOpen = open;
    gFileClose = close;
    gFileRead = read;
    gFileSeek = seek;
    gFileTell = tell;
    gFileName = name;
}

TbIsFirstPersonFn gIsFirstPerson;
TbUseSmallTexturesFn gUseSmallTextures;
TbReadClockFn gReadClock;
TbUpdateMouseFn gUpdateMouse;
TbMallocFn gMalloc = DefaultMalloc;
TbFreeFn gFree = DefaultFree;

void* DefaultMalloc(uint32_t size) {
    DebugStr(reinterpret_cast<const unsigned char*>("\x17" "malloc callback not set"));
    return nullptr;
}

void DefaultFree(void*& block) {
    DebugStr(reinterpret_cast<const unsigned char*>("\x15" "free callback not set"));
}

void SetEngineCallbacks(TbIsFirstPersonFn isFirstPerson, TbUseSmallTexturesFn useSmallTextures,
                        TbReadClockFn readClock, TbUpdateMouseFn updateMouse, TbMallocFn malloc,
                        TbFreeFn free) {
    gIsFirstPerson = isFirstPerson;
    gReadClock = readClock;
    gUseSmallTextures = useSmallTextures;
    gUpdateMouse = updateMouse;
    gMalloc = malloc;
    gFree = free;
}

int32_t BitsSet(uint32_t mask) {
    int32_t count = 0;
    for (; mask != 0; mask >>= 1) {
        if ((mask & 1) != 0) ++count;
    }
    return count;
}

int32_t ShiftFactor(uint32_t mask) {
    if (mask == 0) return 0;
    int32_t shift = 0;
    for (; (mask & 1) == 0; mask >>= 1) ++shift;
    return shift;
}

// The weather's drawing (engine library of SimTheme Park 2.04b3, in the inventory's
// tb_resource_display.cpp range; the original file is tb_rain.cpp by its assertions):
//
//   0x10022b20 CRain::__LightningFlash   (192 bytes)
//   0x10022c10 CRain::__RenderLightning  (700)
//   0x10022efc CRain::__RenderRain       (1,156)
//   0x100233f8 CRain::__RenderSnow       (2,844)
//   0x10038b14 GetCycleCount             (40)
//
// CRain::Render (src/tb_rain_frame.cpp) calls the first three every frame. __RenderLightning
// draws the white flash of a lightning strike (a sprite over the whole screen), ages the bolts
// and draws each live one through RenderLightning2. __RenderRain draws a streak for each drop
// and __RenderSnow a small triangle with a quarter of the snow texture for each flake; both
// move the drops into the camera's space first (the wrapped positions Render works out) and
// add the polygons one by one to a primitive of their own.
//
// Floating point. Every float operation follows the listing: the single-precision adds,
// subtracts and multiplies are plain, the fused multiply-adds (fmadds) go through ppc_fmadds,
// and a float -> int conversion is fctiwz (ppc_fctiwz).
//
// The polygons' vertices are built on the stack in arrays with room for the vertices the clipper
// adds (the original keeps them in its frame and lets the clipper use the arrays as work space),
// and the specular colour of a vertex is never set: stack leftovers in the original, zero here
// and left to what the clipper writes into the array afterwards.

// The Mac's time base register, read through the platform timer: its low 32 bits (a counter that
// only goes up; the Mac's runs at about 25 MHz, the host's in nanoseconds).
uint32_t GetCycleCount() { return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsoluteCycles()); }

// tb_fpu.cpp: the engine's FPU precision switches (include/tb_fpu.h): empty on the Mac.

int16_t FPU_SetSinglePrecision() { return 0; }

void FPU_RestorePrecision(int16_t) {}

void FPU_SetFullPrecision(int16_t) {}

float gSineTable[256];

void CreateSineTable() {
    for (int32_t i = 0; i < 256; ++i) {
        gSineTable[i] = static_cast<float>(std::sin(static_cast<double>(3.1415927f * (2.0f * static_cast<float>(i) * 0.00390625f))));
    }
}

// tb_rotatevertex.cpp (engine library of SimTheme Park 2.04b3; the inventory files the range
// under Wavelet.cpp, next to GetCycleCount): the two turns of a quad of vertices.
//
//   0x10038cd0 RotateVertex_Y  (404 bytes)
//   0x10038e9c RotateVertex_Z  (428)
//
// Both turn the four vertices of a quad (0x10 bytes apart) in place about a centre. Y turns x
// and y by an angle given as a fraction of a turn through the engine's 256-entry sine table;
// Z turns x and z by an angle in radians through the C library's cosine and sine. The sky's
// added sprites, the particles and the sprite quads (SSpriteGameData::SetupVertices) use them.
//
// Each vertex is turned in the order 3, 2, 1, 0, in single precision with one fused
// multiply-add per coordinate, as the listing has them.

namespace {

// Turns the pair (a, b) of one vertex about (ca, cb): the vertex minus the centre is stored
// first, then read back; the new pair is the centre plus (a * c + s * b, -a * s + c * b).
void Turn(float& a, float& b, float ca, float cb, float c, float s) {
    a = a - ca;
    b = b - cb;
    const float x = a;
    const float y = b;
    const float negated = -x;
    a = ca + ppc_fmadds(x, c, s * y);
    b = cb + ppc_fmadds(negated, s, c * y);
}

}  // namespace

// The angle is a fraction of a turn: 256 * angle, cut to an integer, picks the sine (that
// entry of gSineTable) and the cosine (the entry a quarter turn on, 0x40 further). A NaN or
// an angle beyond 2^23 turns picks entry 0 or 0xff as the integer's low byte says. With no
// centre the middle of the quad is used: x halfway between vertices 0 and 1, y halfway
// between vertices 0 and 2.
void RotateVertex_Y(SVertex* vertices, float angle, SVertex* centre) {
    float cx;
    float cy;
    if (centre != nullptr) {
        cx = centre->x;
        cy = centre->y;
    } else {
        cx = 0.5f * (vertices[0].x + vertices[1].x);
        cy = 0.5f * (vertices[0].y + vertices[2].y);
    }
    const int32_t turn = ppc_fctiwz(static_cast<double>(256.0f * angle));
    const float cosine = gSineTable[static_cast<uint32_t>(turn + 0x40) & 0xff];
    const float sine = gSineTable[static_cast<uint32_t>(turn) & 0xff];
    for (int32_t i = 3; i >= 0; --i) Turn(vertices[i].x, vertices[i].y, cx, cy, cosine, sine);
}

// The cosine and sine are the library's (double precision, cut to single); the centre is the
// middle of the quad in x and z and is worked out before them.
void RotateVertex_Z(SVertex* vertices, float angle) {
    const float cx = 0.5f * (vertices[0].x + vertices[1].x);
    const float cz = 0.5f * (vertices[0].z + vertices[2].z);
    const float cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
    const float sine = static_cast<float>(std::sin(static_cast<double>(angle)));
    for (int32_t i = 3; i >= 0; --i) Turn(vertices[i].x, vertices[i].z, cx, cz, cosine, sine);
}

// The mesh file loader (engine library of SimTheme Park 2.04b3, 0x100483f4..0x1004b9c8 and
// AlignTo64Bit at 0x10039078; the original file's name is not in the binary, the inventory
// places the range after tb_printf_opengl.cpp).
//
// LoadM3D2Header reads a whole .md2 file into one block and makes it usable in place.
// The file is little-endian and holds offsets from its start where the structures of
// include/m3d2.h have pointers. The original does two things to every record: it swaps
// each field to the Mac's byte order, and it adds the block's address to each offset that
// is not 0. Here the swaps go through SamsUtilities' block swappers, which do nothing on a
// little-endian host, and an offset becomes a pointer with Ptr32::Relocate.
//
// Two bit fields are the exception to "swap each field": the Mac's compiler numbers the
// bits of a bit field from the other end, so the original REBUILDS those words instead of
// swapping them, and the engine's code then reads the rebuilt layout. They are rebuilt
// here on every host, from the file's little-endian value, into the value the Mac has:
//   a face's first word: index (15 bits) and flag (1 bit) become index << 1 | flag;
//   a compressed vertex: three 10-bit fields a, b, c (from bit 0 up) become
//   a << 22 | b << 12 | c << 2.

// 0x10039078 and 0x100390cc: rounds a size or an address up to a multiple of eight; what
// was added goes onto *moved.
void AlignTo64Bit(uint32_t& value, uint32_t* moved) {
    const uint32_t aligned = (value + 7) & ~7u;
    if (moved != nullptr) *moved += aligned - value;
    value = aligned;
}
void AlignTo64Bit(void*& address, uint32_t* moved) {
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(address) + 7) & ~static_cast<uintptr_t>(7);
    if (moved != nullptr) *moved += static_cast<uint32_t>(aligned - reinterpret_cast<uintptr_t>(address));
    address = reinterpret_cast<void*>(aligned);
}

// A ride's working copy of its animation set (the main app's TPRide.cpp, TPEmbed.cpp, rsse.cpp and
// ui_ErrorDisplay.cpp ranges, as the inventory files them): InstanceRideAnimSet and what it calls
// (InstanceMesh, InstanceAnim, InstanceDynamicMeshSet, DisableDataNodesRender and its sibling), and the
// small functions LoadRideType calls on a loaded set (CorrectFaceFlags1, CorrectNodeFlags2, SetupRideHeads,
// GetFontPath, OverrideActiveChannels) with ERRORBox.
//
// The copies are made the way the original makes them: one block from the allocator (MReleaseAllocF), the
// pieces laid after each other in it, then every pointer that led into the original's tables made to lead
// into the copies. The mesh and animation records keep the file's layout (Ptr32 fields, arena offsets), so
// the original's arithmetic on addresses is arithmetic on offsets here. The game's own records (tag_sMesh,
// the embedded set and its records, tag_sRideAnimSet and its entries, tag_sDMeshSet) have native pointers
// and are larger than on the Mac, so the block's fixed parts are larger too (see InstanceMesh).

// 0x10039120 and 0x10039174, 44 bytes each: rounds a size or an address up to a multiple of four; what was
// added goes onto *moved.
void AlignTo32Bit(uint32_t& value, uint32_t* moved) {
    const uint32_t aligned = (value + 3) & ~3u;
    if (moved != nullptr) *moved += aligned - value;
    value = aligned;
}
void AlignTo32Bit(void*& address, uint32_t* moved) {
    const uintptr_t aligned = (reinterpret_cast<uintptr_t>(address) + 3) & ~static_cast<uintptr_t>(3);
    if (moved != nullptr) *moved += static_cast<uint32_t>(aligned - reinterpret_cast<uintptr_t>(address));
    address = reinterpret_cast<void*>(aligned);
}

// 0x100391c8
bool IsAlignedTo64Bit(void*& pointer) {
    return (reinterpret_cast<uintptr_t>(pointer) & 7) == 0;
}

// Wavelet.cpp, part 2 (engine library of SimTheme Park 2.04b3): the version 0x13 wavelet
// texture decoder and what it uses. LzssDecompress 0x10039210, read_code 0x10039360,
// read_reset 0x10039424, the three dispatching CWaveletImp::Decompress overloads 0x10039694,
// 0x10039718 and 0x100397dc, ReverseTransformVert 0x1003d054, Decompress_v13 0x1003d230, and
// CIgArray<float, float>'s members (GetData 0x1003d1f8, constructor 0x1003ee44, destructor
// 0x1003ee88, SetSize 0x1003ef48). Kept apart from src/Wavelet.cpp (the callbacks and the
// decompressor's lifetime) so a test library can link one without the other.
//
// A version 0x13 file holds two LZSS-packed blocks of wavelet coefficients: the colour
// (brightness at full size, then the two colour differences at half size) and, when the
// header says so, the alpha. Each image is one level of the Daubechies D4 transform: every
// row of the stream is (smooth, detail) pairs; the horizontal pass rebuilds the rows into
// gWaveletRows and the vertical pass the columns into the image's plane. The colour is then
// converted to 16-bit RGBA 4444 pixels (two by two, one colour difference per square).

extern uint32_t gLzssBitCount;

extern uint32_t gLzssBitBuffer;

extern uint32_t gLzssUnused;

extern uint32_t gLzssSourcePos;

extern uint8_t* gLzssDest;

extern const uint8_t* gLzssSource;

uint32_t read_code(uint32_t bits) {
    if (gLzssBitCount < bits) {
        // Two more bytes, low byte first, above the bits that are left.
        const uint8_t* source = gLzssSource;
        gLzssBitBuffer |= ppc_slw(source[gLzssSourcePos++], static_cast<int32_t>(gLzssBitCount));
        gLzssBitBuffer |= ppc_slw(source[gLzssSourcePos++], static_cast<int32_t>(gLzssBitCount + 8));
        gLzssBitCount += 16;
    }
    const uint32_t buffer = gLzssBitBuffer;
    gLzssBitBuffer = ppc_srw(buffer, static_cast<int32_t>(bits));
    const uint32_t code = buffer & (ppc_slw(1, static_cast<int32_t>(bits)) - 1);
    gLzssBitCount -= bits;
    return code;
}

// ---- LZSS ---------------------------------------------------------------------------------
void read_reset() {
    gLzssBitBuffer = 0;
    gLzssBitCount = 0;
}

void LzssDecompress(void* source, void* dest) {
    gLzssSource = static_cast<const uint8_t*>(source);
    gLzssDest = static_cast<uint8_t*>(dest);
    gLzssSourcePos = 0;
    gLzssUnused = 0;
    read_reset();
    uint8_t* out = gLzssDest;
    for (;;) {
        if (read_code(1) == 0) {
            *out++ = static_cast<uint8_t>(read_code(8));
            continue;
        }
        const uint32_t offset = read_code(12);
        const uint32_t length = read_code(7) + 1;
        if (offset == 0) return;
        // A byte at a time, so a copy may overlap its own output.
        const uint8_t* from = out - offset;
        for (uint32_t i = 0; i < length; ++i) *out++ = *from++;
    }
}
