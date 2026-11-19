// host_fly_zone.cpp: where a flying ride's car starts and where a leaving car goes (include/FlyZone.h, EmployEric and FLY_Tick
// in src/game/rides/FlyingRide.cpp) on the host, without game data. Fly zones made as FLY_SetFlyZone makes them from made-up
// hoarding extents, and the game's first zone, with random words as CWorld::GetRand gives them (every 32 bit word, the
// edges of the product's wrap among them): the point along each edge is always on the edge, from its low end to one short
// of its high end, and it is the Mac's own point whenever labs(rand) * 100 does not wrap. The Mac's pick is checked to
// leave the edge for about half the words, as the High Flyer showed. Exits nonzero on the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "FlyZone.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

namespace {

// The Mac's labs on a 32 bit long (wraps for the most negative number), as FlyingRide.cpp's Rand and Rand_2.
int32_t Labs32(uint32_t value) {
    const int32_t v = static_cast<int32_t>(value);
    return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
}

// labs(rand) * 100 in 32 bits.
int32_t Product(uint32_t rand) { return static_cast<int32_t>(static_cast<uint32_t>(Labs32(rand)) * 100u); }

// The Mac's offset: m - (m / span) * span, signed.
int32_t MacOffset(int32_t m, int32_t span) { return m - (m / span) * span; }

struct Zone {
    int32_t b, c, d, a;   // the low and high x (B, D) and the low and high z (C, A)
};

// FLY_SetFlyZone from hoarding extents in cells.
Zone FromHoarding(uint32_t b, uint32_t c, uint32_t d, uint32_t a) {
    return {static_cast<int32_t>((b * 10u + 0x23u) * 300u), static_cast<int32_t>((c * 10u + 0x23u) * 300u),
            static_cast<int32_t>((d * 10u - 0x23u) * 300u), static_cast<int32_t>((a * 10u - 0x23u) * 300u)};
}

}  // namespace

static_assert(tpw::FlyZoneOffset(0, 7) == 0 && tpw::FlyZoneOffset(13, 7) == 6 && tpw::FlyZoneOffset(-1, 7) == 3,
              "the unsigned remainder");

int main() {
    using tpw::FlyZoneOffset;
    const std::vector<Zone> zones = {
        {0x11170, 0x7530, 0x2e630, 0x222e0},   // the game's first zone (FlyingRide.cpp's initial values)
        FromHoarding(2, 2, 126, 126),          // a whole 128 cell map
        FromHoarding(10, 40, 90, 60),          // a narrow park
        FromHoarding(0, 0, 8, 8),              // a tiny one
        FromHoarding(1, 1, 254, 254),          // a 256 cell map
    };
    std::vector<uint32_t> words = {0u, 1u, 2u, 99u, 100u, 101u, 21474835u, 21474836u, 21474837u, 21474838u, 42949672u, 42949673u,
                                   0x7fffffffu, 0x80000000u, 0x80000001u, 0xfffffffeu, 0xffffffffu, 0xfeb7dcccu, 0xfeb7dccbu};
    uint32_t x = 0x12345678u;
    for (int i = 0; i < 400000; ++i) words.push_back(x = x * 1664525u + 1013904223u);

    for (size_t z = 0; z < zones.size(); ++z) {
        const Zone& zone = zones[z];
        const int32_t spans[2] = {zone.d - zone.b, zone.a - zone.c};
        const int32_t lows[2] = {zone.b, zone.c};
        for (int edge = 0; edge < 2; ++edge) {
            const int32_t span = spans[edge], low = lows[edge];
            Check(span > 0, "zone " + std::to_string(z) + " has a positive span");
            long off = 0, macOff = 0, same = 0, fromMac = 0;
            for (const uint32_t w : words) {
                const int32_t m = Product(w);
                const int32_t port = FlyZoneOffset(m, span), mac = MacOffset(m, span);
                const int32_t point = low + port;
                if (port < 0 || port >= span || point < low || point >= low + span) ++off;
                if (mac < 0) ++macOff;
                if (m >= 0) {
                    ++fromMac;
                    if (port == mac) ++same;
                }
            }
            const std::string where = "zone " + std::to_string(z) + " edge " + std::to_string(edge);
            Check(off == 0, where + ": every point on the edge (" + std::to_string(off) + " off it)");
            Check(same == fromMac, where + ": the Mac's own point whenever the product is not negative (" +
                                       std::to_string(fromMac - same) + " differ)");
            const double share = static_cast<double>(macOff) / static_cast<double>(words.size());
            Check(share > 0.4 && share < 0.6, where + ": the Mac's pick leaves the edge for about half the words (" +
                                                  std::to_string(share) + ")");
        }
    }

    // The words at the wrap: 21474836 * 100 is the last product that fits; 21474837 * 100 wraps negative, and the
    // most negative word's labs stays negative.
    const int32_t span = 0x2e630 - 0x11170;
    Check(Product(21474836u) >= 0 && FlyZoneOffset(Product(21474836u), span) == MacOffset(Product(21474836u), span),
          "the last product that fits is the Mac's point");
    Check(Product(21474837u) < 0 && MacOffset(Product(21474837u), span) < 0, "the first that wraps leaves the edge on the Mac");
    Check(FlyZoneOffset(Product(21474837u), span) >= 0 && FlyZoneOffset(Product(21474837u), span) < span,
          "and is on the edge in the port");
    Check(Labs32(0x80000000u) < 0 && FlyZoneOffset(Product(0x80000000u), span) >= 0, "labs of the most negative word");

    if (gFailures == 0) std::printf("host_fly_zone: all checks passed\n");
    return gFailures;
}
