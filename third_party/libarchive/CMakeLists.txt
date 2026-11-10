# libarchive 3.8.11, built as a static library from the sources in this folder (see NOTE.md): the readers of 7-Zip and
# RAR (4 and 5, multi-volume included) archives only, for the launcher's importer (assets/archive_source.cpp). Ours, not
# libarchive's own CMakeLists.txt: config.h is written by hand in place of libarchive's configure step, no install rules,
# no probing of the host, so nothing outside this folder (and zlib's and xz's) is looked at.
cmake_minimum_required(VERSION 3.20)

set(TPW_ARCHIVE_SOURCES
  libarchive/archive_acl.c
  libarchive/archive_blake2s_ref.c
  libarchive/archive_blake2sp_ref.c
  libarchive/archive_check_magic.c
  libarchive/archive_crc32.c
  libarchive/archive_entry.c
  libarchive/archive_entry_sparse.c
  libarchive/archive_entry_xattr.c
  libarchive/archive_options.c
  libarchive/archive_ppmd7.c
  libarchive/archive_random.c
  libarchive/archive_rb.c
  libarchive/archive_read.c
  libarchive/archive_read_add_passphrase.c
  libarchive/archive_read_set_options.c
  libarchive/archive_read_support_format_7zip.c
  libarchive/archive_read_support_format_rar.c
  libarchive/archive_read_support_format_rar5.c
  libarchive/archive_string.c
  libarchive/archive_string_sprintf.c
  libarchive/archive_time.c
  libarchive/archive_util.c
  libarchive/archive_virtual.c)
if(WIN32)
  list(APPEND TPW_ARCHIVE_SOURCES libarchive/archive_windows.c)
endif()
add_library(tpw_archive STATIC ${TPW_ARCHIVE_SOURCES})
add_library(LibArchive::LibArchive ALIAS tpw_archive)
# LIBARCHIVE_STATIC: archive.h declares the functions for a static library (on Windows, not as imported from a DLL).
target_compile_definitions(tpw_archive PUBLIC LIBARCHIVE_STATIC)
target_compile_definitions(tpw_archive PRIVATE HAVE_CONFIG_H)
# SYSTEM, so that <archive.h> is this folder's and the warnings of the port's own flags are not raised in the headers.
target_include_directories(tpw_archive SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libarchive)
target_include_directories(tpw_archive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpw_archive PUBLIC LibLZMA::LibLZMA ZLIB::ZLIB)
if(WIN32)
  # archive_random.c's BCryptGenRandom (a system DLL, on the list devtools/win/build.ps1 accepts).
  target_link_libraries(tpw_archive PUBLIC bcrypt)
endif()
set_target_properties(tpw_archive PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99 C_VISIBILITY_PRESET default)
# libarchive's C is not ours to warn about.
target_compile_options(tpw_archive PRIVATE -w)
