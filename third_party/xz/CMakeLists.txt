# liblzma from xz 5.8.4, built as a static library from the sources in this folder (see NOTE.md): the LZMA and LZMA2
# decoders and their filters only, for libarchive's 7z reader (third_party/libarchive). Ours, not xz's own CMakeLists.txt:
# config.h is written by hand in place of xz's configure step, so nothing outside this folder is looked at.
cmake_minimum_required(VERSION 3.20)

add_library(tpw_lzma STATIC
  src/liblzma/common/common.c
  src/liblzma/common/filter_common.c
  src/liblzma/common/filter_decoder.c
  src/liblzma/lz/lz_decoder.c
  src/liblzma/lzma/lzma_decoder.c
  src/liblzma/lzma/lzma2_decoder.c
  src/liblzma/lzma/lzma_encoder_presets.c
  src/liblzma/delta/delta_common.c
  src/liblzma/delta/delta_decoder.c
  src/liblzma/simple/simple_coder.c
  src/liblzma/simple/simple_decoder.c
  src/liblzma/simple/x86.c
  src/liblzma/simple/powerpc.c
  src/liblzma/simple/ia64.c
  src/liblzma/simple/arm.c
  src/liblzma/simple/armthumb.c
  src/liblzma/simple/arm64.c
  src/liblzma/simple/sparc.c
  src/liblzma/simple/riscv.c)
add_library(LibLZMA::LibLZMA ALIAS tpw_lzma)
# LZMA_API_STATIC: lzma.h declares the functions for a static library (on Windows, not as imported from a DLL).
target_compile_definitions(tpw_lzma PUBLIC LZMA_API_STATIC)
target_compile_definitions(tpw_lzma PRIVATE HAVE_CONFIG_H)
target_include_directories(tpw_lzma SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/api)
target_include_directories(tpw_lzma PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/common
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/common
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/check
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/lz
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/lzma
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/rangecoder
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/delta
  ${CMAKE_CURRENT_SOURCE_DIR}/src/liblzma/simple)
set_target_properties(tpw_lzma PROPERTIES POSITION_INDEPENDENT_CODE ON C_STANDARD 99 C_VISIBILITY_PRESET default)
# xz's C is not ours to warn about.
target_compile_options(tpw_lzma PRIVATE -w)
