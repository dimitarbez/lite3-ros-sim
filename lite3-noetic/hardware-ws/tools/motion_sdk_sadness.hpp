#pragma once

#include <cmath>
#include <cstdint>
#include <string>

// Sadness is one complete six-second, low-energy loop.  The whole-body sink,
// front droop, lateral bias, and low paw hover are deliberately slower and
// smaller than the accepted Joy paw gesture.  All distances are Cartesian
// foot offsets in the body frame; world velocity, yaw, feed-forward torque,
// and gait/action commands remain unused.
constexpr double kSadnessSinkSeconds = 1.20;
constexpr double kSadnessWithdrawSeconds = 1.00;
constexpr double kSadnessHoverSeconds = 0.80;
constexpr double kSadnessPauseSeconds = 1.00;
constexpr double kSadnessPlaceSeconds = 0.80;
constexpr double kSadnessLowerSeconds = 0.55;
constexpr double kSadnessLandingDwellSeconds = 0.25;
constexpr double kSadnessRecoverSeconds = 1.20;
constexpr double kSadnessLoopSeconds = 6.00;
constexpr double kSadnessBodySinkSeconds = 1.50;
constexpr double kSadnessBodySettleSeconds = 0.50;
constexpr double kSadnessSobRiseSeconds = 0.50;
constexpr double kSadnessSobFallSeconds = 0.60;
constexpr int kSadnessSobCycles = 3;
constexpr double kSadnessBodyFinalHoldSeconds = 1.00;
constexpr double kSadnessBodyRecoverSeconds = 1.50;
constexpr double kSadnessBodyVisualSeconds = 7.80;

constexpr double kSadnessCrouchMeters = 0.020;
constexpr double kSadnessFrontDroopMeters = 0.005;
constexpr double kSadnessRollMeters = 0.002;
constexpr double kSadnessStanceMeters = 0.004;
constexpr double kSadnessLiftMeters = 0.020;
constexpr double kSadnessMinimumLiftMeters = 0.015;
constexpr double kSadnessMaximumLiftMeters = 0.025;
constexpr double kSadnessLeftSupportShiftXMeters = 0.020;
constexpr double kSadnessRightSupportShiftXMeters = 0.035;
constexpr double kSadnessSupportShiftYMeters = 0.020;
constexpr double kSadnessBowRearRiseMeters = 0.000;
constexpr double kSadnessBowFrontDropMeters = 0.048;
constexpr double kSadnessSobRiseMeters = 0.014;
constexpr double kSadnessSobRollMeters = 0.004;

// These are stricter than the accepted five-second Joy suite's 50 mm lift in
// 0.60 s.  They cover the lift and the 0.55 s gentle lowering independently.
constexpr double kSadnessMaximumPawVelocityMps = 0.090;
constexpr double kSadnessMaximumPawAccelerationMps2 = 0.50;

inline double SadnessQuinticMaximumSpeed(double distance, double duration) {
  return 1.875 * std::abs(distance) / duration;
}

inline double SadnessQuinticMaximumAcceleration(double distance,
                                                 double duration) {
  return (10.0 / std::sqrt(3.0)) * std::abs(distance) /
      (duration * duration);
}

inline double SadnessRollForTarget(int target_leg) {
  // Positive roll offset lowers the left side and raises the right side in the
  // Cartesian command helper.  Bias toward the opposite support side so the
  // selected front paw is not loaded by the emotional body tilt.
  return target_leg == 0 ? -kSadnessRollMeters : kSadnessRollMeters;
}

inline bool SadnessLimitsValid(double lift_meters) {
  const double duration_sum = kSadnessSinkSeconds +
      kSadnessWithdrawSeconds + kSadnessHoverSeconds +
      kSadnessPauseSeconds + kSadnessPlaceSeconds +
      kSadnessRecoverSeconds;
  const double maximum_target_z = kSadnessCrouchMeters +
      kSadnessFrontDroopMeters - kSadnessRollMeters + lift_meters;
  return std::isfinite(lift_meters) &&
      lift_meters >= kSadnessMinimumLiftMeters &&
      lift_meters <= kSadnessMaximumLiftMeters &&
      std::abs(kSadnessLowerSeconds + kSadnessLandingDwellSeconds -
               kSadnessPlaceSeconds) < 1e-12 &&
      std::abs(duration_sum - kSadnessLoopSeconds) < 1e-12 &&
      maximum_target_z <= 0.050 &&
      SadnessQuinticMaximumSpeed(lift_meters, kSadnessHoverSeconds) <=
          kSadnessMaximumPawVelocityMps &&
      SadnessQuinticMaximumSpeed(lift_meters, kSadnessLowerSeconds) <=
          kSadnessMaximumPawVelocityMps &&
      SadnessQuinticMaximumAcceleration(lift_meters,
                                        kSadnessHoverSeconds) <=
          kSadnessMaximumPawAccelerationMps2 &&
      SadnessQuinticMaximumAcceleration(lift_meters,
                                        kSadnessLowerSeconds) <=
          kSadnessMaximumPawAccelerationMps2;
}

inline bool SadnessBodyVisualLimitsValid() {
  const double deepest_front_z =
      -kSadnessBowRearRiseMeters + kSadnessBowFrontDropMeters;
  const double raised_front_z = -kSadnessBowRearRiseMeters +
      kSadnessBowFrontDropMeters - kSadnessSobRiseMeters;
  const double maximum_sob_travel =
      kSadnessSobRiseMeters + kSadnessSobRollMeters;
  const double duration = kSadnessBodySinkSeconds +
      kSadnessBodySettleSeconds + kSadnessSobCycles *
          (kSadnessSobRiseSeconds + kSadnessSobFallSeconds) +
      kSadnessBodyFinalHoldSeconds + kSadnessBodyRecoverSeconds;
  return std::abs(duration - kSadnessBodyVisualSeconds) < 1e-12 &&
      kSadnessSobCycles == 3 && deepest_front_z <= 0.050 &&
      kSadnessBowRearRiseMeters <= 0.050 &&
      raised_front_z + kSadnessSobRollMeters <= 0.050 &&
      SadnessQuinticMaximumSpeed(deepest_front_z,
                                 kSadnessBodySinkSeconds) <=
          kSadnessMaximumPawVelocityMps &&
      SadnessQuinticMaximumAcceleration(deepest_front_z,
                                        kSadnessBodySinkSeconds) <=
          kSadnessMaximumPawAccelerationMps2 &&
      SadnessQuinticMaximumSpeed(maximum_sob_travel,
                                 kSadnessSobRiseSeconds) <=
          kSadnessMaximumPawVelocityMps &&
      SadnessQuinticMaximumAcceleration(maximum_sob_travel,
                                        kSadnessSobRiseSeconds) <=
          kSadnessMaximumPawAccelerationMps2 &&
      SadnessQuinticMaximumSpeed(maximum_sob_travel,
                                 kSadnessSobFallSeconds) <=
          kSadnessMaximumPawVelocityMps &&
      SadnessQuinticMaximumAcceleration(maximum_sob_travel,
                                        kSadnessSobFallSeconds) <=
          kSadnessMaximumPawAccelerationMps2;
}

inline double SadnessSobRollForCycle(int cycle) {
  return cycle % 2 == 1 ? kSadnessSobRollMeters : -kSadnessSobRollMeters;
}

class SadnessHoverGate {
 public:
  bool BeginHover() {
    if (!next_hover_allowed_) return false;
    next_hover_allowed_ = false;
    return true;
  }

  void CompleteLanding(bool paw_loaded, bool four_feet_recovered,
                       double dwell_seconds) {
    next_hover_allowed_ = paw_loaded && four_feet_recovered &&
        dwell_seconds >= kSadnessLandingDwellSeconds;
  }

  bool next_hover_allowed() const { return next_hover_allowed_; }

 private:
  bool next_hover_allowed_{true};
};

// While a paw is raised, keep only the newest request.  A category change (or
// stale link) suppresses the next sadness loop but never interrupts placement.
class SadnessRetargetTracker {
 public:
  explicit SadnessRetargetTracker(uint64_t initial_sequence)
      : last_sequence_(initial_sequence) {}

  void Observe(bool valid, bool fresh, uint64_t sequence,
               const std::string& emotion, double valence, double arousal) {
    if (stale_) return;
    if (!valid || !fresh) {
      cancellation_requested_ = true;
      stale_ = true;
      emotion_ = "neutral";
      valence_ = 0.0;
      arousal_ = 0.2;
      return;
    }
    if (sequence == last_sequence_) return;
    last_sequence_ = sequence;
    emotion_ = emotion;
    valence_ = valence;
    arousal_ = arousal;
    if (emotion != "sadness") cancellation_requested_ = true;
  }

  bool cancellation_requested() const { return cancellation_requested_; }
  bool stale() const { return stale_; }
  bool may_start_next_hover() const { return !cancellation_requested_; }
  uint64_t last_sequence() const { return last_sequence_; }
  const std::string& emotion() const { return emotion_; }
  double valence() const { return valence_; }
  double arousal() const { return arousal_; }

 private:
  uint64_t last_sequence_{0};
  bool cancellation_requested_{false};
  bool stale_{false};
  std::string emotion_{"sadness"};
  double valence_{-0.7};
  double arousal_{0.3};
};
