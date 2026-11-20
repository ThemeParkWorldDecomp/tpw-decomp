#include "pressing.h"
#include <cstring>
#include <set>

namespace tpw::assets {
namespace {
#include "pressing_data.inc"

bool SamePath(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        auto lower = [](unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
        if (lower(*a++) != lower(*b++)) return false;
    }
    return *a == *b;
}
}

const PressingFingerprint* PressingFingerprints() { return kPressingFingerprints; }
size_t PressingFingerprintCount() { return sizeof(kPressingFingerprints) / sizeof(kPressingFingerprints[0]); }

size_t MatchPressingFingerprint(const char* dest, uint64_t size, const Sha256Digest& digest) {
    for (size_t i = 0; i < PressingFingerprintCount(); ++i) {
        const auto& f = kPressingFingerprints[i];
        if (size == f.size && digest == f.digest && SamePath(dest, f.dest)) return i;
    }
    return kNoPressingFingerprint;
}

Pressing IdentifyPressing(Edition edition, const std::vector<size_t>& matches) {
    if (!IsUsGame(edition)) return Pressing::Unknown;
    std::set<size_t> distinct;
    for (size_t i : matches)
        if (i < PressingFingerprintCount() && kPressingFingerprints[i].pressing == Pressing::UsFirstRelease)
            distinct.insert(i);
    return distinct.size() >= 3 ? Pressing::UsFirstRelease : Pressing::Unknown;
}
}  // namespace tpw::assets
