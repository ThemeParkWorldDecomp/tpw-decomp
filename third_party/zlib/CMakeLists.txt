# zlib 1.3.2, built as a static library from the sources in this folder (see NOTE.md). Ours, not zlib's own CMakeLists.txt:
# no install rules, no probing of the host, no shared library, so nothing outside this folder is looked at.
cmake_minimum_required(VERSION 3.20)

add_library(tpw_zlib STATIC
  adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c
  trees.c uncompr.c zutil.c)
add_library(ZLIB::ZLIB ALIAS tpw_zlib)
# zconf.h reads these two as the cmake-configured copy of zlib's would define them; unistd.h and stdarg.h are on every host
# the port builds on.
target_compile_definitions(tpw_zlib PUBLIC HAVE_UNISTD_H=1 HAVE_STDARG_H=1)
target_compile_definitions(tpw_zlib PRIVATE ZLIB_BUILD)
# SYSTEM, so that <zlib.h> is this folder's and the warnings of the game's own flags are not raised in the headers.
target_include_directories(tpw_zlib SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Linked into shared test libraries as well as the launcher; zlib's C is not ours to warn about.
set_target_properties(tpw_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET default)
target_compile_options(tpw_zlib PRIVATE -w)
