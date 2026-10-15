#include "check.h"
#include "core/coords.h"

TEST(to_camera_splits_into_chunks) {
  const fw::CameraPosition c = fw::to_camera({100.5, 7.25, -0.25});
  CHECK(c.chunk_x == 0);
  CHECK(c.chunk_z == -1);
  CHECK_NEAR(c.x, 100.5, 1e-4);
  CHECK_NEAR(c.y, 7.25, 1e-4);
  CHECK_NEAR(c.z, 511.75, 1e-4);
}

TEST(to_camera_exact_chunk_boundary) {
  const fw::CameraPosition c = fw::to_camera({1024.0, 0.0, 512.0});
  CHECK(c.chunk_x == 2);
  CHECK(c.chunk_z == 1);
  CHECK_NEAR(c.x, 0.0, 1e-6);
  CHECK_NEAR(c.z, 0.0, 1e-6);
}

TEST(negative_coordinate_just_below_zero_is_chunk_minus_one) {
  const fw::CameraPosition c = fw::to_camera({-0.0001, 0.0, -512.0001});
  CHECK(c.chunk_x == -1);
  CHECK(c.chunk_z == -2);
  CHECK_NEAR(fw::to_world(c).x, -0.0001, 1e-3);
  CHECK_NEAR(fw::to_world(c).z, -512.0001, 1e-3);
}

TEST(round_trip_far_from_origin) {
  for (const fw::Vec3 w : {fw::Vec3{-7123.456, 12.5, 98765.4321}, fw::Vec3{250000.25, -3.0, -180000.75}}) {
    const fw::Vec3 back = fw::to_world(fw::to_camera(w));
    CHECK_NEAR(back.x, w.x, 1e-3);
    CHECK_NEAR(back.y, w.y, 1e-3);
    CHECK_NEAR(back.z, w.z, 1e-3);
  }
}
