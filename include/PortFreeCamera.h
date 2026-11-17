// The launcher's "Free camera" (the port's fix freecam): a camera that tilts freely, zooms further, flies, and takes a
// trackpad's swipes and pinches. Only in the port, and only with gPortFreeCamera (include/TPCamera.h) on; off, every line
// of the original camera runs unchanged. The input side is here, header only, so the host test (tests/host_free_camera.cpp)
// reaches it; the camera side is in HandleCamera (src/game/TPCamera.cpp), the pointer's side in TPMain.cpp and the run
// loop's in platform/gamework.cpp.
#pragma once
