#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "motion_sdk_breath_profile.hpp"
#include "motion_sdk_paw_lift.hpp"
#include "motion_sdk_sadness.hpp"

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kStandHipY = -42.0 * kPi / 180.0;
constexpr double kStandKnee = 78.0 * kPi / 180.0;

double StanceOffset(int leg, double stance) {
  return (leg == 0 || leg == 2) ? -stance : stance;
}

double RollOffset(int leg, double roll) {
  return (leg == 0 || leg == 2) ? roll : -roll;
}

void CheckPose(int target_leg, double lift, double shift_x, double shift_y,
               double common_z, double front_z, double roll_z,
               double stance_y) {
  for (int leg = 0; leg < 4; ++leg) {
    const double z = common_z + (leg < 2 ? front_z : 0.0) +
        RollOffset(leg, roll_z) + (leg == target_leg ? lift : 0.0);
    const CartesianLegJointTarget target = SampleLite3CartesianLeg(
        0.0, kStandHipY, kStandKnee, leg, shift_x,
        shift_y + StanceOffset(leg, stance_y), z, 0.0, 0.0, 0.0);
    assert(target.valid);
    assert(std::isfinite(target.hip_x));
    assert(std::isfinite(target.hip_y));
    assert(std::isfinite(target.knee));
    assert(std::abs(target.hip_x) <= 0.20);
    assert(target.hip_y >= -1.20 && target.hip_y <= -0.30);
    assert(target.knee >= 0.80 && target.knee <= 1.80);
  }
}

void CheckPhase(int target_leg, double duration,
                double start_lift, double end_lift,
                double start_x, double end_x,
                double start_y, double end_y,
                double start_common_z, double end_common_z,
                double start_front_z, double end_front_z,
                double start_roll_z, double end_roll_z,
                double start_stance_y, double end_stance_y) {
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
        elapsed, duration, start_common_z, end_common_z);
    const BreathScalar front_z = QuinticBreathSegment(
        elapsed, duration, start_front_z, end_front_z);
    const BreathScalar roll_z = QuinticBreathSegment(
        elapsed, duration, start_roll_z, end_roll_z);
    const BreathScalar stance_y = QuinticBreathSegment(
        elapsed, duration, start_stance_y, end_stance_y);
    for (int leg = 0; leg < 4; ++leg) {
      const double side = (leg == 0 || leg == 2) ? 1.0 : -1.0;
      const double z = common_z.position + (leg < 2 ? front_z.position : 0.0) +
          side * roll_z.position + (leg == target_leg ? lift.position : 0.0);
      const double vz = common_z.velocity +
          (leg < 2 ? front_z.velocity : 0.0) + side * roll_z.velocity +
          (leg == target_leg ? lift.velocity : 0.0);
      const CartesianLegJointTarget target = SampleLite3CartesianLeg(
          0.0, kStandHipY, kStandKnee, leg,
          shift_x.position,
          shift_y.position + StanceOffset(leg, stance_y.position), z,
          shift_x.velocity,
          shift_y.velocity + StanceOffset(leg, stance_y.velocity), vz);
      assert(target.valid);
      assert(std::abs(target.hip_x_velocity) < 1.0);
      assert(std::abs(target.hip_y_velocity) < 1.0);
      assert(std::abs(target.knee_velocity) < 1.0);
    }
  }
}

void CheckCompleteLoop(int target_leg) {
  const double shift_x = target_leg == 0
      ? kSadnessLeftSupportShiftXMeters
      : kSadnessRightSupportShiftXMeters;
  const double shift_y = target_leg == 0
      ? -kSadnessSupportShiftYMeters : kSadnessSupportShiftYMeters;
  const double roll = SadnessRollForTarget(target_leg);
  CheckPhase(target_leg, kSadnessSinkSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             0.0, kSadnessCrouchMeters,
             0.0, kSadnessFrontDroopMeters,
             0.0, roll, 0.0, kSadnessStanceMeters);
  CheckPhase(target_leg, kSadnessWithdrawSeconds,
             0.0, 0.0, 0.0, shift_x, 0.0, shift_y,
             kSadnessCrouchMeters, kSadnessCrouchMeters,
             kSadnessFrontDroopMeters, kSadnessFrontDroopMeters,
             roll, roll, kSadnessStanceMeters, kSadnessStanceMeters);
  CheckPhase(target_leg, kSadnessHoverSeconds,
             0.0, kSadnessLiftMeters, shift_x, shift_x, shift_y, shift_y,
             kSadnessCrouchMeters, kSadnessCrouchMeters,
             kSadnessFrontDroopMeters, kSadnessFrontDroopMeters,
             roll, roll, kSadnessStanceMeters, kSadnessStanceMeters);
  CheckPhase(target_leg, kSadnessPauseSeconds,
             kSadnessLiftMeters, kSadnessLiftMeters,
             shift_x, shift_x, shift_y, shift_y,
             kSadnessCrouchMeters, kSadnessCrouchMeters,
             kSadnessFrontDroopMeters, kSadnessFrontDroopMeters,
             roll, roll, kSadnessStanceMeters, kSadnessStanceMeters);
  CheckPhase(target_leg, kSadnessLowerSeconds,
             kSadnessLiftMeters, 0.0, shift_x, 0.0, shift_y, 0.0,
             kSadnessCrouchMeters, kSadnessCrouchMeters,
             kSadnessFrontDroopMeters, kSadnessFrontDroopMeters,
             roll, roll, kSadnessStanceMeters, kSadnessStanceMeters);
  CheckPhase(target_leg, kSadnessLandingDwellSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             kSadnessCrouchMeters, kSadnessCrouchMeters,
             kSadnessFrontDroopMeters, kSadnessFrontDroopMeters,
             roll, roll, kSadnessStanceMeters, kSadnessStanceMeters);
  CheckPhase(target_leg, kSadnessRecoverSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             kSadnessCrouchMeters, 0.0,
             kSadnessFrontDroopMeters, 0.0,
             roll, 0.0, kSadnessStanceMeters, 0.0);
  CheckPose(target_leg, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
}
}  // namespace

int main() {
  static_assert(kSadnessLowerSeconds + kSadnessLandingDwellSeconds ==
                    kSadnessPlaceSeconds,
                "sadness placement must include landing dwell");
  assert(SadnessLimitsValid(kSadnessLiftMeters));
  assert(SadnessLimitsValid(kSadnessMinimumLiftMeters));
  assert(SadnessLimitsValid(kSadnessMaximumLiftMeters));
  assert(!SadnessLimitsValid(0.014));
  assert(!SadnessLimitsValid(0.026));
  assert(SadnessQuinticMaximumSpeed(
      kSadnessLiftMeters, kSadnessLowerSeconds) <=
      kSadnessMaximumPawVelocityMps);
  assert(SadnessQuinticMaximumAcceleration(
      kSadnessLiftMeters, kSadnessLowerSeconds) <=
      kSadnessMaximumPawAccelerationMps2);

  CheckCompleteLoop(0);
  CheckCompleteLoop(1);
  assert(SadnessBodyVisualLimitsValid());
  constexpr double kBowRoll = 0.0;
  CheckPhase(0, kSadnessBodySinkSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             0.0, -kSadnessBowRearRiseMeters,
             0.0, kSadnessBowFrontDropMeters,
             0.0, kBowRoll, 0.0, kSadnessStanceMeters);
  CheckPhase(0, kSadnessBodySettleSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             -kSadnessBowRearRiseMeters, -kSadnessBowRearRiseMeters,
             kSadnessBowFrontDropMeters, kSadnessBowFrontDropMeters,
             kBowRoll, kBowRoll,
             kSadnessStanceMeters, kSadnessStanceMeters);
  for (int sob = 1; sob <= kSadnessSobCycles; ++sob) {
    const double sob_roll = SadnessSobRollForCycle(sob);
    CheckPhase(0, kSadnessSobRiseSeconds,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
               -kSadnessBowRearRiseMeters, -kSadnessBowRearRiseMeters,
               kSadnessBowFrontDropMeters,
               kSadnessBowFrontDropMeters - kSadnessSobRiseMeters,
               kBowRoll, sob_roll,
               kSadnessStanceMeters, kSadnessStanceMeters);
    CheckPhase(0, kSadnessSobFallSeconds,
               0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
               -kSadnessBowRearRiseMeters, -kSadnessBowRearRiseMeters,
               kSadnessBowFrontDropMeters - kSadnessSobRiseMeters,
               kSadnessBowFrontDropMeters,
               sob_roll, kBowRoll,
               kSadnessStanceMeters, kSadnessStanceMeters);
  }
  CheckPhase(0, kSadnessBodyFinalHoldSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             -kSadnessBowRearRiseMeters, -kSadnessBowRearRiseMeters,
             kSadnessBowFrontDropMeters, kSadnessBowFrontDropMeters,
             kBowRoll, kBowRoll,
             kSadnessStanceMeters, kSadnessStanceMeters);
  CheckPhase(0, kSadnessBodyRecoverSeconds,
             0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
             -kSadnessBowRearRiseMeters, 0.0,
             kSadnessBowFrontDropMeters, 0.0,
             kBowRoll, 0.0, kSadnessStanceMeters, 0.0);

  // Every phase uses quintic seams, so position reaches its target with zero
  // velocity.  Sample the fastest paw phase and verify its finite-difference
  // acceleration remains below the sadness-specific envelope.
  double previous_velocity = 0.0;
  double maximum_speed = 0.0;
  double maximum_acceleration = 0.0;
  for (int millisecond = 0;
       millisecond <= static_cast<int>(kSadnessLowerSeconds * 1000.0);
       ++millisecond) {
    const BreathScalar sample = QuinticBreathSegment(
        millisecond / 1000.0, kSadnessLowerSeconds,
        kSadnessLiftMeters, 0.0);
    maximum_speed = std::max(maximum_speed, std::abs(sample.velocity));
    if (millisecond > 0) {
      maximum_acceleration = std::max(
          maximum_acceleration,
          std::abs((sample.velocity - previous_velocity) / 0.001));
    }
    previous_velocity = sample.velocity;
  }
  assert(maximum_speed <= kSadnessMaximumPawVelocityMps);
  assert(maximum_acceleration <= kSadnessMaximumPawAccelerationMps2 + 0.01);
  const BreathScalar seam_start = QuinticBreathSegment(
      0.0, kSadnessLowerSeconds, kSadnessLiftMeters, 0.0);
  const BreathScalar seam_end = QuinticBreathSegment(
      kSadnessLowerSeconds, kSadnessLowerSeconds,
      kSadnessLiftMeters, 0.0);
  assert(seam_start.velocity == 0.0);
  assert(seam_end.position == 0.0);
  assert(seam_end.velocity == 0.0);

  SadnessHoverGate gate;
  assert(gate.BeginHover());
  assert(!gate.BeginHover());
  gate.CompleteLanding(true, false, kSadnessLandingDwellSeconds);
  assert(!gate.next_hover_allowed());
  gate.CompleteLanding(false, true, kSadnessLandingDwellSeconds);
  assert(!gate.next_hover_allowed());
  gate.CompleteLanding(true, true, kSadnessLandingDwellSeconds - 0.001);
  assert(!gate.next_hover_allowed());
  gate.CompleteLanding(true, true, kSadnessLandingDwellSeconds);
  assert(gate.next_hover_allowed());

  // Same-category updates wait for the next loop.  A category change received
  // while hovering latches only the newest target and stale input stays neutral.
  SadnessRetargetTracker same(10);
  same.Observe(true, true, 11, "sadness", -0.9, 0.2);
  assert(!same.cancellation_requested());
  assert(same.may_start_next_hover());
  SadnessRetargetTracker retarget(20);
  retarget.Observe(true, true, 21, "joy", 0.8, 0.9);
  assert(retarget.cancellation_requested());
  assert(!retarget.may_start_next_hover());
  retarget.Observe(true, true, 22, "anger", -0.8, 0.9);
  assert(retarget.emotion() == "anger");
  assert(retarget.last_sequence() == 22);
  SadnessRetargetTracker stale(30);
  stale.Observe(false, false, 30, "sadness", -0.8, 0.2);
  assert(stale.cancellation_requested());
  assert(stale.stale());
  assert(stale.emotion() == "neutral");
  stale.Observe(true, true, 31, "joy", 1.0, 1.0);
  assert(stale.emotion() == "neutral");
  return 0;
}
