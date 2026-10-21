// FS_System.cpp: name proven by __sinit_FS_System_cpp in the main application.

#include "FS_System.h"
#include "Game.h"
#include "LbFile.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TPString.h"

// FS_System.cpp (decompiled from the main app of SimTheme Park 2.04b3: 0x10003fd4..0x10005188
// and __sinit_FS_System_cpp at 0x100051e0): the game's file functions, the directory
// helpers and the folder paths (CSYS_TPFileInfo).
//
// Also in this range, inline in the headers: bfl::FileSystem::CFile::Read and Write
// (0x100042dc, 0x10004550), CFileInfo's destructor (0x10004994) and CDiscFileStorage's
// (0x10004a10). CSYS_TPFileInfo's destructor (0x10005288, after the static initialiser) is
// implicit.

CSYS_TPFileInfo gTPFileInfo;   // 0x102b3458 (__sinit_FS_System_cpp)

namespace {

constexpr uint32_t kOpenForReading = 0x80000001;   // the mode TPFile_Open opens with

// Seeks and turns the result into TPFile_Seek's: 0 on success, 1 on failure.
uint32_t SeekResult(bfl::FileSystem::CFile* file, uint32_t offset, uint32_t whence) {
    return file->Seek(offset, whence) < 0 ? 1 : 0;
}

}  // namespace

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    if (name == nullptr) return nullptr;
    bfl::FileSystem::CFile* file = new bfl::FileSystem::CFile;
    if (file == nullptr) return nullptr;
    CTPBaseResource* resource = gGame.mResourceManager->pGetCurrentResource();
    if (resource != nullptr) {
        bfl::FileSystem::CFileStorage* storage = resource->pGetLastFoundFileStorage();
        if (storage != nullptr) {
            // first the storage the last file came from
            TbDynamicStringTemplate<char> qualified;
            TbDynamicStringTemplate<char> folder;
            TbDynamicStringTemplate<char> fileName(name);
            folder = resource->mPath;
            if (QualifyFilename(fileName, folder, qualified) == 1 &&
                storage->OpenFile(*file, qualified.mData, kOpenForReading, nullptr).Succeeded())
                return file;
        }
        if (resource->OpenFile(*file, TbDynamicStringTemplate<char>(name), kOpenForReading) == 1) return file;
    }
    if (gGame.mResourceManager->OpenFile(*file, TbDynamicStringTemplate<char>(name)) == 1) return file;
    delete file;
    return nullptr;
}

uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    return static_cast<uint32_t>(file->Read(dest, size * count));
}

uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    file->Close();
    delete file;
    return 0;
}

uint32_t TPFile_Seek(bfl::FileSystem::CFile* file, uint32_t offset, uint32_t whence) {
    if (whence == 1) return SeekResult(file, offset, 1);
    if (whence == 2) return SeekResult(file, offset, 2);
    if (whence == 0) return SeekResult(file, offset, 0);
    return 1;
}

int32_t TPFile_Size(bfl::FileSystem::CFile* file) { return file->Size(); }

uint8_t FS_FindFilePath(const TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& path) {
    if (TPFile_Open(name.mData, "rb") == nullptr) return 0;
    CTPBaseResource* resource = gGame.mResourceManager->pGetCurrentResource();
    bfl::FileSystem::CFileStorage* storage = resource->pGetLastFoundFileStorage();
    TbDynamicStringTemplate<char> folder(resource->mPath);
    TbDynamicStringTemplate<char> fileName(name);
    if (QualifyFilename(fileName, folder, path) != 1) return 0;
    fileName = path;
    storage->FullPath(path, fileName);
    return 1;
}

bool FS_IsSubDirectory(const bfl::FileSystem::CFileInfo& info) {
    return (info.mAttributes & 0x10) != 0 && info.mName[0] != '.';
}

void FS_DelTree(const TbDynamicStringTemplate<char>& path) {
    bfl::FileSystem::CDiscFileStorage storage;
    if (!storage.Open(path.mData).Succeeded()) return;
    bfl::FileSystem::CFileInfo info;
    if (storage.FindFirst(gTPGlobals.mAllFiles.mData, info).Succeeded()) {
        do {
            if (FS_IsSubDirectory(info)) {
                TbDynamicStringTemplate<char> directory(path);
                directory += gTPGlobals.mSeparator;
                directory += info.mName;
                FS_DelTree(directory);
            } else if (info.mName[0] != '.') {
                storage.Delete(info.mName);
            }
        } while (storage.FindNext(info).Succeeded());
    }
    LbFile_DeleteDirectory(path.mData);
}

bool FS_IsWadFile(const bfl::FileSystem::CFileInfo& info) {
    const TbDynamicStringTemplate<char> name(info.mName);
    const TbDynamicStringTemplate<char>& extension = gTPGlobals.mWadExtension;
    return TPStrEq(name.Right(static_cast<uint32_t>(extension.Length())), extension);
}

void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    const TbDynamicStringTemplate<char>& separator = gTPGlobals.mSeparator;
    if (!TPStrEq(path.Right(static_cast<uint32_t>(separator.Length())), separator)) path += separator;
}

void CSYS_TPFileInfo::SetLocalPath(const TbDynamicStringTemplate<char>& path) { mLocalPath = path; }
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const { return mLocalPath; }
void CSYS_TPFileInfo::SetRemotePath(const TbDynamicStringTemplate<char>& path) { mRemotePath = path; }
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetRemotePath() const { return mRemotePath; }
void CSYS_TPFileInfo::SetDataPath(const TbDynamicStringTemplate<char>& path) { mDataPath = path; }
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const { return mDataPath; }

void CSYS_TPFileInfo::SetLevelPath(const TbDynamicStringTemplate<char>& path) {
    mLevelPath = mDataPath;
    const TbDynamicStringTemplate<char>& separator = gTPGlobals.mSeparator;   // FS_AddDirectorySeparator, inline
    if (!TPStrEq(mLevelPath.Right(static_cast<uint32_t>(separator.Length())), separator)) mLevelPath += separator;
    mLevelPath += path;
}

const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLevelPath() const { return mLevelPath; }

void CSYS_TPFileInfo::SetLanguagePath(const TbDynamicStringTemplate<char>& path) {
    mLanguagePath = mDataPath;
    const TbDynamicStringTemplate<char>& separator = gTPGlobals.mSeparator;
    if (!TPStrEq(mLanguagePath.Right(static_cast<uint32_t>(separator.Length())), separator)) mLanguagePath += separator;
    mLanguagePath += path;
}
