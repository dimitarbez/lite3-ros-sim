#include <emotion_bot_lite3_hw/DirectJointCommand.h>
#include <emotion_bot_lite3_hw/DirectJointFeedback.h>
#include <emotion_bot_lite3_hw/DirectJointSafety.h>
#include <emotion_bot_lite3_hw/motion_sdk_protocol.hpp>
#include <ros/ros.h>
#include <std_msgs/String.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using emotion_bot_lite3_hw::CommandHeader;
using emotion_bot_lite3_hw::DirectJointCommand;
using emotion_bot_lite3_hw::DirectJointFeedback;
using emotion_bot_lite3_hw::DirectJointSafety;
using emotion_bot_lite3_hw::JointCommandWire;
using emotion_bot_lite3_hw::RobotCommandWire;

std::atomic<bool> g_stop_requested(false);

void HandleSignal(int) { g_stop_requested.store(true); }

double MonotonicNow() {
  timespec value{};
  if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
    throw std::runtime_error("clock_gettime failed");
  }
  return static_cast<double>(value.tv_sec) + static_cast<double>(value.tv_nsec) * 1e-9;
}

bool Finite(double value) { return std::isfinite(value); }

template <typename Container>
bool AllFinite(const Container& values) {
  return std::all_of(values.begin(), values.end(), [](double value) { return Finite(value); });
}

struct ScalarBoundary {
  double position{0.0};
  double velocity{0.0};
  double acceleration{0.0};
};

class Quintic {
 public:
  Quintic() = default;
  Quintic(const ScalarBoundary& start, const ScalarBoundary& end, double duration)
      : duration_(duration) {
    const double t = duration_;
    coefficients_[0] = start.position;
    coefficients_[1] = start.velocity;
    coefficients_[2] = 0.5 * start.acceleration;
    const double p = end.position -
        (coefficients_[0] + coefficients_[1] * t + coefficients_[2] * t * t);
    const double v = end.velocity - (coefficients_[1] + 2.0 * coefficients_[2] * t);
    const double a = end.acceleration - 2.0 * coefficients_[2];
    coefficients_[3] = (10.0 * p - 4.0 * v * t + 0.5 * a * t * t) / std::pow(t, 3);
    coefficients_[4] = (-15.0 * p + 7.0 * v * t - a * t * t) / std::pow(t, 4);
    coefficients_[5] = (6.0 * p - 3.0 * v * t + 0.5 * a * t * t) / std::pow(t, 5);
  }

  ScalarBoundary Sample(double elapsed) const {
    const double t = std::max(0.0, std::min(duration_, elapsed));
    const auto& c = coefficients_;
    return {
        c[0] + c[1]*t + c[2]*t*t + c[3]*std::pow(t,3) + c[4]*std::pow(t,4) + c[5]*std::pow(t,5),
        c[1] + 2*c[2]*t + 3*c[3]*t*t + 4*c[4]*std::pow(t,3) + 5*c[5]*std::pow(t,4),
        2*c[2] + 6*c[3]*t + 12*c[4]*t*t + 20*c[5]*std::pow(t,3),
    };
  }

 private:
  double duration_{1.0};
  std::array<double, 6> coefficients_{};
};

class MotionSdkBridge {
 public:
  MotionSdkBridge() : private_node_("~") {
    LoadParam("transmit_enabled", "/emotion_bot/hardware/direct_joint/transmit_enabled",
              &transmit_enabled_, false);
    LoadParam("takeover_transition_commissioned",
              "/emotion_bot/hardware/direct_joint/takeover_transition_commissioned",
              &takeover_transition_commissioned_, false);
    LoadParam("target_ip", "/emotion_bot/hardware/network/motion_host_ip",
              &target_ip_, std::string("192.168.1.120"));
    LoadParam("target_port", "/emotion_bot/hardware/network/command_port", &target_port_, 43893);
    LoadParam("sender_rate", "/emotion_bot/hardware/direct_joint/sender_rate", &sender_rate_, 1000.0);
    LoadParam("command_timeout", "/emotion_bot/hardware/direct_joint/command_timeout",
              &command_timeout_, 0.05);
    LoadParam("feedback_timeout", "/emotion_bot/hardware/direct_joint/feedback_timeout",
              &feedback_timeout_, 0.05);
    private_node_.param("safety_timeout", safety_timeout_, 0.15);
    LoadParam("maximum_anchor_delta", "/emotion_bot/hardware/direct_joint/maximum_anchor_delta",
              &maximum_anchor_delta_, 0.08);
    LoadParam("maximum_velocity", "/emotion_bot/hardware/direct_joint/maximum_velocity",
              &maximum_velocity_, 0.20);
    LoadParam("maximum_acceleration", "/emotion_bot/hardware/direct_joint/maximum_acceleration",
              &maximum_acceleration_, 0.80);
    LoadParam("maximum_joint_error", "/emotion_bot/hardware/direct_joint/maximum_joint_error",
              &maximum_joint_error_, 0.08);
    private_node_.param("maximum_send_gap", maximum_send_gap_, 0.010);
    LoadParam("neutral_return_time", "/emotion_bot/hardware/direct_joint/neutral_return_time",
              &neutral_return_time_, 1.5);
    LoadParam("neutral_hold_time", "/emotion_bot/hardware/direct_joint/neutral_hold_time",
              &neutral_hold_time_, 0.35);
    LoadParam("ownership_lock", "/emotion_bot/hardware/direct_joint/ownership_lock",
              &lock_path_, std::string("/dev/shm/emotion_bot_lite3_direct_joint.lock"));
    LoadParam("ownership_marker", "/emotion_bot/hardware/direct_joint/ownership_marker",
              &marker_path_, std::string("/dev/shm/emotion_bot_lite3_direct_joint.owned"));
    if (sender_rate_ < 900.0 || sender_rate_ > 1100.0) {
      throw std::runtime_error("sender_rate must stay near the documented 1 kHz loop");
    }
    AcquireProcessLease();
    OpenSocket();
    RecoverStaleOwnership();
    command_sub_ = node_.subscribe(
        "/emotion_bot/hardware/direct_joint/command", 5,
        &MotionSdkBridge::OnCommand, this);
    feedback_sub_ = node_.subscribe(
        "/emotion_bot/hardware/direct_joint/feedback", 100,
        &MotionSdkBridge::OnFeedback, this);
    safety_sub_ = node_.subscribe(
        "/emotion_bot/hardware/direct_joint/safety", 20,
        &MotionSdkBridge::OnSafety, this);
    status_pub_ = node_.advertise<std_msgs::String>(
        "/emotion_bot/hardware/direct_joint/bridge_status", 10, true);
    PublishStatus("DISARMED", transmit_enabled_ ? "waiting for complete preflight" : "transmit disabled");
  }

  ~MotionSdkBridge() {
    if (owned_) {
      EmergencyRelease("bridge destructor");
    }
    FinishWatchdog();
    if (socket_ >= 0) close(socket_);
    if (lock_fd_ >= 0) close(lock_fd_);
  }

  int Run() {
    ros::AsyncSpinner spinner(2);
    spinner.start();
    const double period = 1.0 / sender_rate_;
    double next = MonotonicNow();
    while (ros::ok() && !finished_) {
      const double now = MonotonicNow();
      Step(now);
      next += period;
      if (next < now - period) next = now + period;
      timespec deadline{};
      deadline.tv_sec = static_cast<time_t>(next);
      deadline.tv_nsec = static_cast<long>((next - deadline.tv_sec) * 1e9);
      while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR) {}
    }
    if (owned_) {
      BeginRelease(MonotonicNow(), "ROS shutdown", MotionSafe(MonotonicNow()));
      const double deadline = MonotonicNow() + neutral_return_time_ + neutral_hold_time_ + 0.5;
      while (owned_ && MonotonicNow() < deadline) {
        Step(MonotonicNow());
        usleep(1000);
      }
      if (owned_) EmergencyRelease("shutdown release deadline");
    }
    FinishWatchdog();
    return 0;
  }

 private:
  template <typename T>
  void LoadParam(const std::string& private_name, const std::string& global_name,
                 T* output, const T& fallback) {
    node_.param(global_name, *output, fallback);
    private_node_.param(private_name, *output, *output);
  }

  enum class State { kDisarmed, kActive, kReturning, kReleased, kFault };

  void OnCommand(const DirectJointCommand::ConstPtr& message) {
    std::lock_guard<std::mutex> guard(mutex_);
    command_ = *message;
    command_at_ = MonotonicNow();
    have_command_ = true;
  }

  void OnFeedback(const DirectJointFeedback::ConstPtr& message) {
    std::lock_guard<std::mutex> guard(mutex_);
    feedback_ = *message;
    feedback_at_ = MonotonicNow();
    have_feedback_ = true;
  }

  void OnSafety(const DirectJointSafety::ConstPtr& message) {
    std::lock_guard<std::mutex> guard(mutex_);
    safety_ = *message;
    safety_at_ = MonotonicNow();
    have_safety_ = true;
  }

  void Step(double now) {
    Snapshot snapshot;
    {
      std::lock_guard<std::mutex> guard(mutex_);
      snapshot.command = command_;
      snapshot.feedback = feedback_;
      snapshot.safety = safety_;
      snapshot.command_at = command_at_;
      snapshot.feedback_at = feedback_at_;
      snapshot.safety_at = safety_at_;
      snapshot.have_command = have_command_;
      snapshot.have_feedback = have_feedback_;
      snapshot.have_safety = have_safety_;
    }
    latest_ = snapshot;
    if (g_stop_requested.load() && state_ == State::kDisarmed) {
      finished_ = true;
      return;
    }
    if (state_ == State::kDisarmed) {
      if (CanAcquire(snapshot, now)) Acquire(snapshot, now);
      return;
    }
    if (state_ == State::kActive) {
      if (g_stop_requested.load()) {
        BeginRelease(now, "process shutdown", MotionSafe(now));
        return;
      }
      if (!Fresh(snapshot.feedback_at, feedback_timeout_, now) ||
          !Fresh(snapshot.safety_at, safety_timeout_, now)) {
        EmergencyRelease("feedback or safety stale");
        return;
      }
      if (!snapshot.safety.motion_safe) {
        EmergencyRelease(snapshot.safety.reason.empty() ? "motion gate failed" : snapshot.safety.reason);
        return;
      }
      if (!Fresh(snapshot.command_at, command_timeout_, now) || snapshot.command.release ||
          !snapshot.command.enable || !snapshot.safety.link_fresh) {
        BeginRelease(now, "command disabled, stale, or EmotionBot link lost", true);
        return;
      }
      if (last_send_at_ > 0.0 && now - last_send_at_ > maximum_send_gap_) {
        EmergencyRelease("1 kHz sender deadline missed");
        return;
      }
      std::string error;
      if (!ValidateCommand(snapshot.command, &error)) {
        EmergencyRelease(error);
        return;
      }
      if (!TrackingHealthy(snapshot.feedback, &error)) {
        EmergencyRelease(error);
        return;
      }
      SendCommand(snapshot.command.position, snapshot.command.velocity,
                  snapshot.command.kp, snapshot.command.kd);
      last_position_ = ToArray(snapshot.command.position);
      last_velocity_ = ToArray(snapshot.command.velocity);
      last_acceleration_ = ToArray(snapshot.command.acceleration);
      return;
    }
    if (state_ == State::kReturning) {
      if (!MotionSafe(now)) {
        EmergencyRelease("motion gate failed during measured-neutral return");
        return;
      }
      const double elapsed = now - return_started_;
      std::array<double, 12> position{}, velocity{}, acceleration{};
      for (std::size_t index = 0; index < 12; ++index) {
        const auto value = return_segments_[index].Sample(elapsed);
        position[index] = value.position;
        velocity[index] = value.velocity;
        acceleration[index] = value.acceleration;
      }
      std::array<double, 12> kp{}, kd{};
      kp.fill(last_kp_);
      kd.fill(last_kd_);
      SendCommand(position, velocity, kp, kd);
      last_position_ = position;
      last_velocity_ = velocity;
      last_acceleration_ = acceleration;
      if (elapsed >= neutral_return_time_ + neutral_hold_time_) {
        if (!AtMeasuredNeutral()) {
          EmergencyRelease("measured neutral was not reached before release deadline");
        } else {
          ReleaseRobot("measured neutral reached");
        }
      }
    }
  }

  struct Snapshot {
    DirectJointCommand command;
    DirectJointFeedback feedback;
    DirectJointSafety safety;
    double command_at{0.0};
    double feedback_at{0.0};
    double safety_at{0.0};
    bool have_command{false};
    bool have_feedback{false};
    bool have_safety{false};
  } latest_;

  bool Fresh(double received, double timeout, double now) const {
    return received > 0.0 && now - received <= timeout;
  }

  bool CanAcquire(const Snapshot& value, double now) {
    if (!transmit_enabled_ || !takeover_transition_commissioned_ || g_stop_requested.load()) {
      return false;
    }
    if (!(value.have_command && value.have_feedback && value.have_safety)) return false;
    if (!(Fresh(value.command_at, command_timeout_, now) &&
          Fresh(value.feedback_at, feedback_timeout_, now) &&
          Fresh(value.safety_at, safety_timeout_, now))) return false;
    if (!value.command.enable || value.command.release || !value.safety.ready ||
        !value.safety.planted_gate_satisfied) return false;
    return true;
  }

  bool MotionSafe(double now) const {
    return latest_.have_feedback && latest_.have_safety &&
        Fresh(latest_.feedback_at, feedback_timeout_, now) &&
        Fresh(latest_.safety_at, safety_timeout_, now) &&
        latest_.safety.motion_safe && latest_.safety.planted_gate_satisfied;
  }

  void Acquire(const Snapshot& value, double now) {
    // The controller captures its anchor from validated feedback and republishes
    // that same position at 100 Hz.  A newer feedback sample can differ by a
    // fraction of a milliradian from encoder noise.  Keep the commanded measured
    // sample as the immutable anchor, but require it to agree closely with the
    // bridge's fresh sample before ownership is acquired.
    const auto fresh_measured = ToArray(value.feedback.position);
    anchor_ = ToArray(value.command.position);
    last_position_ = anchor_;
    last_velocity_.fill(0.0);
    last_acceleration_.fill(0.0);
    std::string error;
    if (!ValidateCommand(value.command, &error)) {
      PublishStatus("FAULT", "pre-acquisition command rejected: " + error);
      return;
    }
    for (std::size_t i = 0; i < 12; ++i) {
      if (std::abs(anchor_[i] - fresh_measured[i]) > 0.005) {
        PublishStatus("FAULT", "first command does not equal fresh measured stance");
        return;
      }
    }
    last_kp_ = value.command.kp[0];
    last_kd_ = value.command.kd[0];
    StartWatchdog();
    std::array<double, 12> zero{};
    std::array<double, 12> kp{}, kd{};
    kp.fill(last_kp_);
    kd.fill(last_kd_);
    SendCommand(anchor_, zero, kp, kd);  // harmless measured hold before ownership
    SendSimple(emotion_bot_lite3_hw::kControlGetSdkCode);
    // From this point onward every exception path must explicitly relinquish
    // SDK ownership.  Do not defer this flag until after marker persistence.
    owned_ = true;
    SendCommand(anchor_, zero, kp, kd);  // less than one loop after acquisition
    WriteMarker();
    state_ = State::kActive;
    last_send_at_ = now;
    PublishStatus("ACTIVE", "SDK_ACQUIRED with measured hold");
  }

  bool ValidateCommand(const DirectJointCommand& command, std::string* error) const {
    if (!(AllFinite(command.position) && AllFinite(command.velocity) &&
          AllFinite(command.acceleration) && AllFinite(command.kp) && AllFinite(command.kd))) {
      *error = "command contains non-finite values";
      return false;
    }
    static const std::array<std::pair<double, double>, 3> limits{{
        {-0.4189, 0.4189}, {-3.4907, 0.3491}, {0.6021, 2.7227}}};
    double compression = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t leg = 0; leg < 4; ++leg) {
      const std::size_t hip_x = 3 * leg;
      const std::size_t hip_y = hip_x + 1;
      const std::size_t knee = hip_x + 2;
      for (std::size_t joint = 0; joint < 3; ++joint) {
        const std::size_t index = hip_x + joint;
        if (command.position[index] < limits[joint].first ||
            command.position[index] > limits[joint].second ||
            std::abs(command.position[index] - anchor_[index]) > maximum_anchor_delta_ ||
            std::abs(command.velocity[index]) > maximum_velocity_ ||
            std::abs(command.acceleration[index]) > maximum_acceleration_) {
          *error = "command exceeds joint p/v/a or measured-anchor limit";
          return false;
        }
      }
      if (std::abs(command.position[hip_x] - anchor_[hip_x]) > 1e-6 ||
          std::abs(command.velocity[hip_x]) > 1e-6 ||
          std::abs(command.acceleration[hip_x]) > 1e-6) {
        *error = "HipX motion is prohibited in the planted milestone";
        return false;
      }
      const double current = anchor_[hip_y] - command.position[hip_y];
      if (current < -1e-6 ||
          std::abs((command.position[knee] - anchor_[knee]) - 2.0 * current) > 1e-5 ||
          std::abs(command.velocity[knee] + 2.0 * command.velocity[hip_y]) > 1e-5 ||
          std::abs(command.acceleration[knee] + 2.0 * command.acceleration[hip_y]) > 1e-5) {
        *error = "command is not symmetric planted 2:1 compression";
        return false;
      }
      if (leg == 0) compression = current;
      if (std::abs(current - compression) > 1e-5) {
        *error = "unilateral or weight-shift joint command is prohibited";
        return false;
      }
      if (std::abs(command.kp[hip_x] - command.kp[hip_y]) > 1e-6 ||
          std::abs(command.kp[hip_x] - command.kp[knee]) > 1e-6 ||
          std::abs(command.kd[hip_x] - command.kd[hip_y]) > 1e-6 ||
          std::abs(command.kd[hip_x] - command.kd[knee]) > 1e-6 ||
          command.kp[hip_x] <= 0.0 || command.kp[hip_x] > 60.0 ||
          command.kd[hip_x] < 0.0 || command.kd[hip_x] > 2.0) {
        *error = "gain command outside reviewed position-control envelope";
        return false;
      }
    }
    return true;
  }

  bool TrackingHealthy(const DirectJointFeedback& feedback, std::string* error) const {
    for (std::size_t index = 0; index < 12; ++index) {
      if (std::abs(feedback.position[index] - last_position_[index]) > maximum_joint_error_) {
        *error = "measured joint error exceeds limit";
        return false;
      }
    }
    return true;
  }

  void BeginRelease(double now, const std::string& reason, bool safe_to_return) {
    if (!owned_) return;
    if (!safe_to_return) {
      EmergencyRelease(reason);
      return;
    }
    for (std::size_t index = 0; index < 12; ++index) {
      return_segments_[index] = Quintic(
          {last_position_[index], last_velocity_[index], last_acceleration_[index]},
          {anchor_[index], 0.0, 0.0}, neutral_return_time_);
    }
    return_started_ = now;
    state_ = State::kReturning;
    PublishStatus("RETURNING", reason);
  }

  bool AtMeasuredNeutral() const {
    if (!latest_.have_feedback) return false;
    for (std::size_t index = 0; index < 12; ++index) {
      if (std::abs(latest_.feedback.position[index] - anchor_[index]) > 0.03 ||
          std::abs(latest_.feedback.velocity[index]) > 0.08) return false;
    }
    return latest_.safety.planted_gate_satisfied;
  }

  void EmergencyRelease(const std::string& reason) {
    bool release_confirmed = !owned_;
    if (owned_) {
      try {
        SendSimple(emotion_bot_lite3_hw::kReleaseRobotCode);
        release_confirmed = true;
      } catch (const std::exception& error) {
        ROS_ERROR("emergency SDK release send failed: %s", error.what());
      }
    }
    owned_ = false;
    unlink(marker_path_.c_str());
    state_ = State::kFault;
    PublishStatus("FAULT_RELEASE_ATTEMPTED", reason + "; explicit ROBOT_RELEASE attempted");
    // If the parent could not transmit release, close the pipe without the
    // acknowledgement byte so the independent watchdog makes its own attempt.
    FinishWatchdog(release_confirmed);
    finished_ = true;
  }

  void ReleaseRobot(const std::string& reason) {
    SendSimple(emotion_bot_lite3_hw::kReleaseRobotCode);
    owned_ = false;
    unlink(marker_path_.c_str());
    state_ = State::kReleased;
    PublishStatus("RELEASED", reason + "; ROBOT_RELEASED");
    FinishWatchdog();
    finished_ = true;
  }

  template <typename Container>
  std::array<double, 12> ToArray(const Container& values) const {
    std::array<double, 12> output{};
    std::copy(values.begin(), values.end(), output.begin());
    return output;
  }

  template <typename Position, typename Velocity, typename Kp, typename Kd>
  void SendCommand(const Position& position, const Velocity& velocity,
                   const Kp& kp, const Kd& kd) {
    RobotCommandWire command{};
    for (std::size_t index = 0; index < 12; ++index) {
      command.joints[index] = JointCommandWire{
          static_cast<float>(position[index]), static_cast<float>(velocity[index]), 0.0f,
          static_cast<float>(kp[index]), static_cast<float>(kd[index])};
    }
    const auto packet = emotion_bot_lite3_hw::JointPacket(command);
    SendBytes(packet.data(), packet.size());
    last_send_at_ = MonotonicNow();
    HeartbeatWatchdog();
  }

  void SendSimple(std::uint32_t code) {
    const auto packet = emotion_bot_lite3_hw::SimplePacket(code);
    SendBytes(packet.data(), packet.size());
  }

  void SendBytes(const void* data, std::size_t size) {
    if (!transmit_enabled_) throw std::runtime_error("attempted UDP output while transmit disabled");
    const ssize_t sent = send(socket_, data, size, MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(size)) throw std::runtime_error("MotionSDK UDP send failed");
  }

  void OpenSocket() {
    socket_ = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (socket_ < 0) throw std::runtime_error("unable to create MotionSDK UDP socket");
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(static_cast<std::uint16_t>(target_port_));
    if (inet_pton(AF_INET, target_ip_.c_str(), &target.sin_addr) != 1 ||
        connect(socket_, reinterpret_cast<sockaddr*>(&target), sizeof(target)) != 0) {
      throw std::runtime_error("unable to connect MotionSDK UDP socket");
    }
  }

  void AcquireProcessLease() {
    lock_fd_ = open(lock_path_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lock_fd_ < 0 || flock(lock_fd_, LOCK_EX | LOCK_NB) != 0) {
      throw std::runtime_error("another direct-joint sender owns the exclusive lease");
    }
    const std::string value = std::to_string(getpid()) + "\n";
    if (ftruncate(lock_fd_, 0) != 0) {
      throw std::runtime_error("unable to truncate direct-joint lease record");
    }
    if (write(lock_fd_, value.data(), value.size()) != static_cast<ssize_t>(value.size())) {
      throw std::runtime_error("unable to record direct-joint lease");
    }
    fsync(lock_fd_);
  }

  void WriteMarker() {
    const int fd = open(marker_path_.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0) throw std::runtime_error("unable to create SDK ownership marker");
    std::ostringstream value;
    value << "pid=" << getpid() << " watchdog_pid=" << watchdog_pid_ << "\n";
    const std::string encoded = value.str();
    if (write(fd, encoded.data(), encoded.size()) != static_cast<ssize_t>(encoded.size()) ||
        fsync(fd) != 0) {
      close(fd);
      throw std::runtime_error("unable to persist SDK ownership marker");
    }
    close(fd);
  }

  void RecoverStaleOwnership() {
    if (access(marker_path_.c_str(), F_OK) != 0) return;
    if (!transmit_enabled_) {
      throw std::runtime_error("SDK recovery marker exists while transmit is disabled");
    }
    SendSimple(emotion_bot_lite3_hw::kReleaseRobotCode);
    usleep(250000);
    unlink(marker_path_.c_str());
  }

  void StartWatchdog() {
    if (watchdog_pid_ > 0) return;
    int pipe_fds[2];
    if (pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK) != 0) {
      throw std::runtime_error("unable to create sender watchdog pipe");
    }
    watchdog_pid_ = fork();
    if (watchdog_pid_ < 0) {
      close(pipe_fds[0]); close(pipe_fds[1]);
      throw std::runtime_error("unable to fork sender watchdog");
    }
    if (watchdog_pid_ == 0) {
      close(pipe_fds[1]);
      pollfd descriptor{pipe_fds[0], POLLIN | POLLHUP, 0};
      bool release = true;
      while (true) {
        const int result = poll(&descriptor, 1, 100);
        if (result == 0) break;
        if (result < 0 && errno == EINTR) continue;
        if (result < 0) break;
        char buffer[64];
        const ssize_t count = read(pipe_fds[0], buffer, sizeof(buffer));
        if (count <= 0) break;
        for (ssize_t index = 0; index < count; ++index) {
          if (buffer[index] == 'R') release = false;
        }
        if (!release) break;
      }
      if (release) {
        const auto packet = emotion_bot_lite3_hw::SimplePacket(
            emotion_bot_lite3_hw::kReleaseRobotCode);
        send(socket_, packet.data(), packet.size(), MSG_NOSIGNAL);
        unlink(marker_path_.c_str());
      }
      close(pipe_fds[0]);
      _exit(release ? 3 : 0);
    }
    close(pipe_fds[0]);
    watchdog_fd_ = pipe_fds[1];
  }

  void HeartbeatWatchdog() {
    if (watchdog_fd_ >= 0) {
      const char heartbeat = 'H';
      if (write(watchdog_fd_, &heartbeat, 1) < 0 && errno != EAGAIN) {
        throw std::runtime_error("sender watchdog heartbeat failed");
      }
    }
  }

  void FinishWatchdog(bool release_confirmed = true) {
    if (watchdog_fd_ >= 0) {
      if (release_confirmed) {
        const char released = 'R';
        for (int attempt = 0; attempt < 10; ++attempt) {
          const ssize_t result = write(watchdog_fd_, &released, 1);
          if (result == 1 || (result < 0 && errno == EPIPE)) break;
          if (result < 0 && errno == EINTR) continue;
          if (result < 0 && errno == EAGAIN) {
            usleep(1000);
            continue;
          }
          break;
        }
      }
      close(watchdog_fd_);
      watchdog_fd_ = -1;
    }
    if (watchdog_pid_ > 0) {
      int status = 0;
      while (waitpid(watchdog_pid_, &status, 0) < 0 && errno == EINTR) {}
      watchdog_pid_ = -1;
    }
  }

  void PublishStatus(const std::string& state, const std::string& reason) {
    if (!status_pub_) return;
    std_msgs::String message;
    std::ostringstream value;
    value << "{\"state\":\"" << state << "\",\"owned\":"
          << (owned_ ? "true" : "false") << ",\"reason\":\"";
    for (char character : reason) {
      if (character == '\"' || character == '\\') value << '\\';
      value << character;
    }
    value << "\"}";
    message.data = value.str();
    status_pub_.publish(message);
  }

  ros::NodeHandle node_;
  ros::NodeHandle private_node_;
  ros::Subscriber command_sub_, feedback_sub_, safety_sub_;
  ros::Publisher status_pub_;
  std::mutex mutex_;
  DirectJointCommand command_;
  DirectJointFeedback feedback_;
  DirectJointSafety safety_;
  double command_at_{0.0}, feedback_at_{0.0}, safety_at_{0.0};
  bool have_command_{false}, have_feedback_{false}, have_safety_{false};
  bool transmit_enabled_{false}, takeover_transition_commissioned_{false};
  bool owned_{false}, finished_{false};
  State state_{State::kDisarmed};
  std::string target_ip_, lock_path_, marker_path_;
  int target_port_{43893};
  double sender_rate_{1000.0}, command_timeout_{0.05}, feedback_timeout_{0.05};
  double safety_timeout_{0.15}, maximum_anchor_delta_{0.08}, maximum_velocity_{0.20};
  double maximum_acceleration_{0.80}, maximum_joint_error_{0.08}, maximum_send_gap_{0.010};
  double neutral_return_time_{1.5}, neutral_hold_time_{0.35};
  double return_started_{0.0}, last_send_at_{0.0}, last_kp_{30.0}, last_kd_{0.7};
  std::array<double, 12> anchor_{}, last_position_{}, last_velocity_{}, last_acceleration_{};
  std::array<Quintic, 12> return_segments_{};
  int socket_{-1}, lock_fd_{-1}, watchdog_fd_{-1};
  pid_t watchdog_pid_{-1};
};

}  // namespace

int main(int argc, char** argv) {
  ros::init(argc, argv, "motion_sdk_bridge", ros::init_options::NoSigintHandler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, HandleSignal);
  signal(SIGTERM, HandleSignal);
  try {
    MotionSdkBridge bridge;
    return bridge.Run();
  } catch (const std::exception& error) {
    ROS_FATAL("motion_sdk_bridge: %s", error.what());
    return 2;
  }
}
