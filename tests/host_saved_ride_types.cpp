// host_saved_ride_types.cpp: the walk CSaveLoad::Load makes over a park's ride records before it reads any module (the port's
// fix heldpark, include/SavedRideTypes.h), on made-up modules: the rides module found between its two tags; a park whose
// rides are all loaded types (nothing missing); one with a ride type no loaded type carries, alone and followed by a second
// (the records LoadRides' skip cannot measure, which lost the rides after them and crashed the track rides' cars); an old
// autosave's ride preview (type 0) measured and walked past, and one that cannot be measured (nothing reported); a block cut
// short; tags with the wrong length between them; and the same park with its words swapped, as a big-endian host reads it.
// Run by ctest ("saved_ride_types") in a build with the port's fixes, which alone has the walk. Exits nonzero on a failure.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "SavedRideTypes.h"

using namespace savedrides;

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

// The made-up ride types: 100 with two time controls, 200 with none, 300 with one; nothing else is loaded.
int32_t Controls(uint32_t type) {
    switch (type) {
    case 100: return 2;
    case 200: return 0;
    case 300: return 1;
    default: return -1;
    }
}

struct Bytes {
    bool swap = false;
    std::vector<uint8_t> data;
    void Byte(uint8_t v) { data.push_back(v); }
    void Half(uint16_t v) {
        if (swap) v = __builtin_bswap16(v);
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        data.insert(data.end(), p, p + 2);
    }
    void Word(uint32_t v) {
        if (swap) v = __builtin_bswap32(v);
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        data.insert(data.end(), p, p + 4);
    }
    void Fill(size_t count, uint8_t v) { data.insert(data.end(), count, v); }
};

struct Ride {
    uint8_t marker = 1;
    uint32_t type = 0;
    uint16_t nodes = 0;      // node words (4 bytes each)
    uint16_t embedded = 0;   // embedded words (8 bytes each)
    uint32_t controls = 0;   // the time controls the file holds (its type's count)
};

// A rides module as SaveRides writes it: the two counts and the first free slot, then each slot's marker and record.
Bytes RidesBlock(const std::vector<Ride>& rides, bool swap) {
    Bytes b;
    b.swap = swap;
    b.Word(static_cast<uint32_t>(rides.size()) - 1);
    b.Word(1);
    b.Word(0);
    for (const Ride& ride : rides) {
        b.Byte(ride.marker);
        if (ride.marker != 1) continue;
        b.Word(ride.type);
        b.Fill(0x26 - 4, 0x5a);   // place, flags and hoardings
        b.Word(0);                // rotation
        b.Half(ride.nodes);
        b.Half(ride.embedded);
        b.Word(0);
        b.Word(0);
        b.Fill(static_cast<size_t>(ride.embedded) * 8, 0x11);
        b.Fill(static_cast<size_t>(ride.nodes) * 4, 0x22);
        b.Fill(static_cast<size_t>(ride.controls) * kTimeControl, 0x33);
    }
    return b;
}

// The unpacked modules around it: the modules before (the game system's last), its tag and size, the block, the next tag.
std::vector<uint8_t> Modules(const Bytes& block, bool swap, uint32_t afterTag = kRidesAfterTag) {
    Bytes m;
    m.swap = swap;
    m.Fill(37, 0x7e);
    m.Word(kRidesBeforeTag);
    m.Word(static_cast<uint32_t>(block.data.size()));
    m.data.insert(m.data.end(), block.data.begin(), block.data.end());
    m.Word(afterTag);
    m.Fill(19, 0x6d);
    return m.data;
}

Missing Walk(const std::vector<uint8_t>& modules, bool swap) {
    const uint8_t* block = nullptr;
    size_t size = 0;
    if (!FindRidesBlock(modules.data(), modules.size(), swap, block, size)) return {0xffffffffu, 0};
    return MissingSavedRideType(block, size, swap, Controls);
}

bool Is(const Missing& m, uint32_t type, uint32_t slot) { return m.type == type && m.slot == slot; }

void Park(bool swap) {
    const std::string order = swap ? " (words swapped)" : "";
    const Ride known1{1, 100, 3, 1, 2}, known2{1, 200, 0, 0, 0}, known3{1, 300, 5, 2, 1}, empty{0};
    const Ride sub{1, 6750, 4, 0, 3}, sub2{1, 6751, 2, 1, 1};

    const Bytes all = RidesBlock({known1, empty, known2, known3}, swap);
    const std::vector<uint8_t> allModules = Modules(all, swap);
    const uint8_t* block = nullptr;
    size_t size = 0;
    Check(FindRidesBlock(allModules.data(), allModules.size(), swap, block, size) && block == allModules.data() + 37 + 8 &&
              size == all.data.size(),
          "the rides module is found between the game system's tag and the ride system's" + order);
    Check(Is(Walk(allModules, swap), 0, 0), "every ride a loaded type: nothing missing" + order);

    Check(Is(Walk(Modules(RidesBlock({known1, empty, sub, known3}, swap), swap), swap), 6750, 3),
          "a ride type no loaded type carries: its number and slot" + order);
    // two such records in a row: the skip cannot measure the first, so LoadRides lost every ride after it
    const Bytes two = RidesBlock({known1, sub, sub2, known2, known3}, swap);
    const uint8_t* start = two.data.data() + 12 + 1 + RecordFixedSize(two.data.data() + 13, swap) + 2 * kTimeControl + 1;
    const uint8_t* after = start + RecordFixedSize(start, swap);
    Check(SkippedTimeControls(after, two.data.data() + two.data.size(), 2, 5, swap, Controls) == -1,
          "two missing records in a row: the first cannot be measured" + order);
    Check(Is(Walk(Modules(two, swap), swap), 6750, 2), "two missing records in a row: the first is reported" + order);

    // an old autosave's ride preview (type 0): measured by the records after it, then walked past
    const Ride preview{1, 0, 2, 1, 2};
    Check(Is(Walk(Modules(RidesBlock({known1, preview, known2, known3}, swap), swap), swap), 0, 0),
          "a ride preview measured and walked past: nothing missing" + order);
    Check(Is(Walk(Modules(RidesBlock({known1, preview, known3, empty, sub}, swap), swap), swap), 0, 0),
          "a ride preview that cannot be measured: nothing reported, LoadRides decides" + order);
    Check(Is(Walk(Modules(RidesBlock({known1, empty, known3, sub}, swap), swap), swap), 6750, 4),
          "the last slot's missing type is reported" + order);

    Bytes cut = RidesBlock({known1, known3, sub}, swap);
    cut.data.resize(12 + 1 + 0x36 + 3 * 4 + 8 + 10);
    Check(Is(Walk(Modules(cut, swap), swap), 0, 0), "a block cut short inside a known ride: nothing reported" + order);

    Check(Walk(Modules(all, swap, kRidesBeforeTag), swap).type == 0xffffffffu,
          "the size word does not lead to the ride system's tag: no rides module" + order);
    const std::vector<uint8_t> none(64, 0);
    Check(Walk(none, swap).type == 0xffffffffu, "no tags: no rides module" + order);
}

}  // namespace

int main() {
    Park(false);
    Park(true);
    if (gFailures != 0) std::printf("%d FAILED\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
