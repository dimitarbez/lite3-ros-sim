#include <array>
#include <cassert>
#include <cmath>

#include "motion_sdk_anger_stomp.hpp"
#include "motion_sdk_breath_profile.hpp"
#include "motion_sdk_paw_lift.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kStandHipY = -42.0 * kPi / 180.0;
constexpr double kStandKnee = 78.0 * kPi / 180.0;

double StanceOffset(int leg) {
  return (leg == 0 || leg == 2) ? -kAngerStanceMeters :
      kAngerStanceMeters;
}

void CheckCombinedPose(int target_leg, double lift, double shift_x,
                       double shift_y) {
  for (int leg = 0; leg < 4; ++leg) {
    const double target_lift = kAngerBraceMeters +
        (leg == target_leg ? lift : 0.0);
    const CartesianLegJointTarget target = SampleLite3CartesianLeg(
        0.0, kStandHipY, kStandKnee, leg, shift_x,
        shift_y + StanceOffset(leg), target_lift,
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

void CheckCrossTransfer(int target_leg, double start_x, double end_x,
                        double start_y, double end_y) {
  const double elapsed = 0.5 * kAngerLiftSeconds;
  const BreathScalar lift = QuinticBreathSegment(
      elapsed, kAngerLiftSeconds, 0.0, kAngerLiftMeters);
  const BreathScalar shift_x = QuinticBreathSegment(
      elapsed, kAngerLiftSeconds, start_x, end_x);
  const BreathScalar shift_y = QuinticBreathSegment(
      elapsed, kAngerLiftSeconds, start_y, end_y);
  for (int leg = 0; leg < 4; ++leg) {
    const CartesianLegJointTarget target = SampleLite3CartesianLeg(
        0.0, kStandHipY, kStandKnee, leg, shift_x.position,
        shift_y.position + StanceOffset(leg),
        kAngerBraceMeters + (leg == target_leg ? lift.position : 0.0),
        shift_x.velocity, shift_y.velocity,
        leg == target_leg ? lift.velocity : 0.0);
    assert(target.valid);
    assert(std::abs(target.hip_x_velocity) < 2.0);
    assert(std::abs(target.hip_y_velocity) < 2.0);
    assert(std::abs(target.knee_velocity) < 2.0);
  }
}

void CheckRelatch(int target_leg, double start_x, double start_y) {
  for (int millisecond = 0;
       millisecond <= static_cast<int>(kAngerRelatchSeconds * 1000.0);
       ++millisecond) {
    const double elapsed = millisecond / 1000.0;
    const BreathScalar shift_x = QuinticBreathSegment(
        elapsed, kAngerRelatchSeconds, start_x, 0.0);
    const BreathScalar shift_y = QuinticBreathSegment(
        elapsed, kAngerRelatchSeconds, start_y, 0.0);
    const BreathScalar common_z = QuinticBreathSegment(
        elapsed, kAngerRelatchSeconds, kAngerBraceMeters, 0.0);
    const BreathScalar stance_y = QuinticBreathSegment(
        elapsed, kAngerRelatchSeconds, kAngerStanceMeters, 0.0);
    for (int leg = 0; leg < 4; ++leg) {
      const double stance_sign =
          (leg == 0 || leg == 2) ? -1.0 : 1.0;
      const CartesianLegJointTarget target = SampleLite3CartesianLeg(
          0.0, kStandHipY, kStandKnee, leg, shift_x.position,
          shift_y.position + stance_sign * stance_y.position,
          common_z.position, shift_x.velocity,
          shift_y.velocity + stance_sign * stance_y.velocity,
          common_z.velocity);
      assert(target.valid);
      assert(std::abs(target.hip_x_velocity) < 2.0);
      assert(std::abs(target.hip_y_velocity) < 2.0);
      assert(std::abs(target.knee_velocity) < 2.0);
    }
  }
}
}  // namespace

int main() {
  static_assert(kAngerLowerSeconds + kAngerLandingDwellSeconds ==
                    kAngerPlaceSeconds,
                "placement must include the complete landing dwell");
  assert(std::abs(kAngerLoopSeconds -
      (kAngerBraceSeconds + 2.0 * (kAngerLiftSeconds +
       kAngerPlaceSeconds + kAngerRelatchSeconds +
       kAngerRelatchHoldSeconds) + kAngerHoldSeconds +
       kAngerRecoverSeconds)) < 1e-12);
  assert(AngerStompLimitsValid(kAngerLiftMeters));
  assert(!AngerStompLimitsValid(0.009));
  assert(!AngerStompLimitsValid(0.036));
  assert(QuinticMaximumSpeed(kAngerLiftMeters, kAngerLowerSeconds) <=
         kAngerMaximumDownwardVelocityMps);
  assert(QuinticMaximumAcceleration(kAngerLiftMeters, kAngerLowerSeconds) <=
         kAngerMaximumDownwardAccelerationMps2);

  // Both possible first-paw orders use the same independently valid left and
  // right workspaces.  The right side retains joy's physically calibrated
  // asymmetric rearward transfer.
  CheckCombinedPose(0, kAngerLiftMeters,
                    kAngerLeftSupportShiftXMeters,
                    -kAngerSupportShiftYMeters);
  CheckCombinedPose(1, kAngerLiftMeters,
                    kAngerRightSupportShiftXMeters,
                    kAngerSupportShiftYMeters);
  CheckCombinedPose(1, kAngerLiftMeters,
                    kAngerRightSupportShiftXMeters,
                    kAngerSupportShiftYMeters);
  CheckCombinedPose(0, kAngerLiftMeters,
                    kAngerLeftSupportShiftXMeters,
                    -kAngerSupportShiftYMeters);
  CheckCrossTransfer(1, 0.0, kAngerRightSupportShiftXMeters,
                     0.0, kAngerSupportShiftYMeters);
  CheckCrossTransfer(0, 0.0, kAngerLeftSupportShiftXMeters,
                     0.0, -kAngerSupportShiftYMeters);
  CheckRelatch(0, kAngerLeftSupportShiftXMeters,
               -kAngerSupportShiftYMeters);
  CheckRelatch(1, kAngerRightSupportShiftXMeters,
               kAngerSupportShiftYMeters);

  // Sample every millisecond through lowering.  Cartesian velocity and a
  // finite-difference acceleration remain inside the anger-specific limits;
  // both seams are stationary, so the dwell commands no post-contact drive.
  double previous_velocity = 0.0;
  double maximum_speed = 0.0;
  double maximum_acceleration = 0.0;
  for (int millisecond = 0;
       millisecond <= static_cast<int>(kAngerLowerSeconds * 1000.0);
       ++millisecond) {
    const double elapsed = millisecond / 1000.0;
    const BreathScalar lift = QuinticBreathSegment(
        elapsed, kAngerLowerSeconds, kAngerLiftMeters, 0.0);
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
  assert(maximum_speed <= kAngerMaximumDownwardVelocityMps);
  assert(maximum_acceleration <= kAngerMaximumDownwardAccelerationMps2 + 0.02);
  const BreathScalar lower_start = QuinticBreathSegment(
      0.0, kAngerLowerSeconds, kAngerLiftMeters, 0.0);
  const BreathScalar lower_end = QuinticBreathSegment(
      kAngerLowerSeconds, kAngerLowerSeconds, kAngerLiftMeters, 0.0);
  assert(lower_start.velocity == 0.0);
  assert(lower_end.position == 0.0);
  assert(lower_end.velocity == 0.0);

  AngerStompGate gate;
  assert(gate.BeginStomp());
  assert(!gate.BeginStomp());
  gate.CompleteLanding(true, kAngerLandingDwellSeconds - 0.001);
  assert(!gate.next_stomp_allowed());
  gate.CompleteLanding(false, kAngerLandingDwellSeconds);
  assert(!gate.next_stomp_allowed());
  gate.CompleteLanding(true, kAngerLandingDwellSeconds);
  assert(gate.next_stomp_allowed());
  assert(gate.BeginStomp());
  assert(!gate.next_stomp_allowed());

  // Chat retargeting latches the newest category but never permits another
  // stomp. The caller must finish the current landing/re-latch and exact
  // neutral transition before handing this target back to ExpressionEngine.
  AngerRetargetTracker retarget(10);
  retarget.Observe(true, true, 10, "anger", -0.8, 0.9);
  assert(!retarget.cancellation_requested());
  assert(retarget.may_start_next_stomp());
  retarget.Observe(true, true, 11, "joy", 0.8, 0.9);
  assert(retarget.cancellation_requested());
  assert(!retarget.may_start_next_stomp());
  assert(retarget.emotion() == "joy");
  retarget.Observe(true, true, 12, "fear", -0.9, 1.0);
  assert(retarget.emotion() == "fear");
  assert(retarget.last_sequence() == 12);

  AngerRetargetTracker placement_retarget(30);
  AngerStompGate placement_gate;
  assert(placement_gate.BeginStomp());
  placement_retarget.Observe(true, true, 31, "neutral", 0.0, 0.2);
  assert(!placement_retarget.may_start_next_stomp());
  placement_gate.CompleteLanding(true, kAngerLandingDwellSeconds);
  assert(placement_gate.next_stomp_allowed());
  assert(!placement_retarget.may_start_next_stomp());

  AngerRetargetTracker stale(20);
  stale.Observe(false, false, 20, "anger", -0.8, 0.9);
  assert(stale.cancellation_requested());
  assert(stale.stale());
  assert(stale.emotion() == "neutral");
  stale.Observe(true, true, 21, "joy", 1.0, 1.0);
  assert(stale.emotion() == "neutral");
  return 0;
}
