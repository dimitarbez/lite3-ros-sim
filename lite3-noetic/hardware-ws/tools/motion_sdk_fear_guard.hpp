#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// Fear is an urgent but low, guarded motion. The commissioning loop is exactly
// five seconds, keeps every non-hovering paw in the tested Cartesian workspace,
// and uses a slower touchdown envelope than the accepted Anger placement.
constexpr double kFearFlinchSeconds = 0.35;
constexpr double kFearRecoilSeconds = 0.55;
constexpr double kFearGuardSeconds = 0.55;
constexpr double kFearPlaceSeconds = 0.55;
constexpr double kFearLowerSeconds = 0.35;
constexpr double kFearLandingDwellSeconds = 0.20;
constexpr double kFearFreezeSeconds = 0.75;
constexpr double kFearRecoverSeconds = 1.15;
constexpr double kFearLoopSeconds = 5.00;

constexpr double kFearLiftMeters = 0.025;
constexpr double kFearMinimumLiftMeters = 0.015;
constexpr double kFearMaximumLiftMeters = 0.025;
constexpr double kFearFlinchMeters = 0.022;
constexpr double kFearGuardedCrouchMeters = 0.018;
constexpr double kFearRecoilXMeters = 0.030;
constexpr double kFearStanceMeters = 0.008;
// The first physical 25 mm FL candidate used a 20 mm rearward shift and
// retained 9.64 N versus a 5.84 N unload threshold. A 30 mm transfer produced
// one marginal pass at 5.99 N and a later 6.53 N failure. The redesigned
// 35 mm transfer still retained 7.26 N on its first physical run, so this
// remains a failed-closed candidate and must not justify weakening the gate.
constexpr double kFearLeftSupportShiftXMeters = 0.035;
constexpr double kFearRightSupportShiftXMeters = 0.035;
constexpr double kFearSupportShiftYMeters = 0.020;
constexpr double kFearMaximumDownwardVelocityMps = 0.135;
constexpr double kFearMaximumDownwardAccelerationMps2 = 1.20;

// A separate all-feet-planted visual diagnostic repeats a five-second
// flinch/recoil/cower cycle three times. It exists so the whole-body emotional
// read can be judged without bypassing a failed paw-unload gate.
constexpr double kFearBodyTrembleSegmentSeconds = 0.45;
constexpr int kFearBodyTrembleSegments = 5;
constexpr double kFearBodyTrembleMeters = 0.008;
constexpr double kFearBodyFreezeSeconds = 0.70;
constexpr double kFearBodyCycleSeconds = 5.00;
constexpr int kFearBodyVisualCycles = 3;
constexpr double kFearBodyVisualSeconds = 15.00;

inline double FearQuinticMaximumSpeed(double distance, double duration) {
  return 1.875 * std::abs(distance) / duration;
}

inline double FearQuinticMaximumAcceleration(double distance,
                                              double duration) {
  return (10.0 / std::sqrt(3.0)) * std::abs(distance) /
      (duration * duration);
}

inline bool FearGuardLimitsValid(double lift_meters) {
  const double duration_sum = kFearFlinchSeconds + kFearRecoilSeconds +
      2.0 * (kFearGuardSeconds + kFearPlaceSeconds) +
      kFearFreezeSeconds + kFearRecoverSeconds;
  return std::isfinite(lift_meters) &&
      lift_meters >= kFearMinimumLiftMeters &&
      lift_meters <= kFearMaximumLiftMeters &&
      std::abs((kFearLowerSeconds + kFearLandingDwellSeconds) -
               kFearPlaceSeconds) < 1e-12 &&
      std::abs(duration_sum - kFearLoopSeconds) < 1e-12 &&
      FearQuinticMaximumSpeed(lift_meters, kFearLowerSeconds) <=
          kFearMaximumDownwardVelocityMps &&
      FearQuinticMaximumAcceleration(lift_meters, kFearLowerSeconds) <=
          kFearMaximumDownwardAccelerationMps2;
}

inline bool FearBodyVisualLimitsValid() {
  const double cycle_seconds = kFearFlinchSeconds + kFearRecoilSeconds +
      kFearBodyTrembleSegments * kFearBodyTrembleSegmentSeconds +
      kFearBodyFreezeSeconds + kFearRecoverSeconds;
  return std::abs(cycle_seconds - kFearBodyCycleSeconds) < 1e-12 &&
      std::abs(kFearBodyVisualCycles * cycle_seconds -
               kFearBodyVisualSeconds) < 1e-12 &&
      kFearBodyTrembleMeters > 0.0 &&
      kFearBodyTrembleMeters <= kFearStanceMeters;
}

class FearGuardGate {
 public:
  bool BeginHover() {
    if (!next_hover_allowed_) return false;
    next_hover_allowed_ = false;
    return true;
  }

  void CompleteLanding(bool four_feet_confirmed, double dwell_seconds) {
    next_hover_allowed_ = four_feet_confirmed &&
        dwell_seconds >= kFearLandingDwellSeconds;
  }

  bool next_hover_allowed() const { return next_hover_allowed_; }

 private:
  bool next_hover_allowed_{true};
};

// Latches only the newest request while Fear owns a guarded hover. A category
// change suppresses every remaining hover, but never interrupts lowering or
// four-foot confirmation. A stale link requests neutral and cannot be revived
// inside the same ownership session.
class FearRetargetTracker {
 public:
  explicit FearRetargetTracker(uint64_t initial_sequence)
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
    if (emotion != "fear") cancellation_requested_ = true;
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
  std::string emotion_{"fear"};
  double valence_{-0.8};
  double arousal_{0.9};
};
