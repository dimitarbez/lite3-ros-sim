#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "motion_sdk_expression_profile.hpp"
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
  // The official owner is the only fake sender instantiated in this harness;
  // Retroid and legacy action senders have no execution path here.
  return 0;
}
