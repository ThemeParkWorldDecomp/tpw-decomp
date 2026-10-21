# Unit w48: creating and deleting a player on disk, the global master save's write-back, SetEasyModeUser and the lobby's SendQuit.
# The library is test_FSSystem's (the real string helpers, FS_DelTree, the player slots' paths) with the unit's four files; the
# unit file includes unit_FSSystem.cpp for its fakes.
set(TPW_UNIT_FSUserSlots src/game/save/FS_Registry.cpp src/game/save/FS_System.cpp src/game/ui/3DFont.cpp
    src/game/save/FS_UserInfo.cpp src/game/save/FS_MasterSave.cpp src/game/online/lobby.cpp)
set(TPW_UNIT_FSUserSlots_KEEP
  __Z10FS_DelTreeRK23TbDynamicStringTemplateIcE   # FS_DelTree(TbDynamicStringTemplate<char> const&)
  __Z10TPUniStrEqRK23TbDynamicStringTemplateIDsES2_   # TPUniStrEq(TbDynamicStringTemplate<char16_t> const&, TbDynamicStringTemplate<char16_t> const&)
  __Z11TPFile_OpenPKcS0_   # TPFile_Open(char const*, char const*)
  __Z11TPFile_ReadPvjjPN3bfl10FileSystem5CFileE   # TPFile_Read(void*, unsigned int, unsigned int, bfl::FileSystem::CFile*)
  __Z11TPFile_SeekPN3bfl10FileSystem5CFileEjj   # TPFile_Seek(bfl::FileSystem::CFile*, unsigned int, unsigned int)
  __Z11TPFile_SizePN3bfl10FileSystem5CFileE   # TPFile_Size(bfl::FileSystem::CFile*)
  __Z12FS_IsWadFileRKN3bfl10FileSystem9CFileInfoE   # FS_IsWadFile(bfl::FileSystem::CFileInfo const&)
  __Z12TPFile_ClosePN3bfl10FileSystem5CFileE   # TPFile_Close(bfl::FileSystem::CFile*)
  __Z15FS_FindFilePathRK23TbDynamicStringTemplateIcERS0_   # FS_FindFilePath(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char>&)
  __Z15FS_ReadRegistryv   # FS_ReadRegistry()
  __Z15RemoveExtensionRK23TbDynamicStringTemplateIcERS0_   # RemoveExtension(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char>&)
  __Z17FS_IsSubDirectoryRKN3bfl10FileSystem9CFileInfoE   # FS_IsSubDirectory(bfl::FileSystem::CFileInfo const&)
  __Z17StringToUniStringR23TbDynamicStringTemplateIDsERKS_IcE   # StringToUniString(TbDynamicStringTemplate<char16_t>&, TbDynamicStringTemplate<char> const&)
  __Z17UniStringToStringR23TbDynamicStringTemplateIcERKS_IDsE   # UniStringToString(TbDynamicStringTemplate<char>&, TbDynamicStringTemplate<char16_t> const&)
  __Z19TPConvertTPLangDefsj   # TPConvertTPLangDefs(unsigned int)
  __Z23FS_ReadLanguageRegistryv   # FS_ReadLanguageRegistry()
  __Z24FS_AddDirectorySeparatorR23TbDynamicStringTemplateIcE   # FS_AddDirectorySeparator(TbDynamicStringTemplate<char>&)
  __Z7TPStrEqRK23TbDynamicStringTemplateIcES2_   # TPStrEq(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __Z8TPStrGtrRK23TbDynamicStringTemplateIcES2_   # TPStrGtr(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN11CFSUserInfo13GameLoginInitEv   # CFSUserInfo::GameLoginInit()
  __ZN11CFSUserInfo15ReturnUsersPathEv   # CFSUserInfo::ReturnUsersPath()
  __ZN11CFSUserInfo16ReturnOnlinePathEv   # CFSUserInfo::ReturnOnlinePath()
  __ZN11CFSUserInfo22CreateUsersDirectoriesERK23TbDynamicStringTemplateIDsEi   # CFSUserInfo::CreateUsersDirectories(TbDynamicStringTemplate<char16_t> const&, int)
  __ZN11CFSUserInfo24GetGameLoginSlotUserNameE19EFSUserInfoGameUser   # CFSUserInfo::GetGameLoginSlotUserName(EFSUserInfoGameUser)
  __ZN11CFSUserInfo24ReturnUsersPathWithThemeEv   # CFSUserInfo::ReturnUsersPathWithTheme()
  __ZN11CFSUserInfo25DeleteGameLoginUserInSlotE19EFSUserInfoGameUser   # CFSUserInfo::DeleteGameLoginUserInSlot(EFSUserInfoGameUser)
  __ZN11CFSUserInfo25GetNumValidGameLoginSlotsEv   # CFSUserInfo::GetNumValidGameLoginSlots()
  __ZN11CFSUserInfo25ReturnPathToSaveDirectoryEv   # CFSUserInfo::ReturnPathToSaveDirectory()
  __ZN11CFSUserInfo26ReturnPathToUsersDirectoryEv   # CFSUserInfo::ReturnPathToUsersDirectory()
  __ZN11CFSUserInfo27IsGameLoginSlotUserEasyModeE19EFSUserInfoGameUser   # CFSUserInfo::IsGameLoginSlotUserEasyMode(EFSUserInfoGameUser)
  __ZN11CFSUserInfo28CreateNewGameLoginUserInSlotE19EFSUserInfoGameUserRK23TbDynamicStringTemplateIDsEi   # CFSUserInfo::CreateNewGameLoginUserInSlot(EFSUserInfoGameUser, TbDynamicStringTemplate<char16_t> const&, int)
  __ZN11CFSUserInfo28InternalSaveGlobalMasterSaveE19EFSUserInfoGameUser   # CFSUserInfo::InternalSaveGlobalMasterSave(EFSUserInfoGameUser)
  __ZN11CFSUserInfo28ReturnGameLoginSlotUsersPathE19EFSUserInfoGameUser   # CFSUserInfo::ReturnGameLoginSlotUsersPath(EFSUserInfoGameUser)
  __ZN11CFSUserInfo29CheckAllThemeDirectoriesExistERK23TbDynamicStringTemplateIDsE   # CFSUserInfo::CheckAllThemeDirectoriesExist(TbDynamicStringTemplate<char16_t> const&)
  __ZN11CFSUserInfo37ReturnGameLoginSlotUsersDirectoryNameE19EFSUserInfoGameUser   # CFSUserInfo::ReturnGameLoginSlotUsersDirectoryName(EFSUserInfoGameUser)
  __ZN11CFSUserInfoC1Ev   # CFSUserInfo::CFSUserInfo()
  __ZN11CFSUserInfoD1Ev   # CFSUserInfo::~CFSUserInfo()
  __ZN15CSYS_TPFileInfo11SetDataPathERK23TbDynamicStringTemplateIcE   # CSYS_TPFileInfo::SetDataPath(TbDynamicStringTemplate<char> const&)
  __ZN15CSYS_TPFileInfo12SetLevelPathERK23TbDynamicStringTemplateIcE   # CSYS_TPFileInfo::SetLevelPath(TbDynamicStringTemplate<char> const&)
  __ZN15CSYS_TPFileInfo12SetLocalPathERK23TbDynamicStringTemplateIcE   # CSYS_TPFileInfo::SetLocalPath(TbDynamicStringTemplate<char> const&)
  __ZN15CSYS_TPFileInfo13SetRemotePathERK23TbDynamicStringTemplateIcE   # CSYS_TPFileInfo::SetRemotePath(TbDynamicStringTemplate<char> const&)
  __ZN15CSYS_TPFileInfo15SetLanguagePathERK23TbDynamicStringTemplateIcE   # CSYS_TPFileInfo::SetLanguagePath(TbDynamicStringTemplate<char> const&)
  __ZN19CFSGlobalMasterSave15SetEasyModeUserEi   # CFSGlobalMasterSave::SetEasyModeUser(int)
  __ZN8CLBLobby8SendQuitEv   # CLBLobby::SendQuit()
  __ZNK15CSYS_TPFileInfo11GetDataPathEv   # CSYS_TPFileInfo::GetDataPath() const
  __ZNK15CSYS_TPFileInfo12GetLevelPathEv   # CSYS_TPFileInfo::GetLevelPath() const
  __ZNK15CSYS_TPFileInfo12GetLocalPathEv   # CSYS_TPFileInfo::GetLocalPath() const
  __ZNK15CSYS_TPFileInfo13GetRemotePathEv   # CSYS_TPFileInfo::GetRemotePath() const
  _gFSRegistryLanguage
  _gFSRegistryLanguages
  _gFSRegistryTitle
  _gFSUserInfoGmsFile
  _gFSUserInfoUpload
  _gLanguageName
  _gTPFileInfo
  _gUserInfo
)
