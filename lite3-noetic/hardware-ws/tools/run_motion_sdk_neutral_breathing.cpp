#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "motionexample.h"
#include "motion_sdk_breath_profile.hpp"
#include "motion_sdk_contact_estimator.hpp"
#include "motion_sdk_expression_profile.hpp"
#include "motion_sdk_paw_lift.hpp"
#include "motion_sdk_safety.hpp"
#include "motion_sdk_shared_state.hpp"
#include "receiver.h"
#include "sender.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kStandHipY = -42.0 * kPi / 180.0;
constexpr double kStandKnee = 78.0 * kPi / 180.0;
constexpr double kFeedbackPauseSeconds = 0.10;
constexpr double kFeedbackReleaseSeconds = 0.25;
constexpr double kFeedbackRecoveryAgeSeconds = 0.02;
constexpr int kFeedbackRecoverySamples = 20;
constexpr double kMaxTrajectoryStepSeconds = 0.005;
constexpr double kTransitionErrorLimit = 0.45;
constexpr double kHoldErrorLimit = 0.10;
constexpr double kPawTrackingErrorLimit = 0.20;
// Shared with the legacy direct-joint and Retroid diagnostic launchers. There
// is exactly one physical command owner regardless of which path is selected.
constexpr char kLockPath[] = "/dev/shm/emotion_bot_lite3_direct_joint.lock";
constexpr char kMarkerPath[] = "/dev/shm/emotion_bot_lite3_official_sdk.owned";
constexpr char kFeedbackPath[] = "/dev/shm/emotion_bot_lite3_telemetry";
constexpr char kRobotStatePath[] = "/dev/shm/emotion_bot_lite3_robot_state";
constexpr char kEmotionStatePath[] = "/dev/shm/emotion_bot_lite3_emotion_state";
constexpr char kSafetyStatePath[] = "/dev/shm/emotion_bot_lite3_safety_state";
constexpr char kExpressionStatusPath[] = "/dev/shm/emotion_bot_lite3_expression_status";
constexpr double kEmotionTimeoutSeconds = 0.75;
constexpr size_t kSharedCapacity = 4096;
constexpr size_t kRobotStateBodySize = 200;
constexpr size_t kRobotStateErrorFlagsOffset = 160;
constexpr size_t kRobotStateBatteryOffset = 168;
constexpr size_t kRobotStateRollOffset = 8;
constexpr size_t kRobotStatePitchOffset = 16;

static_assert(kFeedbackRecoveryAgeSeconds < kFeedbackPauseSeconds,
              "recovery must be fresher than the pause threshold");
static_assert(kFeedbackPauseSeconds < kFeedbackReleaseSeconds,
              "dead-stream release must follow the bounded hold window");

std::atomic<bool> g_stop{false};
std::atomic<bool> g_safety_fault{false};
std::atomic<int64_t> g_feedback_ns{0};
double g_minimum_battery = 25.0;

int64_t MonotonicNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

void OnSignal(int) { g_stop.store(true); }

bool FeedbackFresh() {
  const int64_t received = g_feedback_ns.load();
  return received > 0 &&
      (MonotonicNs() - received) * 1e-9 <= kFeedbackPauseSeconds;
}

double FeedbackAgeSeconds() {
  const int64_t received = g_feedback_ns.load();
  if (received <= 0) return INFINITY;
  return std::max(0.0, (MonotonicNs() - received) * 1e-9);
}

#pragma pack(push, 1)
struct SharedHeader {
  char magic[4];
  uint32_t version;
  uint64_t sequence;
  uint64_t receive_ns;
  uint32_t length;
};
#pragma pack(pop)

static_assert(sizeof(SharedHeader) == 28, "shared telemetry header must be packed");

class FeedbackSource {
 public:
  ~FeedbackSource() {
    if (mapping_ != MAP_FAILED) munmap(mapping_, kSharedCapacity);
    if (fd_ >= 0) close(fd_);
  }

  bool Open() {
    fd_ = open(kFeedbackPath, O_RDONLY);
    if (fd_ < 0) return false;
    struct stat details {};
    if (fstat(fd_, &details) != 0 ||
        details.st_size != static_cast<off_t>(kSharedCapacity)) return false;
    mapping_ = mmap(nullptr, kSharedCapacity, PROT_READ, MAP_SHARED, fd_, 0);
    return mapping_ != MAP_FAILED;
  }

  RobotData GetState() {
    RobotData candidate {};
    for (int attempt = 0; attempt < 8; ++attempt) {
      SharedHeader first {};
      SharedHeader second {};
      std::memcpy(&first, mapping_, sizeof(first));
      std::atomic_thread_fence(std::memory_order_acquire);
      if (std::memcmp(first.magic, "EBL3", 4) != 0 || first.version != 1 ||
          first.sequence % 2 != 0 || first.length < sizeof(RobotData) ||
          first.length > kSharedCapacity - sizeof(SharedHeader)) continue;
      std::memcpy(&candidate,
                  static_cast<const char*>(mapping_) + sizeof(SharedHeader),
                  sizeof(candidate));
      std::atomic_thread_fence(std::memory_order_acquire);
      std::memcpy(&second, mapping_, sizeof(second));
      if (std::memcmp(&first, &second, sizeof(first)) == 0) {
        g_feedback_ns.store(static_cast<int64_t>(first.receive_ns));
        const int64_t now_ns = MonotonicNs();
        max_age_seconds_ = std::max(
            max_age_seconds_,
            std::max(0.0, (now_ns - static_cast<int64_t>(first.receive_ns)) * 1e-9));
        if (first.sequence != last_sequence_) {
          if (last_sequence_ != 0 && first.sequence == last_sequence_ + 2 &&
              first.receive_ns >= last_receive_ns_) {
            max_consecutive_gap_seconds_ = std::max(
                max_consecutive_gap_seconds_,
                (first.receive_ns - last_receive_ns_) * 1e-9);
          } else if (last_sequence_ != 0 && first.sequence > last_sequence_ + 2) {
            skipped_updates_ += (first.sequence - last_sequence_) / 2 - 1;
          }
          last_sequence_ = first.sequence;
          last_receive_ns_ = first.receive_ns;
        }
        state_ = candidate;
        return state_;
      }
    }
    return state_;
  }


  double max_age_seconds() const { return max_age_seconds_; }
  double max_consecutive_gap_seconds() const {
    return max_consecutive_gap_seconds_;
  }
  uint64_t skipped_updates() const { return skipped_updates_; }

 private:
  int fd_{-1};
  void* mapping_{MAP_FAILED};
  RobotData state_ {};
  uint64_t last_sequence_{0};
  uint64_t last_receive_ns_{0};
  uint64_t skipped_updates_{0};
  double max_age_seconds_{0.0};
  double max_consecutive_gap_seconds_{0.0};
};

struct RobotSafetyState {
  bool valid{false};
  uint32_t basic_state{0};
  uint32_t error_flags{0};
  double battery{0.0};
  double roll_deg{0.0};
  double pitch_deg{0.0};
  double age_seconds{INFINITY};
};

class RobotStateSource {
 public:
  ~RobotStateSource() {
    if (mapping_ != MAP_FAILED) munmap(mapping_, kSharedCapacity);
    if (fd_ >= 0) close(fd_);
  }

  bool Open() {
    fd_ = open(kRobotStatePath, O_RDONLY);
    if (fd_ < 0) return false;
    struct stat details {};
    if (fstat(fd_, &details) != 0 ||
        details.st_size != static_cast<off_t>(kSharedCapacity)) return false;
    mapping_ = mmap(nullptr, kSharedCapacity, PROT_READ, MAP_SHARED, fd_, 0);
    return mapping_ != MAP_FAILED;
  }

  RobotSafetyState GetState() {
    RobotSafetyState result;
    for (int attempt = 0; attempt < 8; ++attempt) {
      SharedHeader first {};
      SharedHeader second {};
      std::array<uint8_t, kRobotStateBodySize> body {};
      std::memcpy(&first, mapping_, sizeof(first));
      std::atomic_thread_fence(std::memory_order_acquire);
      if (std::memcmp(first.magic, "EBL3", 4) != 0 || first.version != 1 ||
          first.sequence % 2 != 0 || first.length != body.size()) continue;
      std::memcpy(body.data(),
                  static_cast<const char*>(mapping_) + sizeof(SharedHeader),
                  body.size());
      std::atomic_thread_fence(std::memory_order_acquire);
      std::memcpy(&second, mapping_, sizeof(second));
      if (std::memcmp(&first, &second, sizeof(first)) != 0) continue;
      std::memcpy(&result.basic_state, body.data(), sizeof(result.basic_state));
      std::memcpy(&result.error_flags,
                  body.data() + kRobotStateErrorFlagsOffset,
                  sizeof(result.error_flags));
      std::memcpy(&result.battery,
                  body.data() + kRobotStateBatteryOffset,
                  sizeof(result.battery));
      std::memcpy(&result.roll_deg,
                  body.data() + kRobotStateRollOffset,
                  sizeof(result.roll_deg));
      std::memcpy(&result.pitch_deg,
                  body.data() + kRobotStatePitchOffset,
                  sizeof(result.pitch_deg));
      result.age_seconds = std::max(
          0.0, (MonotonicNs() - static_cast<int64_t>(first.receive_ns)) * 1e-9);
      result.valid = true;
      receive_ns_ = static_cast<int64_t>(first.receive_ns);
      state_ = result;
      return state_;
    }
    if (state_.valid) {
      state_.age_seconds = std::max(
          0.0, (MonotonicNs() - receive_ns_) * 1e-9);
    }
    return state_;
  }

 private:
  int fd_{-1};
  void* mapping_{MAP_FAILED};
  int64_t receive_ns_{0};
  RobotSafetyState state_ {};
};

struct EmotionState {
  bool valid{false};
  uint64_t transport_sequence{0};
  uint64_t state_sequence{0};
  double valence{0.0};
  double arousal{0.2};
  double age_seconds{INFINITY};
  std::string emotion{"neutral"};
  std::string session_id;
  std::string turn_id;
};

class EmotionSource {
 public:
  bool Open() { return reader_.Open(kEmotionStatePath); }

  EmotionState GetState() {
    EmotionStateWire wire {};
    int64_t receive_ns = 0;
    if (reader_.Read(&wire, &receive_ns) &&
        wire.record_version == 0x00010001u &&
        std::isfinite(wire.valence) && std::isfinite(wire.arousal)) {
      const std::string emotion = FixedString(wire.emotion, sizeof(wire.emotion));
      if (PhysicalProfiles().count(emotion) && wire.valence >= -1.0 &&
          wire.valence <= 1.0 && wire.arousal >= 0.0 && wire.arousal <= 1.0) {
        state_.valid = true;
        state_.transport_sequence = wire.transport_sequence;
        state_.state_sequence = wire.state_sequence;
        state_.valence = wire.valence;
        state_.arousal = wire.arousal;
        state_.emotion = emotion;
        state_.session_id = FixedString(wire.session_id, sizeof(wire.session_id));
        state_.turn_id = FixedString(wire.turn_id, sizeof(wire.turn_id));
        receive_ns_ = receive_ns;
      }
    }
    if (state_.valid) {
      state_.age_seconds = std::max(0.0, (MonotonicNs() - receive_ns_) * 1e-9);
    }
    return state_;
  }

 private:
  SequenceRecordReader<EmotionStateWire> reader_;
  int64_t receive_ns_{0};
  EmotionState state_{};
};

class StopSource {
 public:
  bool Open() { return reader_.Open(kSafetyStatePath); }
  bool Read(bool* stop, double* age_seconds) {
    SafetyStateWire wire {};
    int64_t receive_ns = 0;
    if (!reader_.Read(&wire, &receive_ns) || wire.record_version != 0x00010000u) {
      return false;
    }
    *stop = wire.stop != 0;
    *age_seconds = std::max(0.0, (MonotonicNs() - receive_ns) * 1e-9);
    // The ROS receiver rewrites the record on its own timer, so record age
    // alone cannot prove that the motion-host observer is still alive. Keep
    // both authenticated relay freshness and centered manual axes as hard
    // ownership gates for the official runner.
    return SafetyRecordAllowsOwnership(wire) || wire.stop != 0;
  }

 private:
  SequenceRecordReader<SafetyStateWire> reader_;
};

struct ExpressionStatus {
  std::string requested{"neutral"};
  std::string active{"neutral"};
  std::string phase{"preflight"};
  uint64_t profile_cycle{0};
  std::string pending;
  double link_age{INFINITY};
  bool ownership{false};
  bool feedback_paused{false};
  bool estimated_contact_valid{false};
  bool estimated_contact_baseline_valid{false};
  bool estimated_contact_motion_gate_enabled{false};
  int estimated_support_count{0};
  double estimated_total_vertical_force_n{0.0};
  std::array<double, 4> estimated_vertical_force_n{{0.0, 0.0, 0.0, 0.0}};
  std::array<double, 4> estimated_baseline_force_n{{0.0, 0.0, 0.0, 0.0}};
  std::string last_fault;
  std::string release_state{"released"};
};

void PublishExpressionStatus(SequenceRecordWriter* writer,
                             const ExpressionStatus& status) {
  std::ostringstream value;
  value << "{\"schema_version\":\"1.0\""
        << ",\"requested_emotion\":\"" << JsonEscape(status.requested) << "\""
        << ",\"active_emotion\":\"" << JsonEscape(status.active) << "\""
        << ",\"phase\":\"" << JsonEscape(status.phase) << "\""
        << ",\"profile_cycle\":" << status.profile_cycle
        << ",\"pending_emotion\":";
  if (status.pending.empty()) value << "null";
  else value << "\"" << JsonEscape(status.pending) << "\"";
  value << ",\"link_age\":";
  if (std::isfinite(status.link_age)) value << status.link_age;
  else value << "null";
  value << ",\"sdk_ownership\":" << (status.ownership ? "true" : "false")
        << ",\"feedback_paused\":" << (status.feedback_paused ? "true" : "false")
        << ",\"last_fault\":";
  if (status.last_fault.empty()) value << "null";
  else value << "\"" << JsonEscape(status.last_fault) << "\"";
  value << ",\"estimated_contact\":{"
        << "\"valid\":"
        << (status.estimated_contact_valid ? "true" : "false")
        << ",\"baseline_valid\":"
        << (status.estimated_contact_baseline_valid ? "true" : "false")
        << ",\"motion_gate_enabled\":"
        << (status.estimated_contact_motion_gate_enabled ? "true" : "false")
        << ",\"support_count\":" << status.estimated_support_count
        << ",\"total_vertical_force_n\":"
        << status.estimated_total_vertical_force_n
        << ",\"vertical_force_n\":[";
  for (size_t index = 0; index < status.estimated_vertical_force_n.size(); ++index) {
    if (index) value << ',';
    value << status.estimated_vertical_force_n[index];
  }
  value << "],\"baseline_force_n\":[";
  for (size_t index = 0; index < status.estimated_baseline_force_n.size(); ++index) {
    if (index) value << ',';
    value << status.estimated_baseline_force_n[index];
  }
  value << "]}"
        << ",\"release_state\":\"" << JsonEscape(status.release_state) << "\"}";
  writer->Write(value.str());
}

bool RobotSafetyHealthy(RobotStateSource* source, const std::string& phase) {
  const RobotSafetyState state = source->GetState();
  if (!state.valid) {
    std::cerr << "ABORT unavailable 0x0901 safety state in " << phase
              << std::endl;
    g_safety_fault.store(true);
    return false;
  }
  if (RobotStateRequiresStop(state.basic_state, state.error_flags)) {
    std::cerr << "ABORT robot safety state basic_state=" << state.basic_state
              << " error_flags=" << state.error_flags
              << " age_ms=" << state.age_seconds * 1000.0
              << " in " << phase << std::endl;
    g_safety_fault.store(true);
    return false;
  }
  const double battery_percent = state.battery <= 1.0
      ? state.battery * 100.0 : state.battery;
  if (!std::isfinite(state.battery) || battery_percent < g_minimum_battery ||
      !std::isfinite(state.roll_deg) || !std::isfinite(state.pitch_deg) ||
      std::abs(state.roll_deg) > 10.0 || std::abs(state.pitch_deg) > 10.0) {
    std::cerr << "ABORT battery/attitude safety gate battery="
              << battery_percent << " roll=" << state.roll_deg
              << " pitch=" << state.pitch_deg << " in " << phase << std::endl;
    g_safety_fault.store(true);
    return false;
  }
  return true;
}

bool WaitForRobotSafetyState(RobotStateSource* source, double seconds) {
  const auto deadline = std::chrono::steady_clock::now() +
      std::chrono::duration<double>(seconds);
  while (std::chrono::steady_clock::now() < deadline && !g_stop.load()) {
    if (source->GetState().valid) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

bool FiniteFeedback(const RobotData& data) {
  for (const auto& joint : data.joint_data.joint_data) {
    if (!std::isfinite(joint.position) || !std::isfinite(joint.velocity) ||
        std::abs(joint.velocity) > 20.0 || joint.position < -3.60 ||
        joint.position > 3.60) {
      return false;
    }
  }
  for (float value : data.imu.buffer_float) {
    if (!std::isfinite(value)) return false;
  }
  return true;
}

EstimatedFootForces EstimateFootForces(const RobotData& data) {
  std::array<double, 12> position{};
  std::array<double, 12> torque{};
  for (size_t index = 0; index < position.size(); ++index) {
    position[index] = data.joint_data.joint_data[index].position;
    torque[index] = data.joint_data.joint_data[index].torque;
  }
  return EstimateLite3FootForces(position, torque);
}

void UpdateEstimatedContactStatus(const FootLoadMonitor& monitor,
                                  ExpressionStatus* status) {
  status->estimated_contact_valid = monitor.estimate_valid();
  status->estimated_contact_baseline_valid = monitor.baseline_valid();
  status->estimated_support_count = monitor.support_count();
  status->estimated_total_vertical_force_n = monitor.total_load();
  status->estimated_vertical_force_n = monitor.filtered();
  status->estimated_baseline_force_n = monitor.baseline();
}

double TrackingError(const RobotCmd& command, const RobotData& data) {
  double error = 0.0;
  const size_t joint_count =
      sizeof(command.joint_cmd) / sizeof(command.joint_cmd[0]);
  for (size_t i = 0; i < joint_count; ++i) {
    error = std::max(error, std::abs(
        static_cast<double>(command.joint_cmd[i].position) -
        static_cast<double>(data.joint_data.joint_data[i].position)));
  }
  return error;
}

void SetStandCommand(RobotCmd* command, const BreathPose& pose) {
  std::memset(command, 0, sizeof(*command));
  for (int leg = 0; leg < 4; ++leg) {
    const BreathScalar compression = LegCompression(pose, leg);
    auto& hip_x = command->joint_cmd[3 * leg];
    auto& hip_y = command->joint_cmd[3 * leg + 1];
    auto& knee = command->joint_cmd[3 * leg + 2];
    hip_x.position = 0.0f;
    hip_y.position = static_cast<float>(kStandHipY - compression.position);
    knee.position = static_cast<float>(
        kStandKnee + kKneeToHipCompression * compression.position);
    hip_x.velocity = 0.0f;
    hip_y.velocity = static_cast<float>(-compression.velocity);
    knee.velocity = static_cast<float>(
        kKneeToHipCompression * compression.velocity);
    hip_x.torque = hip_y.torque = knee.torque = 0.0f;
    hip_x.kp = hip_y.kp = knee.kp = 80.0f;
    hip_x.kd = hip_y.kd = knee.kd = 0.7f;
  }
}

void SetStandCommand(RobotCmd* command, const ExpressionSample& sample) {
  SetStandCommand(command, ToBreathPose(sample));
}

bool SetPawLiftCommand(RobotCmd* command, int leg, double lift_m,
                       double lift_velocity_mps, double shift_x_m,
                       double shift_y_m, double shift_x_velocity_mps,
                       double shift_y_velocity_mps,
                       double diagonal_support_z_m,
                       double diagonal_support_z_velocity_mps) {
  SetStandCommand(command, BreathPose{});
  if (leg < 0 || leg >= 2) return false;
  for (int current_leg = 0; current_leg < 4; ++current_leg) {
    const int diagonal_support_leg = 3 - leg;
    const double lift = current_leg == leg ? lift_m :
        (current_leg == diagonal_support_leg ? diagonal_support_z_m : 0.0);
    const double lift_velocity = current_leg == leg ? lift_velocity_mps :
        (current_leg == diagonal_support_leg ?
             diagonal_support_z_velocity_mps : 0.0);
    const CartesianLegJointTarget target = SampleLite3CartesianLeg(
        0.0, kStandHipY, kStandKnee, current_leg,
        shift_x_m, shift_y_m, lift,
        shift_x_velocity_mps, shift_y_velocity_mps, lift_velocity);
    if (!target.valid) return false;
    auto& hip_x = command->joint_cmd[3 * current_leg];
    auto& hip_y = command->joint_cmd[3 * current_leg + 1];
    auto& knee = command->joint_cmd[3 * current_leg + 2];
    hip_x.position = static_cast<float>(target.hip_x);
    hip_y.position = static_cast<float>(target.hip_y);
    knee.position = static_cast<float>(target.knee);
    hip_x.velocity = static_cast<float>(target.hip_x_velocity);
    hip_y.velocity = static_cast<float>(target.hip_y_velocity);
    knee.velocity = static_cast<float>(target.knee_velocity);
  }
  return true;
}

struct Watchdog {
  int notify_fd{-1};
  pid_t pid{-1};

  bool Start(const std::string& target_ip, uint16_t target_port) {
    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) return false;
    pid = fork();
    if (pid < 0) {
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return false;
    }
    if (pid == 0) {
      close(pipe_fd[1]);
      char value = 0;
      ssize_t result;
      do {
        result = read(pipe_fd[0], &value, 1);
      } while (result < 0 && errno == EINTR);
      close(pipe_fd[0]);
      if (result == 0) {
        Sender emergency_release(target_ip, target_port);
        emergency_release.ControlGet(ROBOT);
        unlink(kMarkerPath);
      }
      _exit(0);
    }
    close(pipe_fd[0]);
    notify_fd = pipe_fd[1];
    return true;
  }

  void CompleteNormally() {
    if (notify_fd >= 0) {
      const char done = 'D';
      (void)write(notify_fd, &done, 1);
      close(notify_fd);
      notify_fd = -1;
    }
    if (pid > 0) {
      (void)waitpid(pid, nullptr, 0);
      pid = -1;
    }
  }
};

struct FeedbackPauseStats {
  uint64_t pauses{0};
  uint64_t recoveries{0};
  double max_pause_seconds{0.0};
};

struct ScopedPidFile {
  std::string path;
  ~ScopedPidFile() {
    if (!path.empty()) unlink(path.c_str());
  }
};

bool WaitForFeedback(FeedbackSource* receiver, RobotStateSource* robot_state,
                     const std::string& phase, double seconds) {
  const auto deadline = std::chrono::steady_clock::now() +
      std::chrono::duration<double>(seconds);
  uint32_t last_tick = 0;
  bool have_tick = false;
  int samples = 0;
  while (std::chrono::steady_clock::now() < deadline && !g_stop.load()) {
    if (!RobotSafetyHealthy(robot_state, phase)) return false;
    const RobotData snapshot = receiver->GetState();
    if (FeedbackFresh() && FiniteFeedback(snapshot)) {
      if (!have_tick || snapshot.tick != last_tick) {
        last_tick = snapshot.tick;
        have_tick = true;
        ++samples;
      }
      if (samples >= 20) return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

bool BootstrapSdkFeedback(Sender* sender, FeedbackSource* receiver,
                          RobotStateSource* robot_state, RobotCmd* command) {
  std::cout << "PHASE sdk_feedback_bootstrap" << std::endl;
  std::memset(command, 0, sizeof(*command));
  for (auto& joint : command->joint_cmd) {
    joint.kp = 60.0f;
    joint.kd = 0.7f;
  }
  const auto deadline = std::chrono::steady_clock::now() +
      std::chrono::milliseconds(250);
  auto next = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() < deadline && !g_stop.load()) {
    if (!RobotSafetyHealthy(robot_state, "sdk_feedback_bootstrap")) return false;
    sender->SendCmd(*command);
    const RobotData snapshot = receiver->GetState();
    if (FeedbackFresh() && FiniteFeedback(snapshot)) {
      std::cout << "PHASE_COMPLETE sdk_feedback_bootstrap feedback_age_ms="
                << FeedbackAgeSeconds() * 1000.0 << std::endl;
      return true;
    }
    next += std::chrono::milliseconds(1);
    std::this_thread::sleep_until(next);
  }
  return false;
}

template <typename Update>
bool RunPhase(const std::string& name, double duration, double error_limit,
              Sender* sender, FeedbackSource* receiver,
              RobotStateSource* robot_state, RobotCmd* command,
              FeedbackPauseStats* pause_stats, Update update,
              std::array<double, 12>* minimum = nullptr,
              std::array<double, 12>* maximum = nullptr,
              const bool* update_fault = nullptr) {
  std::cout << "PHASE " << name << std::endl;
  auto next = std::chrono::steady_clock::now();
  auto last_trajectory_tick = next;
  auto pause_started = next;
  double elapsed = 0.0;
  bool paused = false;
  uint32_t recovery_last_tick = 0;
  int recovery_samples = 0;
  RobotCmd hold_command = *command;
  int tick = 0;
  while (!g_stop.load()) {
    const auto now = std::chrono::steady_clock::now();
    if (!RobotSafetyHealthy(robot_state, name)) return false;
    // Refresh the shared-memory snapshot before evaluating its age.  Checking
    // the timestamp from the previous poll first falsely classifies a delayed
    // runner thread as a dead telemetry producer even when a current frame is
    // already available in the sequence-locked record.
    const RobotData snapshot = receiver->GetState();
    const double feedback_age = FeedbackAgeSeconds();
    if (feedback_age > kFeedbackReleaseSeconds) {
      if (paused) {
        pause_stats->max_pause_seconds = std::max(
            pause_stats->max_pause_seconds,
            std::chrono::duration<double>(now - pause_started).count());
      }
      std::cerr << "ABORT dead 0x0906 feedback age_ms="
                << feedback_age * 1000.0 << " in " << name << std::endl;
      return false;
    }
    if (feedback_age > kFeedbackPauseSeconds) {
      if (!paused) {
        paused = true;
        pause_started = now;
        recovery_samples = 0;
        recovery_last_tick = snapshot.tick;
        hold_command = *command;
        for (auto& joint : hold_command.joint_cmd) {
          joint.velocity = 0.0f;
          joint.torque = 0.0f;
        }
        ++pause_stats->pauses;
        std::cerr << "PAUSE stale 0x0906 feedback age_ms="
                  << feedback_age * 1000.0 << " in " << name
                  << "; holding last validated command" << std::endl;
      }
      sender->SendCmd(hold_command);
      last_trajectory_tick = now;
      next += std::chrono::milliseconds(1);
      if (next < now) next = now + std::chrono::milliseconds(1);
      std::this_thread::sleep_until(next);
      continue;
    }
    if (!FiniteFeedback(snapshot)) {
      std::cerr << "ABORT invalid joint/IMU feedback in " << name << std::endl;
      return false;
    }
    if (paused) {
      if (feedback_age <= kFeedbackRecoveryAgeSeconds &&
          snapshot.tick != recovery_last_tick) {
        recovery_last_tick = snapshot.tick;
        ++recovery_samples;
      } else if (feedback_age > kFeedbackRecoveryAgeSeconds) {
        recovery_samples = 0;
        recovery_last_tick = snapshot.tick;
      }
      sender->SendCmd(hold_command);
      last_trajectory_tick = now;
      if (recovery_samples < kFeedbackRecoverySamples) {
        next += std::chrono::milliseconds(1);
        if (next < now) next = now + std::chrono::milliseconds(1);
        std::this_thread::sleep_until(next);
        continue;
      }
      const double pause_seconds =
          std::chrono::duration<double>(now - pause_started).count();
      pause_stats->max_pause_seconds = std::max(
          pause_stats->max_pause_seconds, pause_seconds);
      ++pause_stats->recoveries;
      paused = false;
      std::cout << "RESUME fresh 0x0906 samples=" << recovery_samples
                << " pause_ms=" << pause_seconds * 1000.0
                << " in " << name << std::endl;
    } else {
      elapsed += std::min(
          std::chrono::duration<double>(now - last_trajectory_tick).count(),
          kMaxTrajectoryStepSeconds);
    }
    last_trajectory_tick = now;
    update(std::min(elapsed, duration), snapshot, command);
    if (update_fault && *update_fault) {
      std::cerr << "ABORT update safety gate in " << name << std::endl;
      return false;
    }
    if (tick > 50 && TrackingError(*command, snapshot) > error_limit) {
      std::cerr << "ABORT tracking error " << TrackingError(*command, snapshot)
                << " rad in " << name << std::endl;
      return false;
    }
    if (minimum && maximum) {
      for (size_t i = 0; i < 12; ++i) {
        const double value = snapshot.joint_data.joint_data[i].position;
        (*minimum)[i] = std::min((*minimum)[i], value);
        (*maximum)[i] = std::max((*maximum)[i], value);
      }
    }
    sender->SendCmd(*command);
    if (elapsed >= duration) {
      std::cout << "PHASE_COMPLETE " << name << " elapsed=" << elapsed
                << " feedback_age_ms=" << FeedbackAgeSeconds() * 1000.0
                << std::endl;
      return true;
    }
    ++tick;
    next += std::chrono::milliseconds(1);
    if (next < now) next = now + std::chrono::milliseconds(1);
    std::this_thread::sleep_until(next);
  }
  if (g_stop.load()) {
    std::cerr << "ABORT signal in " << name << std::endl;
  }
  return false;
}

bool RunPawLiftSegment(const std::string& name, int leg, double start_lift,
                       double end_lift, double start_shift_x,
                       double end_shift_x, double start_shift_y,
                       double end_shift_y, double start_support_z,
                       double end_support_z, double duration,
                       Sender* sender,
                       FeedbackSource* receiver, RobotStateSource* robot_state,
                       RobotCmd* command, FeedbackPauseStats* pause_stats,
                       FootLoadMonitor* monitor, StopSource* stop_source,
                       ExpressionStatus* status,
                       SequenceRecordWriter* status_writer) {
  bool estimator_fault = false;
  status->phase = name;
  PublishExpressionStatus(status_writer, *status);
  std::array<double, 12> minimum;
  std::array<double, 12> maximum;
  minimum.fill(INFINITY);
  maximum.fill(-INFINITY);
  const bool completed = RunPhase(
      name, duration, kPawTrackingErrorLimit, sender, receiver, robot_state, command,
      pause_stats,
      [=, &estimator_fault](double elapsed, const RobotData& data,
                            RobotCmd* output) {
        const BreathScalar lift = QuinticBreathSegment(
            elapsed, duration, start_lift, end_lift);
        const BreathScalar shift_x = QuinticBreathSegment(
            elapsed, duration, start_shift_x, end_shift_x);
        const BreathScalar shift_y = QuinticBreathSegment(
            elapsed, duration, start_shift_y, end_shift_y);
        const BreathScalar support_z = QuinticBreathSegment(
            elapsed, duration, start_support_z, end_support_z);
        if (!SetPawLiftCommand(output, leg, lift.position, lift.velocity,
                               shift_x.position, shift_y.position,
                               shift_x.velocity, shift_y.velocity,
                               support_z.position, support_z.velocity)) {
          estimator_fault = true;
          return;
        }
        monitor->Update(data.tick, EstimateFootForces(data));
        UpdateEstimatedContactStatus(*monitor, status);
        bool stop = false;
        double stop_age = INFINITY;
        if (!stop_source->Read(&stop, &stop_age) || stop_age > 0.25 || stop) {
          status->last_fault = stop ? "Retroid STOP observed" :
              "STOP safety record unavailable";
          estimator_fault = true;
          return;
        }
        if (!monitor->estimate_valid() || !monitor->baseline_valid()) {
          estimator_fault = true;
          return;
        }
        // Dynamic load transfer can briefly cross a support threshold while
        // every paw remains physically planted. Evaluate the three support
        // paws after the raised hold has settled, not on an intermediate frame.
      }, &minimum, &maximum, &estimator_fault);
  std::cout << "PAW_JOINT_SPAN phase=" << name
            << " hip_x_rad=" << maximum[3 * leg] - minimum[3 * leg]
            << " hip_y_rad=" << maximum[3 * leg + 1] - minimum[3 * leg + 1]
            << " knee_rad=" << maximum[3 * leg + 2] - minimum[3 * leg + 2]
            << std::endl;
  PublishExpressionStatus(status_writer, *status);
  return completed && !estimator_fault;
}

bool OtherPawSupportsValid(const FootLoadMonitor& monitor, int target_leg) {
  return monitor.stable_support_excluding(target_leg);
}

bool RunNeutralTestWindow(Sender* sender, FeedbackSource* receiver,
                          RobotStateSource* robot_state, RobotCmd* command,
                          FeedbackPauseStats* pause_stats,
                          FootLoadMonitor* monitor, StopSource* stop_source,
                          SequenceRecordWriter* status_writer,
                          ExpressionStatus* status) {
  bool safety_fault = false;
  status->requested = "neutral";
  status->active = "neutral";
  status->phase = "neutral_test_5s";
  PublishExpressionStatus(status_writer, *status);
  const bool completed = RunPhase(
      "neutral_test_5s", 5.0, kHoldErrorLimit, sender, receiver, robot_state,
      command, pause_stats,
      [&](double elapsed, const RobotData& data, RobotCmd* output) {
        SetStandCommand(output, SampleAnimalBreath(
            std::fmod(elapsed, kBreathDurationSeconds)));
        monitor->Update(data.tick, EstimateFootForces(data));
        UpdateEstimatedContactStatus(*monitor, status);
        bool stop = false;
        double stop_age = INFINITY;
        if (!stop_source->Read(&stop, &stop_age) || stop_age > 0.25 || stop ||
            !monitor->estimate_valid() || !monitor->baseline_valid()) {
          status->last_fault = stop ? "Retroid STOP observed" :
              "neutral-window safety gate failed";
          safety_fault = true;
        }
      }, nullptr, nullptr, &safety_fault);
  PublishExpressionStatus(status_writer, *status);
  return completed && !safety_fault;
}

bool RunJoyPawTest(Sender* sender, FeedbackSource* receiver,
                   RobotStateSource* robot_state, RobotCmd* command,
                   FeedbackPauseStats* pause_stats, FootLoadMonitor* monitor,
                   StopSource* stop_source, SequenceRecordWriter* status_writer,
                   ExpressionStatus* status, double test_lift,
                   bool five_second_suite = false) {
  constexpr double kDiagonalSupportZ = 0.0;
  const double transfer_seconds = five_second_suite ? 0.50 : 1.00;
  const double lift_seconds = five_second_suite ? 0.60 : 1.00;
  const double hold_seconds = five_second_suite ? 0.30 : 0.75;
  const double lower_seconds = five_second_suite ? 0.60 : 1.00;
  const double settle_seconds = five_second_suite ? 0.50 : 1.00;
  if (!monitor->baseline_valid() || monitor->support_count() != 4) {
    std::cerr << "ABORT paw test requires a valid four-foot baseline" << std::endl;
    return false;
  }
  status->requested = "joy";
  status->active = "joy";
  status->estimated_contact_motion_gate_enabled = true;
  PublishExpressionStatus(status_writer, *status);

  for (int leg = 0; leg < 2; ++leg) {
    const std::string side = leg == 0 ? "front_left" : "front_right";
    // The commissioned robot carries more residual load on the front-right
    // paw for a symmetric 20 mm rearward transfer. The 2026-09-20 suite left
    // 7.3 N on that paw, while the left unloaded to 0.72 N. A larger lateral
    // shift did not improve it, so preserve lateral symmetry and move only the
    // right-side body transfer farther rearward.
    const double support_shift_x = leg == 0 ? 0.020 : 0.035;
    const double support_shift_y = leg == 0 ? -0.020 : 0.020;
    bool okay = RunPawLiftSegment(
        "paw_load_transfer_" + side, leg, 0.0, 0.0,
        0.0, support_shift_x, 0.0, support_shift_y,
        0.0, kDiagonalSupportZ, transfer_seconds,
        sender, receiver, robot_state, command, pause_stats, monitor,
        stop_source, status, status_writer);
    if (okay) {
      okay = RunPawLiftSegment(
          "paw_lift_" + side, leg, 0.0, test_lift,
          support_shift_x, support_shift_x,
          support_shift_y, support_shift_y,
          kDiagonalSupportZ, kDiagonalSupportZ, lift_seconds,
          sender, receiver, robot_state, command, pause_stats, monitor,
          stop_source, status, status_writer);
    }
    if (okay) {
      okay = RunPawLiftSegment(
          "paw_hold_" + side, leg, test_lift, test_lift,
          support_shift_x, support_shift_x,
          support_shift_y, support_shift_y,
          kDiagonalSupportZ, kDiagonalSupportZ, hold_seconds,
          sender, receiver, robot_state, command, pause_stats, monitor,
          stop_source, status, status_writer);
    }
    const bool unload_confirmed = okay && !monitor->loaded(leg) &&
        OtherPawSupportsValid(*monitor, leg);
    std::cout << "PAW_UNLOAD side=" << side
              << " confirmed=" << (unload_confirmed ? "true" : "false")
              << " force_n=" << monitor->filtered()[leg]
              << " support_count=" << monitor->support_count() << std::endl;

    // A missed unload still lowers the paw before the test reports failure.
    if (okay) {
      okay = RunPawLiftSegment(
          "paw_lower_" + side, leg, test_lift, 0.0,
          support_shift_x, support_shift_x,
          support_shift_y, support_shift_y,
          kDiagonalSupportZ, kDiagonalSupportZ, lower_seconds,
          sender, receiver, robot_state, command, pause_stats, monitor,
          stop_source, status, status_writer);
    }
    if (okay) {
      okay = RunPawLiftSegment(
          "paw_settle_" + side, leg, 0.0, 0.0,
          support_shift_x, 0.0, support_shift_y, 0.0,
          kDiagonalSupportZ, 0.0, settle_seconds,
          sender, receiver, robot_state, command, pause_stats, monitor,
          stop_source, status, status_writer);
    }
    const bool landing_confirmed = okay && monitor->loaded(leg) &&
        OtherPawSupportsValid(*monitor, leg);
    std::cout << "PAW_LANDING side=" << side
              << " confirmed=" << (landing_confirmed ? "true" : "false")
              << " force_n=" << monitor->filtered()[leg]
              << " support_count=" << monitor->support_count() << std::endl;
    if (!okay || !unload_confirmed || !landing_confirmed) {
      status->last_fault = "paw unload or landing confirmation failed";
      status->estimated_contact_motion_gate_enabled = false;
      PublishExpressionStatus(status_writer, *status);
      return false;
    }
  }
  status->active = "neutral";
  status->phase = "paw_test_complete";
  status->estimated_contact_motion_gate_enabled = false;
  PublishExpressionStatus(status_writer, *status);
  return true;
}

bool RunExpressionLoop(bool continuous, const std::set<std::string>& commissioned,
                       double profile_scale, Sender* sender,
                       FeedbackSource* receiver, RobotStateSource* robot_state,
                       EmotionSource* emotion_source, StopSource* stop_source,
                       RobotCmd* command, FeedbackPauseStats* pause_stats,
                       FootLoadMonitor* foot_load_monitor,
                       SequenceRecordWriter* status_writer,
                       ExpressionStatus* status) {
  ExpressionEngine engine(commissioned, profile_scale);
  auto next = std::chrono::steady_clock::now();
  auto last_trajectory_tick = next;
  auto pause_started = next;
  double trajectory_time = 0.0;
  bool paused = false;
  bool stale_latched = false;
  uint32_t recovery_last_tick = 0;
  int recovery_samples = 0;
  uint64_t last_transport_sequence = 0;
  RobotCmd hold_command = *command;
  int tick = 0;
  status->phase = "profile";

  while (!g_stop.load()) {
    const auto now = std::chrono::steady_clock::now();
    if (!RobotSafetyHealthy(robot_state, "expression")) {
      status->last_fault = "robot state or error interlock";
      return false;
    }
    bool stop = false;
    double stop_age = INFINITY;
    if (!stop_source->Read(&stop, &stop_age) || stop_age > 0.25) {
      status->last_fault = "STOP safety record unavailable";
      return false;
    }
    if (stop) {
      status->last_fault = "Retroid STOP observed";
      return false;
    }

    const EmotionState emotion = emotion_source->GetState();
    status->link_age = emotion.age_seconds;
    if (!stale_latched && emotion.valid &&
        emotion.age_seconds <= kEmotionTimeoutSeconds &&
        emotion.transport_sequence != last_transport_sequence) {
      last_transport_sequence = emotion.transport_sequence;
      engine.Request(emotion.emotion, emotion.valence, emotion.arousal,
                     trajectory_time);
    }
    if (!stale_latched &&
        (!emotion.valid || emotion.age_seconds > kEmotionTimeoutSeconds)) {
      stale_latched = true;
      engine.LinkStale(trajectory_time);
    }

    const RobotData snapshot = receiver->GetState();
    const double feedback_age = FeedbackAgeSeconds();
    if (feedback_age > kFeedbackReleaseSeconds) {
      status->last_fault = "dead 0x0906 feedback";
      return false;
    }
    if (feedback_age > kFeedbackPauseSeconds) {
      if (!paused) {
        paused = true;
        pause_started = now;
        recovery_samples = 0;
        recovery_last_tick = snapshot.tick;
        hold_command = *command;
        for (auto& joint : hold_command.joint_cmd) {
          joint.velocity = 0.0f;
          joint.torque = 0.0f;
        }
        ++pause_stats->pauses;
      }
      status->feedback_paused = true;
      sender->SendCmd(hold_command);
      last_trajectory_tick = now;
    } else {
      if (!FiniteFeedback(snapshot)) {
        status->last_fault = "invalid joint or IMU feedback";
        return false;
      }
      foot_load_monitor->Update(snapshot.tick, EstimateFootForces(snapshot));
      UpdateEstimatedContactStatus(*foot_load_monitor, status);
      if (paused) {
        if (feedback_age <= kFeedbackRecoveryAgeSeconds &&
            snapshot.tick != recovery_last_tick) {
          recovery_last_tick = snapshot.tick;
          ++recovery_samples;
        } else if (feedback_age > kFeedbackRecoveryAgeSeconds) {
          recovery_samples = 0;
          recovery_last_tick = snapshot.tick;
        }
        sender->SendCmd(hold_command);
        last_trajectory_tick = now;
        if (recovery_samples >= kFeedbackRecoverySamples) {
          const double seconds = std::chrono::duration<double>(
              now - pause_started).count();
          pause_stats->max_pause_seconds = std::max(
              pause_stats->max_pause_seconds, seconds);
          ++pause_stats->recoveries;
          paused = false;
          status->feedback_paused = false;
        }
      } else {
        trajectory_time += std::min(
            std::chrono::duration<double>(now - last_trajectory_tick).count(),
            kMaxTrajectoryStepSeconds);
        last_trajectory_tick = now;
        const ExpressionSample sample = engine.Sample(trajectory_time);
        SetStandCommand(command, sample);
        if (tick > 50 && TrackingError(*command, snapshot) > kHoldErrorLimit) {
          status->last_fault = "tracking error limit";
          return false;
        }
        sender->SendCmd(*command);
      }
    }

    status->requested = engine.requested();
    status->active = engine.active();
    status->phase = engine.phase();
    status->profile_cycle = engine.profile_cycle();
    status->pending = engine.pending();
    if (tick % 20 == 0) PublishExpressionStatus(status_writer, *status);
    if (stale_latched && engine.stale_reset_complete()) {
      status->release_state = "stale_link_release";
      PublishExpressionStatus(status_writer, *status);
      return true;
    }
    if (!continuous && engine.active() == "neutral" &&
        engine.profile_cycle() >= 1 && !stale_latched) {
      stale_latched = true;
      engine.LinkStale(trajectory_time);
    }
    ++tick;
    next += std::chrono::milliseconds(1);
    if (next < now) next = now + std::chrono::milliseconds(1);
    std::this_thread::sleep_until(next);
  }
  status->last_fault = "termination signal";
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  bool execute = false;
  bool continuous = false;
  bool joy_paw_test = false;
  bool joy_suite_test = false;
  double paw_lift_meters = 0.005;
  double profile_scale = 1.0;
  std::set<std::string> commissioned{"neutral"};
  ScopedPidFile pid_file;
  std::string target_ip = "192.168.1.120";
  uint16_t target_port = 43893;
  for (int i = 1; i < argc; ++i) {
    const std::string argument(argv[i]);
    if (argument == "--execute") execute = true;
    else if (argument == "--continuous") continuous = true;
    else if (argument == "--joy-paw-test") joy_paw_test = true;
    else if (argument == "--joy-suite-test") {
      joy_paw_test = true;
      joy_suite_test = true;
    }
    else if (argument.rfind("--paw-lift-meters=", 0) == 0) {
      paw_lift_meters = std::stod(argument.substr(18));
    }
    else if (argument.rfind("--target-ip=", 0) == 0) target_ip = argument.substr(12);
    else if (argument.rfind("--target-port=", 0) == 0) {
      target_port = static_cast<uint16_t>(std::stoi(argument.substr(14)));
    } else if (argument.rfind("--profile-scale=", 0) == 0) {
      profile_scale = std::stod(argument.substr(16));
    } else if (argument.rfind("--minimum-battery=", 0) == 0) {
      g_minimum_battery = std::stod(argument.substr(18));
    } else if (argument.rfind("--commissioned-emotions=", 0) == 0) {
      commissioned.clear();
      std::istringstream values(argument.substr(24));
      std::string value;
      while (std::getline(values, value, ',')) {
        if (!PhysicalProfiles().count(value)) {
          std::cerr << "unknown commissioned emotion: " << value << std::endl;
          return 2;
        }
        commissioned.insert(value);
      }
      commissioned.insert("neutral");
    } else if (argument.rfind("--pid-file=", 0) == 0) {
      pid_file.path = argument.substr(11);
    } else {
      std::cerr << "usage: " << argv[0]
                << " [--execute] [--continuous] [--target-ip=IP]"
                << " [--target-port=PORT] [--profile-scale=0..1]"
                << " [--minimum-battery=PERCENT]"
                << " [--commissioned-emotions=neutral,...] [--pid-file=PATH]"
                << " [--joy-paw-test] [--joy-suite-test]"
                << " [--paw-lift-meters=0.003..0.050]\n";
      return 2;
    }
  }
  if (!(profile_scale > 0.0 && profile_scale <= 1.0)) {
    std::cerr << "profile scale must be in (0, 1]" << std::endl;
    return 2;
  }
  if (!(g_minimum_battery >= 25.0 && g_minimum_battery <= 100.0)) {
    std::cerr << "minimum battery must be in [25, 100] percent" << std::endl;
    return 2;
  }
  if (!(paw_lift_meters >= 0.003 && paw_lift_meters <= 0.050)) {
    std::cerr << "paw lift must be in [0.003, 0.050] metres" << std::endl;
    return 2;
  }

  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);

  int lock_fd = open(kLockPath, O_CREAT | O_RDWR, 0600);
  if (lock_fd < 0 || flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
    std::cerr << "exclusive SDK lease unavailable" << std::endl;
    return 2;
  }

  SequenceRecordWriter status_writer;
  if (!status_writer.Open(kExpressionStatusPath)) {
    std::cerr << "unable to create expression status record" << std::endl;
    return 3;
  }
  ExpressionStatus status;
  PublishExpressionStatus(&status_writer, status);

  FeedbackSource receiver;
  if (!receiver.Open()) {
    std::cerr << "unable to open reviewed passive 0x0906 feed" << std::endl;
    return 3;
  }
  RobotStateSource robot_state;
  if (!robot_state.Open()) {
    std::cerr << "unable to open validated 0x0901 safety feed" << std::endl;
    return 3;
  }
  if (!WaitForRobotSafetyState(&robot_state, 3.0)) {
    std::cerr << "validated 0x0901 safety feed did not become readable"
              << std::endl;
    return 3;
  }
  if (!RobotSafetyHealthy(&robot_state, "preflight")) return 3;
  const RobotSafetyState initial_robot_state = robot_state.GetState();
  if (initial_robot_state.basic_state != 1) {
    std::cerr << "official expression runtime requires initial robot state 1; observed "
              << initial_robot_state.basic_state << std::endl;
    return 3;
  }
  EmotionSource emotion_source;
  if (!emotion_source.Open()) {
    std::cerr << "unable to open validated emotion shared record" << std::endl;
    return 3;
  }
  StopSource stop_source;
  if (!stop_source.Open()) {
    std::cerr << "unable to open STOP safety shared record" << std::endl;
    return 3;
  }
  const bool initial_feedback = WaitForFeedback(
      &receiver, &robot_state, "preflight", 3.0);
  if (g_safety_fault.load()) return 3;
  if (initial_feedback) {
    const RobotData initial = receiver.GetState();
    std::cout << "PREFLIGHT tick=" << initial.tick << " positions=";
    for (const auto& joint : initial.joint_data.joint_data) {
      std::cout << joint.position << ',';
    }
    std::cout << std::endl;
  } else {
    std::cout << "PREFLIGHT 0x0906 pending until RobotStateInit" << std::endl;
  }
  if (!execute) {
    std::cout << "PREFLIGHT_ONLY no command sent" << std::endl;
    return initial_feedback ? 0 : 3;
  }

  std::cout << "PHASE wait_for_fresh_emotion" << std::endl;
  const auto emotion_deadline = std::chrono::steady_clock::now() +
      std::chrono::seconds(30);
  EmotionState initial_emotion;
  bool stop = false;
  double stop_age = INFINITY;
  while (std::chrono::steady_clock::now() < emotion_deadline && !g_stop.load()) {
    initial_emotion = emotion_source.GetState();
    if (initial_emotion.valid && initial_emotion.age_seconds <= kEmotionTimeoutSeconds &&
        stop_source.Read(&stop, &stop_age) && stop_age <= 0.25 && !stop) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (!initial_emotion.valid || initial_emotion.age_seconds > kEmotionTimeoutSeconds ||
      !stop_source.Read(&stop, &stop_age) || stop_age > 0.25 || stop) {
    std::cerr << "fresh emotion and STOP-safe records are required before acquisition"
              << std::endl;
    return 3;
  }

  Watchdog watchdog;
  if (!watchdog.Start(target_ip, target_port)) {
    std::cerr << "failed to start independent release watchdog" << std::endl;
    return 4;
  }

  Sender sender(target_ip, target_port);
  MotionExample motion;
  RobotCmd command;
  std::memset(&command, 0, sizeof(command));
  bool success = false;
  bool acquired = true;
  FeedbackPauseStats pause_stats;
  FootLoadMonitor foot_load_monitor;
  {
    std::ofstream marker(kMarkerPath, std::ios::trunc);
    marker << getpid() << '\n';
  }
  status.requested = initial_emotion.emotion;
  status.ownership = true;
  status.release_state = "owned";
  status.phase = "robot_state_init";
  PublishExpressionStatus(&status_writer, status);
  std::cout << "EXPRESSION_RUNTIME profiles=" << PhysicalProfiles().size()
            << " commissioned=" << commissioned.size()
            << " scale=" << profile_scale
            << " neutral_cycle_seconds=" << kBreathDurationSeconds << std::endl;

  std::cout << "PHASE robot_state_init" << std::endl;
  sender.RobotStateInit();
  bool okay = WaitForFeedback(
      &receiver, &robot_state, "robot_state_init", 3.0);
  if (!okay && !g_safety_fault.load()) {
    okay = BootstrapSdkFeedback(
        &sender, &receiver, &robot_state, &command);
  }
  if (!okay) {
    std::cerr << "ABORT no fresh valid 0x0906 after RobotStateInit" << std::endl;
  } else {
    motion.GetInitData(receiver.GetState().joint_data, 0.0);
    std::cout << "PHASE_COMPLETE robot_state_init feedback_age_ms="
              << FeedbackAgeSeconds() * 1000.0 << std::endl;
  }

  if (okay) {
    status.phase = "pre_stand";
    PublishExpressionStatus(&status_writer, status);
    okay = RunPhase(
        "pre_stand", 1.0, kTransitionErrorLimit, &sender, &receiver,
        &robot_state, &command, &pause_stats,
        [&motion](double elapsed, const RobotData& data, RobotCmd* output) {
          motion.PreStandUp(*output, elapsed, const_cast<RobotData&>(data));
        });
  }

  if (okay) {
    status.phase = "stand";
    PublishExpressionStatus(&status_writer, status);
    motion.GetInitData(receiver.GetState().joint_data, 1.0);
    okay = RunPhase(
        "stand", 1.5, kTransitionErrorLimit, &sender, &receiver,
        &robot_state, &command, &pause_stats,
        [&motion](double elapsed, const RobotData& data, RobotCmd* output) {
          motion.StandUp(*output, 1.0 + elapsed, const_cast<RobotData&>(data));
        });
  }

  if (okay) {
    status.phase = "stand_hold";
    PublishExpressionStatus(&status_writer, status);
    okay = RunPhase(
        "stand_hold", 1.0, kHoldErrorLimit, &sender, &receiver,
        &robot_state, &command, &pause_stats,
        [&foot_load_monitor](double, const RobotData& data, RobotCmd* output) {
          SetStandCommand(output, BreathPose{});
          foot_load_monitor.ObserveStanding(
              data.tick, EstimateFootForces(data));
        });
    const bool baseline_valid = foot_load_monitor.FinalizeBaseline();
    UpdateEstimatedContactStatus(foot_load_monitor, &status);
    std::cout << "ESTIMATED_CONTACT baseline_valid="
              << (baseline_valid ? "true" : "false")
              << " samples=" << foot_load_monitor.baseline_samples()
              << " total_vertical_force_n=" << foot_load_monitor.total_load()
              << " motion_gate_enabled=false" << std::endl;
    PublishExpressionStatus(&status_writer, status);
  }

  if (okay) {
    if (joy_paw_test) {
      if (joy_suite_test) {
        okay = RunNeutralTestWindow(
            &sender, &receiver, &robot_state, &command, &pause_stats,
            &foot_load_monitor, &stop_source, &status_writer, &status);
      }
      if (okay) {
        okay = RunJoyPawTest(
            &sender, &receiver, &robot_state, &command, &pause_stats,
            &foot_load_monitor, &stop_source, &status_writer, &status,
            paw_lift_meters, joy_suite_test);
      }
    } else {
      okay = RunExpressionLoop(
          continuous, commissioned, profile_scale, &sender, &receiver,
          &robot_state, &emotion_source, &stop_source, &command, &pause_stats,
          &foot_load_monitor, &status_writer, &status);
    }
  }

  std::cout << "PHASE release_to_robot" << std::endl;
  status.phase = "release";
  status.release_state = "releasing";
  if (!okay && status.last_fault.empty()) status.last_fault = "runtime phase failed";
  PublishExpressionStatus(&status_writer, status);
  if (acquired) sender.ControlGet(ROBOT);
  acquired = false;
  unlink(kMarkerPath);
  watchdog.CompleteNormally();
  status.ownership = false;
  status.release_state = "released";
  PublishExpressionStatus(&status_writer, status);

  std::cout << "FEEDBACK_STATS max_age_ms="
            << receiver.max_age_seconds() * 1000.0
            << " max_consecutive_update_gap_ms="
            << receiver.max_consecutive_gap_seconds() * 1000.0
            << " skipped_shared_updates=" << receiver.skipped_updates()
            << " pauses=" << pause_stats.pauses
            << " recoveries=" << pause_stats.recoveries
            << " max_pause_ms=" << pause_stats.max_pause_seconds * 1000.0
            << std::endl;
  std::cout << "SDK_OWNERSHIP_RELEASED marker_present="
            << (access(kMarkerPath, F_OK) == 0 ? "true" : "false")
            << std::endl;
  std::cout << "ROBOT_SAFETY_FAULT triggered="
            << (g_safety_fault.load() ? "true" : "false") << std::endl;

  if (okay && !g_stop.load()) {
    std::cout << "PASS official SDK expression session" << std::endl;
    success = true;
  }

  close(lock_fd);
  return success ? 0 : 5;
}
