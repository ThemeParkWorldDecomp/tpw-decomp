// path_text.h: a std::filesystem::path as narrow text, in place of its string() and generic_string().
//
// On Windows the launcher and the game run in the UTF-8 process code page (launcher/win/tpw_launcher.manifest, from
// Windows 10 1903), where every narrow path, fopen and argv is UTF-8. libc++'s string() narrows with
// WideCharToMultiByte(CP_ACP, ..., &usedDefaultChar), and Microsoft's reference says that call fails for UTF-8 when
// usedDefaultChar is not null, which makes string() throw. u8string() does not go through Windows at all. Elsewhere, and
// on an older Windows, which ignores the manifest, string() it is. devtools/check_path_text.py keeps string() out of the
// launcher and the platform layer.
#pragma once

#include <filesystem>
#include <string>

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned int __stdcall GetACP(void);
#endif

namespace tpw {

inline std::string PathText(const std::filesystem::path& path) {
#ifdef _WIN32
    if (GetACP() == 65001) return path.u8string();
#endif
    return path.string();
}

inline std::string GenericPathText(const std::filesystem::path& path) {
#ifdef _WIN32
    if (GetACP() == 65001) return path.generic_u8string();
#endif
    return path.generic_string();
}

}  // namespace tpw
