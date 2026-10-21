"""Unit v5L50: a ride record (RideInfo) in a difftest's world. The PPC side writes a word at the Mac's offset; the host's record is laid
out wider (a vtable in the balance table and 8-byte value slots), so the host side writes it at the field of the same name, which the
unit's test library answers through tests/ride_layout.h (tpw_ride_hostoffset, tpw_ride_size).

A host-side read through a raw Mac offset then misses the word the case wrote, so a test of a function that reads the record by the
Mac's offset fails.
"""
import ctypes as C


def bind(lib):
    lib.tpw_ride_hostoffset.restype, lib.tpw_ride_hostoffset.argtypes = C.c_int64, [C.c_int32]
    lib.tpw_ride_size.restype, lib.tpw_ride_size.argtypes = C.c_int64, []


def host_offset(lib, mac):
    """The host offset of the record word the Mac has at mac (bind first)."""
    off = lib.tpw_ride_hostoffset(mac)
    assert off >= 0, "no host field for the ride record's +0x%x" % mac
    return off


def host_size(lib):
    return lib.tpw_ride_size()
