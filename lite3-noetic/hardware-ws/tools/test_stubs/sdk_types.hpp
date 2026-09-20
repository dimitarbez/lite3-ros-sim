#pragma once

#include <array>
#include <cstdint>

struct StubJoint {
  float position{0.0f};
  float velocity{0.0f};
  float torque{0.0f};
  float kp{0.0f};
  float kd{0.0f};
};

struct StubJointGroup {
  StubJoint joint_data[12]{};
};

struct StubImu {
  std::array<float, 16> buffer_float{};
};

struct RobotData {
  uint32_t tick{0};
  StubJointGroup joint_data{};
  StubImu imu{};
};

struct RobotCmd {
  StubJoint joint_cmd[12]{};
};

constexpr int ROBOT = 0;
