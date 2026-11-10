/* The configuration of the libarchive readers the launcher builds (third_party/libarchive/CMakeLists.txt), written by
 * hand in place of libarchive's configure step. Reading only: the 7-Zip and RAR (RAR 4 and RAR 5) formats, with zlib
 * (third_party/zlib) and liblzma (third_party/xz) for their compressors; no bzip2, zstd, lz4, iconv, crypto library,
 * ACLs or extended attributes. The same file serves every system the launcher builds on: macOS (Apple clang), Windows
 * (llvm-mingw clang) and Linux (GCC or clang, glibc or the Flatpak's runtime). */
#ifndef TPW_LIBARCHIVE_CONFIG_H
#define TPW_LIBARCHIVE_CONFIG_H

/* The headers every one of those systems has (C99, and MinGW's POSIX layer on Windows). */
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1

/* The C library's limits and functions (stdint.h and wchar.h define them all). */
#define HAVE_DECL_INT32_MAX 1
#define HAVE_DECL_INT32_MIN 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_INTMAX_MAX 1
#define HAVE_DECL_INTMAX_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1
#define HAVE_DECL_UINTMAX_MAX 1
#define HAVE_EILSEQ 1
#define HAVE_MBRTOWC 1
#define HAVE_WCRTOMB 1
#define HAVE_WCSLEN 1
#define HAVE_WCTOMB 1
#define HAVE_WMEMCMP 1
#define HAVE_WMEMCPY 1
#define HAVE_WMEMMOVE 1
#define SIZEOF_INT 4
#if defined(_WIN32)
#define SIZEOF_LONG 4
#else
#define SIZEOF_LONG 8
#endif

/* SSIZE_MAX: limits.h on the Unixes, MinGW's sys/types.h (with its ssize_t) on Windows. */
#define HAVE_DECL_SSIZE_MAX 1
#if defined(_WIN32)
/* The time functions are the C runtime's. */
#define HAVE_LOCALTIME_S 1
/* MinGW's sys/types.h has no uid_t or gid_t (archive_platform_stat.h's struct needs them): libarchive's own CMake
 * step gives Windows these. */
#define uid_t short
#define gid_t short
#else
#define HAVE_LOCALTIME_R 1
#define HAVE_SYS_TIME_H 1
#define HAVE_PTHREAD_H 1
/* fcntl.h's open() and O_RDONLY, which archive_random.c includes only under this name (for /dev/urandom, where there is
 * no arc4random_buf). */
#define HAVE_FCNTL 1
#endif

/* macOS has arc4random_buf; elsewhere archive_random.c has its own (from /dev/urandom), or Windows' BCryptGenRandom. Only
 * temporary file names, which the readers never make, use it. */
#if defined(__APPLE__)
#define HAVE_ARC4RANDOM_BUF 1
#endif

/* The compressors the readers decode with. */
#define HAVE_ZLIB_H 1
#define HAVE_LZMA_H 1

/* No iconv and no nl_langinfo: names are converted by libarchive's own UTF-8 and UTF-16 code, into UTF-8 (the one local
 * change, in archive_string.c's default_iconv_charset: NOTE.md). */

#endif
