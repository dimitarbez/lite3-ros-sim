#pragma once

#include <cstdint>
#include <string>

#include "sdk_types.hpp"

class Sender {
 public:
  Sender(const std::string&, uint16_t) {}
  void RobotStateInit() {}
  void SendCmd(const RobotCmd&) {}
  void ControlGet(int) {}
};
