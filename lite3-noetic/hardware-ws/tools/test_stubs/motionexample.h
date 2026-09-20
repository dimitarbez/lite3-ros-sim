#pragma once

#include "sdk_types.hpp"

class MotionExample {
 public:
  void GetInitData(const StubJointGroup&, double) {}
  void PreStandUp(RobotCmd&, double, RobotData&) {}
  void StandUp(RobotCmd&, double, RobotData&) {}
};
