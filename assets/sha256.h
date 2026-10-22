// SHA-256 (FIPS 180-4), streaming. Written for the asset launcher: no third-party code.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace tpw::assets {

struct Sha256Digest {
    uint8_t bytes[32] = {};
    // Lower-case hexadecimal, 64 characters.
    std::string Hex() const;
    bool operator==(const Sha256Digest& o) const;
    bool operator!=(const Sha256Digest& o) const { return !(*this == o); }
};

class Sha256 {
public:
    Sha256();
    void Reset();
    void Update(const void* data, size_t size);
    // Finishes the hash; the object is reset afterwards.
    Sha256Digest Final();
    // One-shot helper.
    static Sha256Digest Of(const void* data, size_t size);

private:
    void Block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t used_;
    uint64_t total_;
};

}  // namespace tpw::assets
