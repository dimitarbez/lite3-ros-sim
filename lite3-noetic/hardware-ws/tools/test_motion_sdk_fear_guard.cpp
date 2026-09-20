#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "motion_sdk_breath_profile.hpp"
#include "motion_sdk_fear_guard.hpp"
#include "motion_sdk_paw_lift.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kStandHipY = -42.0 * kPi / 180.0;
constexpr double kStandKnee = 78.0 * kPi / 180.0;

double StanceOffset(int leg) {
  return (leg == 0 || leg == 2) ? -kFearStanceMeters : kFearStanceMeters;
}

void CheckPose(int target_leg, double lift, double shift_x, double shift_y,
               double common_z) {
  for (int leg = 0; leg < 4; ++leg) {
    const CartesianLegJointTarget target = SampleLite3CartesianLeg(
        0.0, kStandHipY, kStandKnee, leg, shift_x,
        shift_y + StanceOffset(leg),
        common_z + (leg == target_leg ? lift : 0.0),
        0.0, 0.0, 0.0);
    assert(target.valid);
    assert(std::isfinite(target.hip_x));
    assert(std::isfinite(target.hip_y));
    assert(std::isfinite(target.knee));
    assert(std::abs(target.hip_x) <= 0.20);
    assert(target.hip_y >= -1.20 && target.hip_y <= -0.30);
    assert(target.knee >= 0.80 && target.knee <= 1.80);
  }
}

void CheckPhase(double duration, double start_lift, double end_lift,
                double start_x, double end_x, double start_y, double end_y,
                double start_z, double end_z) {
  for (int millisecond = 0;
       millisecond <= static_cast<int>(duration * 1000.0); ++millisecond) {
    const double elapsed = millisecond / 1000.0;
    const BreathScalar lift = QuinticBreathSegment(
        elapsed, duration, start_lift, end_lift);
    const BreathScalar shift_x = QuinticBreathSegment(
        elapsed, duration, start_x, end_x);
    const BreathScalar shift_y = QuinticBreathSegment(
        elapsed, duration, start_y, end_y);
    const BreathScalar common_z = QuinticBreathSegment(
        elapsed, duration, start_z, end_z);
    for (int leg = 0; leg < 4; ++leg) {
      const CartesianLegJointTarget target = SampleLite3CartesianLeg(
          0.0, kStandHipY, kStandKnee, leg, shift_x.position,
          shift_y.position + StanceOffset(leg),
          common_z.position + (leg == 0 ? lift.position : 0.0),
          shift_x.velocity, shift_y.velocity,
          common_z.velocity + (leg == 0 ? lift.velocity : 0.0));
      assert(target.valid);
      assert(std::abs(target.hip_x_velocity) < 2.0);
      assert(std::abs(target.hip_y_velocity) < 2.0);
      assert(std::abs(target.knee_velocity) < 2.0);
    }
  }
}
}  // namespace

int main() {
  static_assert(kFearLowerSeconds + kFearLandingDwellSeconds ==
                    kFearPlaceSeconds,
                "fear placement must include landing dwell");
  assert(std::abs(kFearLoopSeconds -
      (kFearFlinchSeconds + kFearRecoilSeconds +
       2.0 * (kFearGuardSeconds + kFearPlaceSeconds) +
       kFearFreezeSeconds + kFearRecoverSeconds)) < 1e-12);
  assert(FearGuardLimitsValid(kFearLiftMeters));
  assert(FearGuardLimitsValid(kFearMinimumLiftMeters));
  assert(!FearGuardLimitsValid(0.014));
  assert(!FearGuardLimitsValid(0.026));
  assert(FearBodyVisualLimitsValid());
  assert(kFearBodyVisualCycles == 3);
  assert(std::abs(kFearBodyVisualSeconds - 15.0) < 1e-12);

  // Flinch/recoil, both possible paw orders, the combined crouch/transfer/lift
  // workspace, and exact-neutral recovery remain finite and inside bounds.
  CheckPose(0, 0.0, 0.0, 0.0, kFearFlinchMeters);
  CheckPose(0, 0.0, kFearRecoilXMeters, 0.0,
            kFearGuardedCrouchMeters);
  CheckPose(0, kFearLiftMeters, kFearLeftSupportShiftXMeters,
            -kFearSupportShiftYMeters,
            kFearGuardedCrouchMeters);
  CheckPose(1, kFearLiftMeters, kFearRightSupportShiftXMeters,
            kFearSupportShiftYMeters,
            kFearGuardedCrouchMeters);
  CheckPose(1, kFearLiftMeters, kFearRightSupportShiftXMeters,
            kFearSupportShiftYMeters,
            kFearGuardedCrouchMeters);
  CheckPose(0, kFearLiftMeters, kFearLeftSupportShiftXMeters,
            -kFearSupportShiftYMeters,
            kFearGuardedCrouchMeters);
  CheckPose(0, 0.0, 0.0, 0.0, 0.0);
  CheckPhase(kFearGuardSeconds, 0.0, kFearLiftMeters,
             kFearRecoilXMeters, kFearLeftSupportShiftXMeters,
             0.0, -kFearSupportShiftYMeters,
             kFearGuardedCrouchMeters, kFearGuardedCrouchMeters);
  CheckPhase(kFearLowerSeconds, kFearLiftMeters, 0.0,
             kFearLeftSupportShiftXMeters, kFearRecoilXMeters,
             -kFearSupportShiftYMeters, 0.0,
             kFearGuardedCrouchMeters, kFearGuardedCrouchMeters);
  CheckPhase(kFearBodyTrembleSegmentSeconds, 0.0, 0.0,
             kFearRecoilXMeters, kFearRecoilXMeters,
             -kFearBodyTrembleMeters, kFearBodyTrembleMeters,
             kFearGuardedCrouchMeters, kFearGuardedCrouchMeters);

  double previous_velocity = 0.0;
  double maximum_speed = 0.0;
  double maximum_acceleration = 0.0;
  for (int millisecond = 0;
       millisecond <= static_cast<int>(kFearLowerSeconds * 1000.0);
       ++millisecond) {
    const double elapsed = millisecond / 1000.0;
    const BreathScalar lift = QuinticBreathSegment(
        elapsed, kFearLowerSeconds, kFearLiftMeters, 0.0);
    assert(std::isfinite(lift.position));
    assert(std::isfinite(lift.velocity));
    maximum_speed = std::max(maximum_speed, std::abs(lift.velocity));
    if (millisecond > 0) {
      maximum_acceleration = std::max(
          maximum_acceleration,
          std::abs((lift.velocity - previous_velocity) / 0.001));
    }
    previous_velocity = lift.velocity;
  }
  assert(maximum_speed <= kFearMaximumDownwardVelocityMps);
  assert(maximum_acceleration <=
         kFearMaximumDownwardAccelerationMps2 + 0.02);
  const BreathScalar lower_start = QuinticBreathSegment(
      0.0, kFearLowerSeconds, kFearLiftMeters, 0.0);
  const BreathScalar lower_end = QuinticBreathSegment(
      kFearLowerSeconds, kFearLowerSeconds, kFearLiftMeters, 0.0);
  assert(lower_start.velocity == 0.0);
  assert(lower_end.position == 0.0);
  assert(lower_end.velocity == 0.0);

  FearGuardGate gate;
  assert(gate.BeginHover());
  assert(!gate.BeginHover());
  gate.CompleteLanding(true, kFearLandingDwellSeconds - 0.001);
  assert(!gate.next_hover_allowed());
  gate.CompleteLanding(false, kFearLandingDwellSeconds);
  assert(!gate.next_hover_allowed());
  gate.CompleteLanding(true, kFearLandingDwellSeconds);
  assert(gate.next_hover_allowed());
  assert(gate.BeginHover());

  // Requests in a hover/lower/freeze path latch the newest target, prevent a
  // second hover, and cannot be revived after a stale-link cancellation.
  FearRetargetTracker hover_retarget(10);
  hover_retarget.Observe(true, true, 11, "joy", 0.8, 0.9);
  assert(hover_retarget.cancellation_requested());
  assert(!hover_retarget.may_start_next_hover());
  hover_retarget.Observe(true, true, 12, "sadness", -0.8, 0.3);
  assert(hover_retarget.emotion() == "sadness");
  assert(hover_retarget.last_sequence() == 12);

  FearRetargetTracker lower_retarget(20);
  FearGuardGate lower_gate;
  assert(lower_gate.BeginHover());
  lower_retarget.Observe(true, true, 21, "neutral", 0.0, 0.2);
  lower_gate.CompleteLanding(true, kFearLandingDwellSeconds);
  assert(lower_gate.next_hover_allowed());
  assert(!lower_retarget.may_start_next_hover());

  FearRetargetTracker freeze_retarget(30);
  freeze_retarget.Observe(true, true, 31, "surprise", 0.4, 1.0);
  assert(freeze_retarget.cancellation_requested());
  assert(freeze_retarget.emotion() == "surprise");

  FearRetargetTracker stale(40);
  stale.Observe(false, false, 40, "fear", -0.8, 0.9);
  assert(stale.cancellation_requested());
  assert(stale.stale());
  assert(stale.emotion() == "neutral");
  stale.Observe(true, true, 41, "joy", 1.0, 1.0);
  assert(stale.emotion() == "neutral");
  return 0;
}
