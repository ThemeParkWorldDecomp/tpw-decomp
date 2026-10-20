// The pixel format records of the bullfrog library's Colour.cpp (unit v5L21). __sinit_Colour_cpp (0x1001a104, 460 bytes) stores
// the six words of each (four channel masks, the depth, palettised 0); the host initialises the same values as constants.
// The Mac has no names for them: the names say the layout (masks red, green, blue, alpha from the top). Addresses in the comments.
#include "TbPixFmt.h"

TbPixFormat gColourFormatRGBA8888(0xff000000u, 0xff0000, 0xff00, 0xff, 32, 0);        // 0x10073f38
TbPixFormat gColourFormatRGBA4444(0xf000, 0xf00, 0xf0, 0xf, 16, 0);                    // 0x10073f50
TbPixFormat gColourFormatXBGR8888(0xff, 0xff00, 0xff0000, 0, 32, 0);                   // 0x10073f68
TbPixFormat gColourFormatXRGB8888(0xff0000, 0xff00, 0xff, 0, 32, 0);                   // 0x10073f80
TbPixFormat gColourFormatABGR8888(0xff, 0xff00, 0xff0000, 0xff000000u, 32, 0);         // 0x10073f98
TbPixFormat gColourFormatARGB8888(0xff0000, 0xff00, 0xff, 0xff000000u, 32, 0);         // 0x10073fb0
TbPixFormat gColourFormatBGR888(0xff, 0xff00, 0xff0000, 0, 24, 0);                     // 0x10073fc8
TbPixFormat gColourFormatRGB888(0xff0000, 0xff00, 0xff, 0, 24, 0);                     // 0x10073fe0
TbPixFormat gColourFormatARGB4444(0xf00, 0xf0, 0xf, 0xf000, 16, 0);                    // 0x10073ff8
TbPixFormat gColourFormatRGB565(0xf800, 0x7e0, 0x1f, 0, 16, 0);                        // 0x10074010
TbPixFormat gColourFormatXRGB1555(0x7c00, 0x3e0, 0x1f, 0, 16, 0);                      // 0x10074028
TbPixFormat gColourFormatARGB1555(0x7c00, 0x3e0, 0x1f, 0x8000, 16, 0);                 // 0x10074040
// The only record with a TOC slot of its own (TOC+0x374, the others are at TOC+0x610..0x63c): the same layout as 0x10073fb0.
TbPixFormat gColourFormatARGB8888Default(0xff0000, 0xff00, 0xff, 0xff000000u, 32, 0);  // 0x10074058
