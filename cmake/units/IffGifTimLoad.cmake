# Unit w223: IFF, GIF and TIM picture readers.
set(TPW_UNIT_IffGifTimLoad src/bullfrog/AndyGLL.cpp src/bullfrog/Lbgfsave.cpp src/bullfrog/LbDrawText.cpp
    src/bullfrog/TbRenderArea.cpp src/bullfrog/TbPixFmt.cpp)
set(TPW_UNIT_IffGifTimLoad_KEEP
  _LbgfsaveTim16
  _LbgfsaveTim24
  _TbPixFormat16
  _TbPixFormat24
  _TbPixFormat32
  _TbPixFormat8
  __Z21LbPalette_FromCompactP9TbPalettePKhj   # LbPalette_FromCompact(TbPalette*, unsigned char const*, unsigned int)
  __ZN10TbMaskInfo3SetEjj   # TbMaskInfo::Set(unsigned int, unsigned int)
  __ZN11TbGIFLoader10ValidImageERN3bfl10FileSystem5CFileE   # TbGIFLoader::ValidImage(bfl::FileSystem::CFile&)
  __ZN11TbGIFLoader14ReadHeaderInfoERN3bfl10FileSystem5CFileER12TbRenderArea   # TbGIFLoader::ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbGIFLoader8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileERS0_   # TbGIFLoader::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbGIFLoaderC1Ev   # TbGIFLoader::TbGIFLoader()
  __ZN11TbGIFLoaderD1Ev   # TbGIFLoader::~TbGIFLoader()
  __ZN11TbIFFLoader10ValidImageERN3bfl10FileSystem5CFileE   # TbIFFLoader::ValidImage(bfl::FileSystem::CFile&)
  __ZN11TbIFFLoader14ReadHeaderInfoERN3bfl10FileSystem5CFileER12TbRenderArea   # TbIFFLoader::ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbIFFLoader8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileERS0_   # TbIFFLoader::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbIFFLoaderC1Ev   # TbIFFLoader::TbIFFLoader()
  __ZN11TbIFFLoaderD1Ev   # TbIFFLoader::~TbIFFLoader()
  __ZN11TbPixelCaps3SetERK11TbPixFormat   # TbPixelCaps::Set(TbPixFormat const&)
  __ZN11TbTGALoader10ValidImageERN3bfl10FileSystem5CFileE   # TbTGALoader::ValidImage(bfl::FileSystem::CFile&)
  __ZN11TbTGALoader14ReadHeaderInfoERN3bfl10FileSystem5CFileER12TbRenderArea   # TbTGALoader::ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbTGALoader8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileERS0_   # TbTGALoader::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbTGALoaderC1Ev   # TbTGALoader::TbTGALoader()
  __ZN11TbTGALoaderD1Ev   # TbTGALoader::~TbTGALoader()
  __ZN11TbTIMLoader10ValidImageERN3bfl10FileSystem5CFileE   # TbTIMLoader::ValidImage(bfl::FileSystem::CFile&)
  __ZN11TbTIMLoader14ReadHeaderInfoERN3bfl10FileSystem5CFileER12TbRenderArea   # TbTIMLoader::ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbTIMLoader8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileERS0_   # TbTIMLoader::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN11TbTIMLoaderC1Ev   # TbTIMLoader::TbTIMLoader()
  __ZN11TbTIMLoaderD1Ev   # TbTIMLoader::~TbTIMLoader()
  __ZN11_AndyGIFGLL12ReadNextLineERN3bfl10FileSystem5CFileE   # _AndyGIFGLL::ReadNextLine(bfl::FileSystem::CFile&)
  __ZN11_AndyGIFGLL13ReadFirstLineERN3bfl10FileSystem5CFileE   # _AndyGIFGLL::ReadFirstLine(bfl::FileSystem::CFile&)
  __ZN11_AndyGIFGLL14ReadHeaderInfoERN3bfl10FileSystem5CFileEP11TbGIFLoader   # _AndyGIFGLL::ReadHeaderInfo(bfl::FileSystem::CFile&, TbGIFLoader*)
  __ZN11_AndyGIFGLL15_GifGetNextCodeERN3bfl10FileSystem5CFileE   # _AndyGIFGLL::_GifGetNextCode(bfl::FileSystem::CFile&)
  __ZN11_AndyGIFGLL15_GifReadPaletteERN3bfl10FileSystem5CFileEP9TbPalette   # _AndyGIFGLL::_GifReadPalette(bfl::FileSystem::CFile&, TbPalette*)
  __ZN11_AndyGIFGLL19GetSourceLineLengthEv   # _AndyGIFGLL::GetSourceLineLength()
  __ZN11_AndyGIFGLL3sB1E   # _AndyGIFGLL::sB1
  __ZN11_AndyGIFGLL8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileEP11TbGIFLoader   # _AndyGIFGLL::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbGIFLoader*)
  __ZN11_AndyGIFGLL8sPaletteE   # _AndyGIFGLL::sPalette
  __ZN11_AndyGIFGLL9WriteLineEv   # _AndyGIFGLL::WriteLine()
  __ZN11_AndyIFFGLL12ReadNextLineERN3bfl10FileSystem5CFileE   # _AndyIFFGLL::ReadNextLine(bfl::FileSystem::CFile&)
  __ZN11_AndyIFFGLL13ReadFirstLineERN3bfl10FileSystem5CFileE   # _AndyIFFGLL::ReadFirstLine(bfl::FileSystem::CFile&)
  __ZN11_AndyIFFGLL14ReadHeaderInfoERN3bfl10FileSystem5CFileEP11TbIFFLoader   # _AndyIFFGLL::ReadHeaderInfo(bfl::FileSystem::CFile&, TbIFFLoader*)
  __ZN11_AndyIFFGLL19GetSourceLineLengthEv   # _AndyIFFGLL::GetSourceLineLength()
  __ZN11_AndyIFFGLL6sChunkE   # _AndyIFFGLL::sChunk
  __ZN11_AndyIFFGLL8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileEP11TbIFFLoader   # _AndyIFFGLL::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbIFFLoader*)
  __ZN12TbRenderArea10FreeMemoryEv   # TbRenderArea::FreeMemory()
  __ZN12TbRenderArea11AllocMemoryEv   # TbRenderArea::AllocMemory()
  __ZN12TbRenderAreaC1EPK9TbSurface   # TbRenderArea::TbRenderArea(TbSurface const*)
  __ZN12TbRenderAreaC1ERK6TbSizeRK11TbPixFormatPhj   # TbRenderArea::TbRenderArea(TbSize const&, TbPixFormat const&, unsigned char*, unsigned int)
  __ZN12TbRenderAreaC1Ev   # TbRenderArea::TbRenderArea()
  __ZN19TbGraphicFileLoader10ReadHeaderEPKcS1_P12TbRenderArea   # TbGraphicFileLoader::ReadHeader(char const*, char const*, TbRenderArea*)
  __ZN19TbGraphicFileLoader10ReadHeaderERN3bfl10FileSystem5CFileEP12TbRenderArea   # TbGraphicFileLoader::ReadHeader(bfl::FileSystem::CFile&, TbRenderArea*)
  __ZN19TbGraphicFileLoader4LoadERK12TbRenderAreaPKcS4_PS0_   # TbGraphicFileLoader::Load(TbRenderArea const&, char const*, char const*, TbRenderArea*)
  __ZN19TbGraphicFileLoader4LoadERK12TbRenderAreaRN3bfl10FileSystem5CFileEPS0_   # TbGraphicFileLoader::Load(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea*)
  __ZN19TbGraphicFileLoader7IsValidERN3bfl10FileSystem5CFileE   # TbGraphicFileLoader::IsValid(bfl::FileSystem::CFile&)
  __ZN3bfl10FileSystem15_TbBufferedFileD1Ev   # bfl::FileSystem::_TbBufferedFile::~_TbBufferedFile()
  __ZN7_TgaGLL10TGALoadRLEERN3bfl10FileSystem5CFileE   # _TgaGLL::TGALoadRLE(bfl::FileSystem::CFile&)
  __ZN7_TgaGLL12ReadNextLineERN3bfl10FileSystem5CFileE   # _TgaGLL::ReadNextLine(bfl::FileSystem::CFile&)
  __ZN7_TgaGLL13ReadFirstLineERN3bfl10FileSystem5CFileE   # _TgaGLL::ReadFirstLine(bfl::FileSystem::CFile&)
  __ZN7_TgaGLL14ReadHeaderInfoERN3bfl10FileSystem5CFileEP11TbTGALoader   # _TgaGLL::ReadHeaderInfo(bfl::FileSystem::CFile&, TbTGALoader*)
  __ZN7_TgaGLL15ReverseScanLineEPh   # _TgaGLL::ReverseScanLine(unsigned char*)
  __ZN7_TgaGLL19GetSourceLineLengthEv   # _TgaGLL::GetSourceLineLength()
  __ZN7_TgaGLL8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileEP11TbTGALoader   # _TgaGLL::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbTGALoader*)
  __ZN7_TimGLL12ReadNextLineERN3bfl10FileSystem5CFileE   # _TimGLL::ReadNextLine(bfl::FileSystem::CFile&)
  __ZN7_TimGLL13ReadFirstLineERN3bfl10FileSystem5CFileE   # _TimGLL::ReadFirstLine(bfl::FileSystem::CFile&)
  __ZN7_TimGLL14ReadHeaderInfoERN3bfl10FileSystem5CFileEP11TbTIMLoader   # _TimGLL::ReadHeaderInfo(bfl::FileSystem::CFile&, TbTIMLoader*)
  __ZN7_TimGLL19GetSourceLineLengthEv   # _TimGLL::GetSourceLineLength()
  __ZN7_TimGLL8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileEP11TbTIMLoader   # _TimGLL::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbTIMLoader*)
  __ZN8_AndyGLL6DoReadERK12TbRenderAreaRN3bfl10FileSystem5CFileEP19TbGraphicFileLoader   # _AndyGLL::DoRead(TbRenderArea const&, bfl::FileSystem::CFile&, TbGraphicFileLoader*)
  __ZN8_AndyGLL9WriteLineEv   # _AndyGLL::WriteLine()
  __ZNK12TbRenderArea10GetMemSizeEv   # TbRenderArea::GetMemSize() const
  __ZNK12TbRenderArea13SetupClientRAERS_RK6TbRect   # TbRenderArea::SetupClientRA(TbRenderArea&, TbRect const&) const
  __ZNK12TbRenderArea13SetupClientRAERS_RK7TbPointRK6TbSize   # TbRenderArea::SetupClientRA(TbRenderArea&, TbPoint const&, TbSize const&) const
)
set(TPW_UNIT_IffGifTimLoad_OWN_CALLLOG ON)
