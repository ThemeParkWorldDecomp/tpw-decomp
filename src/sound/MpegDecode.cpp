// MpegDecode.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of MpegDecode code, named after it.

#include "CMpegBase.h"
#include "helpers/LbMpegHelpers.h"
#include "platform/interfacelib.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "TbMpegBase.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>

// The sound library's MPEG file player (unit w80c; program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp):
// CMpegMacFilePlayerUnpooled, its pool SamsUtilities::UPooled<CMpegMacFilePlayerUnpooled, 10> and the pool's queue
// UInterruptSafeQueue<unsigned char[28004]>. See include/CMpegBase.h for the layout.
//
// How the host differs from the Mac:
//  - The Mac started a File Manager read (PBReadAsync) into the buffer that was not being decoded and trusted it to be complete
//    by the time the decoder had used up the other one. The host has no asynchronous File Manager: RequestBuffer performs the
//    read at once through tpw::ReadFileReference (platform/file_handles.cpp, the open file named by ioRefNum), which is the only
//    order the original was correct under. The result is what the Mac's completion would have stored: ioActCount the bytes
//    read, ioPosOffset advanced by them, ioResult 0, -39 (eofErr) for a short read or -51 (rfNumErr) for an unknown reference.
//    The positioning mode is taken as fsFromStart, which is the one the player sets.
//  - Enqueue and Dequeue (the Mac OS queue calls) are written out below. The host has no interrupts, but the players are
//    returned to the pool from the audio thread, so they lock.
//  - DebugStr and SamsUtilities::UDebug are the platform layer's (they log). The sound library's own UDebug (0x10003950, 36
//    bytes) returns at once; the platform's is the one the host links, so it has no definition here.
//  - __sinit_LbMpeg_cpp (0x10003acc) constructs the file's static objects in this order: CMpegBase_DecodeMethod at 0x1003e370
//    and _CS_CMpegBase at 0x1003e360 (both in src/TbMpegBase.cpp), then gPoolQueue and gPoolCleanup (below), and registers their
//    destructors to run at exit. On the host these are the file's static objects, and the destructors run at exit.

using Pool = SamsUtilities::MpegFilePlayerPool;
using Queue = Pool::Queue;

// ---------------------------------------------------------------- the queue

namespace SamsUtilities {

// 0x10003b98 (the deleting form frees the cleanup object itself).
template <>
Pool::StaticCleanup::~StaticCleanup() { ClearPool(1); }

// 0x10007ffc: for the player's size, the first element of the queue whose player has no read in flight, taken off the queue;
// NULL when there is none (no heap fallback). Any other size goes to the heap.
// (The game tests the answer: NULL is a pool with no idle player, not an allocation failure.)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnew-returns-null"
#pragma clang diagnostic pop

// 0x10003c58: free the elements whose player has no read in flight. `all` restarts from the head after each (so a player that
// stays busy at the head never lets it end); otherwise one pass.
template <>
void Pool::ClearPool(uint8_t all) {
    ReportPoolStatistics();
    if (all == 0) {
        Queue::Element* element = gPoolQueue.GetHead();
        while (element != nullptr) {
            Queue::Element* next = element->GetNext();
            void* player = element->GetData();
            if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
                ::operator delete(element);
            }
            element = next;
        }
    } else {
        Queue::Element* element;
        while ((element = gPoolQueue.GetHead()) != nullptr) {
            void* player = element->GetData();
            if (IsPoolObjectAvailable(static_cast<CMpegMacFilePlayerUnpooled*>(player)) && gPoolQueue.Remove(element)) {
                ::operator delete(element);
            }
        }
    }
}

// 0x10003f40: ioResult is not positive (no read in flight).
template <>
bool Pool::IsPoolObjectAvailable(const CMpegMacFilePlayerUnpooled* player) { return player->mIo.ioResult <= 0; }

// 0x10004040: empty.
template <>
void Pool::ReportPoolStatistics() {}

}  // namespace SamsUtilities

// The sound library's MPEG audio decoder object, part 1 (unit w71a): CMpegBase's pool, bit reader, header parser, frame
// reader and layer I decoder (program "sound symbols shared", LbMpeg.cpp and StreamableMpeg2.cpp). See include/TbMpegBase.h
// for the layout. PolySynth, dct64, Decode_Layer2 and II_step_two (unit w71b) are declared there and defined elsewhere.

int32_t using_MMX = 0;   // 0x1003e380

CMpegBase* CMpegBase::mFirstCMpegBase = nullptr;
int32_t CMpegBase::mCurrentCMpegBase = 0;
int32_t CMpegBase::mLowCMpegBase = 0;
int32_t CMpegBase::mHighCMpegBase = 0;
int32_t CMpegBase::mTotalCMpegBase = 0;
int32_t CMpegBase::mLevelCMpegBase = 10;

// Decode_Layer1's scratch: file-level arrays on the Mac too (the bit allocations at 0x1003e588, the scale factor indices at
// 0x1003e488 and the samples at 0x1003e388; 0x100 bytes each, two channels of 32 bands). Layer II keeps its own at 0x1003e688.
uint32_t gMpegLayer1Alloc[64];
int32_t gMpegLayer1Scale[64];
int32_t gMpegLayer1Sample[64];

namespace {

// The multipliers' table: muls[27][64] of mpglib, computed the way mpglib's make_decode_tables does (the unit's test compares
// every word with the Mac's table, 0x10035df0).
struct MulsTable {
    float value[27][64];
    MulsTable() {
        static const double kMulMul[27] = {0.0, -2.0 / 3.0, 2.0 / 3.0, 2.0 / 7.0, 2.0 / 15.0, 2.0 / 31.0, 2.0 / 63.0, 2.0 / 127.0,
                                           2.0 / 255.0, 2.0 / 511.0, 2.0 / 1023.0, 2.0 / 2047.0, 2.0 / 4095.0, 2.0 / 8191.0,
                                           2.0 / 16383.0, 2.0 / 32767.0, 2.0 / 65535.0, -4.0 / 5.0, -2.0 / 5.0, 2.0 / 5.0,
                                           4.0 / 5.0, -8.0 / 9.0, -4.0 / 9.0, -2.0 / 9.0, 2.0 / 9.0, 4.0 / 9.0, 8.0 / 9.0};
        for (int k = 0; k < 27; ++k) {
            double m = kMulMul[k];
            int j = 3;
            for (int i = 0; i < 63; ++i, --j) value[k][i] = static_cast<float>(m * std::pow(2.0, static_cast<double>(j) / 3.0));
            value[k][63] = 0.0f;
        }
    }
};

const MulsTable& Muls() {
    static const MulsTable table;
    return table;
}

}  // namespace

int32_t MpegIntegerAllocation(int32_t index) {
    if (index < 0 || index > 31) return 0;   // the Mac reads other data there; no caller does
    return static_cast<int32_t>(1u - (1u << index));
}

float MpegMultiplier(int32_t row, int32_t scale) { return Muls().value[row][scale]; }

_CS_CMpegBase::~_CS_CMpegBase() { CMpegBase::ClearCache(); }

CMpegBase_DecodeMethod::~CMpegBase_DecodeMethod() {
    if (using_MMX != 0) CMpegBase::Close_Tables();
}

CMpegBase_DecodeMethod::CMpegBase_DecodeMethod() { using_MMX = 0; }

// One band of layer II's integer path: the sample of each channel is its bits plus the allocation's offset, times the
// multiplier of (allocation + 1, scale factor), shifted down 12 and stored as a short (channel 0 at band, channel 1 at band
// + 32). An unallocated channel stores 0 and reads no bits.
void CMpegBase::get_sb_stereo(int32_t band, char (*alloc)[2], uint32_t (*scale)[32], int16_t* out, int32_t* mulstab) {
    int16_t result = 0;
    int32_t allocation = static_cast<int8_t>(alloc[band][0]);
    if (allocation != 0) {
        int32_t bits = static_cast<int32_t>(GetBits(allocation + 1));
        int32_t multiplier = mulstab[(allocation + 1) * 0x40 + static_cast<int32_t>(scale[0][band])];
        result = static_cast<int16_t>(Mul(Add(MpegIntegerAllocation(allocation), bits), multiplier) >> 12);
    }
    out[band] = result;
    result = 0;
    allocation = static_cast<int8_t>(alloc[band][1]);
    if (allocation != 0) {
        int32_t bits = static_cast<int32_t>(GetBits(allocation + 1));
        int32_t multiplier = mulstab[static_cast<int32_t>(scale[1][band]) + (allocation + 1) * 0x40];
        result = static_cast<int16_t>(Mul(multiplier, Add(MpegIntegerAllocation(allocation), bits)) >> 12);
    }
    out[band + 0x20] = result;
}
// Layer I: the allocation (4 bits a band, one byte for the pair of channels when stereo), the scale factor indices (6 bits
// where something is allocated), then twelve granules of one sample per allocated band, converted with the multiplier table
// and handed to PolySynth with the 32 (mono) or 64 (stereo) fractions; the output moves on by mChannels32 shorts a granule.
int32_t CMpegBase::Decode_Layer1(int16_t* out) {
    float fraction[2][32];
    if (mChannels == 2) {
        for (int band = 0; band < 0x20; ++band) {
            uint32_t pair = GetBits(8);
            gMpegLayer1Alloc[band * 2] = static_cast<uint32_t>(static_cast<int32_t>(pair) >> 4);
            gMpegLayer1Alloc[band * 2 + 1] = pair & 0xf;
        }
        int32_t* scale = gMpegLayer1Scale;
        for (int band = 0; band < 0x20; ++band) {
            if (gMpegLayer1Alloc[band * 2] != 0) *scale++ = static_cast<int32_t>(GetBits(6));
            if (gMpegLayer1Alloc[band * 2 + 1] != 0) *scale++ = static_cast<int32_t>(GetBits(6));
        }
    } else {
        for (int band = 0; band < 0x20; ++band) gMpegLayer1Alloc[band] = GetBits(4);
        int32_t* scale = gMpegLayer1Scale;
        for (int band = 0; band < 0x20; ++band)
            if (gMpegLayer1Alloc[band] != 0) *scale++ = static_cast<int32_t>(GetBits(6));
    }
    for (int granule = 0; granule < 12; ++granule) {
        if (mChannels == 2) {
            int32_t* sample = gMpegLayer1Sample;
            for (int band = 0; band < 0x20; ++band) {
                if (gMpegLayer1Alloc[band * 2] != 0) *sample++ = static_cast<int32_t>(GetBits(gMpegLayer1Alloc[band * 2] + 1));
                if (gMpegLayer1Alloc[band * 2 + 1] != 0)
                    *sample++ = static_cast<int32_t>(GetBits(gMpegLayer1Alloc[band * 2 + 1] + 1));
            }
            sample = gMpegLayer1Sample;
            const int32_t* scale = gMpegLayer1Scale;
            for (int band = 0; band < 0x20; ++band) {
                for (int channel = 0; channel < 2; ++channel) {
                    uint32_t allocation = gMpegLayer1Alloc[band * 2 + channel];
                    if (allocation == 0) {
                        fraction[channel][band] = 0.0f;
                    } else {
                        int32_t value = Add(Add(static_cast<int32_t>(ppc_slw(0xffffffffu, static_cast<int32_t>(allocation))), *sample++), 1);
                        fraction[channel][band] = static_cast<float>(value) * MpegMultiplier(static_cast<int32_t>(allocation) + 1, *scale++);
                    }
                }
            }
        } else {
            int32_t* sample = gMpegLayer1Sample;
            for (int band = 0; band < 0x20; ++band)
                if (gMpegLayer1Alloc[band] != 0) *sample++ = static_cast<int32_t>(GetBits(gMpegLayer1Alloc[band] + 1));
            sample = gMpegLayer1Sample;
            const int32_t* scale = gMpegLayer1Scale;
            for (int band = 0; band < 0x20; ++band) {
                uint32_t allocation = gMpegLayer1Alloc[band];
                if (allocation == 0) {
                    fraction[0][band] = 0.0f;
                } else {
                    int32_t value = Add(Add(static_cast<int32_t>(ppc_slw(0xffffffffu, static_cast<int32_t>(allocation))), *sample++), 1);
                    fraction[0][band] = static_cast<float>(value) * MpegMultiplier(static_cast<int32_t>(allocation) + 1, *scale++);
                }
            }
        }
        PolySynth(out, &fraction[0][0]);
        out += mChannels32;
    }
    return 0;
}

// The sound library's MPEG audio decoder, part 2 (unit w71b): CMpegBase's synthesis and layer II (program "sound symbols
// shared", StreamableMpeg2.cpp): PolySynth (0x10006c38), dct64 (0x10005adc), WRITE_SAMPLE (0x10006f88), Decode_Layer2
// (0x10005698), II_step_two (0x1000483c) and II_step_twoMMX (0x10004ee0), with the static tables they read. mpglib's
// synth_1to1, dct64, II_step_one (inlined into Decode_Layer2) and II_step_two. The float code keeps the listing's order of
// operations; the fused multiply-adds the listing has (fmadds, fnmsubs in PolySynth) go through ppc_math.h. dct64 has none.
// Part 1 (the object, the bit reader, layer I) is src/TbMpegBase.cpp; the layout is in include/TbMpegBase.h.

// Decode_Layer2's file-level scratch (0x1003e688 up to 0x1003eac8) and dct64's work array (0x1003eac8).
float gMpegL2Fraction[3][2][32];
uint8_t gMpegL2Scale[0xc0];
int8_t gMpegL2Alloc[0x40];
int8_t gMpegL2Select[0x40];
float gMpegDctBuf[64];

namespace {

// ---------------------------------------------------------------- the static tables

const int32_t kIntWinBase[257] = {
    0, -1, -1, -1, -1, -1, -1, -2, -2, -2, -2, -3, -3, -4, -4, -5,
    -5, -6, -7, -7, -8, -9, -10, -11, -13, -14, -16, -17, -19, -21, -24, -26,
    -29, -31, -35, -38, -41, -45, -49, -53, -58, -63, -68, -73, -79, -85, -91, -97,
    -104, -111, -117, -125, -132, -139, -147, -154, -161, -169, -176, -183, -190, -196, -202, -208,
    -213, -218, -222, -225, -227, -228, -228, -227, -224, -221, -215, -208, -200, -189, -177, -163,
    -146, -127, -106, -83, -57, -29, 2, 36, 72, 111, 153, 197, 244, 294, 347, 401,
    459, 519, 581, 645, 711, 779, 848, 919, 991, 1064, 1137, 1210, 1283, 1356, 1428, 1498,
    1567, 1634, 1698, 1759, 1817, 1870, 1919, 1962, 2001, 2032, 2057, 2075, 2085, 2087, 2080, 2063,
    2037, 2000, 1952, 1893, 1822, 1739, 1644, 1535, 1414, 1280, 1131, 970, 794, 605, 402, 185,
    -45, -288, -545, -814, -1095, -1388, -1692, -2006, -2330, -2663, -3004, -3351, -3705, -4063, -4425, -4788,
    -5153, -5517, -5879, -6237, -6589, -6935, -7271, -7597, -7910, -8209, -8491, -8755, -8998, -9219, -9416, -9585,
    -9727, -9838, -9916, -9959, -9966, -9935, -9863, -9750, -9592, -9389, -9139, -8840, -8492, -8092, -7640, -7134,
    -6574, -5959, -5288, -4561, -3776, -2935, -2037, -1082, -70, 998, 2122, 3300, 4533, 5818, 7154, 8540,
    9975, 11455, 12980, 14548, 16155, 17799, 19478, 21189, 22929, 24694, 26482, 28289, 30112, 31947, 33791, 35640,
    37489, 39336, 41176, 43006, 44821, 46617, 48390, 50137, 51853, 53534, 55178, 56778, 58333, 59838, 61289, 62684,
    64019, 65290, 66494, 67629, 68692, 69679, 70590, 71420, 72169, 72835, 73415, 73908, 74313, 74630, 74856, 74992,
    75038};

// The synthesis window, decwin[512 + 32] at 0x1003b488: mpglib's make_decode_tables fills it from intwinbase (a table of
// 257 integers) with the walk below, each value stored twice 16 floats apart; the scale is -0.5 and the sign flips every
// 64 rows. The test compares all 544 words with the Mac's.
struct DecwinTable {
    float value[544];
    DecwinTable() {
        std::memset(value, 0, sizeof(value));
        int32_t index = 0;
        int32_t j = 0;
        double sign = 0.5;
        for (int32_t i = 0; i < 512; ++i) {
            if (index < 512 + 16) {
                // (-intwinbase) as an integer first, so that intwinbase[0] = 0 gives +0.0 as the Mac's table holds
                float v = static_cast<float>(static_cast<double>(-kIntWinBase[j]) * sign);
                value[index] = v;
                if (index + 16 < 544) value[index + 16] = v;
            }
            if (i < 256) ++j;
            else --j;
            index += 32;
            if (i % 32 == 31) index -= 1023;
            if (i % 64 == 63) sign = -sign;
        }
    }
};

const DecwinTable& Decwin() {
    static const DecwinTable table;
    return table;
}

// dct64's cosine constants (pnts[k] of mpglib, 1 / (2 cos((2 i + 1) pi / n)) as floats), read from the Mac's data.
const uint32_t kCosineBits[31] = {
    0x3f002785, 0x3f01668b, 0x3f03f45b, 0x3f07f268,
    0x3f0d9838, 0x3f153b3a, 0x3f1f5c6e, 0x3f2cc03d,
    0x3f3e99ee, 0x3f56df9e, 0x3f78fa3b, 0x3f95b035,
    0x3fbdf91b, 0x4003b2af, 0x405a1642, 0x41230a46,
    0x3f009e8d, 0x3f05c278, 0x3f11233f, 0x3f25961d,
    0x3f49c480, 0x3f87c449, 0x3fdc7926, 0x40a33c9c,
    0x3f0281f7, 0x3f19f1bd, 0x3f6664d7, 0x402406cf,
    0x3f0a8bd4, 0x3fa73d75, 0x3f3504f3};

struct CosineTables {
    float c0[16];
    float c1[8];
    float c2[4];
    float c3[2];
    float c4;
    CosineTables() {
        float all[31];
        for (int i = 0; i < 31; ++i) std::memcpy(&all[i], &kCosineBits[i], 4);
        std::memcpy(c0, all, sizeof(c0));
        std::memcpy(c1, all + 16, sizeof(c1));
        std::memcpy(c2, all + 24, sizeof(c2));
        std::memcpy(c3, all + 28, sizeof(c3));
        c4 = all[30];
    }
};

const CosineTables& Cosines() {
    static const CosineTables table;
    return table;
}

// mpglib's group tables (grp_3tab, grp_5tab, grp_9tab; 0x100394c0, 0x10039520, 0x100396a0): for a code of three samples in
// base 3, 5 or 9, the three rows of the multiplier table, base[i][digit] for the digits from the last to the first. The Mac's
// tables are followed by zero bytes up to the next data (the codes of 5, 7 and 10 bits can exceed 27, 125 and 729), so each
// array here is padded with zeros to the largest code (32, 128 and 1024 entries): a code past the table reads row 0 (zero).
struct GroupTables {
    uint8_t g3[32 * 3];
    uint8_t g5[128 * 3];
    uint8_t g9[1024 * 3];
    GroupTables() {
        std::memset(this, 0, sizeof(*this));
        static const uint8_t kBase3[3] = {1, 0, 2};
        static const uint8_t kBase5[5] = {17, 18, 0, 19, 20};
        static const uint8_t kBase9[9] = {21, 1, 22, 23, 0, 24, 25, 2, 26};
        Fill(g3, kBase3, 3);
        Fill(g5, kBase5, 5);
        Fill(g9, kBase9, 9);
    }
    static void Fill(uint8_t* out, const uint8_t* base, int len) {
        for (int j = 0; j < len; ++j)
            for (int k = 0; k < len; ++k)
                for (int l = 0; l < len; ++l) {
                    *out++ = base[l];
                    *out++ = base[k];
                    *out++ = base[j];
                }
    }
};

const GroupTables& Groups() {
    static const GroupTables tables;
    return tables;
}

// The layer II allocation tables alloc_0 .. alloc_4 (0x1003a2c8, 0x1003a748, 0x1003abf8, 0x1003ad38, 0x1003aef8): one run per
// subband, {the allocation code's bits, 0} first, then 1 << bits entries {sample bits, d}. 27, 30, 8, 12 and 30 subbands.
const al_table kAlloc0[] = {
    {4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},   // band 0
    {4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},   // band 1
    {4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},   // band 2
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 3
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 4
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 5
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 6
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 7
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 8
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 9
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 10
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 11
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 12
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 13
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 14
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 15
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 16
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 17
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 18
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 19
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 20
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 21
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 22
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 23
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 24
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 25
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 26
};
const al_table kAlloc1[] = {
    {4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},   // band 0
    {4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},   // band 1
    {4, 0}, {5, 3}, {3, -3}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383}, {16, -32767},   // band 2
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 3
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 4
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 5
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 6
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 7
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 8
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 9
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {16, -32767},   // band 10
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 11
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 12
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 13
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 14
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 15
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 16
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 17
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 18
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 19
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 20
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 21
    {3, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {16, -32767},   // band 22
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 23
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 24
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 25
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 26
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 27
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 28
    {2, 0}, {5, 3}, {7, 5}, {16, -32767},   // band 29
};
const al_table kAlloc2[] = {
    {4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},   // band 0
    {4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},   // band 1
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 2
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 3
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 4
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 5
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 6
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 7
};
const al_table kAlloc3[] = {
    {4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},   // band 0
    {4, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191}, {15, -16383},   // band 1
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 2
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 3
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 4
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 5
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 6
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 7
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 8
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 9
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 10
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 11
};
const al_table kAlloc4[] = {
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},   // band 0
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},   // band 1
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},   // band 2
    {4, 0}, {5, 3}, {7, 5}, {3, -3}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63}, {8, -127}, {9, -255}, {10, -511}, {11, -1023}, {12, -2047}, {13, -4095}, {14, -8191},   // band 3
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 4
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 5
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 6
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 7
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 8
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 9
    {3, 0}, {5, 3}, {7, 5}, {10, 9}, {4, -7}, {5, -15}, {6, -31}, {7, -63},   // band 10
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 11
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 12
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 13
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 14
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 15
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 16
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 17
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 18
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 19
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 20
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 21
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 22
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 23
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 24
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 25
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 26
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 27
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 28
    {2, 0}, {5, 3}, {7, 5}, {10, 9},   // band 29
};

const al_table* const kAllocTables[5] = {kAlloc0, kAlloc1, kAlloc2, kAlloc3, kAlloc4};   // 0x1003b208
const int32_t kAllocEntries[5] = {sizeof(kAlloc0) / 4, sizeof(kAlloc1) / 4, sizeof(kAlloc2) / 4, sizeof(kAlloc3) / 4,
                                  sizeof(kAlloc4) / 4};

// 0x1003b21c: [sample rate index 3][2 - channels][bit rate index 16] -> the allocation table to use, and 0x1003b27c: the
// number of subbands of each (27, 30, 8, 12, 30; an lsf stream uses 4). The Mac's table is followed by the sblimit
// halfwords, so a sample rate index of 3 or more (not an lsf stream) would read those bytes: not reproduced (reads 0).
const uint8_t kTranslate[0x60] = {
    0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 1, 1, 1, 1, 1, 0,
    0, 2, 2, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 3, 3, 3, 3, 3, 0, 0, 0, 1, 1, 1, 1, 1, 0,
    0, 3, 3, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0};
const int16_t kSbLimits[5] = {27, 30, 8, 12, 30};

}  // namespace

const float* MpegDecwin() { return Decwin().value; }

float MpegDct64Cosine(int32_t index) {
    const CosineTables& t = Cosines();
    if (index < 16) return t.c0[index];
    if (index < 24) return t.c1[index - 16];
    if (index < 28) return t.c2[index - 24];
    if (index < 30) return t.c3[index - 28];
    return t.c4;
}

const al_table* MpegAllocTable(int32_t index) { return kAllocTables[index]; }
int32_t MpegAllocTableEntries(int32_t index) { return kAllocEntries[index]; }

const uint8_t* MpegGroupTable(int32_t d) {
    if (d == 3) return Groups().g3;
    if (d == 5) return Groups().g5;
    if (d == 9) return Groups().g9;
    return nullptr;   // the Mac's pointer table holds 0 for the other steps; layer II tables only use 3, 5 and 9
}

const uint8_t* MpegLayer2Translate() { return kTranslate; }
int32_t MpegLayer2SbLimit(int32_t index) { return kSbLimits[index]; }

// II_step_two (float version): for each subband below mSbLimit and each channel, the three samples of the three
// sub-granules (fraction[g][channel][band]) from the allocation code in balloc: no code is three zeros; a negative step d is
// three samples of `bits` bits, each (code + d) times the multiplier of (bits, scale factor); a positive step 3, 5 or 9 is one
// code of `bits` bits giving three rows of the multiplier table through the group table. The scale factor of a coded band is
// scale[x1] (three a band; x1 picks the third of the frame); the pointers move by three for each coded band. The bands from
// mSbLimit up are zeroed (every channel the object has).
void CMpegBase::II_step_two(char* balloc, float (*fraction)[2][32], char* scale, int32_t x1, const al_table* alloc) {
    float* base = &fraction[0][0][0];
    float* band = base;
    for (int32_t sb = 0; sb < mSbLimit; ++sb) {
        int16_t step = alloc->bits;
        const int8_t* factor = reinterpret_cast<const int8_t*>(scale) + x1;
        float* slot = band;
        for (int32_t channel = 0; channel < mChannels; ++channel) {
            int8_t code = static_cast<int8_t>(*balloc++);
            if (code == 0) {
                slot[0x80] = 0.0f;
                slot[0x40] = 0.0f;
                slot[0] = 0.0f;
            } else {
                const al_table* chosen = alloc + code;
                int32_t d = chosen->d;
                int32_t bits = chosen->bits;
                if (d < 0) {
                    float multiplier = MpegMultiplier(bits, *factor);
                    int32_t value = static_cast<int32_t>(GetBits(bits));
                    slot[0] = multiplier * static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(d) + static_cast<uint32_t>(value)));
                    value = static_cast<int32_t>(GetBits(bits));
                    slot[0x40] = multiplier * static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(d) + static_cast<uint32_t>(value)));
                    value = static_cast<int32_t>(GetBits(bits));
                    slot[0x80] = multiplier * static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(d) + static_cast<uint32_t>(value)));
                } else {
                    int32_t factorValue = *factor;
                    uint32_t group = GetBits(bits);
                    const uint8_t* rows = MpegGroupTable(d) + group * 3;
                    slot[0] = MpegMultiplier(rows[0], factorValue);
                    slot[0x40] = MpegMultiplier(rows[1], factorValue);
                    slot[0x80] = MpegMultiplier(rows[2], factorValue);
                }
                scale += 3;
                factor += 3;
            }
            slot += 32;
        }
        ++band;
        alloc += ppc_slw(1, step);
    }
    // the bands from mSbLimit up are zero, in every sub-granule (the first channel, then the second when there are two)
    if (static_cast<uint32_t>(mSbLimit) <= 0x1f) {
        for (int32_t sb = mSbLimit; sb <= 0x1f; ++sb) {
            base[0x80 + sb] = 0.0f;   // (the Mac stores the sub-granules from the last to the first)
            base[0x40 + sb] = 0.0f;
            base[sb] = 0.0f;
        }
    }
    if (mChannels == 2 && static_cast<uint32_t>(mSbLimit) <= 0x1f) {
        for (int32_t sb = mSbLimit; sb <= 0x1f; ++sb) {
            base[0xa0 + sb] = 0.0f;
            base[0x60 + sb] = 0.0f;
            base[0x20 + sb] = 0.0f;
        }
    }
}

// II_step_twoMMX: the integer version (unused on the Mac: using_MMX stays 0). The multipliers are integers in mulstab
// (mulstab[row * 64 + scale factor], 12 bit fractions) and the samples are int16: sample = (multiplier * (code + d)) >> 12 or
// mulstab[...] >> 12 for a group. Both channels of a subband are done together (the second only when there are two, reading
// the next allocation code), and the scale factor is read at scale[0] (no x1: the caller offsets the pointer).
void CMpegBase::II_step_twoMMX(int32_t* mulstab, char* balloc, int16_t (*fraction)[2][32], char* scale, const al_table* alloc) {
    int16_t* base = &fraction[0][0][0];
    int16_t* band = base;
    for (int32_t sb = 0; sb < mSbLimit; ++sb) {
        int8_t code = static_cast<int8_t>(*balloc++);
        int16_t step = alloc->bits;
        for (int32_t channel = 0; channel < 2; ++channel) {
            // channel 1 only when the object has two channels (otherwise it is skipped, its slots are untouched)
            if (channel == 1) {
                if (mChannels != 2) break;
                code = static_cast<int8_t>(*balloc++);
            }
            int16_t* slot = band + 32 * channel;
            if (code == 0) {
                slot[0x80] = 0;
                slot[0x40] = 0;
                slot[0] = 0;
            } else {
                const al_table* chosen = alloc + code;
                int32_t d = chosen->d;
                int32_t bits = chosen->bits;
                if (d < 0) {
                    int32_t multiplier = mulstab[static_cast<int8_t>(*scale) + bits * 0x40];
                    for (int32_t g = 0; g < 3; ++g) {
                        int32_t value = static_cast<int32_t>(GetBits(bits));
                        int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(d) + static_cast<uint32_t>(value));
                        int32_t product = static_cast<int32_t>(static_cast<uint32_t>(multiplier) * static_cast<uint32_t>(sum));
                        slot[0x40 * g] = static_cast<int16_t>(product >> 12);
                    }
                } else {
                    int32_t factorValue = static_cast<int8_t>(*scale);
                    uint32_t group = GetBits(bits);
                    const uint8_t* rows = MpegGroupTable(d) + group * 3;
                    slot[0] = static_cast<int16_t>(mulstab[factorValue + rows[0] * 0x40] >> 12);
                    slot[0x40] = static_cast<int16_t>(mulstab[factorValue + rows[1] * 0x40] >> 12);
                    slot[0x80] = static_cast<int16_t>(mulstab[factorValue + rows[2] * 0x40] >> 12);
                }
                scale += 3;
            }
        }
        ++band;
        alloc += ppc_slw(1, step);
    }
    if (static_cast<uint32_t>(mSbLimit) <= 0x1f) {
        for (int32_t sb = mSbLimit; sb <= 0x1f; ++sb) {
            base[0x80 + sb] = 0;
            base[0x40 + sb] = 0;
            base[sb] = 0;
        }
    }
    if (mChannels == 2 && static_cast<uint32_t>(mSbLimit) <= 0x1f) {
        for (int32_t sb = mSbLimit; sb <= 0x1f; ++sb) {
            base[0xa0 + sb] = 0;
            base[0x60 + sb] = 0;
            base[0x20 + sb] = 0;
        }
    }
}
// ---------------------------------------------------------------- layer II

// Decode_Layer2: II_step_one inlined. The table is chosen by the header (an lsf stream always uses the fifth), then the
// allocation codes (the two channels of a subband one after the other, each with the bit count the subband's table gives),
// the scale factor selectors (2 bits for each allocated subband and channel) and the scale factors (3 a coded subband and
// channel, 6 bits each or shared by the selector); then 12 times II_step_two for a third of a scale factor set and three
// PolySynth calls (three sub-granules of 32 samples a channel) with the fractions it made. There is no joint stereo handling
// here: both channels are coded in every subband. The output moves by mChannels32 shorts for each PolySynth call (not at all
// when out is NULL).
int32_t CMpegBase::Decode_Layer2(int16_t* out) {
    int32_t index;
    if (mLsf != 0) {
        index = 4;
    } else {
        int32_t at = static_cast<int32_t>(static_cast<uint32_t>(mSampleRateIndex) << 5) + ((2 - mChannels) << 4) + mBitRateIndex;
        index = (at >= 0 && at < 0x60) ? static_cast<int8_t>(kTranslate[at]) : 0;   // (the Mac reads the bytes after the table)
    }
    mSbLimit = kSbLimits[index];
    int32_t sbLimit = mSbLimit;
    const al_table* alloc = kAllocTables[index];
    int32_t total = sbLimit * mChannels;
    const al_table* entry = alloc;
    int8_t* allocated = gMpegL2Alloc;
    if (mChannels == 2) {
        for (int32_t band = sbLimit; band != 0; --band) {
            int32_t bits = entry->bits;
            *allocated++ = static_cast<int8_t>(GetBits(bits));
            *allocated++ = static_cast<int8_t>(GetBits(bits));
            entry += ppc_slw(1, bits);
        }
        const int8_t* source = gMpegL2Alloc;
        int8_t* selector = gMpegL2Select;
        for (int32_t n = total; n != 0; --n) {
            int8_t code = *source++;
            if (code != 0) *selector++ = static_cast<int8_t>(GetBits(2));
        }
    } else {
        for (int32_t band = sbLimit; band != 0; --band) {
            int32_t bits = entry->bits;
            *allocated++ = static_cast<int8_t>(GetBits(bits));
            entry += ppc_slw(1, bits);
        }
        const int8_t* source = gMpegL2Alloc;
        int8_t* selector = gMpegL2Select;
        for (int32_t n = mSbLimit; n != 0; --n) {
            int8_t code = *source++;
            if (code != 0) *selector++ = static_cast<int8_t>(GetBits(2));
        }
    }
    // the scale factors: selector 0 reads three (6 bits each), 1 two (the first shared by the first two bands), 2 one, 3 two
    // (the second shared by the last two)
    const int8_t* source = gMpegL2Alloc;
    const int8_t* selector = gMpegL2Select;
    uint8_t* scale = gMpegL2Scale;
    for (int32_t n = total; n != 0; --n) {
        int8_t code = *source++;
        if (code == 0) continue;
        int8_t select = *selector++;
        uint32_t bits;
        if (select == 1) {
            bits = GetBits(12);
            uint8_t first = (bits >> 6) & 0x3f;
            scale[0] = first;
            scale[1] = first;
            scale[2] = bits & 0x3f;
        } else if (select < 1 && select >= 0) {
            bits = GetBits(0x12);
            scale[0] = (bits >> 12) & 0x3f;
            scale[1] = (bits >> 6) & 0x3f;
            scale[2] = bits & 0x3f;
        } else if (select == 2) {
            uint8_t same = static_cast<uint8_t>(GetBits(6));
            scale[0] = same;
            scale[1] = same;
            scale[2] = same;
        } else {
            bits = GetBits(12);
            uint8_t last = bits & 0x3f;
            scale[0] = (bits >> 6) & 0x3f;
            scale[1] = last;
            scale[2] = last;
        }
        scale += 3;
    }
    int32_t step = out == nullptr ? 0 : mChannels32;
    for (int32_t granule = 0; granule < 12; ++granule) {
        II_step_two(reinterpret_cast<char*>(gMpegL2Alloc), gMpegL2Fraction, reinterpret_cast<char*>(gMpegL2Scale), granule >> 2, alloc);
        PolySynth(out, &gMpegL2Fraction[0][0][0]);
        out += step;
        PolySynth(out, &gMpegL2Fraction[1][0][0]);
        out += step;
        PolySynth(out, &gMpegL2Fraction[2][0][0]);
        out += step;
    }
    return 0;
}

void CMpegBase::Init_Tables() { Close_Tables(); }

void CMpegBase::Close_Tables() {}

// ---------------------------------------------------------------- dct64

// mpglib's dct64: five butterfly stages over the 64 float work array gMpegDctBuf (stage 1 reads the 32 samples and writes
// buf[0..31], stage 2 buf[32..63], and so on back and forth), the last one writing 32 outputs split between out0 and out1.
// The code is the listing's straight-line stores, one statement for each stfs, with the products by the stage's cosines.
void dct64(float* out0, float* out1, float* samples) {
    const CosineTables& cosines = Cosines();
    const float* kCos0 = cosines.c0;
    const float* kCos1 = cosines.c1;
    const float* kCos2 = cosines.c2;
    const float* kCos3 = cosines.c3;
    const float kCos4 = cosines.c4;
    float* buf = gMpegDctBuf;
    const float* in = samples;
    buf[0] = in[0] + in[31];
    buf[31] = kCos0[0] * (in[0] - in[31]);
    buf[1] = in[1] + in[30];
    buf[30] = kCos0[1] * (in[1] - in[30]);
    buf[2] = in[2] + in[29];
    buf[29] = kCos0[2] * (in[2] - in[29]);
    buf[3] = in[3] + in[28];
    buf[28] = kCos0[3] * (in[3] - in[28]);
    buf[4] = in[4] + in[27];
    buf[27] = kCos0[4] * (in[4] - in[27]);
    buf[5] = in[5] + in[26];
    buf[26] = kCos0[5] * (in[5] - in[26]);
    buf[6] = in[6] + in[25];
    buf[25] = kCos0[6] * (in[6] - in[25]);
    buf[7] = in[7] + in[24];
    buf[24] = kCos0[7] * (in[7] - in[24]);
    buf[8] = in[8] + in[23];
    buf[23] = kCos0[8] * (in[8] - in[23]);
    buf[9] = in[9] + in[22];
    buf[22] = kCos0[9] * (in[9] - in[22]);
    buf[10] = in[10] + in[21];
    buf[21] = kCos0[10] * (in[10] - in[21]);
    buf[11] = in[11] + in[20];
    buf[20] = kCos0[11] * (in[11] - in[20]);
    buf[12] = in[12] + in[19];
    buf[19] = kCos0[12] * (in[12] - in[19]);
    buf[13] = in[13] + in[18];
    buf[18] = kCos0[13] * (in[13] - in[18]);
    buf[14] = in[14] + in[17];
    buf[17] = kCos0[14] * (in[14] - in[17]);
    buf[15] = in[15] + in[16];
    buf[16] = kCos0[15] * (in[15] - in[16]);
    buf[32] = buf[0] + buf[15];
    buf[47] = kCos1[0] * (buf[0] - buf[15]);
    buf[33] = buf[1] + buf[14];
    buf[46] = kCos1[1] * (buf[1] - buf[14]);
    buf[34] = buf[2] + buf[13];
    buf[45] = kCos1[2] * (buf[2] - buf[13]);
    buf[35] = buf[3] + buf[12];
    buf[44] = kCos1[3] * (buf[3] - buf[12]);
    buf[36] = buf[4] + buf[11];
    buf[43] = kCos1[4] * (buf[4] - buf[11]);
    buf[37] = buf[5] + buf[10];
    buf[42] = kCos1[5] * (buf[5] - buf[10]);
    buf[38] = buf[6] + buf[9];
    buf[41] = kCos1[6] * (buf[6] - buf[9]);
    buf[39] = buf[7] + buf[8];
    buf[40] = kCos1[7] * (buf[7] - buf[8]);
    buf[48] = buf[16] + buf[31];
    buf[63] = kCos1[0] * (buf[31] - buf[16]);
    buf[49] = buf[17] + buf[30];
    buf[62] = kCos1[1] * (buf[30] - buf[17]);
    buf[50] = buf[18] + buf[29];
    buf[61] = kCos1[2] * (buf[29] - buf[18]);
    buf[51] = buf[19] + buf[28];
    buf[60] = kCos1[3] * (buf[28] - buf[19]);
    buf[52] = buf[20] + buf[27];
    buf[59] = kCos1[4] * (buf[27] - buf[20]);
    buf[53] = buf[21] + buf[26];
    buf[58] = kCos1[5] * (buf[26] - buf[21]);
    buf[54] = buf[22] + buf[25];
    buf[57] = kCos1[6] * (buf[25] - buf[22]);
    buf[55] = buf[23] + buf[24];
    buf[56] = kCos1[7] * (buf[24] - buf[23]);
    buf[0] = buf[32] + buf[39];
    buf[7] = kCos2[0] * (buf[32] - buf[39]);
    buf[1] = buf[33] + buf[38];
    buf[6] = kCos2[1] * (buf[33] - buf[38]);
    buf[2] = buf[34] + buf[37];
    buf[5] = kCos2[2] * (buf[34] - buf[37]);
    buf[3] = buf[35] + buf[36];
    buf[4] = kCos2[3] * (buf[35] - buf[36]);
    buf[8] = buf[40] + buf[47];
    buf[15] = kCos2[0] * (buf[47] - buf[40]);
    buf[9] = buf[41] + buf[46];
    buf[14] = kCos2[1] * (buf[46] - buf[41]);
    buf[10] = buf[42] + buf[45];
    buf[13] = kCos2[2] * (buf[45] - buf[42]);
    buf[11] = buf[43] + buf[44];
    buf[12] = kCos2[3] * (buf[44] - buf[43]);
    buf[16] = buf[48] + buf[55];
    buf[23] = kCos2[0] * (buf[48] - buf[55]);
    buf[17] = buf[49] + buf[54];
    buf[22] = kCos2[1] * (buf[49] - buf[54]);
    buf[18] = buf[50] + buf[53];
    buf[21] = kCos2[2] * (buf[50] - buf[53]);
    buf[19] = buf[51] + buf[52];
    buf[20] = kCos2[3] * (buf[51] - buf[52]);
    buf[24] = buf[56] + buf[63];
    buf[31] = kCos2[0] * (buf[63] - buf[56]);
    buf[25] = buf[57] + buf[62];
    buf[30] = kCos2[1] * (buf[62] - buf[57]);
    buf[26] = buf[58] + buf[61];
    buf[29] = kCos2[2] * (buf[61] - buf[58]);
    buf[27] = buf[59] + buf[60];
    buf[28] = kCos2[3] * (buf[60] - buf[59]);
    buf[32] = buf[0] + buf[3];
    buf[35] = kCos3[0] * (buf[0] - buf[3]);
    buf[33] = buf[1] + buf[2];
    buf[34] = kCos3[1] * (buf[1] - buf[2]);
    buf[36] = buf[4] + buf[7];
    buf[39] = kCos3[0] * (buf[7] - buf[4]);
    buf[37] = buf[5] + buf[6];
    buf[38] = kCos3[1] * (buf[6] - buf[5]);
    buf[40] = buf[8] + buf[11];
    buf[43] = kCos3[0] * (buf[8] - buf[11]);
    buf[41] = buf[9] + buf[10];
    buf[42] = kCos3[1] * (buf[9] - buf[10]);
    buf[44] = buf[12] + buf[15];
    buf[47] = kCos3[0] * (buf[15] - buf[12]);
    buf[45] = buf[13] + buf[14];
    buf[46] = kCos3[1] * (buf[14] - buf[13]);
    buf[48] = buf[16] + buf[19];
    buf[51] = kCos3[0] * (buf[16] - buf[19]);
    buf[49] = buf[17] + buf[18];
    buf[50] = kCos3[1] * (buf[17] - buf[18]);
    buf[52] = buf[20] + buf[23];
    buf[55] = kCos3[0] * (buf[23] - buf[20]);
    buf[53] = buf[21] + buf[22];
    buf[54] = kCos3[1] * (buf[22] - buf[21]);
    buf[56] = buf[24] + buf[27];
    buf[59] = kCos3[0] * (buf[24] - buf[27]);
    buf[57] = buf[25] + buf[26];
    buf[58] = kCos3[1] * (buf[25] - buf[26]);
    buf[60] = buf[28] + buf[31];
    buf[63] = kCos3[0] * (buf[31] - buf[28]);
    buf[61] = buf[29] + buf[30];
    buf[62] = kCos3[1] * (buf[30] - buf[29]);
    buf[0] = buf[32] + buf[33];
    buf[1] = kCos4 * (buf[32] - buf[33]);
    buf[2] = buf[34] + buf[35];
    buf[3] = kCos4 * (buf[35] - buf[34]);
    buf[2] = buf[2] + buf[3];
    buf[4] = buf[36] + buf[37];
    buf[5] = kCos4 * (buf[36] - buf[37]);
    buf[6] = buf[38] + buf[39];
    buf[7] = kCos4 * (buf[39] - buf[38]);
    buf[6] = buf[6] + buf[7];
    buf[4] = buf[4] + buf[6];
    buf[6] = buf[6] + buf[5];
    buf[5] = buf[5] + buf[7];
    buf[8] = buf[40] + buf[41];
    buf[9] = kCos4 * (buf[40] - buf[41]);
    buf[10] = buf[42] + buf[43];
    buf[11] = kCos4 * (buf[43] - buf[42]);
    buf[10] = buf[10] + buf[11];
    buf[12] = buf[44] + buf[45];
    buf[13] = kCos4 * (buf[44] - buf[45]);
    buf[14] = buf[46] + buf[47];
    buf[15] = kCos4 * (buf[47] - buf[46]);
    buf[14] = buf[14] + buf[15];
    buf[12] = buf[12] + buf[14];
    buf[14] = buf[14] + buf[13];
    buf[13] = buf[13] + buf[15];
    buf[16] = buf[48] + buf[49];
    buf[17] = kCos4 * (buf[48] - buf[49]);
    buf[18] = buf[50] + buf[51];
    buf[19] = kCos4 * (buf[51] - buf[50]);
    buf[18] = buf[18] + buf[19];
    buf[20] = buf[52] + buf[53];
    buf[21] = kCos4 * (buf[52] - buf[53]);
    buf[22] = buf[54] + buf[55];
    buf[23] = kCos4 * (buf[55] - buf[54]);
    buf[22] = buf[22] + buf[23];
    buf[20] = buf[20] + buf[22];
    buf[22] = buf[22] + buf[21];
    buf[21] = buf[21] + buf[23];
    buf[24] = buf[56] + buf[57];
    buf[25] = kCos4 * (buf[56] - buf[57]);
    buf[26] = buf[58] + buf[59];
    buf[27] = kCos4 * (buf[59] - buf[58]);
    buf[26] = buf[26] + buf[27];
    buf[28] = buf[60] + buf[61];
    buf[29] = kCos4 * (buf[60] - buf[61]);
    buf[30] = buf[62] + buf[63];
    buf[31] = kCos4 * (buf[63] - buf[62]);
    buf[30] = buf[30] + buf[31];
    buf[28] = buf[28] + buf[30];
    buf[30] = buf[30] + buf[29];
    buf[29] = buf[29] + buf[31];
    out0[256] = buf[0];
    out0[192] = buf[4];
    out0[128] = buf[2];
    out0[64] = buf[6];
    out0[0] = buf[1];
    out1[0] = buf[1];
    out1[64] = buf[5];
    out1[128] = buf[3];
    out1[192] = buf[7];
    buf[8] = buf[8] + buf[12];
    out0[224] = buf[8];
    buf[12] = buf[12] + buf[10];
    out0[160] = buf[12];
    buf[10] = buf[10] + buf[14];
    out0[96] = buf[10];
    buf[14] = buf[14] + buf[9];
    out0[32] = buf[14];
    buf[9] = buf[9] + buf[13];
    out1[32] = buf[9];
    buf[13] = buf[13] + buf[11];
    out1[96] = buf[13];
    buf[11] = buf[11] + buf[15];
    out1[160] = buf[11];
    out1[224] = buf[15];
    buf[24] = buf[24] + buf[28];
    out0[240] = buf[16] + buf[24];
    out0[208] = buf[24] + buf[20];
    buf[28] = buf[28] + buf[26];
    out0[176] = buf[20] + buf[28];
    out0[144] = buf[28] + buf[18];
    buf[26] = buf[26] + buf[30];
    out0[112] = buf[18] + buf[26];
    out0[80] = buf[26] + buf[22];
    buf[30] = buf[30] + buf[25];
    out0[48] = buf[22] + buf[30];
    out0[16] = buf[30] + buf[17];
    buf[25] = buf[25] + buf[29];
    out1[16] = buf[17] + buf[25];
    out1[48] = buf[25] + buf[21];
    buf[29] = buf[29] + buf[27];
    out1[80] = buf[21] + buf[29];
    out1[112] = buf[29] + buf[19];
    buf[27] = buf[27] + buf[31];
    out1[144] = buf[19] + buf[27];
    out1[176] = buf[27] + buf[23];
    out1[208] = buf[23] + buf[31];
    out1[240] = buf[31];
}

// WRITE_SAMPLE: the sample is truncated (fctiwz) and clipped to the int16 range.
void WRITE_SAMPLE(int16_t* out, float sample) {
    int32_t value = ppc_fctiwz(static_cast<double>(sample));
    if (value > 0x7fff) {
        *out = 0x7fff;
        return;
    }
    if (value < -0x8000) {
        *out = static_cast<int16_t>(-0x8000);
        return;
    }
    *out = static_cast<int16_t>(value);
}

// ---------------------------------------------------------------- PolySynth

// mpglib's synth_1to1 for one granule of 32 samples a channel: the offset bo steps down by one (mod 16), dct64 fills the
// current bank and the previous one, and then (when there is somewhere to write) the 32 output samples of the channel come
// from the window and the previous bank: 16 sums of 16 products, one of 8, and 15 mirrored sums of 16, each clipped by
// WRITE_SAMPLE. The sums are the listing's chains of fmadds (the even terms after the first product) and fnmsubs (the odd
// ones; the last 15 samples subtract all 16). The samples of the channels interleave in the output, channels shorts apart.
void CMpegBase::PolySynth(int16_t* out, float* fraction) {
    int32_t channels = mChannels;
    mBo = static_cast<uint8_t>(mBo - 1);
    mBo = static_cast<uint8_t>(mBo & 0xf);
    uint32_t bank = mBo & 1u;
    uint32_t other = bank ^ 1u;
    int32_t bo1 = static_cast<int32_t>(mBo) + static_cast<int32_t>(other);
    const float* decwin = Decwin().value;
    for (int32_t channel = 0; channel < channels; ++channel) {
        float* current = mSynthBuf[channel][bank] + (((mBo + bank) * 4u & 0x3cu) / 4u);
        float* previous = mSynthBuf[channel][other];
        dct64(current, previous + bo1, fraction + 32 * channel);
        if (out == nullptr) continue;
        int16_t* sample = out + channel;
        const float* w = decwin + 16 - bo1;
        const float* b = previous;
        for (int32_t n = 0; n < 16; ++n) {
            float sum = w[0] * b[0];
            for (int32_t k = 2; k < 16; k += 2) sum = ppc_fmadds(w[k], b[k], sum);
            for (int32_t k = 1; k < 16; k += 2) sum = ppc_fnmsubs(w[k], b[k], sum);
            WRITE_SAMPLE(sample, sum);
            sample += channels;
            b += 16;
            w += 32;
        }
        float middle = w[0] * b[0];
        for (int32_t k = 2; k < 16; k += 2) middle = ppc_fmadds(w[k], b[k], middle);
        WRITE_SAMPLE(sample, middle);
        w += 2 * bo1 - 32;
        sample += channels;
        b -= 16;
        for (int32_t n = 0; n < 15; ++n) {
            float sum = -w[-1] * b[0];
            for (int32_t k = 1; k < 15; ++k) sum = ppc_fnmsubs(w[-1 - k], b[k], sum);
            sum = ppc_fnmsubs(w[0], b[15], sum);
            WRITE_SAMPLE(sample, sum);
            sample += channels;
            b -= 16;
            w -= 32;
        }
    }
}
