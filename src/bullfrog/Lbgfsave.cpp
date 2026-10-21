// Lbgfsave.cpp: name proven by __sinit_Lbgfsave_cpp in the Bullfrog library.

#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "helpers/AndyGLLHelpers.h"
#include "helpers/LbgfsaveHelpers.h"
#include "LbFile.h"
#include "platform/macdoze.h"
#include "platform/samsutils.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"
#include "TbRaConvert.h"
#include <algorithm>
#include <cstring>
#include <new>

// The Bullfrog library's picture loading (bullfrog symbols shared 2.04b3): TbRenderArea (0x10016560..0x10016ad0),
// the loader base class (0x10022870..0x100230cc), the TGA loader (0x10023a80..0x10023f30), the TGA line reader
// (0x10021228..0x10021d28) and the line reader's base with LbPalette_FromCompact (0x1001de44, 0x1001e2f4,
// 0x1001a2f8) and the buffered file's destructor (0x10023024), which the loader needs and which are not in the unit's first
// list. See include/TbGraphicFile.h.
//
// The library tests the results of the calls it makes against words in its data that nothing writes: each comparison is
// (result >= 0) == (word == 0), or its negation where the word is -1, so a negative result is a failure in every one. The
// words: 0x10072f94, 0x10072f98, 0x10072f9c and 0x10072fa0 in the loader base class (the two opens, the header read and the
// data read), 0x10072dfc in _TgaGLL::ReadData, 0x10072e64 (-1) in ReadNextLine and 0x10072d84, 0x10072d88 and 0x10072d8c in
// DoRead.

using bfl::FileSystem::CFile;
using bfl::FileSystem::CWadFile;
using bfl::FileSystem::_TbBufferedFile;

// ---- TbGraphicFileLoader

TbResult TbGraphicFileLoader::ReadHeader(const char* path, const char* second, TbRenderArea* area) {
    CWadFile file;
    if (!file.Open(path, second).Succeeded()) return TbResult{-1};
    return ReadHeader(file, area);
}

namespace bfl::FileSystem {

CWadFile::~CWadFile() {}
} // namespace bfl::FileSystem

TbResult TbGraphicFileLoader::ReadHeader(CFile& file, TbRenderArea* area) {
    if (file.Seek(0, 0) != 0) return TbResult{-1};
    TbTGALoader* loader = static_cast<TbTGALoader*>(this);
    if (!Succeeded(loader->ReadHeaderInfo(file, mArea))) return TbResult{-1};
    if (area != nullptr) *area = mArea;
    return TbResult{0};
}

TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, const char* path, const char* second, TbRenderArea* other) {
    CWadFile file;
    if (!file.Open(path, second).Succeeded()) return TbResult{-1};
    return Load(area, file, other);
}

TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, CFile& file, TbRenderArea* other) {
    _TbBufferedFile buffered;
    buffered.Open(file, 0x400);
    if (buffered.Seek(0, 0) != 0) return TbResult{-1};
    if (area.mData == nullptr) return TbResult{-1};
    TbTGALoader* loader = static_cast<TbTGALoader*>(this);
    if (!Succeeded(loader->ReadData(area, buffered, mArea))) return TbResult{-1};
    if (other != nullptr) *other = mArea;
    return TbResult{0};
}

// ---- _TbBufferedFile

// The buffered file's destructor (0x10023024, Lbgfsave.cpp: it sits among the loader's functions): the file base class'
// destructor, and the object itself freed when the flag is positive (the compiler's `delete`). Load makes one on its stack.
bfl::FileSystem::_TbBufferedFile::~_TbBufferedFile() {}
bool TbGraphicFileLoader::IsValid(CFile& file) {
    if (file.Seek(0, 0) != 0) return false;
    return static_cast<TbTGALoader*>(this)->ValidImage(file);
}

// The BMP and PCX picture loaders of Lbgfsave.cpp (bullfrog symbols shared), unit w220.
// Succeeded, SetFormat and BytesOfPixel copy the local helpers of bfl_graphicfile.cpp, lines 26..44.

// 0x10023190
TbResult TbBMPLoader::ReadHeaderInfo(CFile& file, TbRenderArea&) {
    _AndyBMPGLL reader;
    return reader.ReadHeaderInfo(file, this);
}

// 0x10023234
TbResult TbBMPLoader::ReadData(const TbRenderArea& area, CFile& file, TbRenderArea&) {
    _AndyBMPGLL reader;
    return reader.ReadData(area, file, this);
}

// 0x100232e0
bool TbBMPLoader::ValidImage(CFile& file) {
    if (file.GetSize() < 2) return false;
    uint8_t magic[2];
    if (static_cast<uint32_t>(file.Read(magic, 2)) != 2) return false;
    return Be16(magic) == 0x4d42;
}

// IFF, GIF and TIM readers of Lbgfsave.cpp (bullfrog symbols shared), unit w223.

// 0x100233b0
TbResult TbIFFLoader::ReadHeaderInfo(CFile& f, TbRenderArea&) { _AndyIFFGLL r; return r.ReadHeaderInfo(f, this); }
// 0x10023454
TbResult TbIFFLoader::ReadData(const TbRenderArea& a, CFile& f, TbRenderArea&) { _AndyIFFGLL r; return r.ReadData(a, f, this); }
// 0x10023500
bool TbIFFLoader::ValidImage(CFile& f) { uint8_t h[40]; return uint32_t(f.GetSize()) >= 40 && f.Read(h, 40) == 40 && IffMagic(h); }

// 0x10023630
TbResult TbPCXLoader::ReadHeaderInfo(CFile& file, TbRenderArea&) {
    _AndyPCXGLL reader;
    return reader.ReadHeaderInfo(file, this);
}

// 0x100236d4
TbResult TbPCXLoader::ReadData(const TbRenderArea& area, CFile& file, TbRenderArea&) {
    _AndyPCXGLL reader;
    return reader.ReadData(area, file, this);
}

// 0x10023780
bool TbPCXLoader::ValidImage(CFile& file) {
    if (file.GetSize() < 1) return false;
    uint8_t magic;
    if (static_cast<uint32_t>(file.Read(&magic, 1)) != 1) return false;
    return magic == 0xa;
}

// 0x10023850
TbResult TbGIFLoader::ReadHeaderInfo(CFile& f, TbRenderArea&) { _AndyGIFGLL r; return r.ReadHeaderInfo(f, this); }
// 0x100238f4
TbResult TbGIFLoader::ReadData(const TbRenderArea& a, CFile& f, TbRenderArea&) { _AndyGIFGLL r; return r.ReadData(a, f, this); }
// 0x100239a0
bool TbGIFLoader::ValidImage(CFile& f) { char h[3]; return f.GetSize() >= 3 && f.Read(h, 3) == 3 && std::strncmp(h, "GIF", 3) == 0; }

// ---- TbTGALoader

TbTGALoader::TbTGALoader() {}

TbTGALoader::~TbTGALoader() {}

TbResult TbTGALoader::ReadHeaderInfo(CFile& file, TbRenderArea&) {
    _TgaGLL reader;
    return reader.ReadHeaderInfo(file, this);
}

TbResult TbTGALoader::ReadData(const TbRenderArea& area, CFile& file, TbRenderArea&) {
    _TgaGLL reader;
    return reader.ReadData(area, file, this);
}

// A TGA 2.0 footer ("TRUEVISION-XFILE." and a zero) at the end of the file is taken for a TGA file whatever the
// header says; failing that the header is looked at: a colour map type of 0 or 1, an image type of 0 to 3 or 9 to
// 11 and a depth of 8, 15, 16, 24 or 32.
bool TbTGALoader::ValidImage(CFile& file) {
    if (file.GetSize() < 0x12) return false;
    const int32_t headerStart = file.GetSize() - 0x12;
    if (file.Seek(static_cast<uint32_t>(-0x12), 2) != headerStart) return false;
    char footer[0x12];
    if (static_cast<uint32_t>(file.Read(footer, 0x12)) != 0x12) return false;
    if (std::strncmp(footer, "TRUEVISION-XFILE.", 0x12) == 0) return true;
    if (file.Seek(0, 0) != 0) return false;
    uint8_t header[0x12];
    if (static_cast<uint32_t>(file.Read(header, 0x12)) != 0x12) return false;
    const uint8_t mapType = header[1];
    const uint8_t imageType = header[2];
    const uint8_t depth = header[0x10];
    const bool mapOk = mapType == 0 || mapType == 1;
    const bool typeOk = imageType == 0 || imageType == 1 || imageType == 2 || imageType == 3 || imageType == 9 ||
                        imageType == 10 || imageType == 11;
    const bool depthOk = depth == 8 || depth == 15 || depth == 16 || depth == 24 || depth == 32;
    return mapOk && typeOk && depthOk;
}

// 0x10023f7c
TbResult TbTIMLoader::ReadHeaderInfo(CFile& f, TbRenderArea&) { _TimGLL r; return r.ReadHeaderInfo(f, this); }
// 0x10024020
TbResult TbTIMLoader::ReadData(const TbRenderArea& a, CFile& f, TbRenderArea&) { _TimGLL r; return r.ReadData(a, f, this); }
// 0x100240cc
bool TbTIMLoader::ValidImage(CFile& f) { uint8_t h[4]; return f.GetSize() >= 4 && f.Read(h, 4) == 4 && Be32(h) == 0x10; }
