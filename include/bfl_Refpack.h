// RefPack decoder (bullfrog). Mac vtables 0x10071afc and 0x1007232c, size 4.
// MemCompress remains external: u11 only reads the shipped stored and RefPack entries.
#pragma once
#include <cstdint>

namespace bfl::compress {
class CCompressionBase {
public:
    virtual ~CCompressionBase();
    virtual int32_t MemCompress(uint8_t*, uint8_t*, uint32_t, uint8_t*) const = 0;
    virtual int32_t MemDecompress(uint8_t*, uint8_t*, uint8_t*) const = 0;
    virtual int32_t CompressWorkspaceSize() const = 0;
    virtual int32_t DecompressWorkspaceSize() const = 0;
    virtual int32_t DecompressedSize(uint8_t*) const = 0;
};
class CRefpack : public CCompressionBase {
public:
    ~CRefpack() override;
    int32_t MemCompress(uint8_t*, uint8_t*, uint32_t, uint8_t*) const override;
    int32_t MemDecompress(uint8_t*, uint8_t*, uint8_t*) const override;
    int32_t CompressWorkspaceSize() const override;
    int32_t DecompressWorkspaceSize() const override;
    int32_t DecompressedSize(uint8_t*) const override;
};
}
