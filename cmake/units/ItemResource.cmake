# Item resources, with recorded storage and string-helper fakes in the unit library.
set(TPW_UNIT_ItemResource src/game/TPResource.cpp)
set(TPW_UNIT_ItemResource_KEEP
  __ZN15CTPItemResource10GetSamFileERN3bfl10FileSystem5CFileE   # CTPItemResource::GetSamFile(bfl::FileSystem::CFile&)
  __ZN15CTPItemResource11GetTextFileERN3bfl10FileSystem5CFileE   # CTPItemResource::GetTextFile(bfl::FileSystem::CFile&)
  __ZN15CTPItemResource12HasParticlesEv   # CTPItemResource::HasParticles()
  __ZN15CTPItemResource14GetEasySamFileERN3bfl10FileSystem5CFileE   # CTPItemResource::GetEasySamFile(bfl::FileSystem::CFile&)
  __ZN15CTPItemResource16GetOnlineSamFileERN3bfl10FileSystem5CFileE   # CTPItemResource::GetOnlineSamFile(bfl::FileSystem::CFile&)
  __ZN15CTPItemResource17GetCoasterSamFileERN3bfl10FileSystem5CFileE   # CTPItemResource::GetCoasterSamFile(bfl::FileSystem::CFile&)
  __ZN15CTPItemResource8InitItemERKN3bfl10FileSystem9CFileInfoERK23TbDynamicStringTemplateIcE   # CTPItemResource::InitItem(bfl::FileSystem::CFileInfo const&, TbDynamicStringTemplate<char> const&)
  __ZN15CTPItemResource9HasSoundsEv   # CTPItemResource::HasSounds()
  __ZN15CTPItemResourceC1ERKN3bfl10FileSystem9CFileInfoERK23TbDynamicStringTemplateIcE   # CTPItemResource::CTPItemResource(bfl::FileSystem::CFileInfo const&, TbDynamicStringTemplate<char> const&)
  __ZN15CTPItemResourceD1Ev   # CTPItemResource::~CTPItemResource()
  __ZN19CTPItemListResource12RegisterItemER15CTPItemResource   # CTPItemListResource::RegisterItem(CTPItemResource&)
  __ZNK19CTPItemListResource8FindItemERK23TbDynamicStringTemplateIcE   # CTPItemListResource::FindItem(TbDynamicStringTemplate<char> const&) const
)
