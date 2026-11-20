// Known obsolete disc contents. Names, sizes and hashes only; never alternate accepted game data.
#pragma once
#include "manifest.h"
#include "sha256.h"
#include <cstddef>
#include <vector>

namespace tpw::assets {

enum class Pressing { Unknown, UsFirstRelease };

struct PressingFingerprint {
    const char* dest;
    uint64_t size;
    Sha256Digest digest;
    Pressing pressing;
};

const PressingFingerprint* PressingFingerprints();
size_t PressingFingerprintCount();
constexpr size_t kNoPressingFingerprint = static_cast<size_t>(-1);
// A canonical manifest destination (case insensitive), size AND hash must all match. SAM files and films are
// deliberately absent: settings edits and damaged films cannot supply evidence of a first release.
size_t MatchPressingFingerprint(const char* dest, uint64_t size, const Sha256Digest& digest);
// Pass the indexes matched during this drop, including duplicates if convenient. At least three DISTINCT changed
// files from the known disc are required. Unknown differences never contribute. Apply only to US edition cards.
Pressing IdentifyPressing(Edition edition, const std::vector<size_t>& matches);

}  // namespace tpw::assets
