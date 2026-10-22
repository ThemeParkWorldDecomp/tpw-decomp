// Mac Roman (the classic Mac OS text encoding of HFS and MacBinary names) to UTF-8, with the same table as Python's
// "mac_roman" codec (the Euro sign at 0xDB, the Apple logo at 0xF0), which devtools/import_originals.py uses.
#pragma once

#include <string>

namespace tpw::assets {

// Converts bytes to UTF-8. When asFileName is set, a '/' becomes ':' (the Mac's own spelling of it on a POSIX volume,
// hfsdump's rule) so the name is one path component; HFS itself never holds a ':'.
std::string MacRomanToUtf8(const char* bytes, size_t n, bool asFileName);

}  // namespace tpw::assets
