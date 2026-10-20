#include <cmath>
#include <cstdint>
#include <cstring>
#include "Debug.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// USwapInplace32 (lwz, stwbrx): the word where it is, byte-reversed. The long, unsigned long and float instances all do this.
inline void SwapWordAt(uint8_t* p) {
    uint8_t b[4];
    std::memcpy(b, p, 4);
    p[0] = b[3];
    p[1] = b[2];
    p[2] = b[1];
    p[3] = b[0];
}

}  // namespace

namespace SamsUtilities {

// 0x10079740 (480 bytes): swaps every word of `count` records of 0xe4 bytes: words at 0x00..0x30, floats 0x34..0x48,
// words 0x4c..0x8c, the nine words at 0x90 (USwapBlock32 x 9), then words 0xb4..0xe0. Every field is a 32-bit word.
void USwapBlockStruct(FLYBOY* records, int32_t count) {
    uint8_t* record = reinterpret_cast<uint8_t*>(records);
    while (count-- != 0) {
        for (int32_t offset = 0; offset < 0xe4; offset += 4) SwapWordAt(record + offset);
        record += 0xe4;
    }
}

}  // namespace SamsUtilities
