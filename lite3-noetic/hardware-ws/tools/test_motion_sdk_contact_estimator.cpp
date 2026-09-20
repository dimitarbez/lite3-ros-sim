#include <cassert>
#include <cmath>
#include <limits>

#include "motion_sdk_contact_estimator.hpp"

namespace {

bool Near(double left, double right, double tolerance) {
  return std::abs(left - right) <= tolerance;
}

}  // namespace

int main() {
  // Captured during the 2026-09-20 full-scale joy session while all four feet
  // were visibly planted.  The official contact array was all zero.
  const std::array<double, 12> position{{
      -0.0120860189, -0.7181799412, 1.3849692345,
       0.0087290853, -0.7240545750, 1.3985418081,
      -0.0113296509, -0.7141363621, 1.4120204449,
       0.0133067220, -0.7213079929, 1.4303563833}};
  const std::array<double, 12> torque{{
       0.9173736572, -0.5450515747, -3.3295173645,
      -0.7623405457, -0.4351882935, -3.4210739136,
       0.9698638916, -1.2408638000, -4.7565422058,
      -0.9979400635, -0.9698638916, -5.2607002258}};
  const EstimatedFootForces standing =
      EstimateLite3FootForces(position, torque);
  assert(standing.valid);
  const double expected[] = {23.44, 24.34, 31.23, 35.73};
  for (int leg = 0; leg < 4; ++leg) {
    assert(standing.feet[leg].valid);
    assert(Near(standing.feet[leg].vertical_load, expected[leg], 0.08));
  }
  assert(Near(standing.total_vertical_load, 114.74, 0.12));
  assert(Near(standing.total_vertical_load, 11.84 * 9.81, 1.8));

  // Fifty distinct stable frames establish a baseline; repeated ticks do not
  // inflate the sample count.
  FootLoadMonitor monitor;
  for (uint32_t tick = 1; tick <= 50; ++tick) {
    assert(monitor.ObserveStanding(tick, standing));
    assert(!monitor.ObserveStanding(tick, standing));
  }
  assert(monitor.baseline_samples() == 50);
  assert(monitor.FinalizeBaseline());
  assert(monitor.baseline_valid());
  assert(monitor.support_count() == 4);

  // Three distinct low-force frames latch unload. Five loaded frames are
  // required to re-latch contact, preventing single-frame landing spikes.
  EstimatedFootForces left_unloaded = standing;
  left_unloaded.feet[0].vertical_load = 0.5;
  for (uint32_t tick = 51; tick < 100 && monitor.loaded(0); ++tick) {
    monitor.Update(tick, left_unloaded);
  }
  assert(!monitor.loaded(0));
  assert(monitor.support_count() == 3);
  for (uint32_t tick = 100; tick < 160 && !monitor.loaded(0); ++tick) {
    monitor.Update(tick, standing);
  }
  assert(monitor.loaded(0));
  assert(monitor.support_count() == 4);

  // A paw gesture can safely redistribute the load onto two strong diagonal
  // supports. The third non-target paw need not carry measurable load when the
  // two supports still carry a plausible total robot weight.
  EstimatedFootForces diagonal_support = standing;
  diagonal_support.feet[0].vertical_load = 0.5;
  diagonal_support.feet[1].vertical_load = 55.0;
  diagonal_support.feet[2].vertical_load = 55.0;
  diagonal_support.feet[3].vertical_load = 0.2;
  for (uint32_t tick = 160; tick < 240; ++tick) {
    monitor.Update(tick, diagonal_support);
  }
  assert(!monitor.loaded(0));
  assert(monitor.support_count() == 2);
  assert(monitor.stable_support_excluding(0));

  EstimatedFootForces single_support = diagonal_support;
  single_support.feet[2].vertical_load = 2.0;
  for (uint32_t tick = 240; tick < 320; ++tick) {
    monitor.Update(tick, single_support);
  }
  assert(!monitor.stable_support_excluding(0));

  // Nonfinite inputs and a geometrically singular solve fail closed.
  std::array<double, 12> invalid_position = position;
  invalid_position[2] = std::numeric_limits<double>::quiet_NaN();
  assert(!EstimateLite3FootForces(invalid_position, torque).valid);
  std::array<double, 12> singular_position{};
  std::array<double, 12> zero_torque{};
  assert(!EstimateLite3FootForces(singular_position, zero_torque).valid);

  FootLoadMonitor weak_monitor;
  EstimatedFootForces weak = standing;
  for (auto& foot : weak.feet) foot.vertical_load = 1.0;
  weak.total_vertical_load = 4.0;
  for (uint32_t tick = 1; tick <= 80; ++tick) {
    weak_monitor.ObserveStanding(tick, weak);
  }
  assert(!weak_monitor.FinalizeBaseline());
  return 0;
}
