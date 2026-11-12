// tpi_fsh.h: Theme Park Inc's textures are EA SHPI ".fsh" files (one 8-bit paletted image each, RefPack-compressed or
// not, with a 24-bit, 32-bit or 16-bit palette); the game reads 24/32-bit uncompressed TGAs (and WCT). The converter
// (tpi_convert.h) turns each .fsh into a 32-bit type 2 TGA with the same stem.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

// A decoded image: BGRA bytes, row 0 at the top.
struct TpiImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> bgra;
};

// True for the SHPI signature.
bool IsTpiFsh(const std::vector<uint8_t>& bytes);
// Decodes the first image of a .fsh; false with a message for a form the converter does not know.
bool DecodeTpiFsh(const std::vector<uint8_t>& fsh, TpiImage& image, std::string& err);
// A 32-bit uncompressed TGA (type 2, rows bottom-up, 8 alpha bits) of the image, as the game loads them.
std::vector<uint8_t> EncodeTpiTga32(const TpiImage& image);
// Both steps.
bool TpiFshToTga(const std::vector<uint8_t>& fsh, std::vector<uint8_t>& tga, std::string& err);

}  // namespace tpw::assets
