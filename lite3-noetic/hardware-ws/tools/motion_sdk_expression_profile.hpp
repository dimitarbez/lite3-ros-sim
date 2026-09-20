#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "motion_sdk_breath_profile.hpp"

// ROS-independent, four-feet-planted physical choreography.  The declarative
// keyframes preserve the Gazebo ordering but translate its z/roll/pitch axes
// into the already commissioned direct-joint envelope.

struct ExpressionPose {
  double compression{0.0};
  double roll{0.0};
  double pitch{0.0};
};

struct ExpressionSample {
  ExpressionPose position;
  ExpressionPose velocity;
  ExpressionPose acceleration;
};

struct ExpressionKeyframe {
  double duration;
  ExpressionPose target;
};

struct ExpressionProfile {
  std::vector<ExpressionKeyframe> entrance;
  std::vector<ExpressionKeyframe> idle;
};

struct PhysicalProfileDefinition {
  const char* requested_emotion;
  const char* accepted_profile;
  bool physically_accepted;
};

struct PhysicalProfileResolution {
  std::string requested_emotion;
  std::string resolved_emotion;
  std::string profile;
  bool fallback{false};
  std::string fallback_reason;
};

// This table names only the final operator-accepted physical reactions.  The
// runtime allowlist is a separate, fail-closed gate: an accepted reaction is
// not selectable by normal chat until its live chat/retarget test is complete.
inline const std::vector<PhysicalProfileDefinition>& PhysicalProfileTable() {
  static const std::vector<PhysicalProfileDefinition> table{
      {"neutral", "neutral_animal_breath", true},
      {"joy", "joy_alternating_front_paws_50mm", true},
      {"sadness", "sadness_front_bow_48mm", true},
      {"anger", "anger_canonical_paw_placements", true},
      {"fear", "fear_planted_flinch_cower", true},
      {"affection", "neutral_animal_breath", false},
      {"curiosity", "neutral_animal_breath", false},
      {"disgust", "neutral_animal_breath", false},
      {"surprise", "neutral_animal_breath", false},
  };
  return table;
}

inline const PhysicalProfileDefinition& PhysicalProfileFor(
    const std::string& emotion) {
  for (const auto& item : PhysicalProfileTable()) {
    if (emotion == item.requested_emotion) return item;
  }
  throw std::invalid_argument("unknown emotion");
}

inline PhysicalProfileResolution ResolvePhysicalProfile(
    const std::string& emotion, const std::set<std::string>& enabled) {
  const auto& definition = PhysicalProfileFor(emotion);
  if (!definition.physically_accepted) {
    return {emotion, "neutral", "neutral_animal_breath", true,
            "physical_reaction_not_accepted"};
  }
  if (emotion != "neutral" && enabled.count(emotion) == 0) {
    return {emotion, "neutral", "neutral_animal_breath", true,
            "not_enabled_in_normal_allowlist"};
  }
  return {emotion, emotion, definition.accepted_profile, false, ""};
}

// Raised-paw and planted multi-phase reactions sample the state link while
// they own the command stream.  This latch keeps only the newest request and
// never interrupts the current safe phase.  A stale link cannot be revived in
// the same ownership session.
class EmotionRetargetTracker {
 public:
  EmotionRetargetTracker(std::string active_emotion,
                         uint64_t initial_sequence)
      : active_emotion_(std::move(active_emotion)),
        last_sequence_(initial_sequence), emotion_(active_emotion_) {}

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
    valence_ = std::max(-1.0, std::min(1.0, valence));
    arousal_ = std::max(0.0, std::min(1.0, arousal));
    if (emotion != active_emotion_) cancellation_requested_ = true;
  }

  bool cancellation_requested() const { return cancellation_requested_; }
  bool stale() const { return stale_; }
  bool may_start_next_phase() const { return !cancellation_requested_; }
  uint64_t last_sequence() const { return last_sequence_; }
  const std::string& emotion() const { return emotion_; }
  double valence() const { return valence_; }
  double arousal() const { return arousal_; }

 private:
  std::string active_emotion_;
  uint64_t last_sequence_{0};
  bool cancellation_requested_{false};
  bool stale_{false};
  std::string emotion_;
  double valence_{0.0};
  double arousal_{0.2};
};

struct GazeboKeyframe {
  double duration;
  double height;
  double roll;
  double pitch;
  const char* action;
};

inline double ClampExpression(double value, double low, double high) {
  return std::max(low, std::min(high, value));
}

inline ExpressionPose TranslateGazeboPose(double height, double roll,
                                           double pitch) {
  ExpressionPose result;
  result.compression = height >= 0.0
      ? -0.008 * ClampExpression(height / 0.080, 0.0, 1.0)
      : 0.020 * ClampExpression(-height / 0.080, 0.0, 1.0);
  result.roll = 0.004 * ClampExpression(roll / 0.500, -1.0, 1.0);
  result.pitch = 0.003 * ClampExpression(pitch / 0.500, -1.0, 1.0);
  return result;
}

inline void AppendPhysicalKeyframe(std::vector<ExpressionKeyframe>* output,
                                   const GazeboKeyframe& input,
                                   double effective_time_scale) {
  const std::string action(input.action ? input.action : "none");
  if (action == "hop") {
    // Stage one keeps all feet planted: crouch, rise/expand, exact neutral.
    output->push_back({0.75, {0.020, 0.0, 0.0}});
    output->push_back({0.75, {-0.008, 0.0, 0.0}});
    output->push_back({0.75, {0.0, 0.0, 0.0}});
    return;
  }
  if (action == "stomp") {
    // A symmetric double compression pulse; no support shift or foot lift.
    output->push_back({0.75, {0.020, 0.0, 0.0}});
    output->push_back({0.75, {0.0, 0.0, 0.0}});
    output->push_back({0.75, {0.020, 0.0, 0.0}});
    output->push_back({0.75, {0.0, 0.0, 0.0}});
    return;
  }
  output->push_back({std::max(0.75, 2.0 * input.duration * effective_time_scale),
                     TranslateGazeboPose(input.height, input.roll, input.pitch)});
}

inline ExpressionProfile BuildProfile(
    std::initializer_list<GazeboKeyframe> entrance,
    std::initializer_list<GazeboKeyframe> idle,
    double transition_scale = 1.0, double idle_scale = 1.25) {
  ExpressionProfile result;
  for (const auto& item : entrance) {
    AppendPhysicalKeyframe(&result.entrance, item, transition_scale);
  }
  for (const auto& item : idle) {
    AppendPhysicalKeyframe(&result.idle, item, idle_scale);
  }
  return result;
}

inline const std::map<std::string, ExpressionProfile>& PhysicalProfiles() {
  static const std::map<std::string, ExpressionProfile> profiles = [] {
    std::map<std::string, ExpressionProfile> value;
    // Preserve the physically commissioned 3.25 s animal-breath loop exactly.
    value["neutral"] = {{}, {
        {kBreathInhaleSeconds, {kBreathExpansion, kBreathRollBias, -kBreathPitchBias}},
        {kBreathExchangeSeconds, {kBreathCompression, -kBreathRollBias, kBreathPitchBias}},
        {kBreathReturnSeconds, {0.0, 0.0, 0.0}},
    }};
    value["joy"] = BuildProfile(
        {{0.55, -0.055, 0.0, 0.0, "hop"},
         {0.20, 0.080, 0.480, -0.380, "none"},
         {0.20, -0.060, -0.480, 0.340, "none"},
         {0.18, 0.075, 0.440, -0.350, "none"}},
        {{0.28, 0.080, 0.480, -0.380, "none"},
         {0.28, -0.060, -0.480, 0.340, "none"},
         {0.28, 0.078, -0.440, -0.360, "none"},
         {0.28, -0.050, 0.440, 0.300, "none"}});
    value["sadness"] = BuildProfile(
        {{0.28, -0.055, -0.180, 0.350, "none"},
         {0.38, -0.080, -0.320, 0.500, "none"}},
        {{0.75, -0.080, -0.330, 0.500, "none"},
         {0.75, -0.065, 0.330, 0.420, "none"}});
    value["anger"] = BuildProfile(
        {{0.75, 0.0, 0.0, 0.0, "stomp"},
         {0.24, -0.075, 0.450, 0.360, "none"},
         {0.24, -0.060, -0.450, 0.340, "none"},
         {0.24, -0.078, 0.400, 0.400, "none"}},
        {{0.75, -0.070, 0.0, 0.0, "stomp"},
         {0.30, -0.078, 0.460, 0.400, "none"},
         {0.30, -0.060, -0.460, 0.340, "none"},
         {0.30, -0.078, 0.420, 0.400, "none"},
         {0.30, -0.060, -0.420, 0.340, "none"}});
    value["fear"] = BuildProfile(
        {{0.10, 0.045, 0.0, -0.300, "none"},
         {0.20, -0.080, 0.500, 0.470, "none"},
         {0.18, -0.070, -0.500, 0.380, "none"}},
        {{0.18, -0.080, 0.500, 0.470, "none"},
         {0.18, -0.068, -0.500, 0.380, "none"}});
    value["surprise"] = BuildProfile(
        {{0.55, 0.0, 0.0, 0.0, "hop"},
         {0.20, 0.080, 0.220, -0.500, "none"}},
        {{0.32, 0.080, 0.240, -0.500, "none"},
         {0.32, 0.045, -0.240, -0.320, "none"},
         {0.32, 0.078, -0.220, -0.480, "none"},
         {0.32, 0.045, 0.220, -0.320, "none"}});
    value["disgust"] = BuildProfile(
        {{0.12, 0.040, 0.180, -0.220, "none"},
         {0.26, -0.075, -0.500, 0.500, "none"},
         {0.22, -0.035, -0.250, 0.260, "none"}},
        {{0.38, -0.075, -0.500, 0.500, "none"},
         {0.38, -0.030, -0.220, 0.220, "none"},
         {0.38, -0.068, -0.470, 0.460, "none"},
         {0.38, -0.025, -0.180, 0.180, "none"}});
    value["curiosity"] = BuildProfile(
        {{0.24, 0.045, 0.500, -0.420, "none"},
         {0.26, 0.035, -0.500, 0.300, "none"}},
        {{0.48, 0.045, 0.500, -0.420, "none"},
         {0.48, 0.030, -0.500, 0.300, "none"}});
    value["affection"] = BuildProfile(
        {{0.22, 0.025, 0.260, -0.160, "none"},
         {0.34, 0.045, 0.480, -0.240, "none"},
         {0.34, 0.025, -0.480, -0.160, "none"}},
        {{0.55, 0.045, 0.480, -0.240, "none"},
         {0.55, 0.020, -0.480, -0.150, "none"},
         {0.55, 0.040, -0.420, -0.240, "none"},
         {0.55, 0.020, 0.420, -0.150, "none"}});
    return value;
  }();
  return profiles;
}

inline ExpressionPose AddPose(const ExpressionPose& left,
                              const ExpressionPose& right) {
  return {left.compression + right.compression,
          left.roll + right.roll, left.pitch + right.pitch};
}

inline ExpressionPose ScalePose(const ExpressionPose& value, double scale) {
  return {value.compression * scale, value.roll * scale,
          value.pitch * scale};
}

inline ExpressionSample QuinticExpressionSegment(
    double elapsed, double duration, const ExpressionSample& start,
    const ExpressionPose& target) {
  const double t = std::max(0.0, std::min(duration, elapsed));
  const double d = duration;
  auto axis = [t, d](double p0, double v0, double a0, double p1) {
    const double c0 = p0;
    const double c1 = v0;
    const double c2 = 0.5 * a0;
    const double p = p1 - (c0 + c1*d + c2*d*d);
    const double v = -(c1 + 2.0*c2*d);
    const double a = -2.0*c2;
    const double c3 = (10.0*p - 4.0*v*d + 0.5*a*d*d) / (d*d*d);
    const double c4 = (-15.0*p + 7.0*v*d - a*d*d) / (d*d*d*d);
    const double c5 = (6.0*p - 3.0*v*d + 0.5*a*d*d) / (d*d*d*d*d);
    return BreathPose{
        {c0 + c1*t + c2*t*t + c3*t*t*t + c4*std::pow(t,4) + c5*std::pow(t,5),
         c1 + 2*c2*t + 3*c3*t*t + 4*c4*std::pow(t,3) + 5*c5*std::pow(t,4)},
        {2*c2 + 6*c3*t + 12*c4*t*t + 20*c5*std::pow(t,3), 0.0},
        {0.0, 0.0}};
  };
  const BreathPose c = axis(start.position.compression, start.velocity.compression,
                            start.acceleration.compression, target.compression);
  const BreathPose r = axis(start.position.roll, start.velocity.roll,
                            start.acceleration.roll, target.roll);
  const BreathPose p = axis(start.position.pitch, start.velocity.pitch,
                            start.acceleration.pitch, target.pitch);
  return {{c.compression.position, r.compression.position, p.compression.position},
          {c.compression.velocity, r.compression.velocity, p.compression.velocity},
          {c.roll.position, r.roll.position, p.roll.position}};
}

inline ExpressionSample SampleKeyframes(
    const std::vector<ExpressionKeyframe>& frames, double elapsed,
    const ExpressionPose& initial, uint64_t* cycles = nullptr) {
  if (frames.empty()) return {{}, {}, {}};
  double total = 0.0;
  for (const auto& frame : frames) total += frame.duration;
  uint64_t cycle = elapsed <= 0.0 ? 0 : static_cast<uint64_t>(elapsed / total);
  elapsed = std::fmod(std::max(0.0, elapsed), total);
  if (cycles) *cycles = cycle;
  ExpressionPose previous = cycle == 0 ? initial : frames.back().target;
  for (const auto& frame : frames) {
    if (elapsed <= frame.duration) {
      ExpressionSample start{previous, {}, {}};
      return QuinticExpressionSegment(elapsed, frame.duration, start, frame.target);
    }
    elapsed -= frame.duration;
    previous = frame.target;
  }
  return {frames.back().target, {}, {}};
}

class ExpressionEngine {
 public:
  explicit ExpressionEngine(std::set<std::string> commissioned = {"neutral"},
                            double scale = 1.0)
      : commissioned_(std::move(commissioned)), scale_(scale) {
    if (scale_ <= 0.0 || scale_ > 1.0) throw std::invalid_argument("invalid profile scale");
    commissioned_.insert("neutral");
  }

  void Request(const std::string& emotion, double valence, double arousal,
               double now) {
    if (!PhysicalProfiles().count(emotion)) throw std::invalid_argument("unknown emotion");
    requested_ = emotion;
    valence_ = ClampExpression(valence, -1.0, 1.0);
    arousal_ = ClampExpression(arousal, 0.0, 1.0);
    const std::string target =
        ResolvePhysicalProfile(emotion, commissioned_).resolved_emotion;
    if (target == desired_) {
      // Two requested categories may intentionally resolve to one physical
      // profile. Keep the newest request observable without restarting motion.
      if (phase_ == "neutral_return" || phase_ == "neutral_hold") {
        pending_requested_ = emotion;
      }
      return;  // Same resolved-profile updates never restart.
    }
    desired_ = target;
    if (phase_ == "neutral_return" || phase_ == "neutral_hold") {
      pending_ = target;  // Rapid retargeting never restarts the reset.
      pending_requested_ = emotion;
      return;
    }
    pending_ = target;
    pending_requested_ = emotion;
    BeginReturn(now);
  }

  void LinkStale(double now) {
    requested_ = "neutral";
    desired_ = "neutral";
    pending_ = "neutral";
    pending_requested_ = "neutral";
    stale_ = true;
    if (phase_ != "neutral_return" && phase_ != "neutral_hold") BeginReturn(now);
  }

  // A lifted-paw controller owns the complete safe transition itself: finish
  // lowering, restore canonical stand over 1.5 s, then hold exact stand for
  // 0.35 s. Once those gates pass, resume this declarative engine directly at
  // the newest requested profile without commanding a second reset movement.
  void CompleteExternalNeutralTransition(const std::string& emotion,
                                         double valence, double arousal,
                                         double now) {
    if (!PhysicalProfiles().count(emotion)) {
      throw std::invalid_argument("unknown emotion");
    }
    requested_ = emotion;
    valence_ = ClampExpression(valence, -1.0, 1.0);
    arousal_ = ClampExpression(arousal, 0.0, 1.0);
    desired_ = ResolvePhysicalProfile(emotion, commissioned_).resolved_emotion;
    active_ = desired_;
    pending_.clear();
    pending_requested_.clear();
    phase_ = "profile";
    phase_started_ = now;
    return_start_ = {};
    last_ = {};
    profile_cycle_ = 0;
    stale_ = false;
    reset_complete_ = false;
  }

  ExpressionSample Sample(double now) {
    UpdateAffect(now);
    if (phase_ == "neutral_return") {
      const double elapsed = now - phase_started_;
      if (elapsed < kNeutralReturnSeconds) {
        last_ = QuinticExpressionSegment(
            elapsed, kNeutralReturnSeconds, return_start_, {});
        return Scale(last_);
      }
      last_ = {{}, {}, {}};
      phase_ = "neutral_hold";
      phase_started_ += kNeutralReturnSeconds;
    }
    if (phase_ == "neutral_hold") {
      last_ = {{}, {}, {}};
      if (now - phase_started_ < kNeutralHoldSeconds) return last_;
      active_ = pending_.empty() ? desired_ : pending_;
      pending_.clear();
      pending_requested_.clear();
      phase_ = "profile";
      phase_started_ += kNeutralHoldSeconds;
      reset_complete_ = stale_;
    }
    const ExpressionProfile& profile = PhysicalProfiles().at(active_);
    double elapsed = std::max(0.0, now - phase_started_);
    ExpressionPose previous{};
    for (const auto& frame : profile.entrance) {
      if (elapsed <= frame.duration) {
        last_ = QuinticExpressionSegment(elapsed, frame.duration,
                                         {previous, {}, {}}, frame.target);
        profile_cycle_ = 0;
        return Scale(last_);
      }
      elapsed -= frame.duration;
      previous = frame.target;
    }
    last_ = SampleKeyframes(profile.idle, elapsed, previous, &profile_cycle_);
    return Scale(last_);
  }

  const std::string& requested() const { return requested_; }
  const std::string& active() const { return active_; }
  const std::string& pending() const { return pending_; }
  const std::string& pending_requested() const { return pending_requested_; }
  const std::string& phase() const { return phase_; }
  PhysicalProfileResolution requested_resolution() const {
    return ResolvePhysicalProfile(requested_, commissioned_);
  }
  PhysicalProfileResolution active_resolution() const {
    return ResolvePhysicalProfile(active_, commissioned_);
  }
  PhysicalProfileResolution pending_resolution() const {
    return ResolvePhysicalProfile(pending_.empty() ? desired_ : pending_,
                                  commissioned_);
  }
  uint64_t profile_cycle() const { return profile_cycle_; }
  bool stale_reset_complete() const { return reset_complete_; }
  double valence() const { return valence_; }
  double arousal() const { return arousal_; }
  double filtered_valence() const { return filtered_valence_; }
  double filtered_arousal() const { return filtered_arousal_; }

  static constexpr double kNeutralReturnSeconds = 1.5;
  static constexpr double kNeutralHoldSeconds = 0.35;

 private:
  void UpdateAffect(double now) {
    if (!have_last_sample_time_) {
      last_sample_time_ = now;
      have_last_sample_time_ = true;
      return;
    }
    const double dt = std::max(0.0, now - last_sample_time_);
    last_sample_time_ = now;
    const double valence_alpha = 1.0 - std::exp(-dt / 0.45);
    const double arousal_alpha = 1.0 - std::exp(-dt / 0.35);
    filtered_valence_ += (valence_ - filtered_valence_) * valence_alpha;
    filtered_arousal_ += (arousal_ - filtered_arousal_) * arousal_alpha;
  }

  void BeginReturn(double now) {
    return_start_ = last_;
    phase_ = "neutral_return";
    phase_started_ = now;
    reset_complete_ = false;
  }

  ExpressionSample Scale(const ExpressionSample& sample) const {
    // Preserve the Gazebo affect formula. With physical_min_intensity=1.0 the
    // result is deliberately one, so filtered arousal cannot change amplitude
    // until that independent physical behavior has been commissioned.
    constexpr double physical_min_intensity = 1.0;
    const double intensity = physical_min_intensity +
        (1.0 - physical_min_intensity) * filtered_arousal_;
    const double gain = scale_ * ClampExpression(intensity, 0.0, 1.0);
    return {ScalePose(sample.position, gain), ScalePose(sample.velocity, gain),
            ScalePose(sample.acceleration, gain)};
  }

  std::set<std::string> commissioned_;
  double scale_{1.0};
  std::string requested_{"neutral"};
  std::string desired_{"neutral"};
  std::string active_{"neutral"};
  std::string pending_;
  std::string pending_requested_;
  std::string phase_{"profile"};
  double phase_started_{0.0};
  double valence_{0.0};
  double arousal_{0.2};
  double filtered_valence_{0.0};
  double filtered_arousal_{0.2};
  double last_sample_time_{0.0};
  bool have_last_sample_time_{false};
  bool stale_{false};
  bool reset_complete_{false};
  uint64_t profile_cycle_{0};
  ExpressionSample return_start_{};
  ExpressionSample last_{};
};

inline BreathPose ToBreathPose(const ExpressionSample& sample) {
  return {{sample.position.compression, sample.velocity.compression},
          {sample.position.roll, sample.velocity.roll},
          {sample.position.pitch, sample.velocity.pitch}};
}
