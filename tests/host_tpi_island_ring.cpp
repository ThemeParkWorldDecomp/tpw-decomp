// host_tpi_island_ring.cpp: the island screen's second ring for the TPI Worlds and Rides mod (TPW_MOD_HOOK tpi_islands,
// include/tpi_island_ring.h), without the game: which ring a world's island is in (by its level folder, in either case), which
// island the arrows go to inside a ring (wrapping round the ring, never leaving it, staying put when alone), where Tab lands
// (the island last looked at in the other ring when it is still one, else the ring's first, nowhere when the ring is empty), that
// the second ring's places use the lobby description's spare camera slots and stand out of the first ring's fog, that the hint
// names every world of the mod, and that Tab's flight keeps out of every island (flown tick by tick as lobby_island.cpp's
// CalcCamera and TickIslands move the camera, between every island of one ring and every island of the other, from every
// start angle, at frame times from 60 frames a second to the game's longest step). The list used is the game's with the mod on: the original four (places 0..3) then the three worlds
// of platform/tpi_content.h (4..6). Run by ctest ("tpi_island_ring") in a build with the port's fixes; the original has one ring.
// Exits nonzero on a failure.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "tpi_content.h"
#include "tpi_island_ring.h"

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%ld)\n", what, value);
}

bool Contains(const char16_t* text, const char* word) {
    const size_t n = std::strlen(word);
    for (const char16_t* at = text; *at != 0; ++at) {
        size_t i = 0;
        while (i < n && at[i] != 0 && at[i] == static_cast<char16_t>(static_cast<unsigned char>(word[i]))) ++i;
        if (i == n) return true;
    }
    return false;
}

}  // namespace

int main() {
    using namespace tpw;

    // Ring membership by folder name.
    Check(TpiWorldIndex("arabian") == 0, "arabian is world 0");
    Check(TpiWorldIndex("water") == 1, "water is world 1");
    Check(TpiWorldIndex("science") == 2, "science is world 2");
    Check(TpiWorldIndex("Arabian") == 0 && TpiWorldIndex("WATER") == 1 && TpiWorldIndex("ScIeNcE") == 2, "either case");
    Check(TpiWorldIndex("jungle") == -1 && TpiWorldIndex("fantasy") == -1 && TpiWorldIndex("hallow") == -1 &&
              TpiWorldIndex("space") == -1,
          "the original four are no world of the mod");
    Check(TpiWorldIndex("") == -1 && TpiWorldIndex(nullptr) == -1, "empty and missing names");
    Check(TpiWorldIndex("arabia") == -1 && TpiWorldIndex("arabians") == -1 && TpiWorldIndex("waters") == -1, "whole names only");
    Check(TpiIslandRing("jungle") == 0 && TpiIslandRing("space") == 0, "ring one");
    Check(TpiIslandRing("arabian") == 1 && TpiIslandRing("water") == 1 && TpiIslandRing("science") == 1, "ring two");
    Check(kTpiWorldCount == 3, "three worlds", static_cast<long>(kTpiWorldCount));
    // The header's list is platform/tpi_content.h's, entry for entry (the game's code cannot see the platform's header).
    Check(kTpiWorldCount == sizeof(kTpiWorlds) / sizeof(kTpiWorlds[0]), "as many worlds as platform/tpi_content.h");
    for (size_t i = 0; i < kTpiWorldCount; ++i) {
        Check(std::strcmp(kTpiIslandWorlds[i], kTpiWorlds[i].folder) == 0, "the same folder as platform/tpi_content.h", static_cast<long>(i));
        Check(TpiWorldIndex(kTpiWorlds[i].folder) == static_cast<int32_t>(i), "every listed folder finds its own index", static_cast<long>(i));
        Check(kTpiRingOneSlots + static_cast<int32_t>(i) < 10, "list place inside the ten camera places", static_cast<long>(i));
    }

    // The game's list with the mod on and content for all three: jungle fantasy hallow space | arabian water science.
    const int32_t rings[7] = {0, 0, 0, 0, 1, 1, 1};
    // The arrows inside ring one.
    Check(TpiNextInRing(rings, 7, 0, true) == 1 && TpiNextInRing(rings, 7, 1, true) == 2 && TpiNextInRing(rings, 7, 2, true) == 3, "right in ring one");
    Check(TpiNextInRing(rings, 7, 3, true) == 0, "right wraps ring one to its first, not to ring two");
    Check(TpiNextInRing(rings, 7, 0, false) == 3, "left wraps ring one to its last, not to ring two");
    Check(TpiNextInRing(rings, 7, 2, false) == 1, "left in ring one");
    // The arrows inside ring two.
    Check(TpiNextInRing(rings, 7, 4, true) == 5 && TpiNextInRing(rings, 7, 5, true) == 6, "right in ring two");
    Check(TpiNextInRing(rings, 7, 6, true) == 4, "right wraps ring two to its first, not to ring one");
    Check(TpiNextInRing(rings, 7, 4, false) == 6, "left wraps ring two to its last, not to ring one");
    Check(TpiNextInRing(rings, 7, 6, false) == 5, "left in ring two");
    // A full walk in each direction visits each ring's islands once and comes back.
    for (int forwards = 0; forwards < 2; ++forwards) {
        size_t at = 0;
        int seen[7] = {0};
        for (int step = 0; step < 4; ++step) {
            seen[at] += 1;
            at = TpiNextInRing(rings, 7, at, forwards != 0);
        }
        Check(at == 0 && seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[3] == 1 && seen[4] == 0 && seen[5] == 0 && seen[6] == 0,
              "four steps round ring one", forwards);
        at = 4;
        int seen2[7] = {0};
        for (int step = 0; step < 3; ++step) {
            seen2[at] += 1;
            at = TpiNextInRing(rings, 7, at, forwards != 0);
        }
        Check(at == 4 && seen2[4] == 1 && seen2[5] == 1 && seen2[6] == 1 && seen2[0] == 0 && seen2[3] == 0, "three steps round ring two", forwards);
    }
    // The original list alone (mod on, no content yet): the arrows walk the four as before.
    const int32_t four[4] = {0, 0, 0, 0};
    Check(TpiNextInRing(four, 4, 3, true) == 0 && TpiNextInRing(four, 4, 0, false) == 3 && TpiNextInRing(four, 4, 1, true) == 2, "four islands, one ring");
    // An island alone in its ring stays; odd inputs stay put.
    const int32_t alone[5] = {0, 0, 0, 0, 1};
    Check(TpiNextInRing(alone, 5, 4, true) == 4 && TpiNextInRing(alone, 5, 4, false) == 4, "alone in ring two");
    Check(TpiNextInRing(rings, 0, 0, true) == 0 && TpiNextInRing(rings, 7, 9, true) == 9, "empty list or index past the end");
    // Rings in any order in the list (a descriptor with an odd number): still one ring per walk.
    const int32_t mixed[7] = {0, 1, 0, 1, 0, 1, 0};
    Check(TpiNextInRing(mixed, 7, 0, true) == 2 && TpiNextInRing(mixed, 7, 6, true) == 0 && TpiNextInRing(mixed, 7, 1, false) == 5 &&
              TpiNextInRing(mixed, 7, 5, true) == 1,
          "rings interleaved in the list");

    // Where Tab lands.
    Check(TpiRingLanding(rings, 7, 1, -1) == 4, "first time to ring two: its first island");
    Check(TpiRingLanding(rings, 7, 0, -1) == 0, "first time back to ring one: its first island");
    Check(TpiRingLanding(rings, 7, 1, 6) == 6, "back to the ring-two island last looked at");
    Check(TpiRingLanding(rings, 7, 0, 2) == 2, "back to the ring-one island last looked at");
    Check(TpiRingLanding(rings, 7, 1, 2) == 4, "a remembered island of the wrong ring is ignored");
    Check(TpiRingLanding(rings, 7, 0, 5) == 0, "a remembered island of the wrong ring is ignored (ring one)");
    Check(TpiRingLanding(rings, 7, 1, 7) == 4 && TpiRingLanding(rings, 7, 1, 99) == 4, "a remembered index past the list is ignored");
    Check(TpiRingLanding(four, 4, 1, -1) == -1, "no ring two: nowhere (Tab does nothing)");
    Check(TpiRingLanding(four, 4, 1, 2) == -1, "no ring two even with a stale memory");
    Check(TpiRingLanding(rings, 0, 1, -1) == -1, "empty list: nowhere");

    // Ring two's places: the spare camera slots' numbers, inside the lobby (x 0..1000 like ring one, z north of it), farther
    // than the fog's end (250) from anywhere the browsing camera stands in ring one (an island of the first ring, radius 70),
    // and on the sea: lobby.wad's terrain/Base.MD2 is a 24-sided disk round (550.5, 553.1) whose edge is never nearer than 523
    // to its centre, and an island (its slab about 50 from its centre) needs 90 of sea round its centre, so no bare sky shows
    // next to it.
    const float ringOne[4][2] = {{400, 400}, {600, 400}, {600, 600}, {400, 600}};
    float nearest = 1.0e9f;
    for (size_t w = 0; w < kTpiWorldCount; ++w) {
        const TpiRingPlace& place = kTpiRingTwoPlaces[w];
        Check(place.x >= 100.0f && place.x <= 900.0f, "ring two x inside the lobby", static_cast<long>(place.x));
        Check(place.z > 600.0f + 70.0f + 250.0f, "ring two north of ring one's fog", static_cast<long>(place.z));
        const float sx = place.x - 550.5f;
        const float sz = place.z - 553.1f;
        const float sea = 523.0f - std::sqrt(sx * sx + sz * sz);
        Check(sea >= 90.0f, "ring two's island well inside the lobby's sea", static_cast<long>(sea));
        for (const float* island : ringOne) {
            const float dx = place.x - island[0];
            const float dz = place.z - island[1];
            const float distance = std::sqrt(dx * dx + dz * dz) - 70.0f;
            if (distance < nearest) nearest = distance;
        }
        for (size_t v = 0; v < w; ++v) {
            const float dx = place.x - kTpiRingTwoPlaces[v].x;
            const float dz = place.z - kTpiRingTwoPlaces[v].z;
            Check(dx * dx + dz * dz >= 200.0f * 200.0f - 1.0f, "ring two's islands as far apart as ring one's", static_cast<long>(w * 10 + v));
        }
    }
    Check(nearest > 250.0f, "ring two out of the fog from every ring-one camera place", static_cast<long>(nearest));

    // The hint names every world of the mod, and the way back names nothing of the mod's.
    for (size_t w = 0; w < kTpiWorldCount; ++w)
        Check(Contains(kTpiRingHintToTwo, kTpiWorlds[w].title), "the hint names the world", static_cast<long>(w));
    Check(Contains(kTpiRingHintToTwo, "Tab") && Contains(kTpiRingHintToOne, "Tab"), "both hints name the key");
    Check(!Contains(kTpiRingHintToOne, "Arabian"), "the way back does not name the mod's worlds");

    // Tab's flight. The camera circles 70 out and 20 over an island's look-at point (the lobby's SpinRadius and
    // VerticalOffset), turning 0.02 a tick unit (SpinSpeed); each tick it goes a tenth of the way per tick unit to its goal (a
    // tick unit is 100 ms). Every island's mesh stands under 60 over the sea inside 50 of its place; the look-at heights are the
    // lobby description's (ring one's four, then 15 for the mod's three). Any point of the flight within 50 of an island's place
    // must be at least 5 over 60, and the camera must be round the landing island in 10 seconds.
    {
        struct Isle {
            float x;
            float y;
            float z;
        };
        Isle isles[7] = {{400.0f, 12.5f, 400.0f}, {600.0f, 17.5f, 400.0f}, {600.0f, 38.0f, 600.0f}, {400.0f, 35.0f, 600.0f}};
        for (size_t w = 0; w < kTpiWorldCount; ++w) isles[4 + w] = {kTpiRingTwoPlaces[w].x, 15.0f, kTpiRingTwoPlaces[w].z};
        const float units[] = {1.0f / 6.0f, 1.0f / 3.0f, 1.0f, 5.0f};
        float lowest = 1e9f;
        float slowest = 0.0f;
        int flights = 0;
        for (int from = 0; from < 7; ++from) {
            for (int to = 0; to < 7; ++to) {
                if ((from < 4) == (to < 4)) continue;
                for (const float u : units) {
                    for (int degrees = 0; degrees < 360; degrees += 4) {
                        const Isle& start = isles[from];
                        const Isle& land = isles[to];
                        float angle = static_cast<float>(degrees) * 0.017453292f;
                        TpiPoint camera = {start.x + 70.0f * std::sin(angle), start.y + 20.0f, start.z + 70.0f * std::cos(angle)};
                        angle = TpiFacingAngle(camera, land.x, land.z);
                        TpiFlight flight = TpiPlanFlight(camera, {land.x + 70.0f * std::sin(angle), land.y + 20.0f, land.z + 70.0f * std::cos(angle)});
                        float seconds = 0.0f;
                        bool landed = false;
                        while (!landed && seconds < 10.0f) {
                            angle += 0.02f * u;
                            TpiPoint goal = {land.x + 70.0f * std::sin(angle), land.y + 20.0f, land.z + 70.0f * std::cos(angle)};
                            const bool flying = TpiFlightGoal(flight, camera) != nullptr;
                            if (flying) goal = flight.way[0];
                            const TpiPoint was = camera;
                            camera.x += 0.1f * u * (goal.x - camera.x);
                            camera.y += 0.1f * u * (goal.y - camera.y);
                            camera.z += 0.1f * u * (goal.z - camera.z);
                            seconds += u / 10.0f;
                            // The segment flown this tick, in pieces of at most 2.
                            const float span = std::sqrt((camera.x - was.x) * (camera.x - was.x) + (camera.z - was.z) * (camera.z - was.z) +
                                                         (camera.y - was.y) * (camera.y - was.y));
                            const int pieces = 1 + static_cast<int>(span / 2.0f);
                            for (int k = 1; k <= pieces; ++k) {
                                const float f = static_cast<float>(k) / static_cast<float>(pieces);
                                const float x = was.x + f * (camera.x - was.x);
                                const float y = was.y + f * (camera.y - was.y);
                                const float z = was.z + f * (camera.z - was.z);
                                for (const Isle& isle : isles) {
                                    if ((x - isle.x) * (x - isle.x) + (z - isle.z) * (z - isle.z) < 50.0f * 50.0f && y - 60.0f < lowest) lowest = y - 60.0f;
                                }
                            }
                            if (!flying) {
                                const float dx = goal.x - camera.x, dy = goal.y - camera.y, dz = goal.z - camera.z;
                                landed = dx * dx + dy * dy + dz * dz < 20.0f * 20.0f;
                            }
                        }
                        if (!landed) seconds = 99.0f;
                        if (seconds > slowest) slowest = seconds;
                        ++flights;
                    }
                }
            }
        }
        // The plan itself: the landing goal on the island's near side (the camera comes straight in, never over the island to
        // turn round), straight up first, level at kTpiFlightLift over the higher end, and each waypoint let go within reach.
        for (const float high : {10.0f, 90.0f}) {
            const TpiPoint from = {360.0f, high, 1000.0f};
            const float facing = TpiFacingAngle(from, 400.0f, 600.0f);
            const TpiPoint to = {400.0f + 70.0f * std::sin(facing), 55.0f, 600.0f + 70.0f * std::cos(facing)};
            const float toIsland = std::sqrt(40.0f * 40.0f + 400.0f * 400.0f);
            const float toGoal = std::sqrt((to.x - from.x) * (to.x - from.x) + (to.z - from.z) * (to.z - from.z));
            Check(std::fabs(toGoal - (toIsland - 70.0f)) < 0.5f, "Tab lands on the island's side facing the camera", static_cast<long>(toGoal));
            TpiFlight plan = TpiPlanFlight(from, to);
            Check(plan.count == 2 && plan.way[0].x == from.x && plan.way[0].z == from.z && plan.way[1].x == to.x && plan.way[1].z == to.z,
                  "straight up, then across to above the landing", static_cast<long>(high));
            Check(plan.way[0].y == (high > 55.0f ? high : 55.0f) + kTpiFlightLift && plan.way[1].y == plan.way[0].y,
                  "level at the lift over the higher end", static_cast<long>(plan.way[0].y));
            const TpiPoint near = {from.x, plan.way[0].y - kTpiFlightReach + 1.0f, from.z};
            const TpiPoint* next = TpiFlightGoal(plan, near);
            Check(next != nullptr && next->x == to.x && plan.count == 1, "a waypoint within reach gives way to the next");
            const TpiPoint over = plan.way[0];
            Check(TpiFlightGoal(plan, over) == nullptr && plan.count == 0, "the flight ends at its last waypoint");
        }

        std::printf("Tab flights: %d, lowest over an island %+.1f, slowest %.1f s\n", flights, static_cast<double>(lowest), static_cast<double>(slowest));
        Check(flights == 24 * 4 * 90, "every pair of rings, frame time and start angle flown", flights);
        Check(lowest >= 5.0f, "Tab's flight keeps 5 over every island it passes within 50 of", static_cast<long>(std::floor(lowest)));
        Check(slowest < 10.0f, "Tab's flight is round the landing island within 10 seconds", static_cast<long>(slowest * 10.0f));
    }

    std::printf("%s: %d checks, %d failures\n", gFailures == 0 ? "PASS" : "FAIL", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
