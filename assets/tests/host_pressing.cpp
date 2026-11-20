// The checks are assert()s and the build is Release (-DNDEBUG), which would compile every one of them out.
#undef NDEBUG
#include "pressing.h"
#include <cassert>
#include <string>
#include <vector>

int main() {
    using namespace tpw::assets;
    assert(PressingFingerprintCount() >= 3);
    std::vector<size_t> matches;
    for (size_t i = 0; i < PressingFingerprintCount(); ++i) {
        const auto& f = PressingFingerprints()[i];
        std::string lower(f.dest);
        for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        assert(lower.find(".sam") == std::string::npos);
        assert(lower.find("/movies/") == std::string::npos);
        assert(MatchPressingFingerprint(lower.c_str(), f.size, f.digest) == i);
        assert(MatchPressingFingerprint(f.dest, f.size + 1, f.digest) == kNoPressingFingerprint);
        auto damaged = f.digest;
        damaged.bytes[0] ^= 1;
        assert(MatchPressingFingerprint(f.dest, f.size, damaged) == kNoPressingFingerprint);
        assert(MatchPressingFingerprint("data/med.sam", f.size, f.digest) == kNoPressingFingerprint);
        assert(MatchPressingFingerprint("data/Movies/jug.tgq", f.size, f.digest) == kNoPressingFingerprint);
        matches.push_back(i);
    }
    assert(IdentifyPressing(kEditionUsPc, {}) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionUsPc, {0}) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionUsPc, {0, 0, 0}) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionUsPc, {0, 1}) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionUsPc, {0, 1, kNoPressingFingerprint}) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionUsPc, {0, 1, 2}) == Pressing::UsFirstRelease);
    assert(IdentifyPressing(kEditionUs, matches) == Pressing::UsFirstRelease);
    assert(IdentifyPressing(kEditionUsPc, matches) == Pressing::UsFirstRelease);
    assert(IdentifyPressing(kEditionEurPc, matches) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionEur, matches) == Pressing::Unknown);
    assert(IdentifyPressing(kEditionGold, matches) == Pressing::Unknown);
}
