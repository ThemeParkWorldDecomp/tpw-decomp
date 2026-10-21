# Unit v5528: TbJPEGLoader::ReadHeaderInfo, ReadData, ValidImage (src/bullfrog/TbJPEGSaver.cpp). The callees outside the unit are
# recording fakes in tests/unit_TbJPEGLoader_ReadHeaderInfo.cpp, which also defines the globals of other files.
set(TPW_UNIT_TbJPEGLoader_ReadHeaderInfo src/bullfrog/TbJPEGSaver.cpp platform/arena.cpp)
set(TPW_UNIT_TbJPEGLoader_ReadHeaderInfo_KEEP
  __ZN12TbJPEGLoader10ValidImageERN3bfl10FileSystem5CFileE   # TbJPEGLoader::ValidImage(bfl::FileSystem::CFile&)
  __ZN12TbJPEGLoader14ReadHeaderInfoERN3bfl10FileSystem5CFileER12TbRenderArea   # TbJPEGLoader::ReadHeaderInfo(bfl::FileSystem::CFile&, TbRenderArea&)
  __ZN12TbJPEGLoader8ReadDataERK12TbRenderAreaRN3bfl10FileSystem5CFileERS0_   # TbJPEGLoader::ReadData(TbRenderArea const&, bfl::FileSystem::CFile&, TbRenderArea&)
)
