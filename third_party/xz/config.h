/* The configuration of the liblzma decoders the launcher builds (third_party/xz/CMakeLists.txt), written by hand in
 * place of xz's configure step: the raw LZMA, LZMA2, delta and branch-converter (BCJ) decoders that libarchive's 7z
 * reader asks for, no encoders, no threads, no integrity checks (7z checks its own CRCs). The same file serves every
 * system the launcher builds on (macOS, Windows with llvm-mingw, Linux with GCC or clang): everything in it is standard
 * C99, and the compiler's own macros pick the rest (sysdefs.h, tuklib_integer.h). */

#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1

/* The decoders: what libarchive's archive_read_support_format_7zip.c passes to lzma_raw_decoder. */
#define HAVE_DECODER_LZMA1 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_DECODER_DELTA 1
#define HAVE_DECODER_X86 1
#define HAVE_DECODER_POWERPC 1
#define HAVE_DECODER_IA64 1
#define HAVE_DECODER_ARM 1
#define HAVE_DECODER_ARMTHUMB 1
#define HAVE_DECODER_ARM64 1
#define HAVE_DECODER_SPARC 1
#define HAVE_DECODER_RISCV 1

/* The tuklib helpers' symbols get liblzma's prefix, as in xz's own build. */
#define TUKLIB_SYMBOL_PREFIX lzma_

/* Every compiler the launcher is built with (clang and GCC) has these. */
#define HAVE___BUILTIN_BSWAPXX 1
#define HAVE___BUILTIN_ASSUME_ALIGNED 1

/* The processors the launcher is built for (x86-64 and AArch64) read unaligned words quickly. */
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
#define TUKLIB_FAST_UNALIGNED_ACCESS 1
#endif

/* The visibility of liblzma's functions: static, so none is exported from the launcher. */
#define LZMA_API_STATIC 1
