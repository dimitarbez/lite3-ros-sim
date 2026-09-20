#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// Physical anger is a controlled placement, never an impact.  These values
// deliberately keep the first commissioning target below the already accepted
// 50 mm joy lift while making the lowering cadence measurably more deliberate.
constexpr double kAngerBraceSeconds = 0.55;
constexpr double kAngerLiftSeconds = 0.65;
constexpr double kAngerPlaceSeconds = 0.60;
constexpr double kAngerLowerSeconds = 0.35;
constexpr double kAngerLandingDwellSeconds = 0.25;
constexpr double kAngerRelatchSeconds = 1.00;
constexpr double kAngerRelatchHoldSeconds = 0.35;
constexpr double kAngerHoldSeconds = 0.65;
constexpr double kAngerRecoverSeconds = 1.30;
constexpr double kAngerLoopSeconds = 7.70;

constexpr double kAngerLiftMeters = 0.035;
constexpr double kAngerMaximumLiftMeters = 0.035;
constexpr double kAngerLeftSupportShiftXMeters = 0.025;
constexpr double kAngerRightSupportShiftXMeters = 0.035;
constexpr double kAngerSupportShiftYMeters = 0.020;
constexpr double kAngerBraceMeters = 0.008;
constexpr double kAngerStanceMeters = 0.006;
constexpr double kAngerMaximumDownwardVelocityMps = 0.190;
constexpr double kAngerMaximumDownwardAccelerationMps2 = 1.70;

inline double QuinticMaximumSpeed(double distance, double duration) {
  return 1.875 * std::abs(distance) / duration;
}

inline double QuinticMaximumAcceleration(double distance, double duration) {
  // max(abs(d2/du2(10u^3 - 15u^4 + 6u^5))) = 10/sqrt(3)
  return (10.0 / std::sqrt(3.0)) * std::abs(distance) /
      (duration * duration);
}

inline bool AngerStompLimitsValid(double lift_meters) {
  const double duration_sum = kAngerBraceSeconds + 2.0 * kAngerLiftSeconds +
      2.0 * (kAngerPlaceSeconds + kAngerRelatchSeconds +
             kAngerRelatchHoldSeconds) +
      kAngerHoldSeconds + kAngerRecoverSeconds;
  return std::isfinite(lift_meters) && lift_meters >= 0.010 &&
      lift_meters <= kAngerMaximumLiftMeters &&
      std::abs((kAngerLowerSeconds + kAngerLandingDwellSeconds) -
               kAngerPlaceSeconds) < 1e-12 &&
      std::abs(duration_sum - kAngerLoopSeconds) < 1e-12 &&
      QuinticMaximumSpeed(lift_meters, kAngerLowerSeconds) <=
          kAngerMaximumDownwardVelocityMps &&
      QuinticMaximumAcceleration(lift_meters, kAngerLowerSeconds) <=
          kAngerMaximumDownwardAccelerationMps2;
}

class AngerStompGate {
 public:
  bool BeginStomp() {
    if (!next_stomp_allowed_) return false;
    next_stomp_allowed_ = false;
    return true;
  }

  void CompleteLanding(bool four_feet_confirmed, double dwell_seconds) {
    next_stomp_allowed_ = four_feet_confirmed &&
        dwell_seconds >= kAngerLandingDwellSeconds;
  }

  bool next_stomp_allowed() const { return next_stomp_allowed_; }

 private:
  bool next_stomp_allowed_{true};
};

// Tracks the newest chat request while a lifted-paw action owns the command
// stream. A category change never interrupts a raised paw: the runtime uses
// this latch to suppress the next stomp, complete landing/re-latch, recover to
// exact neutral, and only then hand off to the newest pending category.
class AngerRetargetTracker {
 public:
  explicit AngerRetargetTracker(uint64_t initial_sequence)
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
    if (emotion != "anger") cancellation_requested_ = true;
  }

  bool cancellation_requested() const { return cancellation_requested_; }
  bool stale() const { return stale_; }
  bool may_start_next_stomp() const { return !cancellation_requested_; }
  uint64_t last_sequence() const { return last_sequence_; }
  const std::string& emotion() const { return emotion_; }
  double valence() const { return valence_; }
  double arousal() const { return arousal_; }

 private:
  uint64_t last_sequence_{0};
  bool cancellation_requested_{false};
  bool stale_{false};
  std::string emotion_{"anger"};
  double valence_{-0.7};
  double arousal_{0.8};
};
