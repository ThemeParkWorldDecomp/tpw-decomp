// manifest.cpp: the compiled-in manifest (assets/manifest.h). The generated table is included here once, so it is
// compiled once.
#include "manifest.h"

#include <algorithm>
#include <cstring>
#include <iterator>

namespace tpw::assets {

#include "manifest_data.inc"

const ManifestRecord* ManifestRecords() { return kManifestRecords; }
size_t ManifestRecordCount() { return std::size(kManifestRecords); }
const ManifestHash* ManifestHashes() { return kManifestHashes; }
size_t ManifestHashCount() { return std::size(kManifestHashes); }

const ManifestHash& RecordHash(const ManifestRecord& record, unsigned i) { return kManifestHashes[record.first_hash + i]; }

namespace {

// Order of the by-hash index: SHA-256 bytes, then size (the generator sorts by record too, which the lookup ignores).
int Compare(const ManifestHash& h, uint64_t size, const uint8_t* sha256) {
    int c = std::memcmp(h.sha256, sha256, 32);
    if (c != 0) return c;
    return h.size < size ? -1 : (h.size > size ? 1 : 0);
}

}  // namespace

std::vector<const ManifestRecord*> FindByContent(uint64_t size, const uint8_t sha256[32]) {
    std::vector<const ManifestRecord*> out;
    const uint32_t* begin = kManifestByHash;
    const uint32_t* end = kManifestByHash + std::size(kManifestByHash);
    const uint32_t* it = std::partition_point(begin, end, [&](uint32_t slot) { return Compare(kManifestHashes[slot], size, sha256) < 0; });
    for (; it != end && Compare(kManifestHashes[*it], size, sha256) == 0; ++it) out.push_back(&kManifestRecords[kManifestHashes[*it].record]);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

const ManifestRecord* FindByDest(Edition edition, const char* dest) {
    // sorted by destination: the first record with this destination, then the few that share it
    const ManifestRecord* begin = kManifestRecords;
    const ManifestRecord* end = kManifestRecords + std::size(kManifestRecords);
    const ManifestRecord* it = std::partition_point(begin, end, [&](const ManifestRecord& r) { return std::strcmp(r.dest, dest) < 0; });
    for (; it != end && std::strcmp(it->dest, dest) == 0; ++it) {
        if (it->editions & edition) return it;
    }
    return nullptr;
}

size_t CountRecords(unsigned editions, bool required_only) {
    size_t n = 0;
    for (const ManifestRecord& r : kManifestRecords) {
        if ((r.editions & editions) && (!required_only || (r.flags & kFlagRequired))) ++n;
    }
    return n;
}

const char* GroupName(Group group) {
    static const char* const names[kGroupCount] = {"core", "fonts-rsrc", "movies", "language:american", "language:english", "program", "gold"};
    return group < kGroupCount ? names[group] : "?";
}

const char* EditionName(Edition edition) {
    switch (edition) {
        case kEditionUs: return "us";
        case kEditionEur: return "eur";
        case kEditionGold: return "gold";
    }
    return "?";
}

const char* SourceName(DiscSource source) {
    switch (source) {
        case kSourceMacCd: return "mac";
        case kSourcePcCd: return "pc";
        case kSourceGoldCd: return "gold";
    }
    return "?";
}

}  // namespace tpw::assets
