"""The original zlib under the emulator, for the bullfrog library's imports.

The bullfrog library imports inflateInit2_, inflate, inflateEnd, deflateInit2_, deflate and deflateEnd from the game's own zlib library
("zlib symbols shared", version 1.1.3), which is a program of its own with its own code and data. This module
gives the bullfrog emulator stubs for these imports that run the real zlib in a second emulator:

- The stream record (56 bytes) is copied into the zlib emulator's scratch, with the library's two allocator
  pointers replaced by the transition vectors of the allocator's own code, and copied back afterwards with the
  bullfrog library's own pointers.
- The scratch area of the bullfrog emulator (the block, the destination, the workspace the allocator hands out)
  is mirrored into the zlib emulator at the same addresses before the call and back afterwards, so the pointers
  inside the stream record mean the same in both.
- The allocator is the bullfrog library's own code: @unnamed@::zLibAlloc (0x10024bcc, 48 bytes) and zLibFree
  (0x10024c2c, 4 bytes) are copied byte for byte from the bullfrog program into the zlib emulator (they use no
  registers but their arguments, no calls and no TOC). Their opaque record (two words: the next free byte and the
  end of the arena) lives on the bullfrog emulator's stack; its eight bytes are copied to a place of the zlib emulator's
  own around each call (zlib 1.1.3 allocates in inflate as well as in inflateInit2_: its temporary tables).

install(bullfrog_emu, dumps_dir) puts the stubs on the bullfrog emulator (idempotent).
"""
import struct

from unicorn import ppc_const as P

import ppcemu
import tpwtest as T

ZS = 0x7D3F0000          # the stream record's copy in the zlib emulator (above the mirrored scratch)
VERSION = ZS + 0x100     # the version string's copy
ARENA = ZS + 0x200       # the allocator's arena record (two words) in the zlib emulator: a call zeroes the stack it starts on
PAGE = 0x7E400000        # the allocator's code and its transition vectors
CODE_ALLOC, CODE_FREE = PAGE, PAGE + 0x40
TV_ALLOC, TV_FREE = PAGE + 0x100, PAGE + 0x108
ZLIB_ALLOC, ZLIB_ALLOC_SIZE, ZLIB_FREE, ZLIB_FREE_SIZE = 0x10024BCC, 48, 0x10024C2C, 4
STREAM_SIZE = 56


class Bridge:
    def __init__(self, bullfrog, dumps):
        self.b = bullfrog
        self.z = ppcemu.PPCEmu(dumps / "zlib symbols shared")
        T.install_std_stubs(self.z)
        z = self.z
        z.uc.mem_map(PAGE, 0x1000)
        z.uc.mem_write(CODE_ALLOC, bullfrog.read(ZLIB_ALLOC, ZLIB_ALLOC_SIZE))
        z.uc.mem_write(CODE_FREE, bullfrog.read(ZLIB_FREE, ZLIB_FREE_SIZE))
        z.uc.mem_write(TV_ALLOC, struct.pack(">II", CODE_ALLOC, z.toc))
        z.uc.mem_write(TV_FREE, struct.pack(">II", CODE_FREE, z.toc))
        self.entries = {name: z.func(name) for name in ("inflateInit2_", "inflate", "inflateEnd", "deflateInit2_", "deflate", "deflateEnd")}

    # ------------------------------------------------------------------ the mirror
    def _mirror(self, source, target):
        top = min(self.b.scratch_top, ZS)     # the bullfrog emulator hands out the scratch; the zlib one hands out none
        size = top - ppcemu.SCRATCH_BASE
        if size > 0:
            target.write(ppcemu.SCRATCH_BASE, source.read(ppcemu.SCRATCH_BASE, size))

    def _run(self, name, strm, args, version=None):
        z, b = self.z, self.b
        record = bytearray(b.read(strm, STREAM_SIZE))
        original = bytes(record[0x20:0x28])
        opaque = struct.unpack(">I", record[0x28:0x2C])[0]       # the arena record, on the bullfrog emulator's stack
        record[0x20:0x28] = struct.pack(">II", TV_ALLOC, TV_FREE)
        record[0x28:0x2C] = struct.pack(">I", ARENA)
        z.write(ZS, bytes(record))
        z.write(ARENA, b.read(opaque, 8) if opaque else bytes(8))
        self._mirror(b, z)
        result = z.call(self.entries[name], args, max_insns=200_000_000, timeout_us=120_000_000)
        self._mirror(z, b)
        if opaque:
            b.write(opaque, z.read(ARENA, 8))
        record = bytearray(z.read(ZS, STREAM_SIZE))
        record[0x20:0x28] = original
        record[0x28:0x2C] = struct.pack(">I", opaque)
        b.write(strm, bytes(record))
        return result

    # ------------------------------------------------------------------ the imports
    def init2(self, e, call):
        strm, bits, version, size = e.r(3), e.r(4), e.r(5), e.r(6)
        if version:
            self.z.write(VERSION, e.read(version, 8))
        return self._run("inflateInit2_", strm, [ZS, bits, VERSION if version else 0, size])

    def inflate(self, e, call):
        return self._run("inflate", e.r(3), [ZS, e.r(4)])

    def end(self, e, call):
        return self._run("inflateEnd", e.r(3), [ZS])

    def deflate_init2(self, e, call):
        strm, level, method, bits, memory, strategy, version, size = [e.r(i) for i in range(3, 11)]
        if version:
            self.z.write(VERSION, e.read(version, 8))
        return self._run("deflateInit2_", strm, [ZS, level, method, bits, memory, strategy, VERSION if version else 0, size])

    def deflate(self, e, call):
        return self._run("deflate", e.r(3), [ZS, e.r(4)])

    def deflate_end(self, e, call):
        return self._run("deflateEnd", e.r(3), [ZS])


def install(bullfrog, dumps):
    """Stubs for the inflate and deflate imports on the bullfrog emulator."""
    if not hasattr(bullfrog, "zlib_bridge"):
        bullfrog.zlib_bridge = Bridge(bullfrog, dumps)
    bridge = bullfrog.zlib_bridge
    T.stub_cb(bullfrog, "inflateInit2_", bridge.init2)
    T.stub_cb(bullfrog, "inflate", bridge.inflate)
    T.stub_cb(bullfrog, "inflateEnd", bridge.end)
    T.stub_cb(bullfrog, "deflateInit2_", bridge.deflate_init2)
    T.stub_cb(bullfrog, "deflate", bridge.deflate)
    T.stub_cb(bullfrog, "deflateEnd", bridge.deflate_end)
    return bridge
