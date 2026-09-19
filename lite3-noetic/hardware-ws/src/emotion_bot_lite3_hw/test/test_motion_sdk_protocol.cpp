#include <emotion_bot_lite3_hw/motion_sdk_protocol.hpp>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

int main() {
  using namespace emotion_bot_lite3_hw;
  const auto acquire = SimplePacket(kControlGetSdkCode);
  const auto release = SimplePacket(kReleaseRobotCode);
  CommandHeader header{};
  std::memcpy(&header, acquire.data(), sizeof(header));
  assert(header.code == 0x0114 && header.value_or_size == 0 && header.type_and_count == 0);
  std::memcpy(&header, release.data(), sizeof(header));
  assert(header.code == 0x0113 && header.value_or_size == 0 && header.type_and_count == 0);

  RobotCommandWire command{};
  for (int index = 0; index < 12; ++index) {
    command.joints[index] = JointCommandWire{
        static_cast<float>(index), static_cast<float>(index) / 10.0f, 0.0f, 30.0f, 0.7f};
  }
  const auto packet = JointPacket(command);
  assert(packet.size() == 252);
  std::memcpy(&header, packet.data(), sizeof(header));
  assert(header.code == 0x0111 && header.value_or_size == 240 && header.type_and_count == 1);
  RobotCommandWire decoded{};
  std::memcpy(&decoded, packet.data() + sizeof(header), sizeof(decoded));
  assert(decoded.joints[11].position == 11.0f);
  assert(decoded.joints[11].torque == 0.0f);
  assert(std::abs(decoded.joints[11].kd - 0.7f) < 1e-6f);
  return 0;
}
