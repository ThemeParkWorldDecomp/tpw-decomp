#include "WinScrn.h"
#include "TbFPUPrecision.h"
#include "bfl_Refpack.h"
#include "platform/macdoze.h"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>

// ---- WinScrn.cpp: RegisterBullfrogWindowClass 0x1006a824 and _ScreenFPUHandler::~_ScreenFPUHandler 0x1006aa8c

uint8_t gRegisterBullfrogWindowClassGuard;   // 0x100750f4
int32_t gRegisterBullfrogWindowClassDone;    // 0x100750f8

void RegisterBullfrogWindowClass() {
    // A function-local `static int` with a compiler guard byte: the guard zeroes the value once.
    if (gRegisterBullfrogWindowClassGuard == 0) {
        gRegisterBullfrogWindowClassDone = 0;
        gRegisterBullfrogWindowClassGuard = 1;
    }
    if (gRegisterBullfrogWindowClassDone == 0) {
        gRegisterBullfrogWindowClassDone = 1;
        NS_MacDoze::tagWNDCLASSA wc{};
        NS_MacDoze::ZeroMemory(&wc, 0x28);
        wc.style = 3;
        wc.lpfnWndProc = WindowProc;   // the Mac stores the TVector at 0x10070e24
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = LbApp_GetHInstance();
        if (LbApp_GetWindowIcon() != 0) {
            const uint32_t icon = static_cast<uint16_t>(LbApp_GetWindowIcon());
            void* instance = LbApp_GetHInstance();
            wc.hIcon = NS_MacDoze::LoadIcon(instance, reinterpret_cast<const char*>(static_cast<uintptr_t>(icon)));
        }
        wc.hbrBackground = NS_MacDoze::GetStockObject(4);
        wc.lpszMenuName = nullptr;
        std::sprintf(gWinScrnClassName, "_BullfrogLibScreen");   // the string at 0x1006cc44
        wc.lpszClassName = gWinScrnClassName;
        NS_MacDoze::RegisterClass(&wc);
    }
}

// 0x10073a90 (the Mac's TbFPUFlag vtable); the contents are never read by the host.
const void* const kScreenFPUHandlerFlagVtable[2] = {nullptr, nullptr};

__attribute__((weak)) void ScreenFPUHandler_OperatorDelete(void* block) { ::operator delete(block); }

_ScreenFPUHandler* ScreenFPUHandler_Dtor(_ScreenFPUHandler* self, int16_t flag) {
    if (self != nullptr) {
        // The Mac tests this + 4 against NULL (never true for a real object); this == -4 is not modelled on the host.
        self->mVtable = kScreenFPUHandlerVtable;
        self->mVtable = kScreenFPUHandlerFlagVtable;
        reinterpret_cast<TbFPUFlag*>(&self->mVtable)->Reset();
        if (flag > 0) ScreenFPUHandler_OperatorDelete(self);
    }
    return self;
}

// ---- TbSystemInfo.cpp: __sinit_TbSystemInfo_cpp 0x10043d04

bullfrog::CPU gTbSystemInfoCPU;       // 0x100748ac
uint16_t gTbSystemInfoF1C[4];         // 0x100748c8

void __sinit_TbSystemInfo_cpp() {
    gTbSystemInfoCPU.detect();
    gTbSystemInfoF1C[0] = 0;
    gTbSystemInfoF1C[1] = 0;
    gTbSystemInfoF1C[2] = 0;
    gTbSystemInfoF1C[3] = 0;
}

// ---- LbDraw.cpp: LbRect_Contains 0x10029bcc, LbRect_Bounding 0x10029a04

bool LbRect_Contains(const TbRect* outer, const TbRect* inner) {
    bool a = false, b = false, c = false;
    if (outer->left <= inner->left) {
        if (outer->top <= inner->top) a = true;
    }
    if (a) {
        if (outer->right >= inner->right) b = true;
    }
    if (b) {
        if (outer->bottom >= inner->bottom) c = true;
    }
    return c;
}

void LbRect_Bounding(TbRect* rect, const TbRect* other) {
    rect->left = rect->left < other->left ? rect->left : other->left;
    rect->top = rect->top < other->top ? rect->top : other->top;
    rect->right = rect->right > other->right ? rect->right : other->right;
    rect->bottom = rect->bottom > other->bottom ? rect->bottom : other->bottom;
}

// ---- LbSCBack.cpp: bfl::compress::CRefpack::MemCompress 0x1000d53c

__attribute__((weak)) uint8_t* Refpack_NewArray(uint32_t size) { return new uint8_t[size]; }
__attribute__((weak)) void Refpack_DeleteArray(void* block) { delete[] static_cast<uint8_t*>(block); }

namespace {
// The 256 word table at 0x10071f10: the standard reflected CRC-16 table (polynomial 0xA001), generated here.
struct RefpackTable {
    uint32_t v[256];
    constexpr RefpackTable() : v() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xA001u : c >> 1;
            v[i] = c;
        }
    }
};
constexpr RefpackTable kRefpackTableGen;
const uint32_t* const kRefpackTable = kRefpackTableGen.v;

inline uint32_t RefpackHash(const uint8_t* p) {
    const uint32_t a = kRefpackTable[p[0]];
    const uint32_t b = (a >> 8) ^ kRefpackTable[(a ^ p[1]) & 0xff];
    return (b >> 8) ^ kRefpackTable[(b ^ p[2]) & 0xff];
}

// Writes the pending literals in blocks of up to 0x70 bytes (a multiple of 4); fewer than 4 stay pending.
inline void FlushLiterals(uint8_t*& out, const uint8_t*& lit, uint32_t& n) {
    while (n > 3) {
        uint32_t count = n & 0xfffffffcu;
        if (count > 0x70) count = 0x70;
        *out++ = static_cast<uint8_t>((count >> 2) + 0xdf);
        std::memcpy(out, lit, count);
        n -= count;
        lit += count;
        out += count;
    }
}
}  // namespace

int32_t bfl::compress::CRefpack::MemCompress(uint8_t* dst, uint8_t* src, uint32_t length, uint8_t* work) const {
    uint8_t* out = dst + 5;
    const uint8_t* lit = src;
    uint8_t* pos = src;
    uint8_t* allocated = nullptr;
    uint32_t n = 0;
    uint32_t remaining = length;
    dst[0] = 0x10;
    dst[1] = 0xfb;
    dst[2] = static_cast<uint8_t>(length >> 16);
    dst[3] = static_cast<uint8_t>(length >> 8);
    dst[4] = static_cast<uint8_t>(length);
    if (work == nullptr) {
        const uint32_t size = static_cast<uint32_t>(CompressWorkspaceSize());
        allocated = Refpack_NewArray(size);
        if (allocated == nullptr) return 0;
        work = allocated;
    }
    std::memset(work, 0xff, 0x40000);
    auto head = [&](uint32_t h) -> int32_t& { return *reinterpret_cast<int32_t*>(work + static_cast<size_t>(h) * 4); };
    auto link = [&](int32_t i) -> int32_t& {
        return *reinterpret_cast<int32_t*>(work + 0x40000 + ((static_cast<uint32_t>(i) * 4) & 0x7fffc));
    };
    while (remaining != 0) {
        uint32_t bestOffset = 0, bestType = 2, bestLength = 2;
        const uint32_t maxLength = remaining < 0x404 ? remaining : 0x404;
        const int32_t index = static_cast<int32_t>(pos - src);
        const uint32_t h = RefpackHash(pos);
        int32_t candidate = head(h);
        const int32_t limit = (index - 0x1ffff > 0) ? index - 0x1ffff : 0;
        while (candidate >= limit) {
            const uint8_t* cp = src + candidate;
            if (pos[bestLength] == cp[bestLength]) {
                const uint8_t* a = pos;
                const uint8_t* b = cp;
                uint32_t l = 0;
                while (l < maxLength) {
                    const uint8_t x = *a++;
                    const uint8_t y = *b++;
                    if (x != y) break;
                    ++l;
                }
                if (l > bestLength) {
                    const uint32_t offset = static_cast<uint32_t>((pos - 1) - cp);
                    uint32_t type;
                    if (offset < 0x400 && l <= 10) type = 2;
                    else if (offset < 0x4000 && l <= 0x43) type = 3;
                    else type = 4;
                    if (l - type + 4 > bestLength - bestType + 4) {
                        bestLength = l;
                        bestType = type;
                        bestOffset = offset;
                        if (l >= 0x404) break;
                    }
                }
            }
            candidate = link(candidate);
        }
        if (bestType >= bestLength) {
            // No match worth coding: the byte becomes a pending literal.
            ++n;
            --remaining;
            link(index) = head(h);
            head(h) = index;
            ++pos;
            continue;
        }
        FlushLiterals(out, lit, n);
        if (bestType == 2) {
            *out = static_cast<uint8_t>(((bestOffset >> 3) & 0x1fffffe0u) + (bestLength - 3) * 4 + n);
            out[1] = static_cast<uint8_t>(bestOffset);
            out += 2;
        } else if (bestType == 3) {
            *out = static_cast<uint8_t>(bestLength + 0x7c);
            out[1] = static_cast<uint8_t>((n << 6) + (bestOffset >> 8));
            out[2] = static_cast<uint8_t>(bestOffset);
            out += 3;
        } else {
            const uint32_t extra = bestLength - 5;
            *out = static_cast<uint8_t>(((bestOffset >> 12) & 0xffff0u) + ((extra >> 6) & 0x3fffffcu) + n + 0xc0);
            out[1] = static_cast<uint8_t>(bestOffset >> 8);
            out[2] = static_cast<uint8_t>(bestOffset);
            out[3] = static_cast<uint8_t>(extra);
            out += 4;
        }
        if (n != 0) {
            std::memcpy(out, lit, n);
            out += n;
            n = 0;
        }
        if (static_cast<int32_t>(bestLength) > 0) {
            // Every position the match covers goes into the hash chains.
            for (uint32_t k = 0; k < bestLength; ++k) {
                const int32_t i = static_cast<int32_t>(pos - src);
                const uint32_t hh = RefpackHash(pos);
                link(i) = head(hh);
                head(hh) = i;
                ++pos;
            }
        }
        lit = pos;
        remaining -= bestLength;
    }
    FlushLiterals(out, lit, n);
    *out++ = static_cast<uint8_t>(n + 0xfc);
    if (n != 0) {
        std::memcpy(out, lit, n);
        out += n;
    }
    if (allocated != nullptr) Refpack_DeleteArray(allocated);
    // The original answers the written size plus 5 again (the 5 header bytes are already in `out`): kept.
    return static_cast<int32_t>(out - dst) + 5;
}
