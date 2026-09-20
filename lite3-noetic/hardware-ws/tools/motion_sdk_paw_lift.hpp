#pragma once

#include <algorithm>
#include <cmath>

// Sagittal inverse kinematics for a small vertical paw lift from an existing
// MotionSDK joint pose. The Lite3 model uses the opposite sign convention from
// raw SDK joint positions.
struct PawLiftJointTarget {
  double hip_y{0.0};
  double knee{0.0};
  double hip_y_velocity{0.0};
  double knee_velocity{0.0};
  bool valid{false};
};

struct CartesianLegJointTarget {
  double hip_x{0.0};
  double hip_y{0.0};
  double knee{0.0};
  double hip_x_velocity{0.0};
  double hip_y_velocity{0.0};
  double knee_velocity{0.0};
  bool valid{false};
};

inline double NormalizePawAngle(double value) {
  constexpr double pi = 3.14159265358979323846;
  while (value > pi) value -= 2.0 * pi;
  while (value < -pi) value += 2.0 * pi;
  return value;
}

inline CartesianLegJointTarget Lite3CartesianLegPosition(
    double raw_hip_x, double raw_hip_y, double raw_knee, int leg,
    double offset_x, double offset_y, double offset_z) {
  constexpr double hip_length = 0.0985;
  constexpr double upper = 0.20;
  constexpr double lower = 0.21;
  CartesianLegJointTarget result;
  if (leg < 0 || leg >= 4 || !std::isfinite(raw_hip_x) ||
      !std::isfinite(raw_hip_y) || !std::isfinite(raw_knee) ||
      !std::isfinite(offset_x) || !std::isfinite(offset_y) ||
      !std::isfinite(offset_z)) return result;
  const double hip_sign = (leg == 0 || leg == 2) ? -1.0 : 1.0;
  const double signed_hip = hip_sign * hip_length;
  const double q0 = -raw_hip_x;
  const double q1 = -raw_hip_y;
  const double q2 = -raw_knee;
  const double sagittal = upper * std::cos(q1) +
      lower * std::cos(q1 + q2);
  const double x = -upper * std::sin(q1) -
      lower * std::sin(q1 + q2) + offset_x;
  const double y = signed_hip * std::cos(q0) +
      sagittal * std::sin(q0) + offset_y;
  const double z = signed_hip * std::sin(q0) -
      sagittal * std::cos(q0) + offset_z;
  const double sagittal_squared = y * y + z * z -
      signed_hip * signed_hip;
  if (!(sagittal_squared > 0.0)) return result;
  const double target_sagittal = std::sqrt(sagittal_squared);
  const double cosine = (x * x + target_sagittal * target_sagittal -
                         upper * upper - lower * lower) /
      (2.0 * upper * lower);
  if (!std::isfinite(cosine) || cosine < -1.0 || cosine > 1.0) return result;
  const double target_q2 = -std::acos(cosine);
  const double target_q1 = std::atan2(-x, target_sagittal) -
      std::atan2(lower * std::sin(target_q2),
                 upper + lower * std::cos(target_q2));
  const double target_q0 = NormalizePawAngle(
      std::atan2(z, y) - std::atan2(-target_sagittal, signed_hip));
  result.hip_x = -target_q0;
  result.hip_y = -target_q1;
  result.knee = -target_q2;
  result.valid = std::isfinite(result.hip_x) &&
      std::isfinite(result.hip_y) && std::isfinite(result.knee) &&
      std::abs(result.hip_x) <= 0.20 &&
      result.hip_y >= -1.20 && result.hip_y <= -0.30 &&
      result.knee >= 0.80 && result.knee <= 1.80;
  return result;
}

inline CartesianLegJointTarget SampleLite3CartesianLeg(
    double raw_hip_x, double raw_hip_y, double raw_knee, int leg,
    double offset_x, double offset_y, double offset_z,
    double velocity_x, double velocity_y, double velocity_z) {
  CartesianLegJointTarget result = Lite3CartesianLegPosition(
      raw_hip_x, raw_hip_y, raw_knee, leg, offset_x, offset_y, offset_z);
  if (!result.valid || !std::isfinite(velocity_x) ||
      !std::isfinite(velocity_y) || !std::isfinite(velocity_z)) {
    result.valid = false;
    return result;
  }
  constexpr double epsilon_seconds = 1e-4;
  const CartesianLegJointTarget low = Lite3CartesianLegPosition(
      raw_hip_x, raw_hip_y, raw_knee, leg,
      offset_x - epsilon_seconds * velocity_x,
      offset_y - epsilon_seconds * velocity_y,
      offset_z - epsilon_seconds * velocity_z);
  const CartesianLegJointTarget high = Lite3CartesianLegPosition(
      raw_hip_x, raw_hip_y, raw_knee, leg,
      offset_x + epsilon_seconds * velocity_x,
      offset_y + epsilon_seconds * velocity_y,
      offset_z + epsilon_seconds * velocity_z);
  if (!low.valid || !high.valid) {
    result.valid = false;
    return result;
  }
  result.hip_x_velocity = (high.hip_x - low.hip_x) /
      (2.0 * epsilon_seconds);
  result.hip_y_velocity = (high.hip_y - low.hip_y) /
      (2.0 * epsilon_seconds);
  result.knee_velocity = (high.knee - low.knee) /
      (2.0 * epsilon_seconds);
  result.valid = std::isfinite(result.hip_x_velocity) &&
      std::isfinite(result.hip_y_velocity) &&
      std::isfinite(result.knee_velocity);
  return result;
}

inline PawLiftJointTarget Lite3PawLiftPosition(double raw_hip_y,
                                               double raw_knee,
                                               double lift_m) {
  constexpr double upper = 0.20;
  constexpr double lower = 0.21;
  PawLiftJointTarget result;
  if (!std::isfinite(raw_hip_y) || !std::isfinite(raw_knee) ||
      !std::isfinite(lift_m) || lift_m < -0.010 || lift_m > 0.050) {
    return result;
  }
  const double hip = -raw_hip_y;
  const double knee = -raw_knee;
  const double x = -upper * std::sin(hip) -
      lower * std::sin(hip + knee);
  const double z = -upper * std::cos(hip) -
      lower * std::cos(hip + knee) + lift_m;
  const double cosine = (x * x + z * z - upper * upper - lower * lower) /
      (2.0 * upper * lower);
  if (!std::isfinite(cosine) || cosine < -1.0 || cosine > 1.0) return result;
  const double target_knee = -std::acos(cosine);
  const double target_hip = std::atan2(-x, -z) -
      std::atan2(lower * std::sin(target_knee),
                 upper + lower * std::cos(target_knee));
  result.hip_y = -target_hip;
  result.knee = -target_knee;
  result.valid = std::isfinite(result.hip_y) && std::isfinite(result.knee);
  return result;
}

inline PawLiftJointTarget SampleLite3PawLift(double raw_hip_y,
                                             double raw_knee,
                                             double lift_m,
                                             double lift_velocity_mps) {
  PawLiftJointTarget result =
      Lite3PawLiftPosition(raw_hip_y, raw_knee, lift_m);
  if (!result.valid || !std::isfinite(lift_velocity_mps)) {
    result.valid = false;
    return result;
  }
  constexpr double epsilon = 1e-5;
  const double low = std::max(-0.010, lift_m - epsilon);
  const double high = std::min(0.050, lift_m + epsilon);
  const PawLiftJointTarget low_target =
      Lite3PawLiftPosition(raw_hip_y, raw_knee, low);
  const PawLiftJointTarget high_target =
      Lite3PawLiftPosition(raw_hip_y, raw_knee, high);
  if (!low_target.valid || !high_target.valid || high <= low) {
    result.valid = false;
    return result;
  }
  result.hip_y_velocity =
      (high_target.hip_y - low_target.hip_y) / (high - low) *
      lift_velocity_mps;
  result.knee_velocity =
      (high_target.knee - low_target.knee) / (high - low) *
      lift_velocity_mps;
  result.valid = std::isfinite(result.hip_y_velocity) &&
      std::isfinite(result.knee_velocity);
  return result;
}
