#include "TbGraphicFile.h"
#include <cstring>
#include <new>
#include "LbFile.h"
#include "bfl_FileSystem.h"
#include "lobby.h"
#include "TbRaConvert.h"
#include "TbPixFmt.h"

using bfl::FileSystem::CFile;

namespace {
uint32_t Ptr32Of(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x10024324
TbResult TbJPEGLoader::ReadHeaderInfo(CFile& file, TbRenderArea& area) {
    TbResult result;
    if (file.Seek(0, 0) != 0) {
        result.mCode = -1;
        return result;
    }
    jpeg_error_mgr errorManager;
    jpeg_decompress_struct cinfo;
    cinfo.err = Ptr32Of(jpeg_std_error(&errorManager));
    jpeg_CreateDecompress(&cinfo, 0x3e, 0x1d0);
    jpeg_stdio_src(&cinfo, &file);
    jpeg_read_header(&cinfo, 1);
    jpeg_destroy_decompress(&cinfo);
    const uint32_t masks[4] = {0xff, 0xff00, 0xff0000, 0};
    std::memcpy(area.mF00, masks, sizeof masks);
    area.mDepth = 0x18;
    area.mPalettised = 0;
    area.mWidth = static_cast<int32_t>(cinfo.image_width);
    area.mPictureHeight = static_cast<int32_t>(cinfo.image_height);
    area.mStride = static_cast<int32_t>(cinfo.image_width) * 3;
    result.mCode = 0;
    return result;
}

// 0x100244c0. The two paths differ in the bound of the loop: the palettised one compares the scanline with the
// description's height (0x20), the other with its width (0x1c), as the original does (it never ends when that
// bound is below the image's output height).
TbResult TbJPEGLoader::ReadData(const TbRenderArea& description, CFile& file, TbRenderArea& unused) {
    (void)unused;
    TbResult result;
    if (file.Seek(0, 0) != 0) {
        result.mCode = -1;
        return result;
    }
    jpeg_error_mgr errorManager;
    jpeg_decompress_struct cinfo;
    cinfo.err = Ptr32Of(jpeg_std_error(&errorManager));
    jpeg_CreateDecompress(&cinfo, 0x3e, 0x1d0);
    jpeg_stdio_src(&cinfo, &file);
    jpeg_read_header(&cinfo, 1);
    if (description.mPalettised != 0) {
        cinfo.quantize_colors = 1;
        jpeg_start_decompress(&cinfo);
        {
            TbRAConv converter;
            TbSize rowSize = {static_cast<int32_t>(cinfo.output_width), 1};
            TbRenderArea row(rowSize, TbPixFormat8, nullptr, 0);
            row.AllocMemory();
            uint8_t* rows[1] = {row.mData};
            while (cinfo.output_scanline < cinfo.output_height) {
                if (cinfo.output_scanline < static_cast<uint32_t>(description.mPictureHeight)) {
                    jpeg_read_scanlines(&cinfo, rows, 1);
                    TbRenderArea client;
                    TbSize size = {description.mWidth, 1};
                    TbPoint origin = {0, static_cast<int32_t>(cinfo.output_scanline) - 1};
                    description.SetupClientRA(client, origin, size);
                    converter.Move(client, row, nullptr);
                }
            }
            row.FreeMemory();
            uint8_t* entry = mBytes + 0x2c;
            for (int32_t i = 0; i < cinfo.actual_number_of_colors; i++) {
                entry[0] = cinfo.colormap[0][i];
                entry[1] = cinfo.colormap[1][i];
                entry[2] = cinfo.colormap[2][i];
                entry += 4;
            }
        }
    } else {
        jpeg_start_decompress(&cinfo);
        {   // the converter is destroyed after FreeMemory, before finish_decompress
        TbRAConv converter;
        TbPixFormat format(0xff, 0xff00, 0xff0000, 0, 0x18, 0);
        TbSize rowSize = {static_cast<int32_t>(cinfo.output_width), 1};
        TbRenderArea row(rowSize, format, nullptr, 0);
        row.AllocMemory();
        uint8_t* rows[1] = {row.mData};
        while (cinfo.output_scanline < cinfo.output_height) {
            if (cinfo.output_scanline < static_cast<uint32_t>(description.mWidth)) {
                jpeg_read_scanlines(&cinfo, rows, 1);
                TbRenderArea client;
                TbSize size = {description.mWidth, 1};
                TbPoint origin = {0, static_cast<int32_t>(cinfo.output_scanline) - 1};
                description.SetupClientRA(client, origin, size);
                converter.Move(client, row, nullptr);
            }
        }
        row.FreeMemory();
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    result.mCode = 0;
    return result;
}

// 0x100248b8
bool TbJPEGLoader::ValidImage(CFile& file) {
    if (file.Seek(0, 0) != 0) return false;
    jpeg_error_mgr errorManager;
    jpeg_decompress_struct cinfo;
    cinfo.err = Ptr32Of(jpeg_std_error(&errorManager));
    jpeg_CreateDecompress(&cinfo, 0x3e, 0x1d0);
    jpeg_stdio_src(&cinfo, &file);
    int32_t answer = jpeg_read_header(&cinfo, 1);
    jpeg_destroy_decompress(&cinfo);
    return answer == 1;
}
