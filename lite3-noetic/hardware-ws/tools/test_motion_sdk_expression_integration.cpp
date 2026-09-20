#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "motion_sdk_anger_stomp.hpp"
#include "motion_sdk_expression_profile.hpp"
#include "motion_sdk_fear_guard.hpp"
#include "motion_sdk_sadness.hpp"
#include "motion_sdk_shared_state.hpp"

struct FakeSender {
  int acquisitions{0};
  int releases{0};
  uint64_t commands{0};
  void Acquire() { ++acquisitions; }
  void Send(const ExpressionSample&) { ++commands; }
  void Release() { ++releases; }
};

int main() {
  SafetyStateWire safety {};
  safety.record_version = 0x00010000u;
  safety.observed_fresh = 1;
  safety.axes_zero = 1;
  assert(SafetyRecordAllowsOwnership(safety));
  safety.stop = 1;
  assert(!SafetyRecordAllowsOwnership(safety));
  safety.stop = 0;
  safety.observed_fresh = 0;
  assert(!SafetyRecordAllowsOwnership(safety));
  safety.observed_fresh = 1;
  safety.axes_zero = 0;
  assert(!SafetyRecordAllowsOwnership(safety));

  FakeSender sender;
  ExpressionEngine engine({"neutral", "joy", "anger"});
  sender.Acquire();
  std::string status;
  for (uint64_t tick = 0; tick < 7000; ++tick) {
    const double now = tick / 1000.0;
    if (tick == 500) engine.Request("joy", 0.8, 0.9, now);
    if (tick == 800) engine.Request("anger", -0.8, 0.9, now);
    if (tick == 4000) engine.LinkStale(now);
    sender.Send(engine.Sample(now));
    status = std::string("{\"schema_version\":\"1.0\",\"phase\":\"") +
        engine.phase() + "\",\"active_emotion\":\"" + engine.active() + "\"}";
    if (engine.stale_reset_complete()) break;
  }
  sender.Release();
  assert(sender.acquisitions == 1);
  assert(sender.releases == 1);
  assert(sender.commands >= 5851 && sender.commands <= 5852);
  // 4.000 + 1.500 + 0.350 seconds, allowing one binary-float boundary tick.
  assert(status.find("\"schema_version\":\"1.0\"") != std::string::npos);
  assert(status.find("\"phase\":\"profile\"") != std::string::npos);

  // Raised-paw chat retarget: a newer request is held pending, prevents a
  // second stomp, and becomes active only after the external controller has
  // completed lowering, 1.5 s neutral return, and 0.35 s exact hold.
  ExpressionEngine retarget_engine({"neutral", "anger", "joy"});
  retarget_engine.Request("anger", -0.8, 0.9, 0.0);
  retarget_engine.Sample(
      ExpressionEngine::kNeutralReturnSeconds +
      ExpressionEngine::kNeutralHoldSeconds + 0.001);
  assert(retarget_engine.active() == "anger");
  AngerRetargetTracker retarget(100);
  AngerStompGate landing_gate;
  assert(landing_gate.BeginStomp());
  retarget.Observe(true, true, 101, "joy", 0.8, 0.9);
  assert(!retarget.may_start_next_stomp());
  landing_gate.CompleteLanding(true, kAngerLandingDwellSeconds);
  assert(landing_gate.next_stomp_allowed());
  assert(!retarget.may_start_next_stomp());
  retarget_engine.CompleteExternalNeutralTransition(
      retarget.emotion(), retarget.valence(), retarget.arousal(), 10.0);
  assert(retarget_engine.active() == "joy");
  assert(retarget_engine.pending().empty());

  // Fear uses the same external-neutral contract, but its own landing gate
  // prevents the mirrored hover after a request received while the first paw
  // is raised or lowering.
  ExpressionEngine fear_engine({"neutral", "fear", "surprise"});
  fear_engine.Request("fear", -0.8, 0.9, 0.0);
  fear_engine.Sample(
      ExpressionEngine::kNeutralReturnSeconds +
      ExpressionEngine::kNeutralHoldSeconds + 0.001);
  assert(fear_engine.active() == "fear");
  FearRetargetTracker fear_retarget(200);
  FearGuardGate fear_landing_gate;
  assert(fear_landing_gate.BeginHover());
  fear_retarget.Observe(true, true, 201, "surprise", 0.5, 1.0);
  assert(!fear_retarget.may_start_next_hover());
  fear_landing_gate.CompleteLanding(true, kFearLandingDwellSeconds);
  assert(fear_landing_gate.next_hover_allowed());
  assert(!fear_retarget.may_start_next_hover());
  fear_engine.CompleteExternalNeutralTransition(
      fear_retarget.emotion(), fear_retarget.valence(),
      fear_retarget.arousal(), 20.0);
  assert(fear_engine.requested() == "surprise");
  assert(fear_engine.active() == "neutral");
  assert(fear_engine.requested_resolution().fallback);
  assert(fear_engine.pending().empty());

  // Sadness also owns a lifted paw. Same-category updates wait for the next
  // loop, while a different category is handed back only after the runner's
  // placement, 1.5 s exact-neutral return, and 0.35 s hold contract.
  ExpressionEngine sadness_engine({"neutral", "sadness", "curiosity"});
  sadness_engine.Request("sadness", -0.8, 0.3, 0.0);
  sadness_engine.Sample(
      ExpressionEngine::kNeutralReturnSeconds +
      ExpressionEngine::kNeutralHoldSeconds + 0.001);
  assert(sadness_engine.active() == "sadness");
  SadnessRetargetTracker sadness_retarget(300);
  SadnessHoverGate sadness_landing_gate;
  assert(sadness_landing_gate.BeginHover());
  sadness_retarget.Observe(true, true, 301, "sadness", -0.9, 0.2);
  assert(sadness_retarget.may_start_next_hover());
  sadness_retarget.Observe(true, true, 302, "curiosity", 0.2, 0.5);
  assert(!sadness_retarget.may_start_next_hover());
  sadness_landing_gate.CompleteLanding(
      true, true, kSadnessLandingDwellSeconds);
  assert(sadness_landing_gate.next_hover_allowed());
  sadness_engine.CompleteExternalNeutralTransition(
      sadness_retarget.emotion(), sadness_retarget.valence(),
      sadness_retarget.arousal(), 30.0);
  assert(sadness_engine.requested() == "curiosity");
  assert(sadness_engine.active() == "neutral");
  assert(sadness_engine.requested_resolution().fallback);
  assert(sadness_engine.pending().empty());

  // Joy's accepted alternating-paw runtime uses the generic lower-first latch.
  // A mid-left or mid-right request suppresses all later phases and keeps only
  // the newest request for handoff after canonical return and hold.
  for (int raised_paw = 0; raised_paw < 2; ++raised_paw) {
    (void)raised_paw;
    EmotionRetargetTracker joy_retarget("joy", 400);
    joy_retarget.Observe(true, true, 401, "fear", -0.8, 0.9);
    assert(joy_retarget.cancellation_requested());
    assert(!joy_retarget.may_start_next_phase());
    joy_retarget.Observe(true, true, 402, "affection", 0.8, 0.4);
    assert(joy_retarget.emotion() == "affection");
    ExpressionEngine joy_engine({"neutral", "joy", "fear"});
    joy_engine.CompleteExternalNeutralTransition(
        joy_retarget.emotion(), joy_retarget.valence(),
        joy_retarget.arousal(), 40.0);
    assert(joy_engine.requested() == "affection");
    assert(joy_engine.active() == "neutral");
  }

  // The official owner is the only fake sender instantiated in this harness;
  // Retroid and legacy action senders have no execution path here.
  return 0;
}
