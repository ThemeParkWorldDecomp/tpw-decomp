# Unit v5375: CTPItemListResource::OpenFile (two overloads) and the destructor (src/game/TPResource.cpp).
# The callees outside the unit are recording fakes in tests/unit_CTPItemListResource_OpenFile.cpp, which also defines the globals of other files.
set(TPW_UNIT_CTPItemListResource_OpenFile src/game/TPResource.cpp platform/arena.cpp)
set(TPW_UNIT_CTPItemListResource_OpenFile_KEEP
  __ZN19CTPItemListResource8OpenFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcERNS1_12CFileStorageEj   # CTPItemListResource::OpenFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&, bfl::FileSystem::CFileStorage&, unsigned int)
  __ZN19CTPItemListResource8OpenFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcEj   # CTPItemListResource::OpenFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&, unsigned int)
  __ZN19CTPItemListResourceD1Ev   # CTPItemListResource::~CTPItemListResource()
)
