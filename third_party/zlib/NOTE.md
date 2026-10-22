# zlib

- What: zlib, the deflate/inflate library, built static with the game (the bullfrog library's unpacker, src/bullfrog/LbZLib.cpp, inflates with it).
- Version: 1.3.2 (17 February 2026).
- Origin: https://zlib.net/zlib-1.3.2.tar.gz
- Archive SHA-256: bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16 (published on https://zlib.net/, beside the download; the same value is GitHub's digest of madler/zlib release v1.3.2).
- Signature: zlib-1.3.2.tar.gz.asc from https://zlib.net/ verified with Mark Adler's key (fingerprint 5ED4 6A67 21D3 6558 7791 E2AA 783F CD8E 58BC AFBA, published at https://madler.net/madler/pgp.html): good signature.
- Taken: 2026-10-21.
- License: zlib license (LICENSE, as published; the notice is also the head of zlib.h).
- Kept: the library's C sources and headers, unchanged: adler32.c compress.c crc32.c crc32.h deflate.c deflate.h gzclose.c gzguts.h gzlib.c gzread.c gzwrite.c infback.c inffast.c inffast.h inffixed.h inflate.c inflate.h inftrees.c inftrees.h trees.c trees.h uncompr.c zconf.h zlib.h zutil.c zutil.h.
- Dropped: everything else in the archive (upstream's configure, Makefile, CMakeLists.txt, docs, examples, test, contrib and the other platforms' project files).
- Local change: CMakeLists.txt is ours, not zlib's: a static library target `tpw_zlib` (alias `ZLIB::ZLIB`), no install rules, no probing of the host. No source file is changed.
- Export: tpw-decomp and tpw-port (the translation's own library links it).
