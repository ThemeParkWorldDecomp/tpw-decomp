// PortGroundPad.h: a ride's or shop's base drawn a little nearer the camera than the park's ground under it, a switch for
// tpw-port #125 (SRenderListEntry::CalcRenderEntry). A ride placed with flag 0x2000 (TPRide.cpp, SetBaseBlendFlags) has
// bit 0x800 (plain blending) on the texture record of each face set of its base, and with Ground blending on the ground's
// cells under it (CHField::Prepare, GetBlendedBaseTextures) and the base itself draw the same blended ground texture over
// the same place: the base is drawn over the ground at the same depth and wins the depth test only by GL_LEQUAL and the
// draw order. On the reporter's NVIDIA cards (Windows 10, a GeForce MX150 and an RTX 3070) whole parts of those bases
// showed the ground's own copy of the cells instead, lighter or blurrier, flickering as the camera moved and worse zoomed
// out: what a test lost by a depth step or two looks like. No Mac shows it, so the cause is not certain, and the switch is
// off unless the player sets it (launcher.ini's ground_pad_nearer or TPW_GROUND_PAD_NEARER, read once at the start, the
// number of depth steps); off, the Mac's. Header only and pure, so the host test (tests/host_ground_pad.cpp) sees what the
// game runs.
#pragma once

