# xz (liblzma)

- What: liblzma from XZ Utils, the LZMA and LZMA2 decoders and the BCJ and Delta filters, built static into the launcher for libarchive's 7-Zip reader (third_party/libarchive). Decoders only: no encoders, no .xz or .lzma file format, no integrity checks, no threads.
- Version: 5.8.4 (9 September 2026). Not 5.6.0 or 5.6.1 (the releases with the backdoor, CVE-2024-3094); 5.8.4 also has the fix for the security issue its release notes name.
- Origin: https://github.com/tukaani-project/xz/releases/download/v5.8.4/xz-5.8.4.tar.gz
- Archive SHA-256: 0014c7886930454fe8bd4228665b51af55eeae560ea135c9c4cd33f55b2591d9 (computed here from the download).
- Signature: xz-5.8.4.tar.gz.sig from the same release verified with Lasse Collin's key (fingerprint 3690 C240 CE51 B467 0D30 AD1C 38EE 757D 6918 4620, published at https://tukaani.org/misc/lasse_collin_pubkey.txt): good signature.
- Taken: 2026-11-09.
- License: liblzma is under the BSD Zero Clause License (COPYING, which says which parts of XZ Utils are under which licence, and COPYING.0BSD, both as published). No GPL or LGPL file is kept.
- Kept: COPYING, COPYING.0BSD and these files, unchanged: src/common/mythread.h sysdefs.h tuklib_common.h tuklib_config.h tuklib_integer.h; src/liblzma/api/lzma.h and api/lzma/*.h; src/liblzma/check/check.h crc_common.h; common/common.c common.h filter_common.c filter_common.h filter_decoder.c filter_decoder.h memcmplen.h; delta/delta_common.c delta_common.h delta_decoder.c delta_decoder.h delta_private.h; lz/lz_decoder.c lz_decoder.h; lzma/lzma_common.h lzma_decoder.c lzma_decoder.h lzma_encoder_presets.c lzma2_decoder.c lzma2_decoder.h; rangecoder/range_common.h range_decoder.h; simple/arm.c arm64.c armthumb.c ia64.c powerpc.c riscv.c simple_coder.c simple_coder.h simple_decoder.c simple_decoder.h simple_private.h sparc.c x86.c.
- Dropped: everything else in the archive (configure, CMakeLists.txt and the other build files, the xz, xzdec and lzmainfo programs, the scripts, the encoders, the .xz and .lzma containers, the checks, the threaded coder, po, doc, debug, extra, windows, dos and the tests, including the test files).
- Local change: CMakeLists.txt and config.h are ours, not xz's: a static library target `tpw_lzma` (alias `LibLZMA::LibLZMA`) whose config.h is written by hand in place of the configure step, no install rules, no probing of the host. No source file is changed.
- Export: tpw-decomp and tpw-port (with libarchive).
