# Archive test fixtures

The 7-Zip and RAR archives the assets and importer tests open (host_assets.cpp, host_importer.cpp). Each is a uuencoded
text file (`begin 644 NAME`, lines, `end`), decoded by `UuDecode` in ../test_archives.h, so that no binary file is
committed. None holds game data.

## From libarchive (BSD 2-clause)

These are libarchive's own test archives, copied unchanged from libarchive 3.8.11's `libarchive/test` folder (the
release named in third_party/libarchive/NOTE.md, under the licence in third_party/libarchive/COPYING):

- `test_read_format_rar.rar.uu`: RAR 4, `test.txt` and `testdir/test.txt` (20 bytes each), a symbolic link and folders.
- `test_read_format_rar5_compressed.rar.uu`: RAR 5, compressed, `test.bin` (1200 bytes; the little-endian 32-bit word
  at index i is max(0, k*k - 3k + 1) with k = i + 1).
- `test_rar_multivolume_single_file.part1.rar.uu` to `part3`: RAR 4 in three volumes, one file
  (`LibarchiveAddingTest.html`, 20111 bytes, SHA-256 ee16390e87152d7dec632ae2b37566da6135966e46b5166e6f40a1c827fa61ad).
- `test_read_format_rar4_encrypted.rar.uu`, `test_read_format_rar_encryption_header.rar.uu`,
  `test_read_format_rar5_encrypted.rar.uu`: password-protected RAR 4 data, RAR 4 with encrypted headers, RAR 5 data.

## Made here with 7-Zip (7zz 26.04) from synthetic data

The inputs are made by the tests' own builders, never taken from a disc:

- `pattern.bin`: 4096 bytes, byte i = (i * 7 + i / 13) & 255 (integer division).
- `Synthetic Disc.bin`: `Ps1RawFixture(Ps1ImageFixture(), 2352)` from host_importer.cpp (275184 bytes), and
  `Synthetic Disc.cue`: `FILE "Synthetic Disc.bin" BINARY`, `  TRACK 01 MODE2/2352`, `    INDEX 01 00:00:00`, each line
  ending in CR LF.

The archives:

```sh
7zz a -t7z -m0=LZMA2 -mx9 ps1-pair.7z "Synthetic Disc.bin" "Synthetic Disc.cue"
# nested.7z: "Outer Folder/Inner/disc.zip" (the pair, zipped with 7zz a -tzip -mx9) and "Outer Folder/readme.txt"
7zz a -t7z -m0=LZMA nested.7z "Outer Folder"
7zz a -t7z -m0=PPMd method-ppmd.7z pattern.bin
7zz a -t7z -m0=BCJ -m1=LZMA method-bcj.7z pattern.bin
7zz a -t7z -m0=BCJ2 -m1=LZMA:d20 -m2=LZMA:d19 -m3=LZMA:d19 -mb0:1 -mb0s1:2 -mb0s2:3 method-bcj2.7z pattern.bin
7zz a -t7z -m0=Deflate method-deflate.7z pattern.bin
7zz a -t7z -m0=Delta:4 -m1=LZMA2 method-delta.7z pattern.bin
7zz a -t7z -m0=ARM64 -m1=LZMA2 method-arm64.7z pattern.bin
7zz a -t7z -m0=BZip2 method-bzip2.7z pattern.bin      # a method the launcher does not build: the message test
7zz a -t7z -psecret -mhe=off encrypted-data.7z pattern.bin
7zz a -t7z -psecret -mhe=on encrypted-names.7z pattern.bin
uuencode NAME.7z NAME.7z > NAME.7z.uu
```

RAR archives the tests need beyond these (a bin/cue pair, two volumes, a big member for the cancel test) are written in
memory by `RarOf` in ../test_archives.h, stored rather than compressed, since no program on the build machines makes RAR.
