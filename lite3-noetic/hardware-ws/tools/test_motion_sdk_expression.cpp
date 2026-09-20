#include <cassert>
#include <cmath>
#include <set>
#include <string>

#include "motion_sdk_expression_profile.hpp"

namespace {

bool Near(double left, double right, double tolerance = 1e-8) {
  return std::abs(left - right) <= tolerance;
}

double LegValue(const ExpressionSample& sample, int leg) {
  const double roll_sign = (leg == 0 || leg == 2) ? 1.0 : -1.0;
  const double pitch_sign = (leg == 0 || leg == 1) ? 1.0 : -1.0;
  return sample.position.compression + roll_sign * sample.position.roll +
      pitch_sign * sample.position.pitch;
}

}  // namespace

int main() {
  const auto& profiles = PhysicalProfiles();
  assert(profiles.size() == 9);
  for (const char* name : {"neutral", "joy", "sadness", "anger", "fear",
                           "surprise", "disgust", "curiosity", "affection"}) {
    assert(profiles.count(name) == 1);
  }

  // The normal selector is explicit and independent of compiled prototypes.
  // Only operator-accepted final reactions may resolve away from neutral.
  const auto& routing = PhysicalProfileTable();
  assert(routing.size() == 9);
  const std::set<std::string> completed{
      "neutral", "joy", "sadness", "fear", "anger"};
  const std::map<std::string, std::string> accepted_profiles{
      {"neutral", "neutral_animal_breath"},
      {"joy", "joy_alternating_front_paws_50mm"},
      {"sadness", "sadness_front_bow_48mm"},
      {"fear", "fear_planted_flinch_cower"},
      {"anger", "anger_canonical_paw_placements"},
  };
  for (const auto& item : routing) {
    const PhysicalProfileResolution resolution =
        ResolvePhysicalProfile(item.requested_emotion, completed);
    const auto accepted = accepted_profiles.find(item.requested_emotion);
    if (accepted != accepted_profiles.end()) {
      assert(!resolution.fallback);
      assert(resolution.resolved_emotion == item.requested_emotion);
      assert(resolution.profile == accepted->second);
      assert(resolution.fallback_reason.empty());
    } else {
      assert(resolution.fallback);
      assert(resolution.resolved_emotion == "neutral");
      assert(resolution.profile == "neutral_animal_breath");
      assert(resolution.fallback_reason ==
             "physical_reaction_not_accepted");
    }
  }
  const PhysicalProfileResolution gated_joy =
      ResolvePhysicalProfile("joy", {"neutral"});
  assert(gated_joy.fallback);
  assert(gated_joy.fallback_reason == "not_enabled_in_normal_allowlist");

  // The commissioned neutral is byte-for-semantics equivalent to the proven
  // three-segment animal breath at endpoints and throughout the cycle.
  for (int index = 0; index <= 3250; ++index) {
    const double elapsed = index / 1000.0;
    uint64_t cycle = 0;
    const ExpressionSample physical = SampleKeyframes(
        profiles.at("neutral").idle, elapsed, {}, &cycle);
    const BreathPose proven = SampleAnimalBreath(elapsed);
    assert(Near(physical.position.compression, proven.compression.position));
    assert(Near(physical.position.roll, proven.roll.position));
    assert(Near(physical.position.pitch, proven.pitch.position));
  }

  std::set<long long> signatures;
  for (const auto& entry : profiles) {
    const ExpressionProfile& profile = entry.second;
    ExpressionPose previous{};
    for (const auto* frames : {&profile.entrance, &profile.idle}) {
      if (frames == &profile.idle && !profile.entrance.empty()) {
        previous = profile.entrance.back().target;
      }
      for (const auto& frame : *frames) {
        const ExpressionSample start = QuinticExpressionSegment(
            0.0, frame.duration, {previous, {}, {}}, frame.target);
        const ExpressionSample end = QuinticExpressionSegment(
            frame.duration, frame.duration, {previous, {}, {}}, frame.target);
        assert(Near(start.position.compression, previous.compression));
        assert(Near(end.position.compression, frame.target.compression));
        assert(Near(end.position.roll, frame.target.roll));
        assert(Near(end.position.pitch, frame.target.pitch));
        assert(Near(start.velocity.compression, 0.0));
        assert(Near(end.velocity.compression, 0.0));
        assert(Near(start.acceleration.compression, 0.0));
        assert(Near(end.acceleration.compression, 0.0));
        previous = frame.target;
      }
    }
    double duration = 0.0;
    for (const auto& frame : profile.entrance) duration += frame.duration;
    for (const auto& frame : profile.idle) duration += frame.duration;
    assert(duration > 0.0);
    long long signature = 0;
    double maximum_velocity = 0.0;
    double maximum_acceleration = 0.0;
    for (int index = 0; index <= 10000; ++index) {
      const double now = duration * index / 10000.0;
      ExpressionEngine engine({"neutral", entry.first});
      engine.Request(entry.first, 0.2, 0.8, 0.0);
      ExpressionSample sample = engine.Sample(now);
      for (int leg = 0; leg < 4; ++leg) {
        const double compression = LegValue(sample, leg);
        assert(std::isfinite(compression));
        assert(compression >= -0.015000001);
        assert(compression <= 0.027000001);
        const double roll_sign = (leg == 0 || leg == 2) ? 1.0 : -1.0;
        const double pitch_sign = (leg == 0 || leg == 1) ? 1.0 : -1.0;
        const double velocity = sample.velocity.compression +
            roll_sign * sample.velocity.roll + pitch_sign * sample.velocity.pitch;
        const double acceleration = sample.acceleration.compression +
            roll_sign * sample.acceleration.roll + pitch_sign * sample.acceleration.pitch;
        maximum_velocity = std::max(maximum_velocity, 2.0 * std::abs(velocity));
        maximum_acceleration = std::max(maximum_acceleration, 2.0 * std::abs(acceleration));
      }
      signature += static_cast<long long>(std::llround(
          1e8 * (sample.position.compression + 3.0 * sample.position.roll +
                 7.0 * sample.position.pitch))) * (index + 1);
    }
    assert(maximum_velocity <= 0.20);
    assert(maximum_acceleration <= 0.80);
    signatures.insert(signature);
  }
  assert(signatures.size() == 9);

  // Every category change, including neutral -> emotion, takes the exact
  // 1.5 s return plus 0.35 s hold. Retargeting changes only the pending goal.
  ExpressionEngine engine({"neutral", "joy", "anger", "fear"});
  engine.Sample(0.4);
  engine.Request("joy", 0.8, 0.9, 0.4);
  assert(engine.phase() == "neutral_return");
  engine.Request("anger", -0.7, 0.9, 0.8);
  assert(engine.pending() == "anger");
  assert(engine.phase() == "neutral_return");
  ExpressionSample seam = engine.Sample(1.9);
  assert(Near(seam.position.compression, 0.0));
  assert(Near(seam.velocity.compression, 0.0));
  seam = engine.Sample(2.24);
  assert(engine.phase() == "neutral_hold");
  assert(Near(seam.position.compression, 0.0));
  engine.Sample(2.26);
  assert(engine.active() == "anger");

  const uint64_t cycle_before = engine.profile_cycle();
  const double filtered_arousal_before = engine.filtered_arousal();
  engine.Request("anger", -0.2, 0.4, 2.5);
  assert(engine.phase() == "profile");
  assert(engine.profile_cycle() == cycle_before);
  engine.Sample(2.6);
  assert(engine.filtered_arousal() < filtered_arousal_before);
  assert(engine.filtered_arousal() > 0.4);

  // A lifted-paw runtime performs its own 1.5 s neutral return and 0.35 s
  // exact hold. Handoff must enter only the newest target without scheduling
  // a duplicate reset movement.
  engine.CompleteExternalNeutralTransition("joy", 0.7, 0.8, 2.8);
  assert(engine.requested() == "joy");
  assert(engine.active() == "joy");
  assert(engine.pending().empty());
  assert(engine.phase() == "profile");
  const ExpressionSample external_handoff = engine.Sample(2.8);
  assert(Near(external_handoff.position.compression, 0.0));
  assert(Near(external_handoff.velocity.compression, 0.0));

  ExpressionEngine anger_only({"neutral", "anger"});
  anger_only.CompleteExternalNeutralTransition("joy", 0.7, 0.8, 1.0);
  assert(anger_only.requested() == "joy");
  assert(anger_only.active() == "neutral");
  assert(anger_only.phase() == "profile");

  engine.Request("fear", -0.8, 1.0, 3.0);
  engine.LinkStale(3.2);
  assert(engine.phase() == "neutral_return");
  engine.Sample(4.7);
  assert(!engine.stale_reset_complete());
  engine.Sample(5.051);
  assert(engine.stale_reset_complete());

  // Uncommissioned categories are understood but remain on neutral.
  ExpressionEngine neutral_only;
  neutral_only.Request("joy", 1.0, 1.0, 0.1);
  neutral_only.Sample(2.0);
  assert(neutral_only.active() == "neutral");
  assert(neutral_only.requested() == "joy");
  assert(neutral_only.requested_resolution().fallback);

  // Unsupported -> unsupported changes remain truthful in status but create
  // no needless reset because both requests resolve to the same neutral loop.
  ExpressionEngine fallbacks(completed);
  fallbacks.Sample(0.0);
  fallbacks.Request("affection", 0.8, 0.4, 0.1);
  assert(fallbacks.requested() == "affection");
  assert(fallbacks.active() == "neutral");
  assert(fallbacks.phase() == "profile");
  fallbacks.Request("curiosity", 0.4, 0.7, 0.2);
  assert(fallbacks.requested() == "curiosity");
  assert(fallbacks.active() == "neutral");
  assert(fallbacks.phase() == "profile");

  // During one neutral reset, rapid accepted -> unsupported -> accepted
  // retargeting keeps only the newest resolved request.
  ExpressionEngine rapid(completed);
  rapid.Request("joy", 0.8, 0.9, 0.0);
  rapid.Request("surprise", 0.5, 1.0, 0.2);
  assert(rapid.pending() == "neutral");
  assert(rapid.pending_requested() == "surprise");
  rapid.Request("fear", -0.8, 0.9, 0.3);
  assert(rapid.pending() == "fear");
  assert(rapid.pending_requested() == "fear");
  rapid.Sample(ExpressionEngine::kNeutralReturnSeconds);
  rapid.Sample(ExpressionEngine::kNeutralReturnSeconds +
               ExpressionEngine::kNeutralHoldSeconds + 0.001);
  assert(rapid.active() == "fear");
  assert(rapid.requested() == "fear");

  // Every multi-phase runtime uses the same newest-request latch. A category
  // change in any named phase suppresses the next phase; later requests replace
  // the pending target without clearing cancellation.
  const std::vector<std::string> phases{
      "joy_transfer_left", "joy_lift_left", "joy_hold_left",
      "joy_lower_left", "joy_settle_left", "joy_transfer_right",
      "joy_lift_right", "joy_hold_right", "joy_lower_right",
      "joy_settle_right", "sadness_sink", "sadness_settle",
      "sadness_sob_rise", "sadness_sob_fall", "sadness_hold",
      "sadness_recover", "fear_flinch", "fear_recoil",
      "fear_cower", "fear_freeze", "fear_recover"};
  uint64_t sequence = 100;
  for (const auto& phase : phases) {
    (void)phase;
    const std::string active = phase.find("joy_") == 0 ? "joy" :
        (phase.find("sadness_") == 0 ? "sadness" : "fear");
    EmotionRetargetTracker tracker(active, sequence);
    tracker.Observe(true, true, ++sequence, "anger", -0.7, 0.9);
    assert(tracker.cancellation_requested());
    assert(!tracker.may_start_next_phase());
    tracker.Observe(true, true, ++sequence, "affection", 0.7, 0.4);
    assert(tracker.emotion() == "affection");
    assert(tracker.last_sequence() == sequence);
  }
  return 0;
}
