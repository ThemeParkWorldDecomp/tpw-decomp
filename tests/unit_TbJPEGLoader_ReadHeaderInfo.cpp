// Test library for the Bullfrog JPEG loader (unit v5528; libtpw_unit_TbJPEGLoader_ReadHeaderInfo.dylib, tests/test_TbJPEGLoader_ReadHeaderInfo.py), bullfrog:
//   src/Lbgfsave_iffgiftim_TbJPEGLoader_ReadHeaderInfo.cpp   TbJPEGLoader::ReadHeaderInfo, ReadData, ValidImage
//
// The callees are recording fakes: the file's Seek (through its vtable), the eight libjpeg imports (the fakes write the
// fields of the decompress struct the loader reads), TbRenderArea's constructors, AllocMemory, FreeMemory, SetupClientRA,
// TbRAConv::Move and its destructor. TbPixFormat8 is defined here (the program's definition is in a file this library does not compile).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include "TbPixFmt.h"
#include "TbError.h"
#include "bfl_FileSystem.h"
#include "lobby.h"

using namespace calllog;
using bfl::FileSystem::CFile;

TbPixFormat TbPixFormat8(0, 0, 0, 0, 0, 0);

namespace {

uint32_t gColormap = 0;           // what the start_decompress fake stores in the struct
uint8_t* gLastBuffer = nullptr;   // what the last AllocMemory fake gave
std::map<const void*, int32_t> gObjects;   // the stack objects the constructors made (labels 100, 101, ...)
int32_t gNextLabel = 100;

int32_t Lab(const void* p) {
    auto it = gObjects.find(p);
    if (it != gObjects.end()) return it->second;
    return static_cast<int32_t>(lab(const_cast<void*>(p)));
}

int32_t NewLab(const void* p) {
    int32_t n = gNextLabel++;
    gObjects[p] = n;
    return n;
}

uint32_t Q(const char* key) { return static_cast<uint32_t>(ret(key)); }

class FakeFile : public CFile {
public:
    int32_t Seek(uint32_t offset, uint32_t whence) override {
        ev("Seek", {Lab(this), static_cast<int32_t>(offset), static_cast<int32_t>(whence), 1});
        return static_cast<int32_t>(Q("Seek"));
    }
};

}  // namespace

// ---- the file class's own members (imports on the Mac), only so that the library links
namespace bfl { namespace FileSystem {
CFile::CFile() { mImplementation = nullptr; }
CFile::CFile(const CFile& other) { mImplementation = other.mImplementation; }
CFile& CFile::operator=(const CFile& other) { mImplementation = other.mImplementation; return *this; }
CFile::~CFile() {}
void CFile::Close() {}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
CAccessItem* CFile::GetAccessClass() { return mImplementation; }
void CFile::AttachAccessClass(CAccessItem* access) { mImplementation = access; }
} }

// ---- libjpeg fakes
jpeg_error_mgr* jpeg_std_error(jpeg_error_mgr*) {
    ev("jpeg_std_error", {-1});
    return reinterpret_cast<jpeg_error_mgr*>(static_cast<uintptr_t>(Q("jpeg_std_error")));
}

void jpeg_CreateDecompress(jpeg_decompress_struct* cinfo, int32_t version, uint32_t size) {
    ev("jpeg_CreateDecompress", {-1, version, static_cast<int32_t>(size), static_cast<int32_t>(cinfo->err)});
    std::memset(reinterpret_cast<uint8_t*>(cinfo) + 4, 0, sizeof(jpeg_decompress_struct) - 4);
}

void jpeg_stdio_src(jpeg_decompress_struct*, void* file) { ev("jpeg_stdio_src", {-1, Lab(file)}); }

int32_t jpeg_read_header(jpeg_decompress_struct* cinfo, int32_t require) {
    ev("jpeg_read_header", {-1, require});
    cinfo->image_width = Q("iw");
    cinfo->image_height = Q("ih");
    return static_cast<int32_t>(Q("jpeg_read_header"));
}

void jpeg_destroy_decompress(jpeg_decompress_struct*) { ev("jpeg_destroy_decompress", {-1}); }

int32_t jpeg_start_decompress(jpeg_decompress_struct* cinfo) {
    ev("jpeg_start_decompress", {-1, cinfo->quantize_colors});
    cinfo->output_width = Q("ow");
    cinfo->output_height = Q("oh");
    cinfo->actual_number_of_colors = static_cast<int32_t>(Q("nc"));
    cinfo->colormap.mOffset = gColormap;
    return static_cast<int32_t>(Q("jpeg_start_decompress"));
}

uint32_t jpeg_read_scanlines(jpeg_decompress_struct* cinfo, uint8_t** rows, uint32_t max) {
    ev("jpeg_read_scanlines", {-1, (rows[0] != nullptr && rows[0] == gLastBuffer) ? 1 : 0, static_cast<int32_t>(max),
                               static_cast<int32_t>(cinfo->output_scanline)});
    cinfo->output_scanline += 1;
    return 1;
}

int32_t jpeg_finish_decompress(jpeg_decompress_struct*) { ev("jpeg_finish_decompress", {-1}); return 1; }

// ---- TbRenderArea and TbRAConv fakes
TbRenderArea::TbRenderArea() { ev("RACtor0", {NewLab(this)}); }

TbRenderArea::TbRenderArea(const TbSize& size, const TbPixFormat& format, uint8_t* data, uint32_t stride) {
    ev("RACtor", {NewLab(this), size.width, size.height, static_cast<int32_t>(format.mRedMask),
                  static_cast<int32_t>(format.mGreenMask), static_cast<int32_t>(format.mBlueMask),
                  static_cast<int32_t>(format.mAlphaMask), format.mBitsPerPixel, format.mPalettised,
                  data != nullptr ? 1 : 0, static_cast<int32_t>(stride)});
}

TbResult TbRenderArea::AllocMemory() {
    ev("AllocMemory", {Lab(this)});
    mData = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x400));
    gLastBuffer = mData;
    TbResult r;
    r.mCode = 0;
    return r;
}

void TbRenderArea::FreeMemory() { ev("FreeMemory", {Lab(this)}); }

TbResult TbRenderArea::SetupClientRA(TbRenderArea& client, const TbPoint& origin, const TbSize& size) const {
    ev("SetupClientRA", {Lab(this), Lab(&client), origin.x, origin.y, size.width, size.height});
    TbResult r;
    r.mCode = 0;
    return r;
}

TbResult TbRAConv::Move(TbRenderArea& destination, TbRenderArea& source, TbPalette* palette) {
    ev("Move", {-1, Lab(&destination), Lab(&source), palette != nullptr ? 1 : 0});
    TbResult r;
    r.mCode = 0;
    return r;
}

TbRAConv::~TbRAConv() { ev("TbRAConvDtor", {-1, -1}); }

extern "C" {

uint64_t tpw_jpeg_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"fmt8", &TbPixFormat8},
        {"cmap", &gColormap},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The Ptr32 of an arena block (what the Mac's 32-bit pointer words stand for on the host).
uint32_t tpw_jpeg_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<const void*>(address)); }

uint64_t tpw_jpeg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A new case: the stack objects forgotten, the labels from 100 again.
void tpw_jpeg_reset() {
    gObjects.clear();
    gNextLabel = 100;
    gLastBuffer = nullptr;
    gColormap = 0;
}

// A file object whose Seek is the fake.
uint64_t tpw_jpeg_newfile() {
    void* block = tpw::ArenaAllocZeroed(sizeof(FakeFile) + 16);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (block) FakeFile()));
}

int32_t tpw_jpeg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(jpeg_decompress_struct));
    out[n++] = static_cast<int64_t>(offsetof(jpeg_decompress_struct, image_width));
    out[n++] = static_cast<int64_t>(offsetof(jpeg_decompress_struct, quantize_colors));
    out[n++] = static_cast<int64_t>(offsetof(jpeg_decompress_struct, output_width));
    out[n++] = static_cast<int64_t>(offsetof(jpeg_decompress_struct, actual_number_of_colors));
    out[n++] = static_cast<int64_t>(offsetof(jpeg_decompress_struct, output_scanline));
    out[n++] = static_cast<int64_t>(sizeof(TbRenderArea));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mDepth));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mPalettised));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mData));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mWidth));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mPictureHeight));
    out[n++] = static_cast<int64_t>(offsetof(TbRenderArea, mStride));
    out[n++] = static_cast<int64_t>(sizeof(TbJPEGLoader));
    return n;
}

int32_t tpw_jpeg_hdr(uint64_t loader, uint64_t file, uint64_t area) {
    return reinterpret_cast<TbJPEGLoader*>(loader)->ReadHeaderInfo(*reinterpret_cast<CFile*>(file),
                                                                   *reinterpret_cast<TbRenderArea*>(area)).mCode;
}

int32_t tpw_jpeg_data(uint64_t loader, uint64_t description, uint64_t file, uint64_t unused) {
    return reinterpret_cast<TbJPEGLoader*>(loader)->ReadData(*reinterpret_cast<TbRenderArea*>(description),
                                                             *reinterpret_cast<CFile*>(file),
                                                             *reinterpret_cast<TbRenderArea*>(unused)).mCode;
}

uint32_t tpw_jpeg_valid(uint64_t loader, uint64_t file) {
    return reinterpret_cast<TbJPEGLoader*>(loader)->ValidImage(*reinterpret_cast<CFile*>(file)) ? 1 : 0;
}

}  // extern "C"
