#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// ROS- and SDK-independent Lite3 foot-load estimator.  Deeprcs 2.0.153 leaves
// RobotData::contact_force at zero while the official SDK owns control, but it
// does populate joint position and torque.  The maintained Lite3 controller
// uses the same relationship: force = inverse(transpose(J)) * joint_torque.

struct EstimatedFootForce {
  std::array<double, 3> force{{0.0, 0.0, 0.0}};
  double vertical_load{0.0};
  bool valid{false};
};

struct EstimatedFootForces {
  std::array<EstimatedFootForce, 4> feet{};
  double total_vertical_load{0.0};
  bool valid{false};
};

using Matrix3 = std::array<std::array<double, 3>, 3>;

inline bool SolveContactMatrix(Matrix3 matrix, std::array<double, 3> value,
                               std::array<double, 3>* result) {
  for (int column = 0; column < 3; ++column) {
    int pivot = column;
    for (int row = column + 1; row < 3; ++row) {
      if (std::abs(matrix[row][column]) >
          std::abs(matrix[pivot][column])) {
        pivot = row;
      }
    }
    if (!std::isfinite(matrix[pivot][column]) ||
        std::abs(matrix[pivot][column]) < 1e-7) {
      return false;
    }
    if (pivot != column) {
      std::swap(matrix[pivot], matrix[column]);
      std::swap(value[pivot], value[column]);
    }
    const double divisor = matrix[column][column];
    for (int item = column; item < 3; ++item) {
      matrix[column][item] /= divisor;
    }
    value[column] /= divisor;
    for (int row = 0; row < 3; ++row) {
      if (row == column) continue;
      const double factor = matrix[row][column];
      for (int item = column; item < 3; ++item) {
        matrix[row][item] -= factor * matrix[column][item];
      }
      value[row] -= factor * value[column];
    }
  }
  for (double item : value) {
    if (!std::isfinite(item) || std::abs(item) > 1000.0) return false;
  }
  *result = value;
  return true;
}

inline Matrix3 Lite3LegJacobian(const std::array<double, 3>& angle,
                                int leg) {
  constexpr double hip_length = 0.0985;
  constexpr double upper_length = 0.20;
  constexpr double lower_length = 0.21;
  const double signed_hip = (leg == 0 || leg == 2)
      ? -hip_length : hip_length;
  const double effective_length = std::sqrt(
      upper_length * upper_length + lower_length * lower_length +
      2.0 * upper_length * lower_length * std::cos(angle[2]));
  const double effective_angle = angle[1] + angle[2] / 2.0;
  Matrix3 jacobian{};
  jacobian[0][1] = -effective_length * std::cos(effective_angle);
  jacobian[0][2] =
      lower_length * upper_length * std::sin(angle[2]) *
          std::sin(effective_angle) / effective_length -
      effective_length * std::cos(effective_angle) / 2.0;
  jacobian[1][0] =
      -signed_hip * std::sin(angle[0]) + effective_length *
          std::cos(angle[0]) * std::cos(effective_angle);
  jacobian[1][1] =
      -effective_length * std::sin(angle[0]) * std::sin(effective_angle);
  jacobian[1][2] =
      -lower_length * upper_length * std::sin(angle[0]) *
          std::sin(angle[2]) * std::cos(effective_angle) / effective_length -
      effective_length * std::sin(angle[0]) *
          std::sin(effective_angle) / 2.0;
  jacobian[2][0] =
      signed_hip * std::cos(angle[0]) + effective_length *
          std::sin(angle[0]) * std::cos(effective_angle);
  jacobian[2][1] =
      effective_length * std::sin(effective_angle) * std::cos(angle[0]);
  jacobian[2][2] =
      lower_length * upper_length * std::sin(angle[2]) *
          std::cos(angle[0]) * std::cos(effective_angle) / effective_length +
      effective_length * std::sin(effective_angle) *
          std::cos(angle[0]) / 2.0;
  return jacobian;
}

inline EstimatedFootForces EstimateLite3FootForces(
    const std::array<double, 12>& sdk_position,
    const std::array<double, 12>& sdk_torque) {
  EstimatedFootForces estimate;
  estimate.valid = true;
  for (int leg = 0; leg < 4; ++leg) {
    std::array<double, 3> angle{};
    std::array<double, 3> torque{};
    for (int joint = 0; joint < 3; ++joint) {
      const int index = 3 * leg + joint;
      if (!std::isfinite(sdk_position[index]) ||
          !std::isfinite(sdk_torque[index])) {
        estimate.valid = false;
        return estimate;
      }
      // The physical Lite3 config applies joint_direction=-1 to every raw
      // MotionSDK position and torque before using the model Jacobian.
      angle[joint] = -sdk_position[index];
      torque[joint] = -sdk_torque[index];
    }
    const Matrix3 jacobian = Lite3LegJacobian(angle, leg);
    Matrix3 transpose{};
    for (int row = 0; row < 3; ++row) {
      for (int column = 0; column < 3; ++column) {
        transpose[row][column] = jacobian[column][row];
      }
    }
    std::array<double, 3> force{};
    if (!SolveContactMatrix(transpose, torque, &force)) {
      estimate.valid = false;
      return estimate;
    }
    estimate.feet[leg].force = force;
    // Ground reaction is negative Z in the maintained Lite3 model convention.
    estimate.feet[leg].vertical_load = std::max(0.0, -force[2]);
    estimate.feet[leg].valid = true;
    estimate.total_vertical_load += estimate.feet[leg].vertical_load;
  }
  if (!std::isfinite(estimate.total_vertical_load) ||
      estimate.total_vertical_load > 1000.0) {
    estimate.valid = false;
  }
  return estimate;
}

class FootLoadMonitor {
 public:
  bool ObserveStanding(uint32_t tick, const EstimatedFootForces& estimate) {
    if (!AcceptDistinct(tick, estimate)) return false;
    for (int leg = 0; leg < 4; ++leg) {
      const double load = estimate.feet[leg].vertical_load;
      baseline_sum_[leg] += load;
      baseline_square_sum_[leg] += load * load;
    }
    ++baseline_samples_;
    return true;
  }

  bool FinalizeBaseline() {
    baseline_valid_ = baseline_samples_ >= 50;
    double total = 0.0;
    for (int leg = 0; leg < 4; ++leg) {
      const double mean = baseline_samples_ == 0 ? 0.0
          : baseline_sum_[leg] / baseline_samples_;
      const double variance = baseline_samples_ == 0 ? 0.0
          : std::max(0.0, baseline_square_sum_[leg] / baseline_samples_ -
                              mean * mean);
      baseline_[leg] = mean;
      baseline_valid_ = baseline_valid_ && mean >= 5.0 && mean <= 100.0 &&
          std::sqrt(variance) <= 0.35 * mean;
      total += mean;
    }
    baseline_valid_ = baseline_valid_ && total >= 70.0 && total <= 180.0;
    loaded_.fill(baseline_valid_);
    return baseline_valid_;
  }

  bool Update(uint32_t tick, const EstimatedFootForces& estimate) {
    if (!AcceptDistinct(tick, estimate)) return false;
    if (!baseline_valid_) return true;
    for (int leg = 0; leg < 4; ++leg) {
      const double unload_threshold = std::max(2.0, 0.20 * baseline_[leg]);
      const double load_threshold = std::max(5.0, 0.45 * baseline_[leg]);
      if (filtered_[leg] <= unload_threshold) {
        ++unload_count_[leg];
        load_count_[leg] = 0;
        if (unload_count_[leg] >= 3) loaded_[leg] = false;
      } else if (filtered_[leg] >= load_threshold) {
        ++load_count_[leg];
        unload_count_[leg] = 0;
        if (load_count_[leg] >= 5) loaded_[leg] = true;
      } else {
        unload_count_[leg] = 0;
        load_count_[leg] = 0;
      }
    }
    return true;
  }

  bool baseline_valid() const { return baseline_valid_; }
  bool estimate_valid() const { return estimate_valid_; }
  uint64_t baseline_samples() const { return baseline_samples_; }
  const std::array<double, 4>& baseline() const { return baseline_; }
  const std::array<double, 4>& filtered() const { return filtered_; }
  int support_count() const {
    return static_cast<int>(std::count(loaded_.begin(), loaded_.end(), true));
  }
  bool loaded(int leg) const { return loaded_.at(leg); }
  bool stable_support_excluding(int target_leg,
                                double strong_support_floor = 5.0,
                                double total_support_floor = 70.0) const {
    int strong_supports = 0;
    double supported_load = 0.0;
    for (int leg = 0; leg < 4; ++leg) {
      if (leg == target_leg) continue;
      supported_load += filtered_.at(leg);
      if (filtered_.at(leg) >= strong_support_floor) ++strong_supports;
    }
    // During a deliberate front-paw lift the body may settle onto a diagonal
    // pair while the third non-target paw becomes nearly unloaded. Requiring
    // all three paws to exceed a tiny per-foot floor rejects that stable case.
    return strong_supports >= 2 && supported_load >= total_support_floor;
  }
  double total_load() const {
    double result = 0.0;
    for (double value : filtered_) result += value;
    return result;
  }

 private:
  bool AcceptDistinct(uint32_t tick, const EstimatedFootForces& estimate) {
    estimate_valid_ = estimate.valid;
    if (!estimate.valid || (have_tick_ && tick == last_tick_)) return false;
    have_tick_ = true;
    last_tick_ = tick;
    for (int leg = 0; leg < 4; ++leg) {
      const double raw = estimate.feet[leg].vertical_load;
      if (!filter_initialized_) filtered_[leg] = raw;
      else filtered_[leg] += 0.10 * (raw - filtered_[leg]);
    }
    filter_initialized_ = true;
    return true;
  }

  bool have_tick_{false};
  bool filter_initialized_{false};
  bool estimate_valid_{false};
  bool baseline_valid_{false};
  uint32_t last_tick_{0};
  uint64_t baseline_samples_{0};
  std::array<double, 4> filtered_{{0.0, 0.0, 0.0, 0.0}};
  std::array<double, 4> baseline_{{0.0, 0.0, 0.0, 0.0}};
  std::array<double, 4> baseline_sum_{{0.0, 0.0, 0.0, 0.0}};
  std::array<double, 4> baseline_square_sum_{{0.0, 0.0, 0.0, 0.0}};
  std::array<int, 4> unload_count_{{0, 0, 0, 0}};
  std::array<int, 4> load_count_{{0, 0, 0, 0}};
  std::array<bool, 4> loaded_{{false, false, false, false}};
};
