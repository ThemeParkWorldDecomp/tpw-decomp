// The postcard composer (main app, file rsse.cpp, 0x100bf7c4..0x100c0c20): a bitmap class that loads a TGA, grabs the
// screen from OpenGL, blends one bitmap over another and scales one into a buffer (PostcardBitmap), and the postcard
// itself (Postcard): five source pictures, the composed 640 x 240 card and the screen grab. Postcard::save
// (0x100c0c20, libjpeg) and the postcard panel (ui_Postcard.cpp) are not part of this file.
//
// Host design. The Mac objects are 0x10 and 0x70 bytes with 32-bit pointers; these keep the member order and the names
// but hold a native pointer, so their sizes differ. Neither class has a vtable. The Mac destructors take a delete flag
// in r4 (CodeWarrior's convention) that the host has no use for: they are plain destructors (a NULL `this` test and the
// flag are compiler glue), and the members of Postcard are destroyed in the reverse order of their declaration, the
// order of the original's inlined member destructors (mGrab first, mLogo last).
#pragma once
#include <cstdint>

struct _DDSURFACEDESC2;   // Postcard::grab takes one and ignores it

// A picture in memory: width, height, bytes a pixel and the pixels (operator new[]). The pixels are 4 bytes each (the
// first byte is the alpha) in the card, the grab and the TGA pictures that are not palettised; 1 byte for those that are.
class PostcardBitmap {
public:
    PostcardBitmap() : mWidth(0), mHeight(0), mBytesPerPixel(0), mPixels(nullptr) {}   // (inlined by the compiler into Postcard's)
    PostcardBitmap(uint32_t width, uint32_t height, uint32_t bytesPerPixel);   // 0x100bf7c4
    ~PostcardBitmap();                                                         // 0x100bf84c
    PostcardBitmap(const PostcardBitmap&) = delete;   // (host: the pixels have one owner)
    PostcardBitmap& operator=(const PostcardBitmap&) = delete;
    void clear();                                                              // 0x100bf8d4
    // 0x100bf954: reads the TGA `path` + `name` (the two strings joined in a 256-byte buffer, no length test) into this
    // bitmap, 4 bytes a pixel with the file's four bytes reversed when the picture is not palettised (the loader's area says
    // 1 in its palettised word), else 1 byte a pixel as the loader gave them. 0 when the path is NULL, the header or the
    // pixels fail to load or the new pixels cannot be allocated; 1 otherwise. The old pixels are released before the
    // allocation, whatever follows.
    int32_t load_from_tga(const char* path, const char* name);
    // 0x100bfc3c: this takes the screen (gei_Display's width and height) from the front buffer, scales it to 320 x 240
    // through the temporary allocator's buffer and flips it (UPixels format 9, flipped source). 0 only when that buffer
    // cannot be had.
    int32_t copy_from_opengl();
    // 0x100bff2c: this (4 bytes a pixel, the first the alpha) over `dest` at (x, y): dest's first byte becomes 0xff and
    // each colour byte moves towards this one's by alpha / 256 (rounded down). No clipping; the rows skip dest's width
    // less this one's.
    void alphablt(PostcardBitmap& dest, uint32_t x, uint32_t y);
    // 0x100c0050: a box filter of this (4 bytes a pixel) to `width` x `height` pixels at `out` (see the .cpp for the
    // formats). 0 when a buffer cannot be had.
    int32_t resize_into_buffer(uint8_t* out, int32_t width, int32_t height, int32_t format);

    uint32_t mWidth;           // 0x00
    uint32_t mHeight;          // 0x04
    uint32_t mBytesPerPixel;   // 0x08
    uint8_t* mPixels;          // 0x0c
};

// 0x70 bytes on the Mac: seven bitmaps in a row.
class Postcard {
public:
    Postcard();    // 0x100c03a4: all zero, then clear
    ~Postcard();   // 0x100c0478
    void clear();  // 0x100c0658: clear of each bitmap, mLogo first
    // 0x100c07c4: loads the five pictures from "data:postcard:<name>.tga" through FindFilePath (the answers of the loads
    // are not looked at), makes the card (640 x 240, 4 bytes a pixel) and empties the grab. Always 1.
    int32_t load_sources();
    int32_t grab(_DDSURFACEDESC2& surface);   // 0x100c0954: copy_from_opengl of the grab (the parameter is ignored). Always 1.
    void generate();                          // 0x100c09b4: the card's background colour, then the six pictures over it
    // 0x100c0ac4: with the card made, scales it (resize_into_buffer); the original composes the card only when it has no
    // pixels, and then writes from address 0 (kept: see the .cpp).
    void generate_preview_into_buffer(uint8_t* out, int32_t width, int32_t height, int32_t format);

    // 0x100c0c20 (unit v5906, src/Postcard_Postcard_save.cpp): writes the card as a JPEG (quality 75, 3 components, RGB) to `path`;
    // a card that cannot be opened prints "can't open %s" and exits. Always 1.
    int32_t save(char* path);

    PostcardBitmap mLogo;       // 0x00  tpwlogo.tga
    PostcardBitmap mBullfrog;   // 0x10  bullfrog.tga
    PostcardBitmap mStamp;      // 0x20  stamp.tga
    PostcardBitmap mFrank;      // 0x30  frank.tga
    PostcardBitmap mLegal;      // 0x40  legal.tga
    PostcardBitmap mCard;       // 0x50  the composed card, 640 x 240
    PostcardBitmap mGrab;       // 0x60  the screen grab, 320 x 240
};

// Unit v5906: the part of libjpeg's compressor Postcard::save touches (0x174 bytes on the Mac; the error manager is kept as the
// 32-bit word the original stores, not a pointer).
struct jpeg_error_mgr;
struct jpeg_compress_struct {
    uint32_t mErr;                    // 0x00
    uint8_t mF04[0x18];               // 0x04
    uint32_t mImageWidth;             // 0x1c
    uint32_t mImageHeight;            // 0x20
    int32_t mInputComponents;         // 0x24
    int32_t mInColorSpace;            // 0x28
    uint8_t mF2C[0xdc - 0x2c];        // 0x2c
    uint32_t mNextScanline;           // 0xdc
    uint8_t mFE0[0x174 - 0xe0];       // 0xe0
};
static_assert(sizeof(jpeg_compress_struct) == 0x174, "jpeg_compress_struct layout");
void jpeg_CreateCompress(jpeg_compress_struct* cinfo, int32_t version, uint32_t size);   // glue 0x1023a090
void jpeg_stdio_dest(jpeg_compress_struct* cinfo, void* file);                           // glue 0x1023a0c0
void jpeg_set_defaults(jpeg_compress_struct* cinfo);                                     // glue 0x1023a0d8
void jpeg_set_quality(jpeg_compress_struct* cinfo, int32_t quality, int32_t forceBaseline);   // glue 0x1023a0f0
void jpeg_start_compress(jpeg_compress_struct* cinfo, int32_t writeAllTables);           // glue 0x1023a108
uint32_t jpeg_write_scanlines(jpeg_compress_struct* cinfo, uint8_t** rows, uint32_t count);   // glue 0x1023a120
void jpeg_finish_compress(jpeg_compress_struct* cinfo);                                  // glue 0x1023a138
void jpeg_destroy_compress(jpeg_compress_struct* cinfo);                                 // glue 0x1023a150
