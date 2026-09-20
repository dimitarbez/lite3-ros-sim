#include <cassert>
#include <cmath>

#include "motion_sdk_paw_lift.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;
bool Near(double left, double right, double tolerance) {
  return std::abs(left - right) <= tolerance;
}
}  // namespace

int main() {
  const double stand_hip = -42.0 * kPi / 180.0;
  const double stand_knee = 78.0 * kPi / 180.0;
  const PawLiftJointTarget neutral =
      SampleLite3PawLift(stand_hip, stand_knee, 0.0, 0.0);
  assert(neutral.valid);
  assert(Near(neutral.hip_y, stand_hip, 1e-12));
  assert(Near(neutral.knee, stand_knee, 1e-12));

  const PawLiftJointTarget five_mm =
      SampleLite3PawLift(stand_hip, stand_knee, 0.005, 0.006);
  assert(five_mm.valid);
  assert(Near(five_mm.hip_y - stand_hip, -0.0204631, 1e-6));
  assert(Near(five_mm.knee - stand_knee, 0.0383156, 1e-6));
  assert(std::abs(five_mm.hip_y_velocity) < 0.05);
  assert(std::abs(five_mm.knee_velocity) < 0.10);

  const PawLiftJointTarget support_extension =
      SampleLite3PawLift(stand_hip, stand_knee, -0.006, -0.008);
  assert(support_extension.valid);
  assert(support_extension.hip_y > stand_hip);
  assert(support_extension.knee < stand_knee);
  assert(!Lite3PawLiftPosition(stand_hip, stand_knee, -0.011).valid);
  assert(!Lite3PawLiftPosition(stand_hip, stand_knee, 0.051).valid);
  assert(!Lite3PawLiftPosition(NAN, stand_knee, 0.005).valid);

  for (int leg = 0; leg < 4; ++leg) {
    const CartesianLegJointTarget nominal = SampleLite3CartesianLeg(
        0.0, stand_hip, stand_knee, leg, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0);
    assert(nominal.valid);
    assert(Near(nominal.hip_x, 0.0, 1e-12));
    assert(Near(nominal.hip_y, stand_hip, 1e-12));
    assert(Near(nominal.knee, stand_knee, 1e-12));
  }
  // A 30 mm rear and 20 mm side body transfer plus 50 mm FL lift remains
  // inside the explicitly bounded joint workspace and produces finite
  // velocities.
  const CartesianLegJointTarget shifted_fl = SampleLite3CartesianLeg(
      0.0, stand_hip, stand_knee, 0, 0.030, -0.020, 0.050,
      0.030, -0.020, 0.050);
  assert(shifted_fl.valid);
  assert(std::abs(shifted_fl.hip_x) < 0.10);
  assert(std::abs(shifted_fl.hip_y - stand_hip) < 0.30);
  assert(std::abs(shifted_fl.knee - stand_knee) < 0.50);
  const CartesianLegJointTarget loaded_hr = SampleLite3CartesianLeg(
      0.0, stand_hip, stand_knee, 3, 0.030, -0.020, -0.030,
      0.030, -0.020, -0.030);
  assert(loaded_hr.valid);
  assert(std::abs(loaded_hr.hip_x) < 0.10);
  assert(std::abs(loaded_hr.hip_y - stand_hip) < 0.30);
  assert(std::abs(loaded_hr.knee - stand_knee) < 0.50);
  const CartesianLegJointTarget suite_midpoint = SampleLite3CartesianLeg(
      0.0, stand_hip, stand_knee, 0, 0.030, -0.020, 0.025,
      0.0, 0.0, 1.875 * 0.050 / 0.60);
  assert(suite_midpoint.valid);
  assert(std::abs(suite_midpoint.hip_x_velocity) < 2.0);
  assert(std::abs(suite_midpoint.hip_y_velocity) < 2.0);
  assert(std::abs(suite_midpoint.knee_velocity) < 2.0);

  // The physical robot needs a slightly larger rearward right-side load
  // transfer than the left. Validate the 35 x 20 mm transfer and 50 mm lift.
  const CartesianLegJointTarget shifted_fr = SampleLite3CartesianLeg(
      0.0, stand_hip, stand_knee, 1, 0.035, 0.020, 0.050,
      0.035, 0.020, 0.050);
  assert(shifted_fr.valid);
  assert(std::abs(shifted_fr.hip_x) < 0.15);
  assert(std::abs(shifted_fr.hip_y - stand_hip) < 0.30);
  assert(std::abs(shifted_fr.knee - stand_knee) < 0.50);
  return 0;
}
