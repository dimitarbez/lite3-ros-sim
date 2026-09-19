#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace emotion_bot_lite3_hw {

constexpr std::uint32_t kJointCommandCode = 0x0111;
constexpr std::uint32_t kReleaseRobotCode = 0x0113;
// Official Sender::ControlGet(SDK). Live testing proved this is not a seamless
// handoff from the vendor standing controller: joints can move toward the SDK
// controller's initialization state before measured-position commands track.
constexpr std::uint32_t kControlGetSdkCode = 0x0114;

#pragma pack(push, 1)
struct CommandHeader {
  std::uint32_t code;
  std::uint32_t value_or_size;
  std::uint32_t type_and_count;
};

struct JointCommandWire {
  float position;
  float velocity;
  float torque;
  float kp;
  float kd;
};

struct RobotCommandWire {
  JointCommandWire joints[12];
};
#pragma pack(pop)

static_assert(sizeof(CommandHeader) == 12, "MotionSDK command header must be 12 bytes");
static_assert(sizeof(JointCommandWire) == 20, "MotionSDK joint command must be 20 bytes");
static_assert(sizeof(RobotCommandWire) == 240, "MotionSDK RobotCmd must be 240 bytes");

inline std::array<std::uint8_t, sizeof(CommandHeader)> SimplePacket(std::uint32_t code) {
  CommandHeader header{code, 0, 0};
  std::array<std::uint8_t, sizeof(CommandHeader)> packet{};
  std::memcpy(packet.data(), &header, sizeof(header));
  return packet;
}

inline std::vector<std::uint8_t> JointPacket(const RobotCommandWire& command) {
  CommandHeader header{kJointCommandCode, sizeof(RobotCommandWire), 1};
  std::vector<std::uint8_t> packet(sizeof(header) + sizeof(command));
  std::memcpy(packet.data(), &header, sizeof(header));
  std::memcpy(packet.data() + sizeof(header), &command, sizeof(command));
  return packet;
}

}  // namespace emotion_bot_lite3_hw
