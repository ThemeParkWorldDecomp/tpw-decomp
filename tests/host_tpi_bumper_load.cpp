// host_tpi_bumper_load.cpp: the TPI Worlds and Rides mod's repair of parks saved with The Scorpions before 0.5.2
// (include/TpiBumperKind.h, TPW_MOD_HOOK tpi_worlds in BMP_Load and CSaveLoad::PostLoadOperations) on the host, without
// game data. Before 0.5.2 the ride was built from the 208 bytes past gRideTemplates, so a park keeps its id with the word
// found there as its kind. The ids loaded here are a park's: the words of its bumper ride chunk (tag 3) and its object's
// mTrackRideHandle, as a park saved with the placement correction switched off holds them; each must be loaded as an id
// whose kind has a template row and whose index is kept, and a park's in-range ids must load as they were saved.
// Exits nonzero on the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>

#include "TpiBumperKind.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static std::string Hex(uint32_t v) {
    char text[16];
    std::snprintf(text, sizeof text, "%08x", static_cast<unsigned>(v));
    return text;
}

// BMP_CreateRide rebuilding a record from an id copies the 208 bytes at this byte offset in gRideTemplates (the listing's
// base + (-kind) * 0xd0 - 0xd0); the table is 14 rows of 0xd0 bytes.
static bool TemplateInTable(uint32_t id) {
    const int32_t kind = static_cast<int32_t>(id) >> 8;
    const uint32_t offset = static_cast<uint32_t>(-kind) * 0xd0u - 0xd0u;
    return offset < 14u * 0xd0u;
}

int main() {
    using namespace tpw;
    // Every Theme Park World kind at every record index, and no ride (0): loaded as saved.
    for (int32_t kind = kBumperKindFirst; kind >= kBumperKindLast; --kind)
        for (uint32_t index = 0; index < 0x40; ++index) {
            const uint32_t id = (static_cast<uint32_t>(kind) << 8) | index;
            Check(TpiBumperLoadId(id) == id, "in-range id " + Hex(id) + " kept");
        }
    Check(TpiBumperLoadId(0) == 0, "no ride (0) kept");

    // A park saved with the placement correction switched off: The Scorpions placed in Arabian Nights on the arm64 Mac
    // build, where the word past the table is 0x00010001, saved, and loaded on this fix ("port: tpi-worlds: ride 12
    // (type 5140) built from past the bumper ride templates: id 01000100 loaded as fffffa00"). Each loads as the stand-in's
    // kind, with its index, and the rebuilt record's template is in the table, which the saved id's was not.
    struct Saved {
        const char* what;
        uint32_t id;
    };
    const Saved saved[] = {
        {"The Scorpions, chunk 3 and mTrackRideHandle", 0x01000100u},
    };
    for (const Saved& s : saved) {
        const uint32_t loaded = TpiBumperLoadId(s.id);
        Check(!TemplateInTable(s.id), std::string(s.what) + ": saved id " + Hex(s.id) + " names no template");
        Check(static_cast<int32_t>(loaded) >> 8 == kBumperKindStandIn, std::string(s.what) + ": loaded as kind -6");
        Check((loaded & 0xffu) == (s.id & 0xffu), std::string(s.what) + ": index kept");
        Check(TemplateInTable(loaded), std::string(s.what) + ": loaded id's template in the table");
        Check(TpiBumperLoadId(loaded) == loaded, std::string(s.what) + ": loading again changes nothing");
    }

    // Kinds a stray word could give on other builds: The Scorpions' -15, the ends of the range, positive kinds.
    for (int32_t kind : {-15, -16, -100, -0x800000, 1, 14, 0x10001, 0x7fffff})
        for (uint32_t index : {0u, 7u, 0x3fu}) {
            const uint32_t id = (static_cast<uint32_t>(kind) << 8) | index;
            const uint32_t loaded = TpiBumperLoadId(id);
            Check(loaded == ((static_cast<uint32_t>(kBumperKindStandIn) << 8) | index), "stray id " + Hex(id) + " loaded as -6");
            Check(TemplateInTable(loaded), "stray id " + Hex(id) + ": template in the table");
        }

    if (gFailures == 0) std::printf("tpi_bumper_load: all checks passed\n");
    return gFailures;
}
