# Unit v5043: the upload job and the city unpublish request (src/game/online/FS_Online.cpp). The callees outside the
# unit are recording fakes in tests/unit_CFSOnlineSystem_LoggedInRequestUpload.cpp, which also defines the globals of other files and the
# operator new / delete (so the arena, which defines them, is not linked).
set(TPW_UNIT_CFSOnlineSystem_LoggedInRequestUpload src/game/online/FS_Online.cpp)
set(TPW_UNIT_CFSOnlineSystem_LoggedInRequestUpload_KEEP
  __ZN15CFSOnlineSystem13RequestUploadEPKciPKDsS3_iPFviPKvES5_   # CFSOnlineSystem::RequestUpload(char const*, int, char16_t const*, char16_t const*, int, void (*)(int, void const*), void const*)
  __ZN15CFSOnlineSystem13UnpublishParkEi   # CFSOnlineSystem::UnpublishPark(int)
  __ZN15CFSOnlineSystem20ReturnUploadFilenameEv   # CFSOnlineSystem::ReturnUploadFilename()
  __ZN15CFSOnlineSystem21LoggedInRequestUploadEv   # CFSOnlineSystem::LoggedInRequestUpload()
  __ZN15CFSOnlineSystem30UploadServerConnectionCallbackEiiPvS0_   # CFSOnlineSystem::UploadServerConnectionCallback(int, int, void*, void*)
)
